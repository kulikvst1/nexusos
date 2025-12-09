#include "Rig_control.h"
#include "bank_editor.h"
#include "OutControlComponent.h"
#include "MidiStartupShutdown.h"
//==============================================================================
void Rig_control::setOutControlComponent(OutControlComponent* oc) noexcept
{
    outControl = oc;

    if (outControl != nullptr)
    {
        outControl->onMasterGainChanged = [this](float newAvgDb)
            {
                // Если юзер сейчас держит и тащит мастер-слайдер — не лезем
                if (volumeSlider->isMouseButtonDown())
                    return;

                // иначе — привычный silent-апдейт
                int raw = (int)juce::jmap<float>(newAvgDb,
                    -60.0f, 12.0f,
                    0.0f, 127.0f);

                volumeSlider->setValue(raw, juce::dontSendNotification);
                prevVolDb = newAvgDb;
            };

        // начальная синхронизация
        float startAvgDb = 0.5f * (outControl->getGainDbL() + outControl->getGainDbR());
        int   startRaw = (int)juce::jmap<float>(startAvgDb,
            -60.0f, 12.0f,
            0.0f, 127.0f);

        volumeSlider->setValue(startRaw, juce::dontSendNotification);
        prevVolDb = startAvgDb;
        outControl->onClipChanged = [this](bool l, bool r)
            {
                clipLedL.setVisible(l);
                clipLedR.setVisible(r);
            };

    }
    else
    {
       
        outControl->onMasterGainChanged = nullptr;
        outControl->onClipChanged = nullptr;
    }

}

