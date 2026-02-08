#include "Rig_control.h"
#include "bank_editor.h"
#include "OutControlComponent.h"
#include "MidiStartupShutdown.h"
#include "LibraryMode.h"
#include "StompMode.h"
//==============================================================================
std::unique_ptr<juce::Drawable> loadIcon(const void* data, size_t size)
{
    // Преобразуем бинарные данные SVG в строку
    juce::String svgString = juce::String::fromUTF8((const char*)data, (int)size);

    // Парсим строку в XmlElement
    std::unique_ptr<juce::XmlElement> svgXml = juce::parseXML(svgString);

    if (svgXml != nullptr)
        return juce::Drawable::createFromSVG(*svgXml);

    return nullptr; // если парсинг не удался
}

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
                // можно оставить для спец. реакции на клип, например мигание
                if (l) clipLedL.setColour(juce::Label::backgroundColourId, juce::Colours::red);
                if (r) clipLedR.setColour(juce::Label::backgroundColourId, juce::Colours::red);
                // но НЕ трогаем setVisible
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
    // ++++++++++++++++++++++++++++++++ MIDI IN CONTROL ++++++++++++++++++++++++++++++++++
    auto midiInputs = juce::MidiInput::getAvailableDevices();
    for (const auto& input : midiInputs)
    {
        deviceManager.setMidiInputDeviceEnabled(input.identifier, true);
        deviceManager.addMidiInputDeviceCallback(input.identifier, this);
    }
    juce::String savedMidiOutId;
    juce::File settingsFile = juce::File::getSpecialLocation(
        juce::File::userApplicationDataDirectory
    )   .getChildFile("NEXUS_OS_AUDIO_SET")
        .getChildFile("AudioSettings.xml");

    if (settingsFile.existsAsFile())
    {
        juce::XmlDocument doc(settingsFile);
        if (auto xml = doc.getDocumentElement())
        savedMidiOutId = xml->getStringAttribute("defaultMidiOutputDevice");
    }
    // ++++++++++++++++++++++++++++++++ MIDI OUT CONTROL ++++++++++++++++++++++++++++++++
    if (savedMidiOutId.isNotEmpty())
    {
        midiOut = juce::MidiOutput::openDevice(savedMidiOutId);
    }
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
        labels[i]->setColour(juce::Label::backgroundColourId, juce::Colours::lightgrey);
        labels[i]->setColour(juce::Label::textColourId, juce::Colours::darkgrey);
    }

    // 3. Добавляем метку BANK NAME
    static BankNameKomboBox bankNameLF; 
    bankSelector = std::make_unique<juce::ComboBox>("Bank Selector");
    bankSelector->setLookAndFeel(&bankNameLF);
    bankSelector->setJustificationType(juce::Justification::centred);
    bankSelector->setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    bankSelector->setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    bankSelector->setColour(juce::ComboBox::textColourId, juce::Colours::white);
    mainTab->addAndMakeVisible(bankSelector.get());
    libraryLogic = std::make_unique<LibraryMode>(*this);

    //+++++++++++++ Метки названия библиотеки,плагина 
    addAndMakeVisible(activeLibraryLabel);
   // activeLibraryLabel.setText("BANK: NONE", juce::dontSendNotification);
    activeLibraryLabel.setJustificationType(juce::Justification::centredLeft);

    addAndMakeVisible(loadedPluginLabel);
  //  loadedPluginLabel.setText("Plugin: none", juce::dontSendNotification);
    loadedPluginLabel.setJustificationType(juce::Justification::centredLeft);

    //+++++++++++++  Метка статуса контролера FOOT
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
    prevVolDb = juce::jmap<float>((float)volumeSlider->getValue(),0.0f, 127.0f,-60.0f, 12.0f);
    volumeSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    volumeSlider->setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::red);
    volumeSlider->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::white);
    volumeSlider->setColour(juce::Slider::textBoxTextColourId, juce::Colours::black);
    mainTab->addAndMakeVisible(volumeSlider.get());
    
   
    clipLedL.setVisible(true);
    clipLedR.setVisible(true);


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
      //  sButtons[i]->setLookAndFeel(&SWbutoon);
        int ccNumber = sButtonCCs[i];
        sButtons[i]->onStateChange = [this, i, ccNumber]()
            {
                bool pressed = sButtons[i]->isDown();
                sendSButtonState(ccNumber, pressed);  // 🔹 вызываем отдельную функцию
            };
    }
    updateAllSButtons();
    // Загружаем иконку FSC из ресурсов (SVG)
    iconFS = loadIcon(BinaryData::FSC_svg, BinaryData::FSC_svgSize);
    static CustomLookAndFeelFS lfFS;
    lfFS.iconFS = iconFS.get();   // теперь можно присвоить, т.к. поле — обычный указатель
    lfFS.textScale = 0.13f;

    for (int i = 0; i < sButtons.size(); ++i)
    {
        if (auto* btn = sButtons[i].get())
            btn->setLookAndFeel(&lfFS);
    }
    startTimerHz(30);
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
    for (auto* btn : presetButtons)
    {
        btn->removeListener(this);
        btn->setLookAndFeel(nullptr);
    }
    if (bankSelector) bankSelector->setLookAndFeel(nullptr);
      midiInit.reset();
      midiOut.reset();
}
void Rig_control::resized()
{
    auto fullArea = getLocalBounds();
    mainTab->setBounds(fullArea);
    const int margin = 2;
    auto content = mainTab->getLocalBounds().reduced(margin);
    constexpr int numCols = 9;
    constexpr int numRows = 4;
    int usableWidth = content.getWidth();
    int sectorWidth = usableWidth / numCols;
    int extra = usableWidth - (sectorWidth * numCols);
    int sectorHeight = content.getHeight() / numRows;
    
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
    // ─── LIBRARY & PLUGIN LABELS ───
    {
        int labelW = bankRect.getWidth() / 2; // ширина увеличена на половину
        int labelH = bankRect.getHeight() / 4;

        // Library label: левый верхний угол
        juce::Rectangle<int> libRect(bankRect.getX(),
            bankRect.getY(),
            labelW,
            labelH);
        activeLibraryLabel.setBounds(libRect);
        activeLibraryLabel.setJustificationType(juce::Justification::centredLeft);
        activeLibraryLabel.setFont(juce::Font((float)labelH * 0.8f, juce::Font::bold));

        // Plugin label: правый верхний угол
        juce::Rectangle<int> pluginRect(bankRect.getRight() - labelW,
            bankRect.getY(),
            labelW,
            labelH);
        loadedPluginLabel.setBounds(pluginRect);
        loadedPluginLabel.setJustificationType(juce::Justification::centredRight);
        loadedPluginLabel.setFont(juce::Font((float)labelH * 0.8f, juce::Font::bold));
    }
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
//++++++++++++++++++++++ BankEditor CONTROL ++++++++++++++++++++++++++++++++
void Rig_control::setBankEditor(BankEditor* editor)
{
    bankEditor = editor;
    if (!bankEditor)
        return;

    if (!stompModeController)
        stompModeController = std::make_unique<StompMode>(bankEditor, this);

    bankEditor->onPluginChanged = [this]()
        {
            updatePluginLabel();
        };

    bankEditor->onActivePresetChanged = [this](int newActive)
        {
            if (!manualShift)
                setShiftState(newActive >= 3);

            if (stompModeController && stompModeController->isActive())
                stompModeController->updateDisplays();
            else if (libraryLogic && libraryLogic->isActive())
                updateLibraryDisplays();
            else
                updatePresetDisplays();

            updateActiveLibraryLabel();
            updatePluginLabel();   // <── обновляем метку плагина
        };

    bankEditor->onBankChanged = [this]()
        {
            if (!manualShift)
                setShiftState(false);

            if (stompModeController)
                stompModeController->exit();

            for (auto* btn : presetButtons)
                btn->setRadioGroupId(100, juce::dontSendNotification);

            sendStompState();
            updateAllSButtons();
            updateBankSelector();

            if (libraryLogic && libraryLogic->isActive())
                updateLibraryDisplays();
            else
                updatePresetDisplays();

            updateActiveLibraryLabel();
            updatePluginLabel();   // <── обновляем метку плагина
        };

    bankEditor->onBankEditorChanged = [this]()
        {
            if (stompModeController && stompModeController->isActive())
                stompModeController->updateDisplays();
            else if (libraryLogic && libraryLogic->isActive())
                updateLibraryDisplays();
            else
                updatePresetDisplays();

            updateBankSelector();
            updateActiveLibraryLabel();
            updatePluginLabel();   // <── обновляем метку плагина
        };

    updateBankSelector();

    if (stompModeController && stompModeController->isActive())
        stompModeController->updateDisplays();
    else if (libraryLogic && libraryLogic->isActive())
        updateLibraryDisplays();
    else
        updatePresetDisplays();

    // при старте сразу обновляем метки
    updateActiveLibraryLabel();
    updatePluginLabel();
}
//++++++++++++++++++++++++++ PresetDisplays CONTROL +++++++++++++++++++++++++++
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
       const bool wantShift = manualShift
        ? shift          
        : (active >= 3);
    if (shift != wantShift)
    {
        shift = wantShift;
        sendShiftState();    
        updateAllSButtons(); 
    }
    const bool shiftOn = shift;
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
    sendPresetChange(active);
    repaint();
}
//++++++++++++++++++++++++++ BankSelector CONTROL +++++++++++++++++++++++++++++++++
void Rig_control::updateBankSelector()
{
    if (!bankEditor || !bankSelector)
        return;
        bankSelector->clear(juce::dontSendNotification);

    // получаем список банков из текущей библиотеки
    const auto& banksList = bankEditor->getBanks();
    int activeBankIdx = bankEditor->getActiveBankIndex();
    for (int i = 0; i < (int)banksList.size(); ++i)
        bankSelector->addItem(banksList[i].bankName, i + 1);
        bankSelector->setSelectedId(activeBankIdx + 1, juce::dontSendNotification);
        bankSelector->setText(bankEditor->getBank(activeBankIdx).bankName,
        juce::dontSendNotification);
   
    bankSelector->onChange = [this]()
        {
            int selectedId = bankSelector->getSelectedId();
            int bankIndex = selectedId - 1;
            if (bankIndex >= 0)
                bankEditor->setActiveBankIndex(bankIndex);
        };
    bankNameLabel.setText(bankEditor->loadedFileName, juce::dontSendNotification);
 }
