#include"bank_editor.h"

//==============================================================================
BankEditor::BankEditor(VSTHostComponent* host)
    : vstHost(host)
{ 
    // говорим JUCE, что мы можем работать с VST и VST3
    formatManager.addDefaultFormats();

    // Row 0
    addAndMakeVisible(bankIndexLabel);
    bankIndexLabel.setJustificationType(juce::Justification::centred);
    // 1) жестко задаём шрифт
    bankIndexLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    // 2) сбрасываем кеш и принудительно перерисуем
    bankIndexLabel.repaint();
    
    // Row 1: SELECT BANK / PRESET1…6 / VST
    addAndMakeVisible(selectBankButton);
    selectBankButton.setButtonText("SELECT BANK");
    selectBankButton.addListener(this);

    for (int i = 0; i < numPresets; ++i)
    {
        addAndMakeVisible(presetButtons[i]);
        presetButtons[i].setButtonText("PRESET " + juce::String(i + 1));
        presetButtons[i].addListener(this);
    }

    addAndMakeVisible(vstButton);
    vstButton.setButtonText("VST");
    vstButton.addListener(this);

    // Row 2: Bank name editor, Preset editors, Plugin label
    addAndMakeVisible(bankNameEditor);
    bankNameEditor.setMultiLine(false);
    bankNameEditor.setReturnKeyStartsNewLine(false);
    bankNameEditor.onTextChange = [this]()
        {
            banks[activeBankIndex].bankName = bankNameEditor.getText();
            bankIndexLabel.setText(juce::String(activeBankIndex + 1),
                juce::dontSendNotification);

            // оповещаем RigComponent сразу
            if (onBankEditorChanged)
                onBankEditorChanged();
        };

    for (int i = 0; i < numPresets; ++i)
    {
        addAndMakeVisible(presetEditors[i]);
        presetEditors[i].setMultiLine(false);
        presetEditors[i].setReturnKeyStartsNewLine(false);
        presetEditors[i].onTextChange = [this, i]()
            {
                banks[activeBankIndex].presetNames[i] = presetEditors[i].getText();
                if (i == activePreset)
                    updateSelectedPresetLabel();

                // оповещаем RigComponent сразу
                if (onBankEditorChanged)
                    onBankEditorChanged();

            };
    }

    // Row 2: Plugin label
    
    addAndMakeVisible(pluginLabel);
    pluginLabel.setJustificationType(juce::Justification::centredLeft);
    // 1) чуть помельче
    pluginLabel.setFont(juce::Font(12.0f, juce::Font::plain));
    pluginLabel.repaint();

    // Set background colour for bankIndexLabel & pluginLabel to button colour
    auto btnBg = selectBankButton.findColour(juce::TextButton::buttonColourId);
    bankIndexLabel.setColour(juce::Label::backgroundColourId, btnBg);
    bankIndexLabel.setOpaque(true);
    pluginLabel.setColour(juce::Label::backgroundColourId, btnBg);
    pluginLabel.setOpaque(true);

    // Row 3: selectedPresetLabel
    addAndMakeVisible(selectedPresetLabel);
    selectedPresetLabel.setJustificationType(juce::Justification::centred);
    selectedPresetLabel.setOpaque(false);

    // Row 4: SET CC buttons
    for (int i = 0; i < numCCParams; ++i)
    {
        addAndMakeVisible(setCCButtons[i]);
        setCCButtons[i].setButtonText("SET CC" + juce::String(i + 1));
        setCCButtons[i].addListener(this);
    }

    // Row 5: CC name editors
    for (int i = 0; i < numCCParams; ++i)
    {
        addAndMakeVisible(ccNameEditors[i]);
        ccNameEditors[i].setMultiLine(false);
        ccNameEditors[i].setReturnKeyStartsNewLine(false);
        ccNameEditors[i].onTextChange = [this, i]()
            {
                banks[activeBankIndex].ccMappings[i].name = ccNameEditors[i].getText();
            };
        ccNameEditors[i].setColour(juce::TextEditor::backgroundColourId, btnBg);
        ccNameEditors[i].setOpaque(true);
    }

    // Row 6: CC toggle buttons (красный when On)
    for (int i = 0; i < numCCParams; ++i)
    {
        addAndMakeVisible(ccToggleButtons[i]);
        ccToggleButtons[i].setButtonText("CC " + juce::String(i + 1));
        ccToggleButtons[i].setClickingTogglesState(true);
        ccToggleButtons[i].setToggleState(false, juce::dontSendNotification);
        ccToggleButtons[i].setColour(juce::TextButton::buttonOnColourId,
            juce::Colours::red);
        ccToggleButtons[i].addListener(this);
    }

    // Row 11: Default, Load, Store, Save, Cancel
    addAndMakeVisible(defaultButton);
    defaultButton.setButtonText("Default");
    defaultButton.addListener(this);
    defaultButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red);

    addAndMakeVisible(loadButton);
    loadButton.setButtonText("Load");
    loadButton.addListener(this);

    addAndMakeVisible(storeButton);
    storeButton.setButtonText("Store");
    storeButton.addListener(this);

    addAndMakeVisible(saveButton);
    saveButton.setButtonText("Save");
    saveButton.addListener(this);

    addAndMakeVisible(cancelButton);
    cancelButton.setButtonText("Cancel");
    cancelButton.addListener(this);

    // заполнить banks дефолтными значениями
    banks.resize(numBanks);
    // запустить автосохранение каждые 30 секунд
    startTimer(30 * 1000);

    // установить UI по умолчанию
    setActiveBankIndex(0);
    setActivePreset(0);

    vstButton.onClick = [this] { showVSTDialog(); };
    
}