Rig_control::Rig_control(juce::AudioDeviceManager& adm)
    : deviceManager(adm)
{
    // --- MIDI IN ---
    auto midiInputs = juce::MidiInput::getAvailableDevices();
    for (const auto& input : midiInputs)
    {
        deviceManager.setMidiInputDeviceEnabled(input.identifier, true);
        deviceManager.addMidiInputDeviceCallback(input.identifier, this);
        DBG("MIDI IN enabled: " << input.name);
    }

    // --- Читаем сохранённый идентификатор MIDI OUT из настроек ---
    juce::String savedMidiOutId;
    juce::File settingsFile = juce::File::getSpecialLocation(
        juce::File::userDocumentsDirectory
    ).getChildFile("MyPluginAudioSettings.xml");

    if (settingsFile.existsAsFile())
    {
        juce::XmlDocument doc(settingsFile);
        if (auto xml = doc.getDocumentElement())
        {
            // JUCE сам сохраняет этот атрибут при createStateXml()
            savedMidiOutId = xml->getStringAttribute("defaultMidiOutputDevice");
        }
    }

    // --- MIDI OUT ---
    auto midiOutputs = juce::MidiOutput::getAvailableDevices();
    bool opened = false;

    // Если есть сохранённый ID — пробуем открыть его
    if (savedMidiOutId.isNotEmpty())
    {
        for (const auto& out : midiOutputs)
        {
            if (out.identifier == savedMidiOutId)
            {
                midiOut = juce::MidiOutput::openDevice(out.identifier);
                if (midiOut)
                    opened = true;
                break;
            }
        }
    }

    // Если не получилось — открываем дефолтный (индекс 4)
    if (!opened && midiOutputs.size() > 4)
    {
        const auto& selectedOut = midiOutputs[4];
        midiOut = juce::MidiOutput::openDevice(selectedOut.identifier);
        opened = (midiOut != nullptr);
    }

    // Если всё ещё не открыт — берём первый доступный
    if (!opened && !midiOutputs.isEmpty())
    {
        const auto& selectedOut = midiOutputs.getFirst();
        midiOut = juce::MidiOutput::openDevice(selectedOut.identifier);
    }

    // Если удалось открыть — отправляем стартовые команды
    if (midiOut)
    {
        if (!midiInit)
            midiInit = std::make_unique<MidiStartupShutdown>(*this);

        midiInit->sendStartupCommands();
    }


    // 1. Создаём контейнер для элементов интерфейса
    mainTab = std::make_unique<juce::Component>();
    addAndMakeVisible(mainTab.get());
    // Устанавливаем начальный размер, если компонент используется автономно
    setSize(800, 600);
    // 2. Создаём 3 кнопки-пресета (например, для групп A, B, C)
    for (int i = 0; i < 3; ++i)
    {
        auto* preset = new juce::TextButton("Preset " + juce::String(i + 1));
        preset->setClickingTogglesState(true);
        preset->setRadioGroupId(100, juce::dontSendNotification);
        preset->setToggleState(false, juce::dontSendNotification);
        preset->setColour(juce::TextButton::buttonColourId,juce::Colour::fromRGB(200, 230, 255)); // очень светлый голубой
        preset->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        preset->setColour(juce::TextButton::buttonOnColourId, juce::Colours::blue);
        preset->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        preset->addListener(this);
        presetButtons.add(preset);
        mainTab->addAndMakeVisible(preset);
        preset->setLookAndFeel(&presetLP);
    }
    // Если вы создаёте 3 кнопки-пресета, то добавляем метки для них:
    juce::Label* labels[] = { &presetLabel1_4, &presetLabel2_5, &presetLabel3_6 };
    const char* texts[] = { "preset1.4",        "preset2.5",        "preset3.6" };
    for (int i = 0; i < 3; ++i)
    {
        labels[i]->setText(texts[i], juce::dontSendNotification);
        labels[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(*labels[i]);
        labels[i]->setOpaque(true);
        // вот нужный светло-серый фон:
        labels[i]->setColour(juce::Label::backgroundColourId, juce::Colours::lightgrey);
        // и текст пусть будет тёмно-серым
        labels[i]->setColour(juce::Label::textColourId, juce::Colours::darkgrey);

    }
    // 3. Добавляем метку BANK NAME
    static BankNameKomboBox bankNameLF; // живёт всё время работы программы
    bankSelector = std::make_unique<juce::ComboBox>("Bank Selector");
    bankSelector->setLookAndFeel(&bankNameLF);
    bankSelector->setJustificationType(juce::Justification::centred);
    bankSelector->setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    bankSelector->setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    bankSelector->setColour(juce::ComboBox::textColourId, juce::Colours::white);
    // Заполняем, если уже есть bankEditor
    if (bankEditor != nullptr)
    {
        auto numBanks = bankEditor->getBanks().size();
        for (int i = 0; i < numBanks; ++i)
            bankSelector->addItem(bankEditor->getBank(i).bankName, i + 1);

        bankSelector->setSelectedId(bankEditor->getActiveBankIndex() + 1, juce::dontSendNotification);
    }
    // Обработчик выбора
    bankSelector->onChange = [this] {
        if (bankEditor != nullptr)
            bankEditor->setActiveBankIndex(bankSelector->getSelectedId() - 1);
        };

    mainTab->addAndMakeVisible(bankSelector.get());

    // 4. Создаём кнопки SHIFT, TEMPO, UP и DOWN
    shiftButton = std::make_unique<juce::TextButton>("SHIFT");
    shiftButton->setClickingTogglesState(true);
    shiftButton->setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange);
    shiftButton->addListener(this);
   // mainTab->addAndMakeVisible(shiftButton.get());

    tempoButton = std::make_unique<juce::TextButton>("TEMPO");
    tempoButton->setClickingTogglesState(false);
    tempoButton->setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange);
    tempoButton->addListener(this);
   // mainTab->addAndMakeVisible(tempoButton.get());

    upButton = std::make_unique<juce::TextButton>("UP");
    upButton->setClickingTogglesState(true);
    upButton->setColour(juce::TextButton::buttonOnColourId, juce::Colours::green);
    upButton->addListener(this);
 //   mainTab->addAndMakeVisible(upButton.get());

    downButton = std::make_unique<juce::TextButton>("DOWN");
    downButton->setClickingTogglesState(true);
    downButton->setColour(juce::TextButton::buttonOnColourId, juce::Colours::yellow);
    downButton->addListener(this);
  //  mainTab->addAndMakeVisible(downButton.get());

    for (auto* btn : { shiftButton.get(),
                       tempoButton.get(),
                       upButton.get(),
                       downButton.get() })
    {
        if (btn)
            btn->setLookAndFeel(&custom);
    }
   // 5.Метка статуса контролера FOOT
    pedalModeLabel.setText("", juce::dontSendNotification);
    pedalModeLabel.setJustificationType(juce::Justification::centred);
    pedalModeLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    pedalModeLabel.setOpaque(true);

    // по умолчанию скрыта, пока педаль не подключена
    pedalModeLabel.setVisible(false);
    addAndMakeVisible(pedalModeLabel);

    // 6. Создаём Rotary‑слайдер для Volume и его метку
    volumeLabel.setText("GAIN", juce::dontSendNotification);
    volumeLabel.setJustificationType(juce::Justification::centred);
    mainTab->addAndMakeVisible(volumeLabel);

    volumeSlider = std::make_unique<juce::Slider>("GAIN");
    volumeSlider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    volumeSlider->setRange(0, 127, 1);
    volumeSlider->setValue(64);
    volumeSlider->addListener(this);
    prevVolDb = juce::jmap<float>((float)volumeSlider->getValue(),
        0.0f, 127.0f,
        -60.0f, 12.0f);
    volumeSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    volumeSlider->setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::red);
    volumeSlider->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::white);
    volumeSlider->setColour(juce::Slider::textBoxTextColourId, juce::Colours::black);
    mainTab->addAndMakeVisible(volumeSlider.get());
    // LooperComponent создаём ТОЛЬКО если движок уже передан
    if (enginePtr != nullptr)
    {
        looperComponent = std::make_unique<LooperComponent>(*enginePtr);
        addAndMakeVisible(looperComponent.get());
        looperComponent->setVisible(false); // по умолчанию скрыт
    }
    // Tuner button
    tunerBtn.setClickingTogglesState(true);
    tunerBtn.addListener(this);
    //  addAndMakeVisible(tunerBtn);
    // Stomp button
    // addAndMakeVisible(stompBtn);
    stompBtn.setClickingTogglesState(true);
    stompBtn.addListener(this);
    stompBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colours::darkred);
    stompBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);

   // пик метки выхода 
    clipLedL.setText("CLIP", juce::dontSendNotification);
    clipLedR.setText("CLIP", juce::dontSendNotification);

    clipLedL.setColour(juce::Label::backgroundColourId, juce::Colours::red);
    clipLedR.setColour(juce::Label::backgroundColourId, juce::Colours::red);

    clipLedL.setJustificationType(juce::Justification::centred);
    clipLedR.setJustificationType(juce::Justification::centred);

    clipLedL.setVisible(false);
    clipLedR.setVisible(false);

    addAndMakeVisible(clipLedL);
    addAndMakeVisible(clipLedR);
    //пик метки входа
    for (auto* l : { &inClipLedL, &inClipLedR })
    {
        l->setText("CLIP", juce::dontSendNotification);
        l->setJustificationType(juce::Justification::centred);
        l->setColour(juce::Label::backgroundColourId, juce::Colours::red);
        l->setColour(juce::Label::textColourId, juce::Colours::white);
        l->setVisible(false);
        addAndMakeVisible(*l);
    }

    /// SWITCH CONTROL OUT
    const std::array<int, 4> sButtonCCs = { 116, 115, 118, 117 };

    for (int i = 0; i < 4; ++i)
    {
        sButtons[i] = std::make_unique<juce::TextButton>("S" + juce::String(i + 1));
        addAndMakeVisible(*sButtons[i]);
        sButtons[i]->setLookAndFeel(&SWbutoon);
        int ccNumber = sButtonCCs[i]; // сохраняем CC в локальную переменную

        // Реакция на любое изменение состояния кнопки
        sButtons[i]->onStateChange = [this, i, ccNumber]()
            {
                if (!midiOut) return;

                bool pressed = sButtons[i]->isDown(); // true при нажатии
                int value = pressed ? 127 : 0;

                midiOut->sendMessageNow(
                    juce::MidiMessage::controllerEvent(1, ccNumber, value) // канал 1
                );

                DBG("Sent S" << (i + 1) << " CC" << ccNumber << " value " << value);
            };
    }

    // Настраиваем S4 по умолчанию
    sButtons[3]->setButtonText("SHIFT\n(TUNER)");
    sButtons[3]->setColour(juce::TextButton::buttonColourId, juce::Colours::lightgrey);
    sButtons[3]->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    sButtons[3]->setColour(juce::TextButton::textColourOnId, juce::Colours::black);

    // вместо looperBtn.setToggleState(...)
    setLooperState(false);

    updateAllSButtons();

    
}