//+++++++++++++++++++++++++++++ updateActiveLibraryLabel +++++++++++++++++++++++++++++++
void Rig_control::updateActiveLibraryLabel()
{
    if (bankEditor)
    {
        juce::String libName = bankEditor->loadedFileName;

        if (!libName.isEmpty())
        {
            // если это путь — берём только имя без расширения
            juce::File f = juce::File::getCurrentWorkingDirectory().getChildFile(libName);
            libName = f.getFileNameWithoutExtension();
        }
        else
        {
            libName = "<BANK NOT LOADET>";
        }

        activeLibraryLabel.setText("BANK: " + libName, juce::dontSendNotification);
    }
}
//+++++++++++++++++++++++++++++ updatePluginLabel ++++++++++++++++++++++++++++++++++++++
void Rig_control::updatePluginLabel()
{
    if (bankEditor)
    {
        juce::String pluginName = bankEditor->getCurrentPluginDisplayName();
        loadedPluginLabel.setText(pluginName, juce::dontSendNotification);
    }
    else
    {
        loadedPluginLabel.setText("PLUGIN: NONE", juce::dontSendNotification);
    }

}

//+++++++++++++++++++++++++ PRESET Button Control +++++++++++++++++++++++++++++++++++
void Rig_control::buttonClicked(juce::Button* button)
{
    if (presetButtons.contains(static_cast<juce::TextButton*>(button)))
    {
        int idx = presetButtons.indexOf(static_cast<juce::TextButton*>(button));
        if (idx >= 0)
        {
            if (stompModeController && stompModeController->isActive())
            {
                bool shiftOn = shift;
                stompModeController->handleButtonClick(idx, shiftOn);
                return;
            }
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
//++++++++++++++++++++++++++ slider Value CONTROL +++++++++++++++++++++++++++++++++++
void Rig_control::sliderValueChanged(juce::Slider* slider)
{
    if (slider == volumeSlider.get())
    {
        // 1) абсолютное новое значение мастера в дБ
        float newVolDb = juce::jmap<float>(
        (float)volumeSlider->getValue(),0.0f, 127.0f,-60.0f, 12.0f);
        float leftDb = outControl ? outControl->getGainDbL() : prevVolDb;
        float rightDb = outControl ? outControl->getGainDbR() : prevVolDb;
        float maxCh = juce::jmax(leftDb, rightDb);
        float minCh = juce::jmin(leftDb, rightDb);
        float deltaDb = 0.0f;
        if (newVolDb > maxCh)      deltaDb = newVolDb - maxCh;
        else if (newVolDb < minCh) deltaDb = newVolDb - minCh;
        if (deltaDb != 0.0f && outControl)
        outControl->offsetGainDb(deltaDb);
        prevVolDb = newVolDb;
        int midiValue = (int)volumeSlider->getValue();
        sendVolumeToController(midiValue);   
        return;
    }
}
// вспомогательная функция для плавного цвета по уровню
static juce::Colour getMeterColour(float db)
{
    // ниже -60 dB метки скрываем
    if (db < -59.0f)
        return juce::Colours::transparentBlack;

    if (db > 0.0f)
        return juce::Colours::red;

    if (db >= -1.0f)
    {
        // от -1 до 0 dB → красный
        return juce::Colours::red;
    }

    if (db >= -6.0f)
    {
        // от -6 до -1 dB → жёлтый
        return juce::Colours::yellow;
    }

    // от -60 до -6 dB → плавный переход от тёмно‑зелёного к более светлому зелёному
    juce::Colour darkGreen = juce::Colour::fromRGB(0, 64, 0);      // тёмно‑зелёный
    juce::Colour midGreen = juce::Colour::fromRGB(0, 180, 0);     // не ярко‑зелёный

    float t = (db + 60.0f) / 54.0f; // нормализация диапазона -60…-6
    return darkGreen.interpolatedWith(midGreen, t);
}

void Rig_control::timerCallback()
{
    static double peakHoldTsOutL = 0.0, peakHoldTsOutR = 0.0;
    static double peakHoldTsInL = 0.0, peakHoldTsInR = 0.0;
    static constexpr double peakHoldMs = 800.0;

    if (outControl && inputControl)
    {
        float levelOutL = std::max(outControl->getMeterLevelL(), 1e-6f);
        float levelOutR = std::max(outControl->getMeterLevelR(), 1e-6f);
        

        auto setColorSmooth = [&](juce::Label& lbl, float level, double& peakTs)
            {
                float db = juce::Decibels::gainToDecibels(level, -60.0f);
                auto now = juce::Time::getMillisecondCounterHiRes();

                if (db < -59.0f)
                {
                    lbl.setVisible(false);
                    lbl.setText("", juce::dontSendNotification);
                    return;
                }

                if (db > 0.0f)
                {
                    peakTs = now;
                    lbl.setVisible(true);
                    lbl.setText("PEAK", juce::dontSendNotification);
                    lbl.setColour(juce::Label::backgroundColourId, juce::Colours::red);
                }
                else if (now - peakTs < peakHoldMs)
                {
                    lbl.setVisible(true);
                    lbl.setText("PEAK", juce::dontSendNotification);
                    lbl.setColour(juce::Label::backgroundColourId, juce::Colours::red);
                }
                else
                {
                    juce::Colour c = getMeterColour(db);
                    lbl.setVisible(true);
                    lbl.setText(juce::String(db, 1) + " dB", juce::dontSendNotification);
                    lbl.setColour(juce::Label::backgroundColourId, c);
                }

                lbl.repaint();
            };

        // выходные метки
        setColorSmooth(clipLedL, levelOutL, peakHoldTsOutL);
        setColorSmooth(clipLedR, levelOutR, peakHoldTsOutR);

       
    }
}


void Rig_control::handleExternalPresetChange(int newPresetIndex) noexcept
{
    manualShift = false;           
    updatePresetDisplays();        
}
void Rig_control::setTunerComponent(TunerComponent* t) noexcept
{
    externalTuner = t;
    if (externalTuner)
    {
        addAndMakeVisible(*externalTuner);    
        externalTuner->setVisible(false);    
    }
}

void Rig_control::updateTapButton(double bpm)
{
    if (2 >= 0 && 2 < sButtons.size())
    {
        juce::String text = "TAP\n" + juce::String((int)bpm) + ":BPM\n\n\n\n\n LOOPER";
        sButtons[2]->setButtonText(text);
    }
}

void Rig_control::setCurrentBpm(double bpm)
{
    currentBpm = bpm;
}
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

    // Базовые надписи
    updateSButton(0, "PRESET\nUP\n\n\n\n BANK\nMODE", presetColor);
    updateSButton(1, "PRESET\nDOWN\n\n\n\n STOMP\nMODE", presetColor);
    updateSButton(2, "TAP\n" + juce::String((int)currentBpm) + ":BPM\n\n\n\n\n LOOPER", presetColor);

    if (libraryLogic && libraryLogic->isActive())
    {
        updateSButton(0, "\n\n\n\n BANK\nOFF", presetColor);
        updateSButton(1, "\n\n\n\n\n BACK", juce::Colours::green);
        sButtons[1]->setToggleState(true, juce::dontSendNotification);
        sButtons[1]->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        updateSButton(3, "\n\n\n\n\n NEXT", juce::Colours::green);
        sButtons[3]->setToggleState(true, juce::dontSendNotification);
        sButtons[3]->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        return;
    }
    // Stomp Mode
    if (stompModeController && stompModeController->isActive())
    {
        updateSButton(1, "STOMP\n OFF\n\n\n\n\n DOWN", juce::Colours::red);
        sButtons[1]->setToggleState(true, juce::dontSendNotification);
        sButtons[1]->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    }
    else
    {
        sButtons[1]->setToggleState(false, juce::dontSendNotification);
        sButtons[1]->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    }
    // Looper Mode
    const bool looperActive = (looperComponent && looperComponent->isVisible());
    if (looperActive)
    {
        updateSButton(2, "CLEAN\n\n\n\nLOOPER\n OFF", presetColor);
        auto label = looperComponent->getControlButtonText();
        auto color = looperComponent->getControlButtonColor();
        juce::String shiftedLabel = "\n\n\n\n" + label;
        updateSButton(3, shiftedLabel, color);
        return; // выходим, чтобы не перезаписать SHIFT ниже
    }
    // SHIFT Mode
    if (shift)
    {
        updateSButton(3, "SHIFT\n\n\n\n\nTUNER", juce::Colours::blue);
        sButtons[3]->setToggleState(true, juce::dontSendNotification);
        sButtons[3]->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    }
    else
    {
        updateSButton(3, "SHIFT\n\n\n\n\nTUNER", presetColor);
        sButtons[3]->setToggleState(false, juce::dontSendNotification);
        sButtons[3]->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    }
}

void Rig_control::bankUp()
{
    auto totalBanks = (int)bankEditor->getBanks().size();
    auto idx = bankEditor->getActiveBankIndex();
    idx = (idx + 1) % totalBanks;
    bankEditor->setActiveBankIndex(idx);
}
void Rig_control::bankDown()
{
    auto totalBanks = (int)bankEditor->getBanks().size();
    auto idx = bankEditor->getActiveBankIndex();
    idx = (idx + totalBanks - 1) % totalBanks;
    bankEditor->setActiveBankIndex(idx);
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
//+++++++++++++++++++++++++++++++++++ LOOPER CONTROL ++++++++++++++++++++++++++++++++++
void Rig_control::setLooperState(bool on)
{
    if (looperComponent)
        looperComponent->setVisible(on);
    if (enginePtr)
        enginePtr->setMode(on ? LooperEngine::Mode::Looper: LooperEngine::Mode::Player);
        sendLooperToggle(on);   
    if (onLooperButtonChanged)
        onLooperButtonChanged(on);
        resized(); 
}
void Rig_control::toggleLooper()
{
    bool newState = !(looperComponent && looperComponent->isVisible());
    setLooperState(newState);
}
void Rig_control::controlLooperState(bool on)
{
    setLooperState(on);
}

bool Rig_control::getLooperState() const noexcept
{
    return looperComponent && looperComponent->isVisible();
}

void Rig_control::syncLooperStateToMidi()
{
    if (enginePtr)
        sendLooperModeToMidi(enginePtr->getState());
}
void Rig_control::setLooperEngine(LooperEngine& eng) noexcept
{
    enginePtr = &eng;
    enginePtr->onStateChanged = [this](LooperEngine::State s)
        {
            sendLooperModeToMidi(s);
        };
    enginePtr->onCleared = [this]()
        {
            sendLooperClearToMidi();
        };
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
                    sendLooperClearToMidi(); 
                }
                else
                {
                    if (enginePtr->isPlaying())
                        sendLooperModeToMidi(LooperMode::Play); 
                    else
                        sendLooperModeToMidi(LooperMode::Stop); 
                }
            }
        };
    if (!looperComponent)
    {
        looperComponent = std::make_unique<LooperComponent>(*enginePtr, true); // режим embedded
        looperComponent->setRigControl(this);
        addAndMakeVisible(*looperComponent);
        looperComponent->setVisible(false);
    }

}
void Rig_control::sendLooperModeToMidi(LooperEngine::State state)
{
    switch (state)
    {
    case LooperEngine::Recording: sendLooperModeToMidi(LooperMode::Record); break;
    case LooperEngine::Stopped:   sendLooperModeToMidi(LooperMode::Stop);   break;
    case LooperEngine::Playing:   sendLooperModeToMidi(LooperMode::Play);   break;
    case LooperEngine::Clean:     sendLooperClearToMidi();                  break;
    }
}
//+++++++++++++++++++++++++++++++ TUNER CONTROL +++++++++++++++++++++++++++++++
void Rig_control::setTunerState(bool on)
{
    if (externalTuner)
    {
        externalTuner->setVisible(on);
        externalTuner->repaint();
        resized(); 
        if (onTunerVisibilityChanged)
            onTunerVisibilityChanged(on);
    }
    sendTunerMidi(on);
}
void Rig_control::toggleTuner()
{
    bool newState = !(externalTuner && externalTuner->isVisible());
    setTunerState(newState);
}
//++++++++++++++++++++++++++++++ INPUT CONTROL +++++++++++++++++++++++++++++++++
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
//+++++++++++++++++++++++++++++++ MidiMessage +++++++++++++++++++++++++++++++++++++++++++++++++
void Rig_control::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message)
{
    if (!message.isController())
        return;

    const int cc = message.getControllerNumber();
    const int value = message.getControllerValue();
    const int channel = message.getChannel();
    const bool isOn = (value == 127);
    auto msgCopy = message;

    // единый вызов
    juce::MessageManager::callAsync([this, cc, channel, value, isOn, msgCopy]()
        {
            processMidiInput(channel, cc, value, isOn, msgCopy);
        });
}