BankEditor::~BankEditor() = default;

//==============================================================================
void BankEditor::paint(juce::Graphics& g)
{
    g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));
}

void BankEditor::resized()
{
    // Область с отступом
    auto area = getLocalBounds().reduced(8);
    int W = area.getWidth();
    int H = area.getHeight();
    int sW = W / 20;    // ширина «ячейки»
    int sH = H / 12;    // высота «строки»

    // —————————— Row 0: bankIndexLabel ——————————
    {
        // шрифт 50% от высоты строки
        float indexFontSize = sH * 0.5f;
        bankIndexLabel.setFont(juce::Font(indexFontSize, juce::Font::bold));
        bankIndexLabel.setJustificationType(juce::Justification::centred);
        // минимум 70% горизонтального масштаба
        bankIndexLabel.setMinimumHorizontalScale(0.7f);
        bankIndexLabel.setBounds(0 * sW, 0 * sH, sW, sH);
    }

    // —————————— Row 1: SELECT / PRESET1…6 / VST ——————————
    {
        int bw = W / 8;
        for (int i = 0; i < 8; ++i)
        {
            juce::Button* b = (i == 0 ? &selectBankButton
                : i < 7 ? &presetButtons[i - 1]
                : &vstButton);
            b->setBounds(i * bw, 1 * sH, bw, sH);
        }
    }

    // —————————— Row 2: Bank name editor, Preset editors, Plugin label ——————————
    {
        int fw = W / 8;
        int fy = 2 * sH;

        bankNameEditor.setBounds(0 * fw, fy, fw, sH);
        for (int i = 0; i < numPresets; ++i)
            presetEditors[i].setBounds((i + 1) * fw, fy, fw, sH);

        // плагин-лейбл
        pluginLabel.setFont(juce::Font(sH * 0.45f, juce::Font::plain));
        pluginLabel.setJustificationType(juce::Justification::centredLeft);
        pluginLabel.setMinimumHorizontalScale(0.7f);
        pluginLabel.setBounds(7 * fw, fy, fw, sH);
    }

    // —————————— Row 3: selectedPresetLabel ——————————
    selectedPresetLabel.setBounds(0, 3 * sH, W, sH);

    // —————————— Row 4: SET CC buttons ——————————
    for (int i = 0; i < numCCParams; ++i)
        setCCButtons[i].setBounds(i * 2 * sW, 4 * sH, 2 * sW, sH);

    // —————————— Row 5: CC name editors ——————————
    for (int i = 0; i < numCCParams; ++i)
        ccNameEditors[i].setBounds(i * 2 * sW, 5 * sH, 2 * sW, sH);

    // —————————— Row 6: CC toggle buttons ——————————
    for (int i = 0; i < numCCParams; ++i)
        ccToggleButtons[i].setBounds(i * 2 * sW, 6 * sH, 2 * sW, sH);

    // —————————— Row 11: Default/Load/Store/Save/Cancel ——————————
    defaultButton.setBounds(0 * sW, 11 * sH, 2 * sW, sH);
    loadButton.setBounds(6 * sW, 11 * sH, 2 * sW, sH);
    storeButton.setBounds(9 * sW, 11 * sH, 2 * sW, sH);
    saveButton.setBounds(12 * sW, 11 * sH, 2 * sW, sH);
    cancelButton.setBounds(18 * sW, 11 * sH, 2 * sW, sH);
}