Rig_control::~Rig_control()
{
    stopTimer();
    midiOut.reset();
    if (bankEditor != nullptr)
    {
        bankEditor->onBankEditorChanged = nullptr;
        bankEditor->onActivePresetChanged = nullptr;
        bankEditor->setMidiOutput(nullptr);
    }

    if (hostComponent != nullptr)
    {
        // hostComponent->setParameterChangeCallback(nullptr);
        // hostComponent->setPresetCallback(nullptr);
        // hostComponent->setLearnCallback(nullptr);
        // hostComponent->setBpmDisplayLabel(nullptr);
    }

    for (auto* btn : presetButtons)
    {
        btn->removeListener(this);
        btn->setLookAndFeel(nullptr);
    }

    if (shiftButton) { shiftButton->removeListener(this); shiftButton->setLookAndFeel(nullptr); }
    if (tempoButton) { tempoButton->removeListener(this); tempoButton->setLookAndFeel(nullptr); }
    if (upButton) { upButton->removeListener(this);    upButton->setLookAndFeel(nullptr); }
    if (downButton) { downButton->removeListener(this);  downButton->setLookAndFeel(nullptr); }
    if (bankSelector) bankSelector->setLookAndFeel(nullptr);

    // Сначала убираем MIDI init, потом сам output
    midiInit.reset();
    midiOut.reset();
}
void Rig_control::resized()
{
    // Вся доступная область компонента
    auto fullArea = getLocalBounds();

    // mainTab занимает всю область
    mainTab->setBounds(fullArea);

    // Внутренний отступ от краёв mainTab (можно уменьшить до 0 для плотной компоновки)
    const int margin = 2;
    auto content = mainTab->getLocalBounds().reduced(margin);

    // Настройка сетки: 9 колонок × 4 строки
    constexpr int numCols = 9;
    constexpr int numRows = 4;

    // Вычисляем размеры ячеек
    int usableWidth = content.getWidth();
    int sectorWidth = usableWidth / numCols;
    int extra = usableWidth - (sectorWidth * numCols); // остаток ширины
    int sectorHeight = content.getHeight() / numRows;

    // Предварительно создаём прямоугольники для всех ячеек сетки
    std::vector<juce::Rectangle<int>> sectors;
    sectors.reserve(numCols * numRows);
    for (int row = 0; row < numRows; ++row)
    {
        int y = content.getY() + row * sectorHeight;
        int x = content.getX();
        for (int col = 0; col < numCols; ++col)
        {
            int extraWidth = (col < extra ? 1 : 0); // распределяем остаток ширины
            int w = sectorWidth + extraWidth;
            sectors.push_back(juce::Rectangle<int>(x, y, w, sectorHeight));
            x += w;
        }
    }

    // Утилиты для получения прямоугольников
    auto getSectorRect = [&sectors](int sectorNumber) -> juce::Rectangle<int>
        {
            return sectors[sectorNumber - 1]; // нумерация с 1
        };

    auto getUnionRect = [&sectors](int startSector, int endSector) -> juce::Rectangle<int>
        {
            const auto& r1 = sectors[startSector - 1];
            const auto& r2 = sectors[endSector - 1];
            int x = r1.getX();
            int y = r1.getY();
            int width = r2.getRight() - x;
            return juce::Rectangle<int>(x, y, width, r1.getHeight());
        };
    // ─── INPUT CLIP ───
    {
        auto inputSector = getSectorRect(1).reduced(1);
        int gap = 4;
        int halfW = (inputSector.getWidth() - gap) / 2;

        // делаем их пониже: 1/8 сектора (как у выхода)
        int clipH = inputSector.getHeight() / 8;
        int yClip = inputSector.getY(); // верх сектора

        inClipLedL.setBounds(inputSector.getX(), yClip, halfW, clipH);
        inClipLedR.setBounds(inputSector.getX() + halfW + gap, yClip, halfW, clipH);

        inClipLedL.setJustificationType(juce::Justification::centred);
        inClipLedR.setJustificationType(juce::Justification::centred);

        inClipLedL.setFont(juce::Font((float)clipH * 0.6f, juce::Font::bold));
        inClipLedR.setFont(juce::Font((float)clipH * 0.6f, juce::Font::bold));
    }

    // ─── PEDAL MODE LABEL ───
    {
        auto sector0 = getSectorRect(1).reduced(1);
        int w = sector0.getWidth();
        int h = sector0.getHeight() / 3; // размер оставляем прежний

        // смещаем вниз: под CLIP‑метками
        int yPedal = sector0.getY() + (sector0.getHeight() / 8) + 4; // clipH + зазор

        juce::Rectangle<int> modeBounds(sector0.getX(), yPedal, w, h);

        pedalModeLabel.setBounds(modeBounds);
        pedalModeLabel.setJustificationType(juce::Justification::centred);
        pedalModeLabel.setOpaque(false);
        pedalModeLabel.setFont(juce::Font((float)h * 0.6f, juce::Font::bold));
    }

// ─── VOLUME ───
auto volumeSector = getSectorRect(9).reduced(1);

// слайдер оставляем во весь сектор
if (volumeSlider)
{
    auto sliderArea = volumeSector.reduced(0, volumeSector.getHeight() * 0.1f);
    // сверху и снизу по 10% убрали
    volumeSlider->setBounds(sliderArea);
}
// метки CLIP — половина ширины сектора каждая, но с зазором
int gap = 4;
int halfW = (volumeSector.getWidth() - gap) / 2;

// делаем их пониже: 1/8 сектора вместо 1/6
int clipH = volumeSector.getHeight() / 8;

// размещаем в верхней части сектора
int yClip = volumeSector.getY();

clipLedL.setBounds(volumeSector.getX(), yClip, halfW, clipH);
clipLedR.setBounds(volumeSector.getX() + halfW + gap, yClip, halfW, clipH);

clipLedL.setJustificationType(juce::Justification::centred);
clipLedR.setJustificationType(juce::Justification::centred);

clipLedL.setFont(juce::Font((float)clipH * 0.6f, juce::Font::bold));
clipLedR.setFont(juce::Font((float)clipH * 0.6f, juce::Font::bold));

// метка громкости остаётся как была
int volumeLabelW = volumeSector.getWidth() * 2;
int volumeLabelH = 40;
juce::Rectangle<int> volumeLabelRect(volumeLabelW, volumeLabelH);
volumeLabelRect.setCentre(volumeSector.getCentre());
volumeLabel.setBounds(volumeLabelRect);
volumeLabel.setFont(juce::Font(volumeSector.getHeight() * 0.17f, juce::Font::bold));


    // ─── КНОПКИ ПРЕСЕТОВ ───
    juce::Rectangle<int> preset1Bounds, preset2Bounds, preset3Bounds;
    if (presetButtons.size() > 0)
    {
        preset1Bounds = getUnionRect(28, 30).reduced(1);
        presetButtons[0]->setBounds(preset1Bounds);
    }
    if (presetButtons.size() > 1)
    {
        preset2Bounds = getUnionRect(31, 33).reduced(1);
        presetButtons[1]->setBounds(preset2Bounds);
    }
    if (presetButtons.size() > 2)
    {
        preset3Bounds = getUnionRect(34, 36).reduced(1);
        presetButtons[2]->setBounds(preset3Bounds);
    }

    // ─── BANK NAME ───
    auto bankRect = getUnionRect(2, 8).reduced(1);
    if (bankSelector)
        bankSelector->setBounds(bankRect);


    // ─── МЕТКИ ПРЕСЕТОВ ───
    std::array<juce::Label*, 3> labels = { &presetLabel1_4, &presetLabel2_5, &presetLabel3_6 };
    std::array<juce::Rectangle<int>, 3> bounds = { preset1Bounds, preset2Bounds, preset3Bounds };

    auto layoutPresetLabel = [&](juce::Label& lbl, const juce::Rectangle<int>& area)
        {
            int w = int(area.getWidth() / 1.5f);
            int h = int(area.getHeight() / 4.0f);
            juce::Rectangle<int> r(area.getRight() - w, area.getBottom() - h, w, h);
            lbl.setBounds(r);
            lbl.setJustificationType(juce::Justification::centred);
            lbl.setColour(juce::Label::textColourId, juce::Colours::grey);
            lbl.setColour(juce::Label::backgroundColourId, juce::Colours::lightgrey);
            lbl.setOpaque(true);
            float fontSize = r.getHeight() * 0.9f;
            lbl.setFont(juce::Font(fontSize, juce::Font::bold));
        };

    for (int i = 0; i < 3; ++i)
    {
        if (i < presetButtons.size())
            layoutPresetLabel(*labels[i], bounds[i]);
    }

    // ─── КНОПКИ UP / DOWN / STOMP ───
    {
        auto sec20 = getSectorRect(20).reduced(1);
        int btnW = sec20.getWidth() / 3;
        int btnH = sec20.getHeight() / 2;

        auto upArea = sec20.removeFromLeft(btnW);
        auto downArea = sec20.removeFromLeft(btnW);
        auto stompArea = sec20;

        if (upButton)   upButton->setBounds(upArea.withSizeKeepingCentre(btnW, btnH));
        if (downButton) downButton->setBounds(downArea.withSizeKeepingCentre(btnW, btnH));
        stompBtn.setBounds(stompArea.withSizeKeepingCentre(btnW, btnH));
    }

    // ─── КНОПКИ LOOPER / TUNER / SHIFT / TEMPO ───
    {
        auto sec26 = getSectorRect(26).reduced(1);
        int btnW = sec26.getWidth() / 4;
        int btnH = sec26.getHeight() / 2;

        auto looperArea = sec26.removeFromLeft(btnW);
        auto tunerArea = sec26.removeFromLeft(btnW);
        auto shiftArea = sec26.removeFromLeft(btnW);
        auto tempoArea = sec26;

        // looperBtn.setBounds(looperArea.withSizeKeepingCentre(btnW, btnH));
        tunerBtn.setBounds(tunerArea.withSizeKeepingCentre(btnW, btnH));
        if (shiftButton) shiftButton->setBounds(shiftArea.withSizeKeepingCentre(btnW, btnH));
        if (tempoButton) tempoButton->setBounds(tempoArea.withSizeKeepingCentre(btnW, btnH));
    }

    // ─── ПЕРЕДНИЙ ПЛАН ───
    // looperBtn.toFront(false);
    tunerBtn.toFront(false);
    stompBtn.toFront(false);

    // ─── LOOPER и TUNER ───
    if (!enginePtr)
    {
        if (looperComponent)
            looperComponent->setBounds(0, 0, 0, 0);
        return;
    }

    auto m2 = 1;
    auto topRow = getUnionRect(11, 17).reduced(m2);
    auto bottomRow = getUnionRect(20, 25).reduced(m2);
    juce::Rectangle<int> sharedArea{
        topRow.getX(), topRow.getY(),
        topRow.getWidth(),
        bottomRow.getBottom() - topRow.getY()
    };

    if (looperComponent)
        looperComponent->setBounds(looperComponent->isVisible() ? sharedArea
            : juce::Rectangle<int>());

    if (externalTuner)
        externalTuner->setBounds(externalTuner->isVisible() ? sharedArea
            : juce::Rectangle<int>());


    // ─── КНОПКИ SWITCH (S1–S4) ───
    if (sButtons[0])
        sButtons[0]->setBounds(getSectorRect(10).reduced(1)); // S1

    if (sButtons[1])
        sButtons[1]->setBounds(getSectorRect(19).reduced(1)); // S2

    if (sButtons[2])
        sButtons[2]->setBounds(getSectorRect(18).reduced(1)); // S3

    if (sButtons[3])
        sButtons[3]->setBounds(getSectorRect(27).reduced(1)); // S4

    // Обновляем визуальное состояние кнопок
    updateAllSButtons();
}
// Новый метод для установки BankEditor и подписки на его изменения:
void Rig_control::setBankEditor(BankEditor* editor)
{
    bankEditor = editor;
    if (!bankEditor)
        return;

    if (bankSelector)
    {
        bankSelector->onChange = [this] {
            if (bankEditor)
                bankEditor->setActiveBankIndex(bankSelector->getSelectedId() - 1);
            };
    }

    bankEditor->onActivePresetChanged = [this](int)
        {
            if (stompMode)
                updateStompDisplays();
            else
                updatePresetDisplays();
        };

    bankEditor->onBankChanged = [this]()
        {
            shiftButton->setToggleState(false, juce::dontSendNotification);
            sendShiftState();
            stompMode = false;
            stompBtn.setToggleState(false, juce::dontSendNotification);

            for (auto* btn : presetButtons)
                btn->setRadioGroupId(100, juce::dontSendNotification);
            sendStompState();
            updateAllSButtons();
            updatePresetDisplays();
        };

    bankEditor->onBankEditorChanged = [this]()
        {
            if (stompMode)
                updateStompDisplays();
            else
                updatePresetDisplays();
        };

    shiftButton->setClickingTogglesState(true);
    shiftButton->onClick = [this]()
        {
            if (stompMode)
                updateStompDisplays();
            else
                updatePresetDisplays();
        };

    if (stompMode)
        updateStompDisplays();
    else
        updatePresetDisplays();
    
}