//+++++++++++++++++++++++++++++++ КОНТРОЛЬ ПЕДАЛИ ++++++++++++++++++++++++++++++++++++
// --- Подключение педали ---
void Rig_control::handlePedalConnected()
{
    pedalOn = true;
    if (inputControl)
    {
        inputControl->setPedalConnected(true);
        pedalModeLabel.setVisible(true);
        pedalModeLabel.setText("PEDAL", juce::dontSendNotification);
        pedalModeLabel.setColour(juce::Label::backgroundColourId, juce::Colours::darkgrey);
        sendPedalConfigToMidi();
    }
}
// --- Отключение педали ---
void Rig_control::handlePedalDisconnected()
{
    pedalOn = false;
    if (inputControl)
        inputControl->setPedalConnected(false);
        pedalModeLabel.setVisible(false);
}
void Rig_control::handlePedalSwitch(int switchIndex, bool isOn)
{
    if (inputControl)
        inputControl->syncSwitchState(switchIndex, isOn);
        sendPedalSwitchState(switchIndex, isOn);
}
// --- SW1 ---
void Rig_control::handlePedalSwitch1(bool isOn)
{
    if (inputControl)
        inputControl->syncSwitchState(1, isOn);
        currentPedalMode = PedalMode::SW1;
        pedalModeLabel.setVisible(true);
        pedalModeLabel.setText("SW-1", juce::dontSendNotification);
        pedalModeLabel.setColour(juce::Label::backgroundColourId, juce::Colours::lightgreen);
        pedalModeLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    if (bankEditor)
        bankEditor->updateCCParameter(12, isOn ? 127 : 0);
}
// --- SW2 ---
void Rig_control::handlePedalSwitch2(bool isOn)
{
    if (inputControl)
        inputControl->syncSwitchState(2, isOn);
        currentPedalMode = PedalMode::SW2;
        pedalModeLabel.setVisible(true);
        pedalModeLabel.setText("SW-2", juce::dontSendNotification);
        pedalModeLabel.setColour(juce::Label::backgroundColourId, juce::Colours::yellow);
        pedalModeLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    if (bankEditor)
        bankEditor->updateCCParameter(13, isOn ? 127 : 0);
}
// --- Ось педали ---
void Rig_control::handlePedalAxis(int slot, int cc, int value)
{
    float norm = value / 127.0f;
    if (inputControl)
        inputControl->syncPedalSliderByCC(cc, value);
    if (bankEditor)
        bankEditor->applyPedalValue(slot, norm);
}
// --- Min ---
void Rig_control::handlePedalMin(int value)
{
    if (inputControl)
    {
        inputControl->syncPedalSliderByCC(5, value);
        inputControl->showPressDown();
    }
}
// --- Max ---
void Rig_control::handlePedalMax(int value)
{
    if (inputControl)
    {
        inputControl->syncPedalSliderByCC(6, value);
        inputControl->showPressUp();
    }
}
// --- AutoConfig ---
void Rig_control::handlePedalAutoConfig(bool isOn)
{
    if (inputControl)
        inputControl->syncAutoConfigButton(isOn);
}
// --- Threshold ---
void Rig_control::handlePedalThreshold(int value)
{
    if (inputControl)
    {
        inputControl->syncPedalSliderByCC(9, value);
        inputControl->showThresholdSetting();
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
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//                               + MIDI INPUT CONTROL  +                                     +
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void Rig_control::processMidiInput(int channel, int cc, int value, bool isOn, const juce::MidiMessage& msgCopy)
{
    // --- Bank control ---
    if (channel == 1 && cc == 105) bankUp();
    if (channel == 1 && cc == 106) bankDown();

    // --- Preset / Library ---
    if (!(stompModeController && stompModeController->isActive()))
    {
        if (channel == 2 && cc == 1) selectPreset(0);
        if (channel == 2 && cc == 2) selectPreset(1);
        if (channel == 2 && cc == 3) selectPreset(2);

        if (channel == 4 && cc == 1) selectLibraryFile(0);
        if (channel == 4 && cc == 2) selectLibraryFile(1);
        if (channel == 4 && cc == 3) selectLibraryFile(2);
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
    {
        manualShift = true;
        if (value == 127)
            setShiftState(!shift);
        return;
    }

    // --- Stomp ---
    if (channel == 1 && cc == 100 && value == 127)
    {
        setStompState(!(stompModeController && stompModeController->isActive()));
        return;
    }

    // --- Looper / Tuner / Tempo ---
    if (channel == 1 && cc == 102) toggleLooper();
    if (channel == 1 && cc == 110) toggleTuner();
    if (channel == 1 && cc == 103 && isOn)
    {
        if (hostComponent && hostComponent->getPluginInstance())
        {
            tapTempo.tap();
            hostComponent->updateBPM(tapTempo.getBpm());
        }
    }

    // --- Library Mode ---
    if (channel == 1 && cc == 107)
    {
        if (value == 127 && (!libraryLogic || !libraryLogic->isActive()))
        { 
            setShiftState(false);
            setStompState(false);
            toggleLibraryMode();
            sendLibraryMode(true);
            updateAllSButtons();
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
                            sendLibraryState(i, true);
                            sent = true;
                            break;
                        }
                    }
                    if (!sent && lastActivePresetIndex >= 0)
                        sendLibraryState(lastActivePresetIndex, false);
                }
            }
        }
        else if (value == 0 && (libraryLogic && libraryLogic->isActive()))
        {
            toggleLibraryMode();
            sendLibraryMode(false);
            updateAllSButtons();
        }
    }

    // --- Library navigation ---
    if (channel == 4 && cc == 105 && isOn) prevLibraryBlock();
    if (channel == 4 && cc == 101 && isOn) nextLibraryBlock();

    // --- Looper control ---
    if (channel == 1 && cc == 20 && isOn && looperComponent)
        looperComponent->handleReset();

    if (channel == 1 && cc == 24 && isOn && looperComponent)
        looperComponent->handleControl();


    // --- Volume ---
    if (msgCopy.getChannel() == 5 && msgCopy.getControllerNumber() == 1)
        if (auto* s = volumeSlider.get())
            s->setValue(msgCopy.getControllerValue(), juce::sendNotificationSync);

    // --- Pedal (channel 6) ---
    if (channel == 6 && inputControl)
    {
        switch (cc)
        {
        case 1: if (value == 0) handlePedalDisconnected(); else if (value == 127) handlePedalConnected(); break;
        case 2: if (pedalOn) handlePedalSwitch1(isOn); break;
        case 3: if (pedalOn) handlePedalSwitch2(isOn); break;
        case 4: if (pedalOn) handlePedalAxis(10, cc, value); break;
        case 8: if (pedalOn) handlePedalAxis(11, cc, value); break;
        case 5: if (pedalOn) handlePedalMin(value); break;
        case 6: if (pedalOn) handlePedalMax(value); break;
        case 7: if (pedalOn) handlePedalAutoConfig(isOn); break;
        case 9: if (pedalOn) handlePedalThreshold(value); break;
        default: break;
        }
    }
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+                                MIDI CONTROL OUT                                      +
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//++++++++++++++++++++++++++++++++ FOR FOOT PEDAL CONTROL ++++++++++++++++++++++++++++++++
void Rig_control::sendMidiCC(int channel, int cc, int value)
{
    if (midiOut)
    {
        auto msg = juce::MidiMessage::controllerEvent(channel, cc, value);
        midiOut->sendMessageNow(msg);
    }
}
void Rig_control::sendPedalSwitchState(int switchIndex, bool isOn)
{
    int cc = (switchIndex == 1 ? 2 : 3); // SW1=CC2, SW2=CC3
    int value = isOn ? 127 : 0;
    sendMidiCC(6, cc, value);
}
void Rig_control::sendPedalMin(int value)
{
    sendMidiCC(6, 5, value);
}
void Rig_control::sendPedalMax(int value)
{
    sendMidiCC(6, 6, value);
}
void Rig_control::sendPedalThreshold(int value)
{
    sendMidiCC(6, 9, value);
}
void Rig_control::sendPedalAutoConfig(bool isOn)
{
    int value = isOn ? 127 : 0;
    sendMidiCC(6, 7, value);
}
void Rig_control::sendPedalConfigToMidi()
{
    if (!inputControl) return;
    int minVal = (int)inputControl->getPedalMinSlider().getValue();
    int maxVal = (int)inputControl->getPedalMaxSlider().getValue();
    int thrVal = (int)inputControl->getPedalThresholdSlider().getValue();
    sendMidiCC(6, 5, minVal);
    sendMidiCC(6, 6, maxVal);
    sendMidiCC(6, 9, thrVal);
    bool invertState = inputControl->getInvertButton().getToggleState();
    sendMidiCC(6, 15, invertState ? 127 : 0);
}
//+++++++++++++++++++++++++++++++++ BUTTON S CONTROL MIDI OUT +++++++++++++++++++++++++++ 
void Rig_control::sendSButtonState(int ccNumber, bool pressed)
{
    sendMidiCC(1, ccNumber, pressed ? 127 : 0);
}
//+++++++++++++++++++++++++++++++++ SHIFT CONTROL MIDI OUT +++++++++++++++++++++++++++++++
void Rig_control::sendShiftState()
{
    static bool lastSentShift = false;
    bool shiftOn = shift;
    if (shiftOn != lastSentShift)
    {
        lastSentShift = shiftOn;
        sendMidiCC(1, 101, shiftOn ? 127 : 0);
    }
}
//++++++++++++++++++++ РЕЖИМ БИБЛИОТЕКИ МИДИ ОУТ +++++++++++++++++++++++++++++
void Rig_control::sendLibraryMode(bool active)
{
    sendMidiCC(1, 107, active ? 127 : 0);
}
void Rig_control::sendLibraryState(int presetIndex, bool active)
{
    if (presetIndex < 0) return;
    int value = active ? 127 : 0;
    sendMidiCC(4, presetIndex + 1, value);
    if (active) lastActivePresetIndex = presetIndex;
}
//++++++++++++++++++++++++++++++ РЕЖИМ ПРЕСЕТ МИДИ ОУТ +++++++++++++++++++++
void Rig_control::sendPresetChange(int active)
{
    if (active != lastSentPresetIndex)
    {
        lastSentPresetIndex = active;
        int presetNumber = active + 1; // 1..6
        sendMidiCC(2, presetNumber, 127);
    }
}
//++++++++++++++++++++++++++++++ РЕЖИМ СТОМP  МИДИ ОУТ +++++++++++++++++++++
void Rig_control::sendStompState()
{
    static bool lastSentStomp = false;
    bool stompOn = (stompModeController && stompModeController->isActive());
    if (stompOn != lastSentStomp)
    {
        lastSentStomp = stompOn;
        sendMidiCC(1, 100, stompOn ? 127 : 0);
    }
}
void Rig_control::setStompState(bool active)
{
    sendShiftState();
    if (stompModeController)
        stompModeController->setState(active);
    sendStompState();
    updateAllSButtons();
}
void Rig_control::sendStompButtonState(int ccIndex, bool state)
{
    int globalCC = (ccIndex % 3) + 1; // CC1..3 на канале 3
    sendMidiCC(3, globalCC, state ? 127 : 0);
}
//+++++++++++++++++++++++++ TAP TEMPO MIDI OUT +++++++++++++++++++
void Rig_control::sendBpmToMidi(double bpm)
{
    if (bpm > 381.0) bpm = 380.0;
    int cc, value;
    if (bpm < 128.0) { cc = 110; value = (int)bpm; }
    else if (bpm < 255.0) { cc = 111; value = (int)(bpm - 127.0); }
    else { cc = 112; value = (int)(bpm - 254.0); }
    sendMidiCC(5, cc, value);
}
//+++++++++++++++++++++++++++++  MENU MIDI OUT ++++++++++++++++++++
void Rig_control::sendSettingsMenuState(bool isOpen)
{
    sendMidiCC(1, 55, isOpen ? 127 : 0);
}
//++++++++++++++++++++++++++++  IMPEDANS MIDI OUT +++++++++++++++++
void Rig_control::sendImpedanceCC(int ccNumber, bool on)
{
    sendMidiCC(1, ccNumber, on ? 127 : 0);
}
//+++++++++++++++++++++++++++  TUNER MIDI OUT ++++++++++++++++++++++
void Rig_control::sendTunerMidi(bool on)
{
    sendMidiCC(1, 110, on ? 127 : 0);
}
//+++++++++++++++++++++++++ VOLUME MIDI OUT +++++++++++++++++++++++
void Rig_control::sendVolumeToController(int midiValue)
{
    sendMidiCC(5, 1, midiValue); 
}
//++++++++++++++ LOOPER / PLAYER MIDI OUT +++++++++++++
void Rig_control::sendLooperModeToMidi(LooperMode mode)
{
    int cc = 0;
    switch (mode)
    {
    case LooperMode::Record: cc = 21; break;
    case LooperMode::Stop:   cc = 22; break;
    case LooperMode::Play:   cc = 23; break;
    }
    sendMidiCC(1, cc, 127);
}
void Rig_control::sendPlayerModeToMidi(bool isPlaying)
{
    int cc = isPlaying ? 23 : 22;
    sendMidiCC(1, cc, 127);
}
void Rig_control::sendLooperClearToMidi()
{
    sendMidiCC(1, 20, 127);
    sendMidiCC(1, 20, 0);
}
void Rig_control::sendLooperToggle(bool on)
{
    sendMidiCC(1, 102, on ? 127 : 0);
}




