//==============================================================================
void BankEditor::buttonClicked(juce::Button* b)
{
    // SELECT BANK
    if (b == &selectBankButton) { showBankSelectionMenu();   return; }
    // VST
    if (b == &vstButton) { showVSTDialog();           return; }

    // PRESET 1…6
    for (int i = 0; i < numPresets; ++i)
        if (b == &presetButtons[i]) { setActivePreset(i);       return; }

    // SET CC 1…10
    for (int i = 0; i < numCCParams; ++i)
        if (b == &setCCButtons[i]) { editCCParameter(i);       return; }

    // TOGGLE CC 1…10
    for (int i = 0; i < numCCParams; ++i)
        if (b == &ccToggleButtons[i])
        {
            banks[activeBankIndex].ccMappings[i].enabled
                = ccToggleButtons[i].getToggleState();
            return;
        }

    // bottom row
    if (b == &defaultButton) resetAllDefaults();
    else if (b == &loadButton)    loadFromDisk();
    else if (b == &storeButton)   storeToBank();
    else if (b == &saveButton)    saveToDisk();
    else if (b == &cancelButton)  cancelChanges();
}

//==============================================================================
void BankEditor::showBankSelectionMenu()
{
    juce::PopupMenu menu;

    // 1) Формируем пункты: ID = i+1, текст = имя банка, галочка у текущего
    for (int i = 0; i < numBanks; ++i)
        menu.addItem(i + 1,
            banks[i].bankName,
            /*isEnabled*/ true,
            /*isTicked*/   (i == activeBankIndex));

    // 2) Показываем меню асинхронно прямо под кнопкой selectBankButton
    menu.showMenuAsync(juce::PopupMenu::Options()
        .withTargetComponent(&selectBankButton)
        .withPreferredPopupDirection(
            juce::PopupMenu::Options::PopupDirection::downwards)
        ,
        [this](int result)
        {
            // result==0 — отмена, result>0 — выбран пункт с ID=result
            if (result > 0)
                setActiveBankIndex(result - 1);

        });
}

/*
void BankEditor::showVSTDialog()
{
    // TODO: ваш диалог выбора VST-плагина
    // После выбора:
    // banks[activeBankIndex].pluginName = выбранноеИмя;
    // pluginLabel.setText ("Plugin: " + banks[activeBankIndex].pluginName,
    //                      juce::dontSendNotification);
}
*/
void BankEditor::editCCParameter(int ccIndex)
{
    
    // 1) Копируем текущий mapping (в том числе его name) из модели
    const CCMapping initialMap = banks[activeBankIndex].ccMappings[ccIndex];

    // 2) Готовим заголовок окна
    juce::String slotName = "Set CC " + juce::String(ccIndex + 1);

    // 3) Создаём диалог и показываем его
    new SetCCDialog(vstHost,          // ваш VSTHostComponent*
        initialMap,       // копия текущего состояния CCMapping
        slotName,         // заголовок
        [this, ccIndex]   // callback-лямбда
        (CCMapping newMap, bool ok)
        {
            if (!ok)
                return;  // пользователь закрыл диалог по Cancel

            // 4) Сохраняем результат в модель
            banks[activeBankIndex].ccMappings[ccIndex] = newMap;

            // 5) Обновляем UI:
            //    – текстовый редактор ccNameEditors показывает newMap.name
            ccNameEditors[ccIndex]
                .setText(newMap.name,
                    juce::dontSendNotification);

            //    – toggle-кнопка отражает newMap.enabled
            ccToggleButtons[ccIndex]
                .setToggleState(newMap.enabled,
                    juce::dontSendNotification);
        });
}