void Rig_control::updatePresetDisplays()
{
    if (!bankEditor || stompMode) // 🚫 не обновляем пресетный UI, если активен стомп
        return;

    const int bankIdx = bankEditor->getActiveBankIndex();
    const int active = bankEditor->getActivePresetIndex();
    auto names = bankEditor->getPresetNames(bankIdx);
    if (names.size() < 6)
        return;

    // 0) Решаем, включён ли сейчас Shift:
    const bool wantShift = manualShift
        ? shiftButton->getToggleState()
        : (active >= 3);

    // синхронизируем саму кнопку (без колбеков)
    shiftButton->setToggleState(wantShift, juce::dontSendNotification);
    const bool shiftOn = wantShift;
    sendShiftState();
    // 1) Имя банка
    if (bankSelector)
    {
        bankSelector->clear(juce::dontSendNotification);

        const auto& banksList = bankEditor->getBanks();
        for (int i = 0; i < (int)banksList.size(); ++i)
            bankSelector->addItem(banksList[i].bankName, i + 1);

        // Устанавливаем ID
        bankSelector->setSelectedId(bankIdx + 1, juce::dontSendNotification);

        // Принудительно обновляем текст, даже если ID тот же
        bankSelector->setText(bankEditor->getBank(bankIdx).bankName,
            juce::dontSendNotification);

        bankSelector->repaint();
    }

    // 2) Сбросим и заполним тексты/состояния трёх кнопок и меток
    std::array<juce::Button*, 3> btns = {
        presetButtons[0], presetButtons[1], presetButtons[2]
    };
    std::array<juce::Label*, 3> labs = {
        &presetLabel1_4, &presetLabel2_5, &presetLabel3_6
    };

    for (int i = 0; i < 3; ++i)
    {
        int btnIdx = shiftOn ? (3 + i) : i;
        int lblIdx = shiftOn ? i : (3 + i);

        // 🔹 Сброс цветовой схемы на «пресетную»
        btns[i]->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(200, 230, 255));
        btns[i]->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        btns[i]->setColour(juce::TextButton::buttonOnColourId, juce::Colours::blue);
        btns[i]->setColour(juce::TextButton::textColourOnId, juce::Colours::white);

        btns[i]->setButtonText(names[btnIdx]);
        btns[i]->setToggleState(false, juce::dontSendNotification);

        labs[i]->setText(names[lblIdx], juce::dontSendNotification);
        labs[i]->setOpaque(true);
        labs[i]->setColour(juce::Label::backgroundColourId, juce::Colours::lightgrey);
        labs[i]->setColour(juce::Label::textColourId, juce::Colours::darkgrey);
    }

    // 3) Подсветить единственный active
    if (active < 3)
    {
        if (shiftOn)
        {
            labs[active]->setColour(juce::Label::backgroundColourId, juce::Colours::blue);
            labs[active]->setColour(juce::Label::textColourId, juce::Colours::white);
        }
        else
        {
            btns[active]->setToggleState(true, juce::dontSendNotification);
        }
    }
    else
    {
        int i = active - 3;
        if (shiftOn)
            btns[i]->setToggleState(true, juce::dontSendNotification);
        else
        {
            labs[i]->setColour(juce::Label::backgroundColourId, juce::Colours::blue);
            labs[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        }
    }
    // --- MIDI OUT при смене пресета ---
 
    if (active != lastSentPresetIndex) // пресет реально сменился
    {
        lastSentPresetIndex = active;

        if (midiOut)
        {
            int presetNumber = active + 1; // 1..6
            midiOut->sendMessageNow(
                juce::MidiMessage::controllerEvent(2, presetNumber, 127)
            );
          
        }
    }
   
    updateAllSButtons();
    repaint();
}

