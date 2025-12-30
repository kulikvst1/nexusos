#include "Rig_control.h"
#include "bank_editor.h"
#include "OutControlComponent.h"
#include "MidiStartupShutdown.h"
#include "LibraryMode.h"
#include "StompMode.h"
//==============================================================================
class MainContentComponent; // forward declaration
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
        juce::File::userApplicationDataDirectory
    ).getChildFile("NEXUS_OS_AUDIO_SET")
        .getChildFile("AudioSettings.xml");

    if (settingsFile.existsAsFile())
    {
        juce::XmlDocument doc(settingsFile);
        if (auto xml = doc.getDocumentElement())
            savedMidiOutId = xml->getStringAttribute("defaultMidiOutputDevice");
    }

    // --- MIDI OUT ---
    // открываем только сохранённый ID, если он есть
    if (savedMidiOutId.isNotEmpty())
    {
        midiOut = juce::MidiOutput::openDevice(savedMidiOutId);
    }

    // Если удалось открыть — отправляем стартовые команды
    if (midiOut)
    {
        if (!midiInit)
            midiInit = std::make_unique<MidiStartupShutdown>(*this);

        midiInit->sendStartupCommands();
    }

    //////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////RIG CONTROL///////////////////////////////////////////////////

    // 1. Создаём контейнер для элементов интерфейса
    mainTab = std::make_unique<juce::Component>();
    addAndMakeVisible(mainTab.get());
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
    mainTab->addAndMakeVisible(bankSelector.get());
    // Делегируем заполнение в LibraryMode
    if (libraryLogic)
        libraryLogic->updateBankSelector();

    libraryLogic = std::make_unique<LibraryMode>(*this);



   ///+++++++++++++  Метка статуса контролера FOOT
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
    // Первичная отрисовка всех кнопок
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

    // Внутренний отступ от краёв mainTab
    const int margin = 2;
    auto content = mainTab->getLocalBounds().reduced(margin);

    // Настройка сетки: 9 колонок × 4 строки
    constexpr int numCols = 9;
    constexpr int numRows = 4;

    // Вычисляем размеры ячеек
    int usableWidth = content.getWidth();
    int sectorWidth = usableWidth / numCols;
    int extra = usableWidth - (sectorWidth * numCols);
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
            int extraWidth = (col < extra ? 1 : 0);
            int w = sectorWidth + extraWidth;
            sectors.push_back(juce::Rectangle<int>(x, y, w, sectorHeight));
            x += w;
        }
    }

    // Утилиты
    auto getSectorRect = [&sectors](int sectorNumber) -> juce::Rectangle<int>
        {
            return sectors[sectorNumber - 1];
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
        int clipH = inputSector.getHeight() / 8;
        int yClip = inputSector.getY();

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
        int h = sector0.getHeight() / 3;
        int yPedal = sector0.getY() + (sector0.getHeight() / 8) + 4;

        juce::Rectangle<int> modeBounds(sector0.getX(), yPedal, w, h);

        pedalModeLabel.setBounds(modeBounds);
        pedalModeLabel.setJustificationType(juce::Justification::centred);
        pedalModeLabel.setOpaque(false);
        pedalModeLabel.setFont(juce::Font((float)h * 0.6f, juce::Font::bold));
    }

    // ─── VOLUME ───
    auto volumeSector = getSectorRect(9).reduced(1);
    if (volumeSlider)
    {
        auto sliderArea = volumeSector.reduced(0, volumeSector.getHeight() * 0.1f);
        volumeSlider->setBounds(sliderArea);
    }

    int gap = 4;
    int halfW = (volumeSector.getWidth() - gap) / 2;
    int clipH = volumeSector.getHeight() / 8;
    int yClip = volumeSector.getY();

    clipLedL.setBounds(volumeSector.getX(), yClip, halfW, clipH);
    clipLedR.setBounds(volumeSector.getX() + halfW + gap, yClip, halfW, clipH);

    clipLedL.setJustificationType(juce::Justification::centred);
    clipLedR.setJustificationType(juce::Justification::centred);

    clipLedL.setFont(juce::Font((float)clipH * 0.6f, juce::Font::bold));
    clipLedR.setFont(juce::Font((float)clipH * 0.6f, juce::Font::bold));

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
            float fontSize = r.getHeight() * 0.9f;
            lbl.setFont(juce::Font(fontSize, juce::Font::bold));
        };

    for (int i = 0; i < 3; ++i)
    {
        if (i < presetButtons.size())
            layoutPresetLabel(*labels[i], bounds[i]);
    }

    // ─── LOOPER и TUNER ───
    if (!enginePtr)
    {
        if (looperComponent) looperComponent->setBounds(0, 0, 0, 0);
        return;
    }

    auto topRow = getUnionRect(11, 17).reduced(1);
    auto bottomRow = getUnionRect(20, 25).reduced(1);
    juce::Rectangle<int> sharedArea{
        topRow.getX(), topRow.getY(),
        topRow.getWidth(),
        bottomRow.getBottom() - topRow.getY()
    };

    if (looperComponent)
        looperComponent->setBounds(looperComponent->isVisible() ? sharedArea : juce::Rectangle<int>());

    if (externalTuner)
        externalTuner->setBounds(externalTuner->isVisible() ? sharedArea : juce::Rectangle<int>());
    
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

    if (!stompModeController)
        stompModeController = std::make_unique<StompMode>(bankEditor, this);

    bankEditor->onActivePresetChanged = [this](int newActive)
        {
            if (!manualShift)
                setShiftState(newActive >= 3); // авто‑Shift по пресету

            if (stompModeController && stompModeController->isActive())
                stompModeController->updateDisplays();
            else if (libraryLogic && libraryLogic->isActive())
                updateLibraryDisplays();
            else
                updatePresetDisplays();
        };

    bankEditor->onBankChanged = [this]()
        {
            if (!manualShift)
                setShiftState(false); // сброс Shift

            if (stompModeController)
                stompModeController->exit();

            for (auto* btn : presetButtons)
                btn->setRadioGroupId(100, juce::dontSendNotification);

            sendStompState();
            updateAllSButtons();

            if (libraryLogic && libraryLogic->isActive())
                updateLibraryDisplays();
            else
                updatePresetDisplays();
        };

    bankEditor->onBankEditorChanged = [this]()
        {
            if (stompModeController && stompModeController->isActive())
                stompModeController->updateDisplays();
            else if (libraryLogic && libraryLogic->isActive())
                updateLibraryDisplays();
            else
                updatePresetDisplays();
        };

    // блок с shiftButton убираем полностью —
    // теперь управляем только через setShiftState()

    updateBankSelector();

    if (stompModeController && stompModeController->isActive())
        stompModeController->updateDisplays();
    else if (libraryLogic && libraryLogic->isActive())
        updateLibraryDisplays();
    else
        updatePresetDisplays();
}
////////////////////////////////////////////////FOR PRESET MODE/////////////////////////////////////////
void Rig_control::updatePresetDisplays()
{
    if (!bankEditor || (stompModeController && stompModeController->isActive())
        || (libraryLogic && libraryLogic->isActive()))
        return;

    const int bankIdx = bankEditor->getActiveBankIndex();
    const int active = bankEditor->getActivePresetIndex();
    auto names = bankEditor->getPresetNames(bankIdx);
    if (names.size() < 6)
        return;

    // 0) Решаем, включён ли сейчас Shift:
    const bool wantShift = manualShift
        ? shift          // читаем наш флаг вместо кнопки
        : (active >= 3);

    // синхронизируем флаг (как будто кнопка переключилась)
    if (shift != wantShift)
    {
        shift = wantShift;
        sendShiftState();    // CC101 наружу
        updateAllSButtons(); // обновить индикаторы
    }

    const bool shiftOn = shift;

    // 1) Имя банка
    if (bankSelector)
    {
        bankSelector->clear(juce::dontSendNotification);

        const auto& banksList = bankEditor->getBanks();
        for (int i = 0; i < (int)banksList.size(); ++i)
            bankSelector->addItem(banksList[i].bankName, i + 1);

        bankSelector->setSelectedId(bankIdx + 1, juce::dontSendNotification);
        bankSelector->setText(bankEditor->getBank(bankIdx).bankName,
            juce::dontSendNotification);
        bankSelector->repaint();
    }

    // 2) Сбросим и заполним тексты/состояния трёх кнопок и меток
    std::array<juce::Button*, 3> btns = { presetButtons[0], presetButtons[1], presetButtons[2] };
    std::array<juce::Label*, 3> labs = { &presetLabel1_4, &presetLabel2_5, &presetLabel3_6 };

    for (int i = 0; i < 3; ++i)
    {
        int btnIdx = shiftOn ? (3 + i) : i;
        int lblIdx = shiftOn ? i : (3 + i);

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
    if (active != lastSentPresetIndex)
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

    repaint();
}

void Rig_control::buttonClicked(juce::Button* button)
{
      
    
    // --- Клик по кнопке пресета ---
    if (presetButtons.contains(static_cast<juce::TextButton*>(button)))
    {
        int idx = presetButtons.indexOf(static_cast<juce::TextButton*>(button));
        if (idx >= 0)
        {
            if (stompModeController && stompModeController->isActive())
            {
                // вместо shiftButton->getToggleState()
                bool shiftOn = shift;
                stompModeController->handleButtonClick(idx, shiftOn);
                return;
            }

            // обычная логика пресета
            auto* btn = static_cast<juce::TextButton*>(button);
            if (btn->getRadioGroupId() == 100 && btn->getToggleState())
            {
                manualShift = true; // как и раньше, при клике включаем ручной режим

                if (libraryLogic && libraryLogic->isActive())
                {
                    selectLibraryFile(idx);
                }
                else
                {
                    // вместо shiftButton->getToggleState()
                    bool shiftOn = shift;
                    int presetIndex = shiftOn ? (idx + 3) : idx;

                    if (bankEditor)
                        bankEditor->setActivePreset(presetIndex);

                    updatePresetDisplays();

                    if (presetChangeCb)
                        presetChangeCb(presetIndex);
                }
            }
            return;
        }
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
    const auto presetColor = juce::Colour::fromRGB(200, 230, 255);

    // --- База: все кнопки светло-голубые с чёрным текстом ---
    updateSButton(0, "UP\n(LIBRARY)", presetColor);
    updateSButton(1, "DOWN\n(STOMP)", presetColor);
    updateSButton(2, "TAP\n(LOOPER)", presetColor);   // S3 базовая надпись
    updateSButton(3, "SHIFT\n(TUNER)", presetColor);

    for (int i = 0; i < sButtons.size(); ++i)
    {
        sButtons[i]->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        sButtons[i]->setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    }

    // --- Переопределения для активных режимов ---

    // Library Mode
    if (libraryLogic && libraryLogic->isActive())
    {
        updateSButton(0, "LIBRARY OFF", juce::Colours::green);
        updateSButton(1, "BACK", juce::Colours::green);
        updateSButton(3, "NEXT", juce::Colours::green);
    }

    // Stomp Mode
    if (stompModeController && stompModeController->isActive())
    {
        updateSButton(1, "STOMP OFF\n(DOWN)", juce::Colours::red);
    }

    // Looper Mode
    const bool looperActive = (looperComponent && looperComponent->isVisible());
    if (looperActive)
    {
        // S3: когда лупер активен → показываем CLEAN (LOOPER OFF)
        updateSButton(2, "CLEAN\n(LOOPER OFF)", juce::Colours::darkgrey);

        // S4: управление лупером (приоритет над Library/Shift)
        auto label = looperComponent->getControlButtonText();
        auto color = looperComponent->getControlButtonColor();
        updateSButton(3, label, color);
    }
    else
    {
        // Лупер не активен → S3 остаётся базовым "TAP\n(LOOPER)" светло‑голубым (НЕ переопределяем)

        // S4: если нет активного лупера, показываем Library/Shift
        if (libraryLogic && libraryLogic->isActive())
        {
            updateSButton(3, "NEXT", juce::Colours::green);
        }
        else if (shift)
        {
            updateSButton(3, "SHIFT\n(TUNER)", juce::Colours::gold);
        }
        else
        {
            updateSButton(3, "SHIFT\n(TUNER)", presetColor);
        }
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
        for (int i = 0; i < presetButtons.size(); ++i)
            presetButtons[i]->setToggleState(i == index, juce::sendNotification);
    }
}
void Rig_control::setShiftState(bool on)
{
    if (shift == on)
        return;

    shift = on;
    manualShift = true;

    sendShiftState();
    updateAllSButtons();

    if (stompModeController && stompModeController->isActive())
    {
        stompModeController->updateDisplays(); // 🔹 ключевой вызов
    }
    else if (libraryLogic && libraryLogic->isActive())
    {
        updateLibraryDisplays();
    }
    else
    {
        updatePresetDisplays();
    }
}

void Rig_control::sendShiftState()
{
    if (!midiOut) return;

    static bool lastSentShift = false; // защита от дублей
    bool shiftOn = shift;              // читаем наш флаг вместо кнопки

    if (shiftOn != lastSentShift)
    {
        lastSentShift = shiftOn;
        int value = shiftOn ? 127 : 0;

        midiOut->sendMessageNow(
            juce::MidiMessage::controllerEvent(1, 101, value) // канал 1, CC101
        );
    }
}

void Rig_control::toggleLooper()
{
    bool newState = !(looperComponent && looperComponent->isVisible());
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
                sendLooperModeToMidi(enginePtr->getState());
            }
            else // Player
            {
                if (!enginePtr->isReady())
                {
                    sendLooperClearToMidi(); // CC20
                }
                else
                {
                    if (enginePtr->isPlaying())
                        sendLooperModeToMidi(LooperMode::Play); // CC23
                    else
                        sendLooperModeToMidi(LooperMode::Stop); // CC22
                }
            }
        };

    // Создаём компонент один раз
    if (!looperComponent)
    {
        looperComponent = std::make_unique<LooperComponent>(*enginePtr);
        looperComponent->setRigControl(this);
        addAndMakeVisible(*looperComponent);
        looperComponent->setVisible(false); // по умолчанию скрыт
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
    if (externalTuner)
    {
        externalTuner->setVisible(on);
        if (onTunerVisibilityChanged)
            onTunerVisibilityChanged(on);
    }

    // --- Отправка состояния в MIDI ---
    if (midiOut)
    {
        int value = on ? 127 : 0;
        midiOut->sendMessageNow(
            juce::MidiMessage::controllerEvent(1, 110, value) // ch1 CC110
        );
    }
}

void Rig_control::toggleTuner()
{
    bool newState = !(externalTuner && externalTuner->isVisible());
    setTunerState(newState);
}

void Rig_control::sendTunerMidi(bool on)
{
    if (!midiOut) return;

    const int value = on ? 127 : 0; // включено → 127, выключено → 0
    midiOut->sendMessageNow(
        juce::MidiMessage::controllerEvent(1, 110, value) // ch1 CC110
    );
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

            // --- Пресеты / Library ---
            if (!(stompModeController && stompModeController->isActive()))
            {
                // пресеты на канале 2
                if (channel == 2 && cc == 1) selectPreset(0);
                if (channel == 2 && cc == 2) selectPreset(1);
                if (channel == 2 && cc == 3) selectPreset(2);

                // библиотеки на канале 4
                if (channel == 4 && cc == 1) selectLibraryFile(0);
                if (channel == 4 && cc == 2) selectLibraryFile(1);
                if (channel == 4 && cc == 3) selectLibraryFile(2);
            }
            else
            {
                // Stomp Mode: кнопки на канале 3
                if (channel == 3 && cc == 1 && presetButtons.size() > 0)
                    presetButtons[0]->setToggleState(isOn, juce::sendNotification);
                if (channel == 3 && cc == 2 && presetButtons.size() > 1)
                    presetButtons[1]->setToggleState(isOn, juce::sendNotification);
                if (channel == 3 && cc == 3 && presetButtons.size() > 2)
                    presetButtons[2]->setToggleState(isOn, juce::sendNotification);
            }
            /*+
            // --- Shift через MIDI ---
            if (channel == 1 && cc == 101)
            {
                manualShift = true; // включаем ручной режим

                if (value == 127)
                    setShiftState(true);   // 🔹 через метод
                else if (value == 0)
                    setShiftState(false);  // 🔹 через метод

                return;
            }


            // --- Stomp ---
            if (channel == 1 && cc == 100)
            {
                if (value == 127)
                    setStompState(true);
                else if (value == 0)
                    setStompState(false);
            }
            */
            // --- Shift через MIDI ---
            if (channel == 1 && cc == 101)
            {
                manualShift = true; // включаем ручной режим

                if (value == 127)
                {
                    // переключаем состояние Shift
                    bool newState = !shift;
                    setShiftState(newState);
                }

                return;
            }
            // --- Stomp ---
            if (channel == 1 && cc == 100)
            {
                if (value == 127)
                {
                    // переключаем StompMode как тумблер
                    bool newState = !(stompModeController && stompModeController->isActive());
                    setStompState(newState);
                }
                return;
            }

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
            // --- Library Mode ON/OFF ---
            if (channel == 1 && cc == 107)
            {
                // Включение режима
                if (value == 127 && (!libraryLogic || !libraryLogic->isActive()))
                {
                    toggleLibraryMode();

                    // подтверждаем включение режима
                    sendLibraryMode(true);
                    // сразу обновляем кнопки S
                    updateAllSButtons();
                    // синхронизация кнопок пресетов (radio group)
                    if (libraryLogic && bankEditor)
                    {
                        auto files = bankEditor->scanNumericBankFiles();
                        if (!files.isEmpty())
                        {
                            bool sent = false;

                            for (int i = 0; i < 3; ++i)
                            {
                                int fileIdx = (libraryLogic->getCurrentLibraryOffset() + i) % files.size();
                                if (fileIdx == libraryLogic->getCurrentLibraryFileIndex())
                                {
                                    sendLibraryState(i, true); // активная кнопка → 127
                                    sent = true;
                                    break;
                                }
                            }

                            // если активной нет → погасить последнюю
                            if (!sent && lastActivePresetIndex >= 0)
                                sendLibraryState(lastActivePresetIndex, false);
                        }
                    }
                }
                // Выключение режима
                else if (value == 0 && (libraryLogic && libraryLogic->isActive()))
                {
                    toggleLibraryMode();

                    // подтверждаем выключение режима
                    sendLibraryMode(false);
                    // сразу обновляем кнопки S
                    updateAllSButtons();
                    // radio group не трогаем
                }
            }

             // --- Library Prev Block ---
            if (channel == 4 && cc == 105 && isOn)
                prevLibraryBlock();

            // --- Library Next Block ---
            if (channel == 4 && cc == 101 && isOn)
                nextLibraryBlock();

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
//++++++++++++++++++++  РЕЖИМ БИБЛИОТЕКИ +++++++++++++++++++
void Rig_control::prevLibraryBlock()
{
    if (libraryLogic) libraryLogic->prevLibraryBlock();
}

void Rig_control::nextLibraryBlock()
{
    if (libraryLogic) libraryLogic->nextLibraryBlock();
}

void Rig_control::toggleLibraryMode()
{
    if (libraryLogic) libraryLogic->toggleLibraryMode();
}

void Rig_control::updateLibraryDisplays()
{
    if (libraryLogic) libraryLogic->updateLibraryDisplays();
}

void Rig_control::updateBankSelector()
{
    if (libraryLogic) libraryLogic->updateBankSelector();
}
void Rig_control::selectLibraryFile(int idx)
{
    if (libraryLogic)
        libraryLogic->selectLibraryFile(idx);
}
void Rig_control::syncLibraryToLoadedFile()
{
    if (libraryLogic) libraryLogic->syncLibraryToLoadedFile();
}

void Rig_control::syncLibraryToLoadedFile(bool resetOffset)
{
    if (libraryLogic) libraryLogic->syncLibraryToLoadedFile(resetOffset);
}

void Rig_control::sendLibraryMode(bool active)
{
    if (!midiOut) return;

    // канал 1, CC107 → 127 при включении, 0 при выключении
    midiOut->sendMessageNow(
        juce::MidiMessage::controllerEvent(1, 107, active ? 127 : 0));
}

void Rig_control::sendLibraryState(int presetIndex, bool active)
{
    if (!midiOut) return;

    if (active)
    {
        // radio group: только одна активная кнопка → 127
        midiOut->sendMessageNow(
            juce::MidiMessage::controllerEvent(4, presetIndex + 1, 127));

        lastActivePresetIndex = presetIndex; // запоминаем последнюю активную
    }
    else if (presetIndex >= 0)
    {
        // если все кнопки выключены → погасить последнюю активную
        midiOut->sendMessageNow(
            juce::MidiMessage::controllerEvent(4, presetIndex + 1, 0));
    }
}
//++++++++++++++++++++++++++++++ РЕЖИМ СТОМ ++++++++++++++++++++++++++++++++++++
void Rig_control::sendStompState()
{
    if (!midiOut) return;

    static bool lastSentStomp = false; // защита от дублей
    bool stompOn = (stompModeController && stompModeController->isActive());

    if (stompOn != lastSentStomp)
    {
        lastSentStomp = stompOn;
        int value = stompOn ? 127 : 0;

        midiOut->sendMessageNow(
            juce::MidiMessage::controllerEvent(1, 100, value) // канал 1, CC100
        );
    }
}
// --- Переключение Stomp режима (только через контроллер) ---
void Rig_control::setStompState(bool active)
{
    sendShiftState();

    if (stompModeController)
        stompModeController->setState(active);

    sendStompState(); // CC100 отправляем уже после смены состояния

    updateAllSButtons();
}

void Rig_control::sendStompButtonState(int ccIndex, bool state)
{
    if (!midiOut) return;

    int globalCC = (ccIndex % 3) + 1; // CC1..3 на канале 3
    int value = state ? 127 : 0;

    midiOut->sendMessageNow(
        juce::MidiMessage::controllerEvent(3, globalCC, value)
    );
}



