//==============================================================================
void BankEditor::setActiveBankIndex(int newIdx)
{
    setActiveBank(newIdx);
    if (onBankEditorChanged) onBankEditorChanged();
    restartAutoSaveTimer();
}

void BankEditor::setActiveBank(int newBank)
{
    activeBankIndex = juce::jlimit(0, numBanks - 1, newBank);
    updateUI();
}

void BankEditor::setActivePreset(int newPreset)
{
    activePreset = juce::jlimit(0, numPresets - 1, newPreset);
    updateSelectedPresetLabel();
}

void BankEditor::updateUI()
{
    // Bank index & name
    bankIndexLabel.setText(juce::String(activeBankIndex + 1),
        juce::dontSendNotification);
    bankNameEditor.setText(banks[activeBankIndex].bankName,
        juce::dontSendNotification);

    // Preset names
    for (int i = 0; i < numPresets; ++i)
        presetEditors[i].setText(banks[activeBankIndex].presetNames[i],
            juce::dontSendNotification);

    // Plugin label
    pluginLabel.setText(banks[activeBankIndex].pluginName,
        juce::dontSendNotification);

    // Selected preset label
    updateSelectedPresetLabel();

    // CC names & toggles
    for (int i = 0; i < numCCParams; ++i)
    {
        ccNameEditors[i].setText(
            banks[activeBankIndex].ccMappings[i].name,
            juce::dontSendNotification);

        ccToggleButtons[i].setToggleState(
            banks[activeBankIndex].ccMappings[i].enabled,
            juce::dontSendNotification);
    }
}

void BankEditor::updateSelectedPresetLabel()
{
    selectedPresetLabel.setText(
        banks[activeBankIndex].presetNames[activePreset],
        juce::dontSendNotification);
}

//==============================================================================
void BankEditor::timerCallback()
{
    saveSettings();
}

void BankEditor::restartAutoSaveTimer()
{
    stopTimer();
    startTimer(30 * 1000);
}

//==============================================================================
void BankEditor::loadSettings()
{
    auto file = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("MyPluginBanks.conf");
    loadSettingsFromFile(file);
    updateUI();
}

void BankEditor::saveSettings()
{
    auto file = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("MyPluginBanks.conf");
    saveSettingsToFile(file);
}

void BankEditor::loadSettingsFromFile(const juce::File& file)
{
    // TODO: парсинг из файла → заполняете banks vector
}

void BankEditor::saveSettingsToFile(const juce::File& file)
{
    // TODO: сериализация banks → записать в файл
}

//==============================================================================
void BankEditor::resetAllDefaults()
{
    for (int i = 0; i < numBanks; ++i)
        banks[i] = Bank();

    setActiveBankIndex(0);
    setActivePreset(0);
    updateUI();
}

void BankEditor::storeToBank()
{
    // TODO: «Store» (записать текущую конфигурацию в железо или память)
}

void BankEditor::cancelChanges()
{
    loadSettings();
}
//------------------------------------------------------------------------------
// Заглушки для кнопок Default/Load/Store/Save/Cancel
//------------------------------------------------------------------------------

void BankEditor::loadFromDisk()
{
    // просто переложим на ваш метод loadSettings()
    loadSettings();
    updateUI();
    restartAutoSaveTimer();
}