void Rig_control::buttonClicked(juce::Button* button)
{
    // --- Переключение банка UP / DOWN ---
    if (button == upButton.get() || button == downButton.get())
    {
        // 1. Сколько всего банков?
        auto totalBanks = (int)bankEditor->getBanks().size();

        // 2. Текущий индекс
        auto idx = bankEditor->getActiveBankIndex();

        // 3. Новый
        if (button == upButton.get())
            idx = (idx + 1) % totalBanks;
        else idx = (idx + totalBanks - 1) % totalBanks;

        // 4. По железу меняем банк
        bankEditor->setActiveBankIndex(idx);

        // UI подтянется через onBankEditorChanged → updatePresetDisplays()
        return;
    }
    if (button == shiftButton.get())
    {
        manualShift = true;
        updatePresetDisplays();
        updateAllSButtons();
        return;
    }
     // Переключение Stomp режима
    if (button == &stompBtn)
    {
        sendShiftState();
        stompMode = stompBtn.getToggleState();
        sendStompState(); // ← отправляем сразу
        if (stompMode)
        {
            // Запоминаем состояние Shift в пресет-режиме
            presetShiftState = shiftButton->getToggleState();

            // Выключаем Shift для стомп-режима
            shiftButton->setToggleState(false, juce::dontSendNotification);

            // В стомп-режиме пресет-кнопки не радиогруппа
            for (auto* btn : presetButtons)
                btn->setRadioGroupId(0, juce::dontSendNotification);
            sendShiftState();
            updateStompDisplays();
        }
        else
        {
            // Возвращаем состояние Shift, которое было в пресет-режиме
            shiftButton->setToggleState(presetShiftState, juce::dontSendNotification);

            // В пресет-режиме кнопки снова радиогруппа
            for (auto* btn : presetButtons)
                btn->setRadioGroupId(100, juce::dontSendNotification);

            updatePresetDisplays();
        }
        updateAllSButtons();
        return;
    }

    // Клик по кнопке пресета в стомп-режиме
    if (stompMode && presetButtons.contains(static_cast<juce::TextButton*>(button)))
    {
        int idx = presetButtons.indexOf(static_cast<juce::TextButton*>(button));
        if (idx >= 0 && bankEditor)
        {
            const int bankIdx = bankEditor->getActiveBankIndex();
            const int presetIdx = bankEditor->getActivePresetIndex();
            const bool shiftOn = shiftButton->getToggleState();
            int ccIndex = shiftOn ? (idx + 3) : idx; // 0..5

            bool currentState = bankEditor->getBank(bankIdx).ccPresetStates[presetIdx][ccIndex];
            bankEditor->updateCCParameter(ccIndex, !currentState);

            updateStompDisplays();
        }
        return;
    }

    // Обработка preset-кнопок (RadioGroup ID == 100)
    if (button->getRadioGroupId() == 100)
    {
        auto* btn = static_cast<juce::TextButton*>(button);
        int  idx = presetButtons.indexOf(btn);
        if (idx >= 0 && btn->getToggleState())
        {
            manualShift = true;  // переход на эту страницу — ручной режим

            bool shiftOn = shiftButton->getToggleState();
            int  presetIndex = shiftOn ? (idx + 3) : idx;

            if (bankEditor)
                bankEditor->setActivePreset(presetIndex);

            updatePresetDisplays();

            if (presetChangeCb)
                presetChangeCb(presetIndex);
        }
        return;
    }
    if (button == tempoButton.get())
    {
        if ((hostComponent == nullptr) || (hostComponent->getPluginInstance() == nullptr))
        {
            hostComponent->updateBPM(120.0); // дефолтный темп, уйдёт в MIDI через onBpmChanged
            return;
        }

        tapTempo.tap();
        double newBpm = tapTempo.getBpm();
        hostComponent->updateBPM(newBpm); // уйдёт в MIDI через onBpmChanged
    }
    // Tuner
    if (button == &tunerBtn && externalTuner != nullptr)
    {
        const bool show = !externalTuner->isVisible();
        tunerBtn.setToggleState(show, juce::dontSendNotification);

        // прячем лупер
        if (looperComponent) looperComponent->setVisible(false);

        // показываем/прячем тюнер
        externalTuner->setVisible(show);
        if (show) externalTuner->toFront(false);

        resized();

        // уведомляем MainContentComponent
        if (onTunerVisibilityChanged)
            onTunerVisibilityChanged(show);

        return;
    }
    
}

void Rig_control::sliderValueChanged(juce::Slider* slider)
{
    if (slider == volumeSlider.get())
    {
        // 1) абсолютное новое значение мастера в дБ
        float newVolDb = juce::jmap<float>(
            (float)volumeSlider->getValue(),  // raw 0…127
            0.0f, 127.0f,
            -60.0f, 12.0f
        );

        // 2) читаем текущие дБ каналов
        float leftDb = outControl ? outControl->getGainDbL() : prevVolDb;
        float rightDb = outControl ? outControl->getGainDbR() : prevVolDb;

        // 3) вычисляем фазу роста/падения
        float maxCh = juce::jmax(leftDb, rightDb);
        float minCh = juce::jmin(leftDb, rightDb);
        float deltaDb = 0.0f;

        if (newVolDb > maxCh)      deltaDb = newVolDb - maxCh;
        else if (newVolDb < minCh) deltaDb = newVolDb - minCh;

        // 4) сдвигаем оба канала только если есть delta
        if (deltaDb != 0.0f && outControl)
            outControl->offsetGainDb(deltaDb);

        // 5) сохраняем текущее мастера
        prevVolDb = newVolDb;

        // 6) Отправка обратно на контроллер (Ch5 CC1)
        int midiValue = (int)volumeSlider->getValue();
        sendMidiCC(5, 1, midiValue);

        return;
    }
}

void Rig_control::timerCallback()
{
    // Реализация периодических обновлений (если нужно).
}
void Rig_control::handleExternalPresetChange(int newPresetIndex) noexcept
{
    manualShift = false;           // сброс ручного режима
    updatePresetDisplays();        // перерисовать Shift-кнопку и пресеты
}
void Rig_control::setTunerComponent(TunerComponent* t) noexcept
{
    externalTuner = t;
    if (externalTuner)
    {
        addAndMakeVisible(*externalTuner);    // вставляем в иерархию Rig_control
        externalTuner->setVisible(false);     // по умолчанию скрыт
    }
}
void Rig_control::setLooperComponent(LooperComponent* l) noexcept
{
    externalLooper = l;
}

void Rig_control::updateStompDisplays()
{
    // Сразу отправляем актуальное состояние SHIFT
    sendShiftState();

    if (!bankEditor) return;

    const int bankIdx = bankEditor->getActiveBankIndex();
    const int presetIdx = bankEditor->getActivePresetIndex();
    const bool shiftOn = shiftButton->getToggleState();

    std::array<juce::Button*, 3> btns = {
        presetButtons[0], presetButtons[1], presetButtons[2]
    };
    std::array<juce::Label*, 3> labs = {
        &presetLabel1_4, &presetLabel2_5, &presetLabel3_6
    };

    for (int i = 0; i < 3; ++i)
    {
        int btnCC = shiftOn ? (i + 3) : i;
        int lblCC = shiftOn ? i : (i + 3);

        // --- КНОПКА ---
        juce::String btnName = bankEditor->getCCName(btnCC);
        if (btnName.isEmpty())
            btnName = "CC" + juce::String(btnCC + 1);
        btns[i]->setButtonText(btnName);

        bool btnActive = bankEditor->getBank(bankIdx).ccPresetStates[presetIdx][btnCC];

        btns[i]->setColour(juce::TextButton::buttonColourId, btnActive ? juce::Colours::red : juce::Colours::black);
        btns[i]->setColour(juce::TextButton::buttonOnColourId, btnActive ? juce::Colours::red : juce::Colours::black);
        btns[i]->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        btns[i]->setColour(juce::TextButton::textColourOnId, juce::Colours::white);

        btns[i]->setToggleState(btnActive, juce::dontSendNotification);

        // --- МЕТКА ---
        juce::String lblName = bankEditor->getCCName(lblCC);
        if (lblName.isEmpty())
            lblName = "CC" + juce::String(lblCC + 1);
        labs[i]->setText(lblName, juce::dontSendNotification);

        bool lblActive = bankEditor->getBank(bankIdx).ccPresetStates[presetIdx][lblCC];
        labs[i]->setOpaque(true);
        labs[i]->setColour(juce::Label::backgroundColourId, lblActive ? juce::Colours::red : juce::Colours::lightgrey);
        labs[i]->setColour(juce::Label::textColourId, lblActive ? juce::Colours::white : juce::Colours::darkgrey);

        // --- MIDI OUT для кнопки ---
        if (midiOut)
        {
            // Приводим к диапазону 1..3
            int globalCC = (btnCC % 3) + 1;

            int value = btnActive ? 127 : 0;

            midiOut->sendMessageNow(
                juce::MidiMessage::controllerEvent(3, globalCC, value) // канал 3
            );

        }

    }

    updateAllSButtons();
    repaint();
}

/// SWITCH CONTROL
void Rig_control::updateSButton(int index, const juce::String& text, juce::Colour colour)
{
    if (index < 0 || index >= (int)sButtons.size() || !sButtons[index])
        return;

    auto& btn = sButtons[index];
    btn->setButtonText(text);
    btn->setColour(juce::TextButton::buttonColourId, colour);
    btn->setColour(juce::TextButton::buttonOnColourId, colour);
    btn->repaint();
}
void Rig_control::updateAllSButtons()
{
    // S1
    updateSButton(0, "UP", juce::Colours::green);

    // S2
    if (stompMode)
        updateSButton(1, "STOMP\n(DOWN)", juce::Colours::red);
    else
        updateSButton(1, "DOWN\n(STOMP)", juce::Colours::green);

    // S3
    if (embeddedLooperVisible) // встроенный Looper внутри Rig
    {
        if (looperActive)
            updateSButton(2, "CLEAN\n(LOOPER OFF)", juce::Colours::darkgrey);
        else
            updateSButton(2, "TAP\n(LOOPER)", juce::Colours::steelblue);
    }
    else
    {
        // если вкладка Looper есть → кнопка всегда по умолчанию
        updateSButton(2, "TAP\n(LOOPER)", juce::Colours::steelblue);
    }

    // S4
    if (!shiftButton)
        return;

    if (looperComponent && looperComponent->isVisible())
    {
        auto label = looperComponent->getControlButtonText();
        auto color = looperComponent->getControlButtonColor();
        updateSButton(3, label, color);
    }
    else
    {
        auto color = shiftButton->getToggleState()
            ? juce::Colours::gold
            : juce::Colours::lightgrey;

        updateSButton(3, "SHIFT\n(TUNER)", color);
    }
}


void Rig_control::bankUp()
{
    auto totalBanks = (int)bankEditor->getBanks().size();
    auto idx = bankEditor->getActiveBankIndex();

    idx = (idx + 1) % totalBanks;
   
    bankEditor->setActiveBankIndex(idx);
    // UI подтянется через onBankEditorChanged → updatePresetDisplays()
}

void Rig_control::bankDown()
{
    auto totalBanks = (int)bankEditor->getBanks().size();
    auto idx = bankEditor->getActiveBankIndex();

    idx = (idx + totalBanks - 1) % totalBanks;
  
    bankEditor->setActiveBankIndex(idx);
    // UI подтянется через onBankEditorChanged → updatePresetDisplays()
}

void Rig_control::selectPreset(int index)
{
    if (index >= 0 && index < presetButtons.size())
    {
        // Включаем только выбранный пресет, остальные выключаем
        for (int i = 0; i < presetButtons.size(); ++i)
            presetButtons[i]->setToggleState(i == index, juce::sendNotification);
    }

}

void Rig_control::setShiftState(bool on)
{
    if (shiftButton != nullptr)
        shiftButton->setToggleState(on, juce::sendNotification);
}
void Rig_control::sendShiftState()
{
    if (!midiOut || !shiftButton) return;

    static bool lastSentShift = false; // защита от дублей
    bool shiftOn = shiftButton->getToggleState();

    if (shiftOn != lastSentShift)
    {
        lastSentShift = shiftOn;
        int value = shiftOn ? 127 : 0;

        midiOut->sendMessageNow(
            juce::MidiMessage::controllerEvent(1, 101, value) // канал 1, CC101
        );

    }
}
void Rig_control::sendStompState()
{
    if (!midiOut) return;

    static bool lastSentStomp = false; // защита от дублей
    bool stompOn = stompMode; // или stompBtn.getToggleState()

    if (stompOn != lastSentStomp)
    {
        lastSentStomp = stompOn;
        int value = stompOn ? 127 : 0;

        midiOut->sendMessageNow(
            juce::MidiMessage::controllerEvent(1, 100, value) // канал 1, CC100
        );

    }
}