void BankEditor::saveToDisk()
{
    // аналогично — на saveSettings()
    saveSettings();
    restartAutoSaveTimer();
}
void BankEditor::showVSTDialog()
{
    // Сбрасываем всё “висящее” меню
    juce::PopupMenu::dismissAllActiveMenus();

    if (vstHost == nullptr)
        return;

    // —————————————————————————————————————————
    // Ветка A: плагин ещё НЕ загружен
    // —————————————————————————————————————————
    if (vstHost->getPluginInstance() == nullptr)
    {
        const auto& files = vstHost->getPluginFiles();
        if (files.isEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "No Plugins Found",
                "No plugins found in default folders.");
            return;
        }

        juce::PopupMenu menu;
        for (int i = 0; i < files.size(); ++i)
            menu.addItem(i + 1, files.getReference(i).getFileName());

        menu.showMenuAsync(
            juce::PopupMenu::Options().withTargetComponent(&vstButton),
            [this](int choice)
            {
                if (choice <= 0)
                    return;

                const auto& files2 = vstHost->getPluginFiles();
                auto         fileToLoad = files2.getReference(choice - 1);

                auto& adm = vstHost->getAudioDeviceManagerRef();
                auto* dev = adm.getCurrentAudioDevice();
                double sr = dev ? dev->getCurrentSampleRate() : 44100.0;
                int    bs = dev ? dev->getCurrentBufferSizeSamples() : 512;

                vstHost->loadPlugin(fileToLoad, sr, bs);
                pluginLabel.setText(fileToLoad.getFileName(),
                    juce::dontSendNotification);
                vstButton.setButtonText("Close Plugin");
            });

        return;
    }

    // —————————————————————————————————————————
    // Ветка B: плагин ЗАГРУЖЕН → спрашиваем выгрузку
    // —————————————————————————————————————————
    auto* aw = new juce::AlertWindow(
        "Close Plugin",
        "Are you sure you want to close the current plugin?",
        juce::AlertWindow::WarningIcon);

    // 1) создаём две кнопки
    aw->addButton("Yes", 1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    // 2) достаём сами Button* и вешаем onClick, где:
    //    – сначала снимаем модальность,
    //    – прячем окно,
    //    – выполняем логику,
    //    – удаляем окно из кучи.
    if (auto* yesBtn = aw->getButton(0))
    {
        yesBtn->onClick = [aw, this]
            {
                aw->exitModalState(1);
                aw->setVisible(false);

                vstHost->unloadPlugin();
                pluginLabel.setText("Plugin: None",
                    juce::dontSendNotification);
                vstButton.setButtonText("Load Plugin");

                delete aw;
            };
    }

    if (auto* noBtn = aw->getButton(1))
    {
        noBtn->onClick = [aw]
            {
                aw->exitModalState(0);
                aw->setVisible(false);
                delete aw;
            };
    }

    // 3) запускаем как модальное, без автоматического удаления коллбэка
    aw->enterModalState(true, nullptr, false);
}
//==============================================================================
//===== PUBLIC API IMPLEMENTATION =============================================

void BankEditor::setActivePresetIndex(int newPresetIndex)
{
    if (newPresetIndex >= 0 && newPresetIndex < numPresets)
    {
        activePreset = newPresetIndex;
        updateSelectedPresetLabel();
        sendChange();
    }
}

juce::StringArray BankEditor::getPresetNames(int bankIndex) const noexcept
{
    juce::StringArray names;
    if (bankIndex >= 0 && bankIndex < (int)banks.size())
    {
        for (int i = 0; i < numPresets; ++i)
            names.add(banks[bankIndex].presetNames[i]);
    }
    return names;
}

std::vector<CCMapping> BankEditor::getCCMappings(int bankIndex,
    int presetIndex) const
{
    std::vector<CCMapping> mappings;
    if (bankIndex >= 0 && bankIndex < (int)banks.size()
        && presetIndex >= 0 && presetIndex < numPresets)
    {
        const auto& bank = banks[bankIndex];
        const auto& states = bank.ccPresetStates[presetIndex];
        for (int cc = 0; cc < numCCParams; ++cc)
        {
            auto m = bank.ccMappings[cc];
            m.enabled = states[cc];
            mappings.push_back(m);
        }
    }
    return mappings;
}