void Rig_control::setStompState(bool on)
{
    stompBtn.setToggleState(on, juce::sendNotification);
}

void Rig_control::setLooperState(bool on)
{
    // Флаг состояния
    looperActive = on;

    // Движок
    if (enginePtr)
        enginePtr->setMode(on ? LooperEngine::Mode::Looper
            : LooperEngine::Mode::Player);

    // Встроенный Looper внутри Rig показываем ТОЛЬКО если он разрешён и включён
    if (looperComponent)
        looperComponent->setVisible(embeddedLooperVisible && looperActive);

    // MIDI
    if (midiOut)
        midiOut->sendMessageNow(
            juce::MidiMessage::controllerEvent(1, 102, on ? 127 : 0));

    if (onLooperVisibilityChanged)
        onLooperVisibilityChanged(on);
}

void Rig_control::setEmbeddedLooperVisible(bool shouldShow)
{
    embeddedLooperVisible = shouldShow;

    if (looperComponent)
        looperComponent->setVisible(embeddedLooperVisible && looperActive);
}
void Rig_control::sendLooperOn()
{
    if (midiOut)
        midiOut->sendMessageNow(juce::MidiMessage::controllerEvent(1, 102, 127));
}

void Rig_control::sendLooperOff()
{
    if (midiOut)
        midiOut->sendMessageNow(juce::MidiMessage::controllerEvent(1, 102, 0));
}
void Rig_control::toggleLooper()
{
    bool newState = !looperActive; // переключаем состояние напрямую
    setLooperState(newState);
}

void Rig_control::sendLooperModeToMidi(LooperEngine::State state)
{
    switch (state)
    {
    case LooperEngine::Recording: sendLooperModeToMidi(LooperMode::Record); break; // CC21
    case LooperEngine::Stopped:   sendLooperModeToMidi(LooperMode::Stop);   break; // CC22
    case LooperEngine::Playing:   sendLooperModeToMidi(LooperMode::Play);   break; // CC23
    case LooperEngine::Clean:     sendLooperClearToMidi();                  break; // CC20
    }
}
void Rig_control::setLooperEngine(LooperEngine& eng) noexcept
{
    enginePtr = &eng;

    // Смена состояния внутри лупера
    enginePtr->onStateChanged = [this](LooperEngine::State s)
        {
            sendLooperModeToMidi(s);
        };

    // Очистка
    enginePtr->onCleared = [this]()
        {
            sendLooperClearToMidi();
        };

    // Смена режима Looper/Player
    enginePtr->onModeChanged = [this](LooperEngine::Mode m)
        {
            if (m == LooperEngine::Mode::Looper)
            {
                // Отправляем текущее состояние лупера, включая Clean
                sendLooperModeToMidi(enginePtr->getState());
            }
            else // Player
            {
                if (!enginePtr->isReady())
                {
                    // Файл не загружен → Clean
                    sendLooperClearToMidi(); // CC20
                }
                else
                {
                    // Файл есть → Stop или Play
                    if (enginePtr->isPlaying())
                        sendLooperModeToMidi(LooperMode::Play); // CC23
                    else
                        sendLooperModeToMidi(LooperMode::Stop); // CC22
                }
            }
        };

    if (!looperComponent)
    {
        looperComponent = std::make_unique<LooperComponent>(*enginePtr);
        looperComponent->setRigControl(this);
        addAndMakeVisible(*looperComponent);
        looperComponent->setVisible(false);
    }
}

void Rig_control::sendLooperModeToMidi(LooperMode mode)
{
    if (!midiOut) return;

    int cc = 0;
    switch (mode)
    {
    case LooperMode::Record: cc = 21; break;
    case LooperMode::Stop:   cc = 22; break;
    case LooperMode::Play:   cc = 23; break;
    }
    midiOut->sendMessageNow(juce::MidiMessage::controllerEvent(1, cc, 127));
}
void Rig_control::sendPlayerModeToMidi(bool isPlaying)
{
    if (!midiOut) return;

    int cc = isPlaying ? 23 : 22; // те же CC, что и у лупера
    midiOut->sendMessageNow(juce::MidiMessage::controllerEvent(1, cc, 127));
}


void Rig_control::sendLooperClearToMidi()
{
    if (!midiOut) return;

    midiOut->sendMessageNow(juce::MidiMessage::controllerEvent(1, 20, 127));
    midiOut->sendMessageNow(juce::MidiMessage::controllerEvent(1, 20, 0));
   }

void Rig_control::setTunerState(bool on)
{
    tunerBtn.setToggleState(on, juce::sendNotification);
    if (externalTuner)
    {
        externalTuner->setVisible(on);
        if (onTunerVisibilityChanged)
            onTunerVisibilityChanged(on);
    }
}

void Rig_control::toggleTuner()
{
    bool newState = !tunerBtn.getToggleState();
    setTunerState(newState);
}
void Rig_control::sendMidiCC(int channel, int cc, int value)
{
    if (midiOut)
    {
        auto msg = juce::MidiMessage::controllerEvent(channel, cc, value);
        midiOut->sendMessageNow(msg);
    }
}

void Rig_control::handleIncomingMidiMessage(juce::MidiInput* /*source*/, const juce::MidiMessage& message)
{
    if (!message.isController())
        return;

    const int cc = message.getControllerNumber();
    const int value = message.getControllerValue();
    const int channel = message.getChannel();
    const bool isOn = (value == 127);

    // Делаем копию message, чтобы можно было безопасно использовать внутри лямбды
    auto msgCopy = message;

    juce::MessageManager::callAsync([this, cc, channel, isOn, msgCopy, value]()
        {
            // --- Bank Up ---
            if (channel == 1 && cc == 105) bankUp();
            // --- Bank Down ---
            if (channel == 1 && cc == 106) bankDown();

            // --- Пресеты ---
            if (!stompMode)
            {
                if (channel == 2 && cc == 1) selectPreset(0);
                if (channel == 2 && cc == 2) selectPreset(1);
                if (channel == 2 && cc == 3) selectPreset(2);
            }
            else
            {
                if (channel == 3 && cc == 1 && presetButtons.size() > 0)
                    presetButtons[0]->setToggleState(isOn, juce::sendNotification);
                if (channel == 3 && cc == 2 && presetButtons.size() > 1)
                    presetButtons[1]->setToggleState(isOn, juce::sendNotification);
                if (channel == 3 && cc == 3 && presetButtons.size() > 2)
                    presetButtons[2]->setToggleState(isOn, juce::sendNotification);
            }

            // --- Shift ---
            if (channel == 1 && cc == 101)
                setShiftState(!shiftButton->getToggleState());

            // --- Stomp ---
            if (channel == 1 && cc == 100)
                setStompState(!stompBtn.getToggleState());

            // --- Looper (toggle) ---
            if (channel == 1 && cc == 102) toggleLooper();

            // --- Tuner (toggle) ---
            if (channel == 1 && cc == 110) toggleTuner();

            // --- Tap Tempo ---
            if (channel == 1 && cc == 103 && isOn)
            {
                if (hostComponent && hostComponent->getPluginInstance())
                {
                    tapTempo.tap();
                    double newBpm = tapTempo.getBpm();
                    hostComponent->updateBPM(newBpm);
                }
            }

            // --- Looper Reset ---
            if (channel == 1 && cc == 20 && isOn)
                if (looperComponent) looperComponent->pressResetButton();

            // --- Looper Control ---
            if (channel == 1 && ( cc == 24) && isOn)
                if (looperComponent) looperComponent->pressControlButton();

            // --- Master Volume Encoder (Ch5 CC1) ---
            if (msgCopy.getChannel() == 5 && msgCopy.getControllerNumber() == 1)
                if (auto* s = volumeSlider.get())
                    s->setValue(msgCopy.getControllerValue(), juce::sendNotificationSync);
        });
    //контроль педали 
    if (inputControl && channel == 6)
    {
        juce::MessageManager::callAsync([this, cc, value, isOn]()
            {
                switch (cc)
                {
                case 1: // ON/OFF STATUS
                    if (value == 0) {
                        pedalOn = false;
                        inputControl->setPedalConnected(false);

                        // скрываем метку при отключении
                        pedalModeLabel.setVisible(false);
                    }
                    else if (value == 127) {
                        pedalOn = true;
                        inputControl->setPedalConnected(true);

                        // показываем метку при подключении
                        pedalModeLabel.setVisible(true);
                        pedalModeLabel.setText("PEDAL", juce::dontSendNotification);
                        pedalModeLabel.setColour(juce::Label::backgroundColourId, juce::Colours::darkgrey);

                        // отправляем Min/Max/Threshold и Invert
                        int minVal = (int)inputControl->getPedalMinSlider().getValue();
                        int maxVal = (int)inputControl->getPedalMaxSlider().getValue();
                        int thrVal = (int)inputControl->getPedalThresholdSlider().getValue();

                        sendMidiCC(6, 5, minVal);
                        sendMidiCC(6, 6, maxVal);
                        sendMidiCC(6, 9, thrVal);

                        bool invertState = inputControl->getInvertButton().getToggleState();
                        sendMidiCC(6, 15, invertState ? 127 : 0);
                    }
                    break;

                    // --- остальные команды работают только если pedalOn == true ---
                case 2: // SW1
                    if (pedalOn)
                    {
                        if (inputControl)
                            inputControl->syncSwitchState(cc, isOn);

                        currentPedalMode = PedalMode::SW1;

                        pedalModeLabel.setVisible(true);
                        pedalModeLabel.setText("SW-1", juce::dontSendNotification);
                        pedalModeLabel.setColour(juce::Label::backgroundColourId, juce::Colours::lightgreen);
                        pedalModeLabel.setColour(juce::Label::textColourId, juce::Colours::black);
                        if (bankEditor)
                            bankEditor->updateCCParameter(12, isOn ? 127 : 0);
                    }
                    break;

                case 3: // SW2
                    if (pedalOn)
                    {
                        if (inputControl)
                            inputControl->syncSwitchState(cc, isOn);

                        currentPedalMode = PedalMode::SW2;

                        pedalModeLabel.setVisible(true);
                        pedalModeLabel.setText("SW-2", juce::dontSendNotification);
                        pedalModeLabel.setColour(juce::Label::backgroundColourId, juce::Colours::yellow);
                        pedalModeLabel.setColour(juce::Label::textColourId, juce::Colours::black);
                        if (bankEditor)
                            bankEditor->updateCCParameter(13, isOn ? 127 : 0);
                    }
                    break;


                case 4: // Pedal axis (0–127)
                    if (pedalOn)
                    {
                        float norm = value / 127.0f;

                        if (inputControl)
                            inputControl->syncPedalSliderByCC(cc, value);

                        if (bankEditor)
                        {
                           
                                bankEditor->applyPedalValue(10, norm); // слот 10 = SW1 (ось)
                           
                        }
                    }
                    break;

                case 5: // Pedal min (calibration stream)
                    if (pedalOn) {
                        inputControl->syncPedalSliderByCC(cc, value);
                        inputControl->showPressDown(); // PRESS DOWN теперь на CC5
                    }
                    break;

                case 6: // Pedal max / normal stream
                    if (pedalOn) {
                        inputControl->syncPedalSliderByCC(cc, value);
                        inputControl->showPressUp();   // PRESS UP теперь на CC6
                    }
                    break;

                case 7: // autoConfig / calibration finished
                    if (pedalOn)
                        inputControl->syncAutoConfigButton(isOn);
                    break;
                case 8: // Pedal axis (0–127)
                    if (pedalOn)
                    {
                        float norm = value / 127.0f;

                        if (inputControl)
                            inputControl->syncPedalSliderByCC(cc, value);

                        if (bankEditor)
                        {
                           
                                bankEditor->applyPedalValue(11, norm); // слот 11 = SW2 (ось)
                        }
                    }
                    break;
                case 9: // Pedal threshold
                    if (pedalOn) {
                        inputControl->syncPedalSliderByCC(cc, value);
                        inputControl->showThresholdSetting(); // твоя функция для подсветки режима
                    }
                    break;

                default:
                    break;
                }
            });
    }


}
void Rig_control::sendBpmToMidi(double bpm)
{
    if (!midiOut)   // защита от обращения к невалидному указателю
        return;

    if (bpm > 381.0)
        bpm = 380.0;

    int cc = 110;
    int value = 0;

    if (bpm < 128.0) {
        cc = 110;
        value = static_cast<int>(bpm);
    }
    else if (bpm < 255.0) {
        cc = 111;
        value = static_cast<int>(bpm - 127.0);
    }
    else {
        cc = 112;
        value = static_cast<int>(bpm - 254.0);
    }

    midiOut->sendMessageNow(
        juce::MidiMessage::controllerEvent(5, cc, value)
    );
}


void Rig_control::sendSettingsMenuState(bool isOpen)
{
    constexpr int midiChannel = 1; // канал 1
    constexpr int ccNumber = 55; // CC 55
    const int value = isOpen ? 127 : 0;

    // Отправляем MIDI
    sendMidiCC(midiChannel, ccNumber, value);
}

void Rig_control::sendImpedanceCC(int ccNumber, bool on)
{
    constexpr int midiChannel = 1; // канал 1
    const int value = on ? 127 : 0;
    sendMidiCC(midiChannel, ccNumber, value);
}
void Rig_control::setInputControlComponent(InputControlComponent* ic) noexcept
{
    inputControl = ic;

    if (inputControl != nullptr)
    {
        inputControl->onInputClipChanged = [this](bool l, bool r)
            {
                juce::MessageManager::callAsync([this, l, r]
                    {
                        inClipLedL.setVisible(l);
                        inClipLedR.setVisible(r);
                    });
            };

    }
    else
    {
        if (inputControl)
            inputControl->onInputClipChanged = nullptr;
    }
}

















