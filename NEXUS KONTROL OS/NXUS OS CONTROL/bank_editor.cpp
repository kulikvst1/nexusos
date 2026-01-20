#include"bank_editor.h"
#include "plugin_process_callback.h"     // ← без лишней точки!
#include "custom_audio_playhead.h"
#include "LearnController.h"
#include "cpu_load.h"
#include <memory>
#include <atomic>

namespace {
    CCMapping combineMapping(const CCMapping& global, const PresetCCMapping& preset)
    {
        CCMapping result;
        result.paramIndex = global.paramIndex;  // Глобальное назначение сохраняется
        result.name = global.name;          // Глобальное имя
        result.enabled = preset.enabled;       // Пресетное состояние
        result.ccValue = preset.ccValue;       // Пресетное значение
        result.invert = preset.invert;        // Пресетная инверсия
        return result;
    }
}

//==============================================================================
BankEditor::BankEditor(VSTHostComponent* host)
    : vstHost(host)
{
    ensureDefaultConfigExists(); // создаём или загружаем дефолтный конфиг
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
    bankNameEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0, 0, 80));  // Изменяем цвет
    bankNameEditor.setOpaque(true);
    bankNameEditor.setFont(juce::Font(20.0f));        // Устанавливаем шрифт 18pt
    bankNameEditor.setJustification(juce::Justification::centred);  // Центрируем текст по горизонтали
    // Обработчик изменения текста
    bankNameEditor.onTextChange = [this]() {
        banks[activeBankIndex].bankName = bankNameEditor.getText();
        bankIndexLabel.setText(juce::String(activeBankIndex + 1), juce::dontSendNotification);
        if (onBankEditorChanged)
            onBankEditorChanged();
        };

    // Теперь для presetEditors:
    for (int i = 0; i < numPresets; ++i)
    {
        addAndMakeVisible(presetEditors[i]);
        presetEditors[i].setMultiLine(false);
        presetEditors[i].setReturnKeyStartsNewLine(false);
        presetEditors[i].setColour(juce::TextEditor::backgroundColourId, juce::Colour(0, 0, 80)); // Изменяем цвет 
        presetEditors[i].setOpaque(true);
        presetEditors[i].setFont(juce::Font(20.0f));     // Устанавливаем шрифт 16pt
        presetEditors[i].setJustification(juce::Justification::centred);  // Центрируем текст
        // Обработчик изменения текста 
        presetEditors[i].onTextChange = [this, i]()
            {
                banks[activeBankIndex].presetNames[i] = presetEditors[i].getText();
                if (i == activePreset)
                    updateSelectedPresetLabel(); // Обновляем отображаемое имя выбранного пресета
                if (onBankEditorChanged)
                    onBankEditorChanged();
            };



    }
    // Row 2: Plugin label
    addAndMakeVisible(pluginLabel);
    pluginLabel.setJustificationType(juce::Justification::centredLeft);
    pluginLabel.setFont(juce::Font(12.0f, juce::Font::plain));// Устанавливаем шрифт 
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
    selectedPresetLabel.setFont(juce::Font(30.0f));// Устанавливаем шрифт

    // Row 4: SET CC buttons
    for (int i = 0; i < numCCParams; ++i)
    {
        addAndMakeVisible(setCCButtons[i]);
        setCCButtons[i].setButtonText("SET CC" + juce::String(i + 1));
        setCCButtons[i].addListener(this);
        setCCButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colour(213, 204, 175));
        setCCButtons[i].setColour(juce::TextButton::textColourOnId, juce::Colours::black);// Устанавливаем шрифт 16pt
        setCCButtons[i].setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    }

    // Row 5: CC name editors
    for (int i = 0; i < numCCParams; ++i)
    {
        addAndMakeVisible(ccNameEditors[i]);
        ccNameEditors[i].setMultiLine(false);
        ccNameEditors[i].setReturnKeyStartsNewLine(false);
        ccNameEditors[i].setFont(juce::Font(18.0f));
        ccNameEditors[i].setJustification(juce::Justification::centred);
        ccNameEditors[i].onTextChange = [this, i]()
            {
                // 1. Сохраняем новое имя в модель
                banks[activeBankIndex].globalCCMappings[i].name = ccNameEditors[i].getText();

                // 2. Уведомляем всех подписчиков (в т.ч. Rig_control)
                if (onBankEditorChanged)
                    onBankEditorChanged();
            };

        ccNameEditors[i].setColour(juce::TextEditor::backgroundColourId, juce::Colour(0, 0, 80));// Изменяем цвет
        ccNameEditors[i].setOpaque(true);
    }

    // Row 6: CC toggle buttons (active = red, inactive = dark red)
    for (int i = 0; i < numCCParams; ++i)
    {
        addAndMakeVisible(ccToggleButtons[i]);
        ccToggleButtons[i].setButtonText("CC " + juce::String(i + 1));
        ccToggleButtons[i].setClickingTogglesState(true);
        ccToggleButtons[i].setToggleState(false, juce::dontSendNotification);
        ccToggleButtons[i].setColour(juce::TextButton::buttonOnColourId, juce::Colours::red); // Цвет кнопки в активном состоянии 
        ccToggleButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colour(50, 0, 0));// Цвет кнопки в неактивном состоянии 
        ccToggleButtons[i].addListener(this);
    }

    // --- Row 7: LEARN CC buttons
    for (int i = 0; i < numCCParams; ++i)
    {
        addAndMakeVisible(learnButtons[i]);
        learnButtons[i].setButtonText("LEARN " + juce::String(i + 1));
        learnButtons[i].setColour(juce::TextButton::buttonColourId,
            juce::Colour(0, 80, 0));
        learnButtons[i].setColour(juce::TextButton::textColourOffId,
            juce::Colours::white);
        learnButtons[i].addListener(this);
        learnButtons[i].setTooltip("Click and twist the plugin knob"
            + juce::String(i + 1));
    }

    // Row 11: Default, Load, Store, Save, Cancel
    addAndMakeVisible(defaultButton);
    defaultButton.setButtonText("Default");
    defaultButton.addListener(this);
    defaultButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red);

    addAndMakeVisible(storeButton);
    storeButton.setButtonText("Store");
    storeButton.addListener(this);

    addAndMakeVisible(loadButton);
    loadButton.setButtonText("Load");
    loadButton.addListener(this);

    addAndMakeVisible(saveButton);
    saveButton.setButtonText("Save");
    saveButton.addListener(this);

    addAndMakeVisible(cancelButton);
    cancelButton.setButtonText("Cancel");
    cancelButton.addListener(this);

    // заполнить banks дефолтными значениями
    ensureDefaultConfigExists();
    // запустить автосохранение каждые 30 секунд
   // startTimer(30 * 1000);

    // установить UI по умолчанию
    setActiveBankIndex(0);
    setActivePreset(0);
    // ─────────────── Pedal slots ───────────────
    // Pedal 1
    learnButtons[10].setButtonText("Learn Pedal 1");
    setCCButtons[10].setButtonText("Set Pedal 1");
    ccNameEditors[10].setText("Name Pedal 1");
    ccToggleButtons[10].setButtonText("Toggle Pedal 1");

    // Pedal 2
    learnButtons[11].setButtonText("Learn Pedal 2");
    setCCButtons[11].setButtonText("Set Pedal 2");
    ccNameEditors[11].setText("Name Pedal 2");
    ccToggleButtons[11].setButtonText("Toggle Pedal 2");

    // SW1
    learnButtons[12].setButtonText("Learn SW1");
    setCCButtons[12].setButtonText("Set SW1");
    ccNameEditors[12].setText("Name SW1");
    ccToggleButtons[12].setButtonText("Toggle SW1");

    // SW2
    learnButtons[13].setButtonText("Learn SW2");
    setCCButtons[13].setButtonText("Set SW2");
    ccNameEditors[13].setText("Name SW2");
    ccToggleButtons[13].setButtonText("Toggle SW2");

    addAndMakeVisible(pedalGroup);
    pedalGroup.setText("Pedals & Switches");
    pedalGroup.setTextLabelPosition(juce::Justification::centredTop);

    // >>> Здесь регулируешь цвет рамки и текста <<<
    pedalGroup.setColour(juce::GroupComponent::outlineColourId, juce::Colours::orange);
    pedalGroup.setColour(juce::GroupComponent::textColourId, juce::Colours::white);

     // 2. ПОДПИСКА на изменение параметров плагина  ▶▶▶  ДОБАВЬТЕ ЭТО
   
    if (vstHost != nullptr)
    {
        // 1) уже существующая подписка на параметры — оставляем без изменений
        vstHost->setParameterChangeCallback([this](int idx, float norm)
            {
                onPluginParameterChanged(idx, norm);
            });

        // 2) Preset: VSTHost → BankEditor
        vstHost->setPresetCallback([this](int idx)
            {
                if (isSettingPreset)            // guard-флаг: если мы внутри setActivePreset — выходим
                    return;

                isSettingPreset = true;         // блокируем повторные входы
                setActivePreset(idx);           // вызываем вашу «тяжёлую» логику
                isSettingPreset = false;        // снимаем блок
            });

        // 3) Убираем двустороннюю подписку — теперь UI не дергает хост напрямую
        onActivePresetChanged = nullptr;
    }

    // Learn-колл-бэк по той же схеме
    if (vstHost != nullptr)
    {
        vstHost->setLearnCallback([this](int cc, bool on)
            {
                if (isSettingLearn)             // guard-флаг для Learn
                    return;

                isSettingLearn = true;
                toggleLearnFromHost(cc, on);    // ваша внутренняя логика learn.begin()/cancel()
                isSettingLearn = false;
            });

        onLearnToggled = nullptr;          // убираем старый обратный зов
    }
    startTimerHz(2); // мигание 2 раза в секунду

}
BankEditor::~BankEditor()
{
    if (vstHost != nullptr)
        vstHost->setParameterChangeCallback(nullptr);  // снимаем слушатель
}
//==============================================================================
void BankEditor::paint(juce::Graphics& g)
{
    g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));
}
void BankEditor::resized()
{
    // Область с равными отступами
    auto area = getLocalBounds().reduced(8);
    int baseX = area.getX();
    int baseY = area.getY();
    int W = area.getWidth();
    int H = area.getHeight();
    int sW = W / 20;    // ширина «ячейки»
    int sH = H / 12;    // высота «строки»

    // —————————— Row 0: bankIndexLabel ——————————
    {
        float indexFontSize = sH * 0.5f;
        bankIndexLabel.setFont(juce::Font(indexFontSize, juce::Font::bold));
        bankIndexLabel.setJustificationType(juce::Justification::centred);
        bankIndexLabel.setMinimumHorizontalScale(0.7f);
        bankIndexLabel.setBounds(baseX + 0 * sW, baseY + 0 * sH, sW, sH);
    }

    // —————————— Row 1: SELECT / PRESET1…6 / VST ——————————
    {
        int bw = W / 8;
        for (int i = 0; i < 8; ++i)
        {
            juce::Button* b = (i == 0 ? &selectBankButton
                : i < 7 ? &presetButtons[i - 1]
                : &vstButton);
            b->setBounds(baseX + i * bw, baseY + 1 * sH, bw, sH);
        }
    }

    // —————————— Row 2: Bank name editor, Preset editors, Plugin label ——————————
    {
        int fw = W / 8;
        int fy = baseY + 2 * sH;
        bankNameEditor.setBounds(baseX + 0 * fw, fy, fw, sH);
        for (int i = 0; i < numPresets; ++i)
            presetEditors[i].setBounds(baseX + (i + 1) * fw, fy, fw, sH);
        pluginLabel.setFont(juce::Font(sH * 0.45f, juce::Font::plain));
        pluginLabel.setJustificationType(juce::Justification::centredLeft);
        pluginLabel.setMinimumHorizontalScale(0.7f);
        pluginLabel.setBounds(baseX + 7 * fw, fy, fw, sH);
    }

    // —————————— Row 3: selectedPresetLabel ——————————
    selectedPresetLabel.setBounds(baseX, baseY + 3 * sH, W, sH);

    // —————————— Row 4: Learn buttons (0..9) ——————————
    for (int i = 0; i < 10; ++i)
        learnButtons[i].setBounds(baseX + i * 2 * sW, baseY + 4 * sH, 2 * sW, sH);

    // —————————— Row 5: SET CC buttons (0..9) ——————————
    for (int i = 0; i < 10; ++i)
        setCCButtons[i].setBounds(baseX + i * 2 * sW, baseY + 5 * sH, 2 * sW, sH);

    // —————————— Row 6: CC name editors (0..9) ——————————
    for (int i = 0; i < 10; ++i)
        ccNameEditors[i].setBounds(baseX + i * 2 * sW, baseY + 6 * sH, 2 * sW, sH);

    // —————————— Row 7: CC toggle buttons (0..9) ——————————
    for (int i = 0; i < 10; ++i)
        ccToggleButtons[i].setBounds(baseX + i * 2 * sW, baseY + 7 * sH, 2 * sW, sH);

    // ─────────────── Pedal block (центрированный, с рамкой) ───────────────
    int startCol = 6;              // начало блока (из 20 колонок)
    int offsetX = baseX + startCol * sW;
    int blockWidth = 8 * sW;         // ширина блока педалей (8 колонок)
    int blockHeight = 2 * sH;         // высота блока педалей (2 строки)

    // >>> Регуляторы <<<
    // Вертикальное смещение блока педалей (в долях строки)
    int pedalYOffset = sH / 2;        // половина строки вниз

    // Дополнительная ширина рамки (в долях колонки)
    int pedalExtraWidth = sW / 4;     // рамка шире на пол-колонки слева и справа

    // Дополнительная высота рамки (в долях строки)
    int pedalExtraHeight = sH / 3;    // рамка выше на пол-строки сверху и снизу

    // Новые координаты с учётом смещения
    int y8 = baseY + 8 * sH + pedalYOffset; // строка Learn/Set
    int y9 = baseY + 9 * sH + pedalYOffset; // строка Name

    // Рамка вокруг блока педалей
    pedalGroup.setBounds(offsetX - pedalExtraWidth,                // левее на pedalExtraWidth
        y8 - pedalExtraHeight,                    // выше на pedalExtraHeight
        blockWidth + 2 * pedalExtraWidth,         // шире на 2*pedalExtraWidth
        blockHeight + 2 * pedalExtraHeight);      // выше/ниже на 2*pedalExtraHeight

    // Row 8: Learn+Set (по половине сектора)
    learnButtons[10].setBounds(offsetX + 0 * sW, y8, sW, sH);
    setCCButtons[10].setBounds(offsetX + 1 * sW, y8, sW, sH);

    learnButtons[12].setBounds(offsetX + 2 * sW, y8, sW, sH);
    setCCButtons[12].setBounds(offsetX + 3 * sW, y8, sW, sH);

    learnButtons[11].setBounds(offsetX + 4 * sW, y8, sW, sH);
    setCCButtons[11].setBounds(offsetX + 5 * sW, y8, sW, sH);

    learnButtons[13].setBounds(offsetX + 6 * sW, y8, sW, sH);
    setCCButtons[13].setBounds(offsetX + 7 * sW, y8, sW, sH);

    // Row 9: Name‑редакторы под каждой парой
    ccNameEditors[10].setBounds(offsetX + 0 * sW, y9, 2 * sW, sH);
    ccNameEditors[12].setBounds(offsetX + 2 * sW, y9, 2 * sW, sH);
    ccNameEditors[11].setBounds(offsetX + 4 * sW, y9, 2 * sW, sH);
    ccNameEditors[13].setBounds(offsetX + 6 * sW, y9, 2 * sW, sH);

    // —————————— Row 11: Default/Load/Store/Save/Cancel ——————————
    defaultButton.setBounds(baseX + 0 * sW, baseY + 11 * sH, 2 * sW, sH);
    loadButton.setBounds(baseX + 6 * sW, baseY + 11 * sH, 2 * sW, sH);
    storeButton.setBounds(baseX + 9 * sW, baseY + 11 * sH, 2 * sW, sH);
    saveButton.setBounds(baseX + 12 * sW, baseY + 11 * sH, 2 * sW, sH);
    cancelButton.setBounds(baseX + 18 * sW, baseY + 11 * sH, 2 * sW, sH);
}



//==============================================================================
void BankEditor::updatePresetButtons()
{
    for (int i = 0; i < numPresets; ++i)
    {
        if (i == activePreset)
        {
            // Для выбранного пресета: синий фон, белый текст
            presetButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colours::blue);
            presetButtons[i].setColour(juce::TextButton::textColourOnId, juce::Colours::white);
            presetButtons[i].setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        }
        else
        {
            // Для остальных: серый фон, чёрный текст
            presetButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colours::grey);
            presetButtons[i].setColour(juce::TextButton::textColourOnId, juce::Colours::black);
            presetButtons[i].setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        }
    }
}
void BankEditor::buttonClicked(juce::Button* b)
{
    // SELECT BANK
    if (b == &selectBankButton) { showBankSelectionMenu();   return; }
    // VST
    if (b == &vstButton) { showVSTDialog();           return; }

    // PRESET 1–6
    for (int i = 0; i < numPresets; ++i)
        if (b == &presetButtons[i])
        {
            isSettingPreset = true;
            setActivePreset(i);           // обновляем UI + saveSettings
            if (!isSettingPreset && vstHost)
                vstHost->setExternalPresetIndex(i);
            isSettingPreset = false;

            // здесь отсыпаем «событие» наружу
            if (onActivePresetChanged)
                onActivePresetChanged(i);

            return;
        }
    // SET CC 1…10
    for (int i = 0; i < numCCParams; ++i)
        if (b == &setCCButtons[i]) { editCCParameter(i);       return; }

    // TOGGLE CC 1…10
    for (int i = 0; i < numCCParams; ++i)
    {
        if (b == &ccToggleButtons[i])
        {
            bool state = ccToggleButtons[i].getToggleState();

            // 1) обновляем модель (используем индивидуальные настройки для активного пресета)
            banks[activeBankIndex].presetCCMappings[activePreset][i].enabled = state;

            // Если матрица ccPresetStates по-прежнему нужна для синхронизации UI:
            banks[activeBankIndex].ccPresetStates[activePreset][i] = state;

            // 2) гоняем CC → MIDI + плагин
            updateCCParameter(i, state);

            // 3) сохраняем
            saveSettings();
            return;
        }
    }
    // LEARN 1–10 через хост
    for (int i = 0; i < numCCParams; ++i)
        if (b == &learnButtons[i])
        {
            if (isSettingLearn) return;

            isSettingLearn = true;
            bool newState = !(learn.isActive() && learn.slot() == i);
            vstHost->setExternalLearnState(i, newState);
            isSettingLearn = false;
            return;
        }
    // bottom row
    if (b == &defaultButton)
    {
        juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::WarningIcon,
            "Reset to Default",
            "This will reset the bank to default settings. All current changes will be lost. Continue?",
            "Yes", "Cancel",
            nullptr,
            juce::ModalCallbackFunction::create([this](int result)
                {
                    if (result == 1) // Yes
                    {
                        resetAllDefaults();
                    }
                })
        );
    }
    else if (b == &loadButton)
    {
        auto bankDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("Banks");

        auto* fm = new FileManager(bankDir, FileManager::Mode::Load);
        fm->setMinimalUI(true);

        fm->setWildcardFilter("*.xml");

        fm->setConfirmCallback([this](const juce::File& file)
            {
                if (!file.existsAsFile())
                    return;

                loadSettingsFromFile(file);
                updateUI();
                applyBankToPlugin(activeBankIndex, true);
            });

        juce::DialogWindow::LaunchOptions opts;
        opts.content.setOwned(fm);
        opts.dialogTitle = "Load Bank File";
        opts.dialogBackgroundColour = juce::Colours::darkgrey;
        opts.escapeKeyTriggersCloseButton = true;
        opts.useNativeTitleBar = false;
        opts.resizable = false; // ← запрет на изменение размера

        auto* dialog = opts.launchAsync();
        if (dialog != nullptr)
        {
            fm->setDialogWindow(dialog);
            dialog->centreWithSize(800, 600); // ← задаём фиксированный размер
        }

    }

    else if (b == &storeButton)
    {
        if (banks[activeBankIndex] != bankSnapshot)
        {
            juce::AlertWindow::showOkCancelBox(
                juce::AlertWindow::WarningIcon,
                "Save Bank",
                "Do you want to overwrite this bank? All current changes will be saved.",
                "Yes", "Cancel",
                nullptr,
                juce::ModalCallbackFunction::create([this](int result)
                    {
                        if (result == 1) // Yes
                        {
                            allowSave = true;
                            storeToBank();
                            allowSave = false;
                            banks[activeBankIndex].bankName = banks[activeBankIndex].bankName.trim();
                            bankSnapshot = banks[activeBankIndex];
                        }
                    })
            );
        }
        else
        {
            allowSave = true;
            storeToBank();
            allowSave = false;
            banks[activeBankIndex].bankName = banks[activeBankIndex].bankName.trim();
            bankSnapshot = banks[activeBankIndex];
        }
    }
    else if (b == &saveButton)
    {
        auto saveDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("Banks");

        auto* fm = new FileManager(saveDir, FileManager::Mode::Save);
        fm->setMinimalUI(true); // отключаем лишние кнопки
        fm->setWildcardFilter("*.xml");

        fm->setConfirmCallback([this](const juce::File& file)
            {
                if (file.getFullPathName().isEmpty())
                    return;

                snapshotCurrentBank();
                allowSave = true;
                storeToBank();
                allowSave = false;

                saveSettingsToFile(file);
            });

        juce::DialogWindow::LaunchOptions opts;
        opts.content.setOwned(fm);
        opts.dialogTitle = "Save Bank File";
        opts.dialogBackgroundColour = juce::Colours::darkgrey;
        opts.escapeKeyTriggersCloseButton = true;
        opts.useNativeTitleBar = false;
        opts.resizable = false; // ← запрещаем изменение размера

        auto* dialog = opts.launchAsync();
        if (dialog != nullptr)
        {
            fm->setDialogWindow(dialog);
            dialog->centreWithSize(800, 600); // ← вот здесь задаём размер!
        }

    }


    else if (b == &cancelButton)
    {
        if (banks[activeBankIndex] != bankSnapshot)
        {
            juce::AlertWindow::showOkCancelBox(
                juce::AlertWindow::WarningIcon,
                "Cancel Changes",
                "All unsaved changes will be lost. Do you want to continue?",
                "Yes", "Cancel",
                nullptr,
                juce::ModalCallbackFunction::create([this](int result)
                    {
                        if (result == 1) // Yes
                        {
                            banks[activeBankIndex] = bankSnapshot;
                            applyBankToPlugin(activeBankIndex, true); // синхронно
                            updateUI();
                        }
                    })
            );
        }
        else
        {
            banks[activeBankIndex] = bankSnapshot;
            applyBankToPlugin(activeBankIndex, true); // синхронно
            updateUI();
        }
    }
}
// Кастомный LookAndFeel для всплывающего меню.ВЫБОРА БАНКОВ
class CustomPopupMenuLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Значение, которое можно задавать извне – минимальная ширина для пунктов меню.
    int minimumPopupWidth = 0;
    // Используем увеличенный шрифт для пунктов меню.
    juce::Font getPopupMenuFont() override
    {
        return juce::Font(22.0f);
    }
    // Переопределение отрисовки пункта меню с учетом минимальной ширины.
    void drawPopupMenuItem(juce::Graphics& g,
        const juce::Rectangle<int>& area,
        bool isSeparator,
        bool isActive,
        bool isHighlighted,
        bool isTicked,
        bool hasSubMenu,
        const juce::String& text,
        const juce::String& shortcutKeyText,
        const juce::Drawable* icon,
        const juce::Colour* textColour) override
    {
        juce::Rectangle<int> r(area);
        if (minimumPopupWidth > r.getWidth())
            r.setWidth(minimumPopupWidth);

        // Вызываем базовую реализацию с изменённой областью.
        LookAndFeel_V4::drawPopupMenuItem(g, r, isSeparator, isActive, isHighlighted,
            isTicked, hasSubMenu, text, shortcutKeyText, icon, textColour);
    }
};
//==============================================================================
void BankEditor::showBankSelectionMenu()
{
    juce::PopupMenu menu;
    // Заполняем меню пунктами: ID = i+1, текст = имя банка.
    for (int i = 0; i < numBanks; ++i)
        menu.addItem(i + 1, banks[i].bankName, true, (i == activeBankIndex));
    // Используем статический экземпляр нашего кастомного LookAndFeel.
    static CustomPopupMenuLookAndFeel customPopupLAF;
    customPopupLAF.minimumPopupWidth = selectBankButton.getWidth();
    // Назначаем кастомное оформление для меню.
    menu.setLookAndFeel(&customPopupLAF);
    // Отображаем меню под кнопкой.
    menu.showMenuAsync(juce::PopupMenu::Options()
        .withTargetComponent(&selectBankButton)
        .withPreferredPopupDirection(juce::PopupMenu::Options::PopupDirection::downwards),
        [this](int result)
        {
            if (result > 0)
                setActiveBankIndex(result - 1);
        });
}
void BankEditor::propagateInvert(int slot, bool newInvert)
{
    auto& bank = banks[activeBankIndex];
    bank.globalCCMappings[slot].invert = newInvert;   // чтобы редакторы имени видели
    for (int p = 0; p < numPresets; ++p)
        bank.presetCCMappings[p][slot].invert = newInvert;
}
void BankEditor::editCCParameter(int ccIndex)
{
    const CCMapping initialMap = combineMapping(
        banks[activeBankIndex].globalCCMappings[ccIndex],
        banks[activeBankIndex].presetCCMappings[activePreset][ccIndex]);
    juce::String slotName = "Set CC " + juce::String(ccIndex + 1);

    new SetCCDialog(vstHost, initialMap, slotName,
        [this, ccIndex](CCMapping newMap, bool ok)
        {
            if (!ok) return;
            // --- 1. Глобальный слой ------------------------------------------
            auto& global = banks[activeBankIndex].globalCCMappings[ccIndex];
            const bool paramChanged = (global.paramIndex != newMap.paramIndex);
            global.paramIndex = newMap.paramIndex;
            // имя сбрасываем ТОЛЬКО при смене параметра
            if (paramChanged)
                if (newMap.paramIndex >= 0 && vstHost != nullptr)
                {
                    if (auto* inst = vstHost->getPluginInstance())
                        global.name = safeGetParamName(inst, newMap.paramIndex, 64);
                }
                else
                {
                    global.name = "<none>";
                }
            // --- 2. Пресетный слой -------------------------------------------
            auto& preset = banks[activeBankIndex]
                .presetCCMappings[activePreset][ccIndex];
            preset.ccValue = newMap.ccValue;
            propagateInvert(ccIndex, newMap.invert);
            preset.enabled = newMap.enabled;
            // --- 3. GUI ------------------------------------------------------
            ccNameEditors[ccIndex].setText(global.name,
                juce::dontSendNotification);
            ccToggleButtons[ccIndex].setToggleState(preset.enabled,
                juce::dontSendNotification);
            updateCCParameter(
                ccIndex,
                banks[activeBankIndex]
                .presetCCMappings[activePreset][ccIndex].enabled);
        });
}
//==============================================================================
void BankEditor::setActiveBankIndex(int newIdx)
{
    if (newIdx == activeBankIndex) return;

    snapshotCurrentBank(); // сохраняем старый банк

    activeBankIndex = juce::jlimit(0, numBanks - 1, newIdx);

    isSwitchingBank = true; // блокируем проверку dirty

    // Обычное переключение — асинхронно
    applyBankToPlugin(activeBankIndex, false);
    updateUI();
    setActivePreset(0);
    // Уведомляем подписчиков (Rig_control) о смене банка
    if (onBankChanged)
        onBankChanged();

    // Через 200 мс фиксируем эталон и снимаем блокировку
    juce::Timer::callAfterDelay(200, [this] {
        bankSnapshot = banks[activeBankIndex];
        isSwitchingBank = false;
        });
}

void BankEditor::setActiveBank(int newBank)
{
    activeBankIndex = juce::jlimit(0, numBanks - 1, newBank);
    updateUI();
}
void BankEditor::setActivePreset(int newPreset)
{
    if (newPreset < 0 || newPreset >= numPresets)
        return;

    activePreset = newPreset;
    updateSelectedPresetLabel();
    updatePresetButtons();

    for (int i = 0; i < 10; ++i)//педадб отключена от присетов
    {
        bool state = banks[activeBankIndex]
            .presetCCMappings[activePreset][i].enabled;
        ccToggleButtons[i].setToggleState(state, juce::dontSendNotification);
        updateCCParameter(i, state);
    }

    // оповещение старым JUCE-механизмом
    sendChange();

    // вот это — наш callback, на который подписан Rig_control
    if (onActivePresetChanged)
        onActivePresetChanged(activePreset);

    saveSettings();
}

void BankEditor::resetCCSlotState(int slot)
{
    auto& bank = banks[activeBankIndex];
    bank.globalCCMappings[slot].paramIndex = -1;
    bank.globalCCMappings[slot].name = "<none>";
    bank.globalCCMappings[slot].invert = false;
    for (int p = 0; p < numPresets; ++p)
    {
        auto& m = bank.presetCCMappings[p][slot];
        m.enabled = false;
        m.ccValue = 64;
        m.invert = false;
        bank.ccPresetStates[p][slot] = false;
    }
    ccNameEditors[slot].setText("<none>", juce::dontSendNotification);
    ccToggleButtons[slot].setToggleState(false, juce::dontSendNotification);
}
void BankEditor::clearCCMappingsForActiveBank()
{
    for (int s = 0; s < numCCParams; ++s)
        resetCCSlotState(s);
}
void BankEditor::updateUI()
{
    // Обновляем номер банка и имя банка:
    bankIndexLabel.setText(juce::String(activeBankIndex + 1), juce::dontSendNotification);
    bankNameEditor.setText(banks[activeBankIndex].bankName, juce::dontSendNotification);
    if (onBankChanged)
        onBankChanged();
    // Обновляем текст для каждого редактора имен пресетов:
    for (int i = 0; i < numPresets; ++i)
        presetEditors[i].setText(banks[activeBankIndex].presetNames[i], juce::dontSendNotification);
    // Обновляем метку плагина. Если имя плагина в банке пустое, метка покажет пустое значение.
    pluginLabel.setText(getCurrentPluginDisplayName(), juce::dontSendNotification);

    // Обновляем метку выбранного пресета (например, для выделения активного пресета):
    updateSelectedPresetLabel();
    // Обновляем настройки CC для активного пресета:
    for (int i = 0; i < numCCParams; ++i)
    {
        ccNameEditors[i].setText(banks[activeBankIndex].globalCCMappings[i].name, juce::dontSendNotification);
        ccToggleButtons[i].setToggleState(banks[activeBankIndex].presetCCMappings[activePreset][i].enabled, juce::dontSendNotification);
    }
    for (int i = 0; i < numCCParams; ++i)
    {
        bool hasParam = banks[activeBankIndex]
            .globalCCMappings[i].paramIndex >= 0;
        learnButtons[i].setColour(juce::TextButton::buttonColourId,
            hasParam ? juce::Colour(0, 80, 0)  // тёмно-зелёный
            : juce::Colour(30, 30, 30)); // «пустой» слот
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
    if (isSwitchingBank || isLoadingFromFile)
        return; // не проверяем во время переключения или загрузки

    bool modified = (banks[activeBankIndex] != bankSnapshot);

    if (modified) {
        bool blink = (juce::Time::getMillisecondCounter() / 500) % 2;
        storeButton.setColour(juce::TextButton::buttonColourId,
            blink ? juce::Colours::red : juce::Colours::darkred);
    }
    else {
        storeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
    }
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
    if (!allowSave)
    {
        DBG("saveSettings() вызван, но сохранение запрещено (не Store)");
        return;
    }
   
    saveSettingsToFile(getDefaultConfigFile());
}

void BankEditor::loadSettingsFromFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return;

    isLoadingFromFile = true; // блокируем реакции и проверку dirty

    std::unique_ptr<juce::XmlElement> xml = juce::XmlDocument::parse(file);
    if (!xml || !xml->hasTagName("BanksConfig"))
    {
        isLoadingFromFile = false; // сброс флага при ошибке
        return;
    }

    // Читаем индекс активного банка и пресета
    activeBankIndex = xml->getIntAttribute("activeBankIndex", 0);
    activePreset = xml->getIntAttribute("activePreset", 0);

    // --- Глобальные данные о плагине ---
    globalPluginName = xml->getStringAttribute("pluginName");
    globalPluginId = xml->getStringAttribute("pluginId");
    globalActiveProgram = xml->getIntAttribute("activeProgram", -1);

    // Глобальные параметры плагина
    globalPluginParamValues.clear();
    if (auto* paramsEl = xml->getChildByName("PluginParams"))
    {
        forEachXmlChildElementWithTagName(*paramsEl, pe, "Param")
            globalPluginParamValues.push_back((float)pe->getDoubleAttribute("value", 0.0));
    }

    // Глобальный state плагина
    globalPluginState.reset();
    if (auto* stateEl = xml->getChildByName("PluginState"))
        globalPluginState.fromBase64Encoding(stateEl->getAllSubText().trim());

    // --- Загружаем банки ---
    banks.assign(numBanks, Bank{});

    forEachXmlChildElementWithTagName(*xml, bankEl, "Bank")
    {
        int idx = bankEl->getIntAttribute("index", -1);
        if (idx < 0 || idx >= numBanks) continue;

        auto& b = banks[idx];
        b.bankName = bankEl->getStringAttribute("bankName");
        b.pluginName = bankEl->getStringAttribute("pluginName");
        b.pluginId = bankEl->getStringAttribute("pluginId");
        b.activeProgram = bankEl->getIntAttribute("activeProgram", -1);

        // Параметры плагина для банка
        b.pluginParamValues.clear();
        if (auto* paramsEl = bankEl->getChildByName("PluginParams"))
        {
            forEachXmlChildElementWithTagName(*paramsEl, pe, "Param")
                b.pluginParamValues.push_back((float)pe->getDoubleAttribute("value", 0.0));
        }

        // Полный state плагина для банка
        b.pluginState.reset();
        if (auto* stateEl = bankEl->getChildByName("PluginState"))
            b.pluginState.fromBase64Encoding(stateEl->getAllSubText().trim());

        // Diff’ы параметров
        b.paramDiffs.clear();
        if (auto* diffsEl = bankEl->getChildByName("ParamDiffs"))
        {
            forEachXmlChildElementWithTagName(*diffsEl, de, "Diff")
            {
                int pIdx = de->getIntAttribute("index", -1);
                if (pIdx >= 0)
                    b.paramDiffs[pIdx] = (float)de->getDoubleAttribute("value", 0.0);
            }
        }

        // Preset names
        if (auto* presetsEl = bankEl->getChildByName("PresetNames"))
        {
            forEachXmlChildElementWithTagName(*presetsEl, pe, "Preset")
            {
                int pIdx = pe->getIntAttribute("index", -1);
                if (pIdx >= 0 && pIdx < numPresets)
                    b.presetNames[pIdx] = pe->getStringAttribute("name");
            }
        }

        // CC состояния и назначения
        if (auto* ccStatesEl = bankEl->getChildByName("CCPresetStates"))
        {
            forEachXmlChildElementWithTagName(*ccStatesEl, presetEl, "Preset")
            {
                int pIdx = presetEl->getIntAttribute("index", -1);
                if (pIdx >= 0 && pIdx < numPresets)
                {
                    forEachXmlChildElementWithTagName(*presetEl, ccEl, "CC")
                    {
                        int cc = ccEl->getIntAttribute("number", -1);
                        if (cc >= 0 && cc < numCCParams)
                        {
                            auto& presetMap = b.presetCCMappings[pIdx][cc];
                            presetMap.enabled = ccEl->getBoolAttribute("enabled", false);
                            presetMap.ccValue = (uint8_t)ccEl->getIntAttribute("ccValue", 64);
                            presetMap.invert = ccEl->getBoolAttribute("invert", false);

                            auto& globalMap = b.globalCCMappings[cc];
                            globalMap.paramIndex = ccEl->getIntAttribute("paramIndex", -1);
                            globalMap.name = ccEl->getStringAttribute("paramName");
                        }
                    }
                }
            }
        }
    }
    // Сохраняем загруженный файл как дефолтный
    saveSettingsToFile(getDefaultConfigFile());

    // Даём хосту время восстановить своё состояние,
    // потом перебиваем его нашим активным банком
    juce::Timer::callAfterDelay(200, [this] {
        // Применяем активный банк синхронно
        applyBankToPlugin(activeBankIndex, true); // <-- synchronous = true

        // Фиксируем baseline уже после применения state
        bankSnapshot = banks[activeBankIndex];
        isLoadingFromFile = false;

        // Обновляем UI — теперь плагин уже в нужном состоянии
        updateUI();

        });

}

void BankEditor::saveSettingsToFile(const juce::File& file)
{
    DBG("Save: activeBankIndex = " << activeBankIndex);
    juce::XmlElement root("BanksConfig");
    root.setAttribute("version", 1);
    root.setAttribute("activeBankIndex", activeBankIndex);
    root.setAttribute("activePreset", activePreset);

    // --- Глобальные данные о плагине ---
    root.setAttribute("pluginName", globalPluginName);
    root.setAttribute("pluginId", globalPluginId);
    root.setAttribute("activeProgram", globalActiveProgram);

    // Параметры плагина (глобальные)
    {
        auto* paramsEl = new juce::XmlElement("PluginParams");
        for (float v : globalPluginParamValues)
        {
            auto* pe = new juce::XmlElement("Param");
            pe->setAttribute("value", v);
            paramsEl->addChildElement(pe);
        }
        root.addChildElement(paramsEl);
    }

    // Полный state плагина (Base64)
    if (globalPluginState.getSize() > 0)
    {
        auto* stateEl = new juce::XmlElement("PluginState");
        stateEl->addTextElement(globalPluginState.toBase64Encoding());
        root.addChildElement(stateEl);
    }

    // --- Данные банков ---
    for (int i = 0; i < static_cast<int>(banks.size()); ++i)
    {
        const auto& b = banks[i];
        auto* bankEl = new juce::XmlElement("Bank");
        bankEl->setAttribute("index", i);
        bankEl->setAttribute("bankName", b.bankName);
        bankEl->setAttribute("activeProgram", b.activeProgram);
        bankEl->setAttribute("pluginName", b.pluginName);
        bankEl->setAttribute("pluginId", b.pluginId);

        // Параметры плагина для банка
        {
            auto* paramsEl = new juce::XmlElement("PluginParams");
            for (float v : b.pluginParamValues)
            {
                auto* pe = new juce::XmlElement("Param");
                pe->setAttribute("value", v);
                paramsEl->addChildElement(pe);
            }
            bankEl->addChildElement(paramsEl);
        }

        // Полный state плагина для банка
        if (b.pluginState.getSize() > 0)
        {
            auto* stateEl = new juce::XmlElement("PluginState");
            stateEl->addTextElement(b.pluginState.toBase64Encoding());
            bankEl->addChildElement(stateEl);
        }

        // Отличающиеся параметры (paramDiffs)
        if (!b.paramDiffs.empty())
        {
            auto* diffsEl = new juce::XmlElement("ParamDiffs");
            for (auto& [paramIndex, value] : b.paramDiffs)
            {
                auto* diffEl = new juce::XmlElement("Diff");
                diffEl->setAttribute("index", paramIndex);
                diffEl->setAttribute("value", value);
                diffsEl->addChildElement(diffEl);
            }
            bankEl->addChildElement(diffsEl);
        }

        // Preset names
        auto* presetsEl = new juce::XmlElement("PresetNames");
        for (int p = 0; p < numPresets; ++p)
        {
            auto* pe = new juce::XmlElement("Preset");
            pe->setAttribute("index", p);
            pe->setAttribute("name", b.presetNames[p]);
            presetsEl->addChildElement(pe);
        }
        bankEl->addChildElement(presetsEl);

        // CC состояния и назначения
        auto* ccStatesEl = new juce::XmlElement("CCPresetStates");
        for (int p = 0; p < numPresets; ++p)
        {
            auto* presetEl = new juce::XmlElement("Preset");
            presetEl->setAttribute("index", p);

            for (int cc = 0; cc < numCCParams; ++cc)
            {
                const auto& presetMap = b.presetCCMappings[p][cc];
                const auto& globalMap = b.globalCCMappings[cc];

                auto* ccEl = new juce::XmlElement("CC");
                ccEl->setAttribute("number", cc);
                ccEl->setAttribute("ccValue", (int)presetMap.ccValue);
                ccEl->setAttribute("invert", presetMap.invert);
                ccEl->setAttribute("enabled", presetMap.enabled); // теперь сохраняем флаг

                ccEl->setAttribute("paramIndex", globalMap.paramIndex);
                ccEl->setAttribute("paramName", globalMap.name);

                presetEl->addChildElement(ccEl);
            }

            ccStatesEl->addChildElement(presetEl);
        }
        bankEl->addChildElement(ccStatesEl);


        root.addChildElement(bankEl);
    }
    bankSnapshot = banks[activeBankIndex];
    file.replaceWithText(root.toString());
}
void BankEditor::resetAllDefaults()
{
    for (int i = 0; i < numBanks; ++i)
    {
        banks[i] = Bank();
        banks[i].bankName = "BANK" + juce::String(i + 1);
    }

    if (vstHost) // защита
        vstHost->unloadPlugin();

    setActiveBankIndex(0);
    setActivePreset(0);
    updateUI();

    // 1. Фиксируем эталон, чтобы Store не мигала
    bankSnapshot = banks[activeBankIndex];

    // 2. Сохраняем дефолт сразу в конфиг
    saveSettingsToFile(getDefaultConfigFile());
}

void BankEditor::storeToBank()
{
    if (activeBankIndex < 0 || activeBankIndex >= (int)banks.size())
        return;

    auto& b = banks[activeBankIndex];

    // Имена из UI
    b.bankName = bankNameEditor.getText();
    for (int i = 0; i < numPresets; ++i)
        b.presetNames[i] = presetEditors[i].getText();

    if (vstHost != nullptr)
    {
        if (auto* inst = vstHost->getPluginInstance())
        {
            juce::PluginDescription desc;
            inst->fillInPluginDescription(desc);
            b.pluginName = desc.name;
            b.pluginId = desc.fileOrIdentifier;
            b.activeProgram = inst->getCurrentProgram();

            // Сохраняем полный state ТОЛЬКО при Store
            b.pluginState.reset();
            inst->getStateInformation(b.pluginState);

            // Обновляем baseline параметров
            const auto& params = inst->getParameters();
            const int N = (int)params.size();
            if ((int)b.pluginParamValues.size() != N)
                b.pluginParamValues.assign(N, 0.0f);

            static constexpr float eps = 1.0e-4f;
            std::unordered_map<int, float> newDiffs;

            for (int i = 0; i < N; ++i)
            {
                const float v = params[i]->getValue();
                const float oldBaseline = b.pluginParamValues[i];

                if (std::abs(v - oldBaseline) > eps)
                    newDiffs[i] = v;

                b.pluginParamValues[i] = v;
            }
            b.paramDiffs = std::move(newDiffs);
        }
    }

    // Сохраняем в файл
    const auto cfgFile = getDefaultConfigFile();
    std::unique_ptr<juce::XmlElement> root;
    if (cfgFile.existsAsFile())
        root = juce::XmlDocument::parse(cfgFile);

    if (!root || !root->hasTagName("BanksConfig"))
    {
        root.reset(new juce::XmlElement("BanksConfig"));
        root->setAttribute("version", 1);
    }

    // Удаляем старый банк
    forEachXmlChildElementWithTagName(*root, oldBank, "Bank")
    {
        if (oldBank->getIntAttribute("index", -1) == activeBankIndex)
        {
            root->removeChildElement(oldBank, true);
            break;
        }
    }

    // Добавляем новый
    root->addChildElement(serializeBank(b, activeBankIndex));

    // Обновляем активные индексы
    root->setAttribute("activeBankIndex", activeBankIndex);
    root->setAttribute("activePreset", activePreset);

    cfgFile.replaceWithText(root->toString());

    DBG("STORE -> file updated: bank=" << (activeBankIndex + 1)
        << " name=" << b.bankName
        << " plugin=" << b.pluginName);
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

}
void BankEditor::saveToDisk()
{
    // аналогично — на saveSettings()
    saveSettings();
  
}
void BankEditor::showVSTDialog()
{
    // Закрываем любые активные PopupMenu
    juce::PopupMenu::dismissAllActiveMenus();
    if (vstHost == nullptr)
        return;

    // A: плагин ещё не загружен — строим меню из PluginManager
    if (vstHost->getPluginInstance() == nullptr)
    {
        auto entries = vstHost->getPluginManager().getPluginsSnapshot();

        juce::PopupMenu menu;
        std::vector<int> indexMap;
        int itemId = 1;

        for (int i = 0; i < (int)entries.size(); ++i)
        {
            if (!entries[i].enabled)
                continue;

            menu.addItem(itemId, entries[i].desc.name);
            indexMap.push_back(i);
            ++itemId;
        }

        if (indexMap.empty())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "No Plugins Enabled",
                "Please enable the plugin(s) in the Plugin Manager.");
            return;
        }

        menu.showMenuAsync(
            juce::PopupMenu::Options().withTargetComponent(&vstButton),
            [this, entries, indexMap](int choice)
            {
                if (choice < 1 || choice >(int)indexMap.size())
                    return;

                int realIdx = indexMap[choice - 1];
                auto& desc = entries[realIdx].desc;

                auto& adm = vstHost->getAudioDeviceManagerRef();
                auto* dev = adm.getCurrentAudioDevice();
                double sr = dev ? dev->getCurrentSampleRate() : 44100.0;
                int    bs = dev ? dev->getCurrentBufferSizeSamples() : 512;

                vstHost->loadPlugin(juce::File(desc.fileOrIdentifier), sr, bs);

                auto* inst = vstHost->getPluginInstance();
                int   nParam = inst ? (int)inst->getParameters().size() : 0;
                for (int s = 0; s < numCCParams; ++s)
                {
                    auto& map = banks[activeBankIndex].globalCCMappings[s];
                    if (map.paramIndex < 0 || map.paramIndex >= nParam)
                    {
                        map.paramIndex = -1;
                        map.name = "<none>";
                    }
                }

                banks[activeBankIndex].pluginName = desc.name;
                pluginLabel.setText(desc.name, juce::dontSendNotification);
                vstButton.setButtonText("Close Plugin");
            });

        return;
    }

    // B: плагин уже загружен — спрашиваем, закрыть ли его
    juce::AlertWindow::showOkCancelBox(
        juce::AlertWindow::WarningIcon,
        "Close Plugin",
        "Are you sure you want to close the current plugin?",
        "Yes", "Cancel",
        nullptr,
        juce::ModalCallbackFunction::create([this](int result)
            {
                if (result == 1)  // пользователь нажал Yes
                {
                    // Глобальная очистка всех банков и глобальных данных
                    unloadPluginEverywhere();

                    vstButton.setButtonText("Load Plugin");
                }
            }));
}

//===== PUBLIC API IMPLEMENTATION =============================================
void BankEditor::setActivePresetIndex(int newPresetIndex)
{
    if (newPresetIndex >= 0 && newPresetIndex < numPresets)
    {
        activePreset = newPresetIndex;
        updateSelectedPresetLabel();
        updatePresetButtons();
        repaint();                     
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
std::vector<CCMapping> BankEditor::getCCMappings(int bankIndex, int presetIndex) const
{
    std::vector<CCMapping> mappings;
    if (bankIndex >= 0 && bankIndex < static_cast<int>(banks.size()) &&
        presetIndex >= 0 && presetIndex < numPresets)
    {
        const auto& bank = banks[bankIndex];
        for (int cc = 0; cc < numCCParams; ++cc)
        {
            CCMapping combined;
            // Берем глобальные данные, назначенные для данной CC-кнопки:
            const auto& global = bank.globalCCMappings[cc];
            // Берем пресетные данные (уровень, состояние включения) из текущего пресета:
            const auto& preset = bank.presetCCMappings[presetIndex][cc];
            combined.paramIndex = global.paramIndex;
            combined.invert = global.invert;
            combined.name = global.name;
            // Поле enabled теперь берём из пресетной структуры:
            combined.enabled = preset.enabled;
            // ccValue берется из пресетных настроек:
            combined.ccValue = preset.ccValue;
            mappings.push_back(combined);
        }
    }
    return mappings;
}
void BankEditor::updateCCParameter(int index, bool state)
{
    if (activeBankIndex < 0 || activeBankIndex >= (int)banks.size())
        return;

    auto& bank = banks[activeBankIndex];
    if (activePreset < 0 || activePreset >= (int)bank.ccPresetStates.size())
        return;

    if (index < 0 || index >= numCCParams)
        return;

    auto& globalMapping = bank.globalCCMappings[index];
    auto& presetMapping = bank.presetCCMappings[activePreset][index];

    // 1. Сохраняем новое состояние в модель
    bank.ccPresetStates[activePreset][index] = state;
    presetMapping.enabled = state;

    // 2. Вычисляем значение для отправки
    uint8_t effective = 0;
    if (!presetMapping.invert)
    {
        // обычный режим: ON → ccValue, OFF → 0
        effective = state ? presetMapping.ccValue : 0;
    }
    else
    {
        // инвертированный: ON → 0, OFF → ccValue
        effective = state ? 0 : presetMapping.ccValue;
    }

    // 3. Отправляем MIDI (если нужно)
    if (midiOutput != nullptr)
    {
        auto msg = juce::MidiMessage::controllerEvent(1, index + 1, effective);
        midiOutput->sendMessageNow(msg);
    }

    // 4. Обновляем плагин (если нужно)
    if (vstHost != nullptr && globalMapping.paramIndex >= 0)
        vstHost->setPluginParameter(globalMapping.paramIndex, effective);

    // 5. Уведомляем UI
    sendChange();
}

// -------------------------------------------------------------------
void BankEditor::onPluginParameterChanged(int paramIdx, float normalised)
{
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      1.  РЕЖИМ LEARN
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    juce::String paramName;

    if (auto* inst = (vstHost != nullptr ? vstHost->getPluginInstance() : nullptr))
        paramName = safeGetParamName(inst, paramIdx, 128); // без deprecated-API

    if (paramName.isEmpty())
        paramName = "<unnamed>";          // запасное имя, если строка пуста

    learn.parameterTouched(paramIdx, paramName);

    if (learn.isActive())                 // событие поглощено Learn-режимом
        return;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      2.  ПРОВЕРКИ И ПРЕОБРАЗОВАНИЯ (АУДИО-ТРЕД, БЕЗ GUI)
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    if (vstHost == nullptr || vstHost->getPluginInstance() == nullptr
        || isSettingPreset)
        return;

    const int v127 = juce::jlimit(0, 127,
        int(std::lround(normalised * 127.0f)));

    // ищем CC-слот
    int slot = -1;
    for (int i = 0; i < numCCParams; ++i)
        if (banks[activeBankIndex].globalCCMappings[i].paramIndex == paramIdx)
        {
            slot = i; break;
        }

    if (slot < 0) return;

    auto& preset = banks[activeBankIndex]
        .presetCCMappings[activePreset][slot];

    const bool invert = preset.invert;
    const bool shouldBeEnabled = invert ? (v127 == 0) : (v127 > 0);
    const int  newCCValue = invert ? (127 - v127) : v127;

    // записываем результаты в модель (это ПОКА что не GUI)
    preset.enabled = shouldBeEnabled;
    if (shouldBeEnabled)
        preset.ccValue = newCCValue;

    banks[activeBankIndex].ccPresetStates[activePreset][slot] = shouldBeEnabled;

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      3.  ПЕРЕДАЁМ ОБНОВЛЕНИЕ GUI В MessageThread
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    juce::MessageManager::callAsync([this, slot, shouldBeEnabled]
        {
            ccToggleButtons[slot].setToggleState(shouldBeEnabled,
                juce::dontSendNotification);
            updatePresetButtons();
            repaint();
        });
}
//────────────────────────────  Learn callbacks  ────────────────────────────
void BankEditor::learnStarted(int slot)
{
    auto& btn = learnButtons[slot];
    oldLearnColour = btn.findColour(juce::TextButton::buttonColourId);
    btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0, 200, 0));

    selectedPresetLabel.setText("Move any control on the plugin",
        juce::dontSendNotification);

    if (onLearnToggled) onLearnToggled(slot, true);
}

void BankEditor::learnCancelled(int slot)
{
    learnButtons[slot].setColour(juce::TextButton::buttonColourId, oldLearnColour);
    updateSelectedPresetLabel();

    if (onLearnToggled) onLearnToggled(slot, false);
}

void BankEditor::learnFinished(int slot, int paramIdx, const juce::String& pname)
{
    learnCancelled(slot); // вернуть кнопку

    if (vstHost)
        vstHost->setExternalLearnState(slot, false);

    if (slot >= 0 && slot < numCCParams)
    {
        auto& map = banks[activeBankIndex].globalCCMappings[slot];
        map.paramIndex = paramIdx;
        map.name = pname.isEmpty() ? "<none>" : pname;

        // обновляем подпись в UI (если есть редактор имени для этого слота)
        if (slot < (int)std::size(ccNameEditors))
            ccNameEditors[slot].setText(map.name, juce::dontSendNotification);
    }

    saveSettings();
}


void BankEditor::toggleLearnFromHost(int cc, bool on)
{
    if (cc < 0 || cc >= numCCParams) return;
    if (on)  learn.begin(cc);
    else     learn.cancel();
}
void BankEditor::toggleCC(int ccIndex, bool state)
{
    if (activeBankIndex < 0 || activeBankIndex >= (int)banks.size()) return;
    auto& bank = banks[activeBankIndex];
    if (activePreset < 0 || activePreset >= (int)bank.ccPresetStates.size()) return;
    if (ccIndex < 0 || ccIndex >= numCCParams) return;

    // 1. Обновляем модель
    bank.presetCCMappings[activePreset][ccIndex].enabled = state;
    bank.ccPresetStates[activePreset][ccIndex] = state;

    // 2. Применяем к железу/плагину
    updateCCParameter(ccIndex, state);

    // 3. Сохраняем и уведомляем UI
    saveSettings();
    sendChange();
}
juce::File BankEditor::getDefaultConfigFile() const
{
    // Путь к дефолтному файлу (можно поменять папку на свою)
    auto baseDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("NEXUS_KONTROL_OS");
    return baseDir.getChildFile(kDefaultConfigName);
}

void BankEditor::ensureDefaultConfigExists()
{
    // Гарантируем, что массив банков инициализирован
    if (banks.size() != (size_t)numBanks)
        banks.assign(numBanks, Bank{});

    auto cfg = getDefaultConfigFile();
    auto dir = cfg.getParentDirectory();
    if (!dir.exists())
        dir.createDirectory();

    if (cfg.existsAsFile())
    {
        DBG("Default config exists, loading...");
        loadDefaultConfig();
    }
    else
    {
        DBG("Default config NOT found, creating new...");
        resetAllDefaults();
        writeDefaultConfig();
    }

}

void BankEditor::writeDefaultConfig()
{
    auto file = getDefaultConfigFile();
    DBG("Writing default config to: " << file.getFullPathName());
    saveSettingsToFile(file);
}


void BankEditor::loadDefaultConfig()
{
    // Загружаем banks[] из XML через уже существующую функцию
    loadSettingsFromFile(getDefaultConfigFile());
}
juce::XmlElement* BankEditor::serializeBank(const Bank& b, int index) const
{
    auto* bankEl = new juce::XmlElement("Bank");
    bankEl->setAttribute("index", index);
    bankEl->setAttribute("bankName", b.bankName);

    // Плагин: имя, путь, программа
    bankEl->setAttribute("pluginName", b.pluginName);
    bankEl->setAttribute("pluginId", b.pluginId);
    bankEl->setAttribute("activeProgram", b.activeProgram);

    // Preset names
    {
        auto* presetsEl = new juce::XmlElement("PresetNames");
        for (int p = 0; p < numPresets; ++p)
        {
            auto* pe = new juce::XmlElement("Preset");
            pe->setAttribute("index", p);
            pe->setAttribute("name", b.presetNames[p]);
            presetsEl->addChildElement(pe);
        }
        bankEl->addChildElement(presetsEl);
    }

    // CC состояния и назначения — теперь пишем ВСЕ
    {
        auto* ccStatesEl = new juce::XmlElement("CCPresetStates");
        for (int p = 0; p < numPresets; ++p)
        {
            auto* presetEl = new juce::XmlElement("Preset");
            presetEl->setAttribute("index", p);

            for (int cc = 0; cc < numCCParams; ++cc)
            {
                const auto& presetMap = b.presetCCMappings[p][cc];
                const auto& globalMap = b.globalCCMappings[cc];

                auto* ccEl = new juce::XmlElement("CC");
                ccEl->setAttribute("number", cc);
                ccEl->setAttribute("ccValue", (int)presetMap.ccValue);
                ccEl->setAttribute("invert", presetMap.invert);
                ccEl->setAttribute("enabled", presetMap.enabled); // новый атрибут

                ccEl->setAttribute("paramIndex", globalMap.paramIndex);
                ccEl->setAttribute("paramName", globalMap.name);

                presetEl->addChildElement(ccEl);
            }
            ccStatesEl->addChildElement(presetEl);
        }
        bankEl->addChildElement(ccStatesEl);
    }

    // Полный state плагина (Base64)
    if (b.pluginState.getSize() > 0)
    {
        auto* stateEl = new juce::XmlElement("PluginState");
        stateEl->addTextElement(b.pluginState.toBase64Encoding());
        bankEl->addChildElement(stateEl);
    }

    // Baseline параметров плагина
    {
        auto* paramsEl = new juce::XmlElement("PluginParams");
        for (float v : b.pluginParamValues)
        {
            auto* pe = new juce::XmlElement("Param");
            pe->setAttribute("value", v);
            paramsEl->addChildElement(pe);
        }
        bankEl->addChildElement(paramsEl);
    }

    // Diff’ы параметров
    if (!b.paramDiffs.empty())
    {
        auto* diffsEl = new juce::XmlElement("ParamDiffs");
        for (const auto& [idx, val] : b.paramDiffs)
        {
            auto* de = new juce::XmlElement("Diff");
            de->setAttribute("index", idx);
            de->setAttribute("value", val);
            diffsEl->addChildElement(de);
        }
        bankEl->addChildElement(diffsEl);
    }

    return bankEl;
}


void BankEditor::deserializeBank(Bank& b, const juce::XmlElement& bankEl)
{
    b.bankName = bankEl.getStringAttribute("bankName");
    b.pluginName = bankEl.getStringAttribute("pluginName");
    b.pluginId = bankEl.getStringAttribute("pluginId");
    b.activeProgram = bankEl.getIntAttribute("activeProgram", -1);

    // Preset names
    if (auto* presetsEl = bankEl.getChildByName("PresetNames"))
    {
        forEachXmlChildElementWithTagName(*presetsEl, pe, "Preset")
        {
            int pIdx = pe->getIntAttribute("index", -1);
            if (pIdx >= 0 && pIdx < numPresets)
                b.presetNames[pIdx] = pe->getStringAttribute("name");
        }
    }

    // CC состояния и назначения
    if (auto* ccStatesEl = bankEl.getChildByName("CCPresetStates"))
    {
        forEachXmlChildElementWithTagName(*ccStatesEl, presetEl, "Preset")
        {
            int pIdx = presetEl->getIntAttribute("index", -1);
            if (pIdx >= 0 && pIdx < numPresets)
            {
                forEachXmlChildElementWithTagName(*presetEl, ccEl, "CC")
                {
                    int cc = ccEl->getIntAttribute("number", -1);
                    if (cc >= 0 && cc < numCCParams)
                    {
                        auto& presetMap = b.presetCCMappings[pIdx][cc];
                        presetMap.enabled = ccEl->getBoolAttribute("enabled", false); // читаем из XML
                        presetMap.ccValue = (uint8_t)ccEl->getIntAttribute("ccValue", 64);
                        presetMap.invert = ccEl->getBoolAttribute("invert", false);

                        auto& globalMap = b.globalCCMappings[cc];
                        globalMap.paramIndex = ccEl->getIntAttribute("paramIndex", -1);
                        globalMap.name = ccEl->getStringAttribute("paramName");

                    }
                }
            }
        }
    }

    // Полный state плагина
    b.pluginState.reset();
    if (auto* stateEl = bankEl.getChildByName("PluginState"))
        b.pluginState.fromBase64Encoding(stateEl->getAllSubText().trim());

    // Baseline параметров
    b.pluginParamValues.clear();
    if (auto* paramsEl = bankEl.getChildByName("PluginParams"))
    {
        forEachXmlChildElementWithTagName(*paramsEl, pe, "Param")
            b.pluginParamValues.push_back((float)pe->getDoubleAttribute("value", 0.0));
    }

    // Diff’ы параметров
    b.paramDiffs.clear();
    if (auto* diffsEl = bankEl.getChildByName("ParamDiffs"))
    {
        forEachXmlChildElementWithTagName(*diffsEl, de, "Diff")
        {
            int idx = de->getIntAttribute("index", -1);
            if (idx >= 0)
                b.paramDiffs[idx] = (float)de->getDoubleAttribute("value", 0.0);
        }
    }
}
void BankEditor::applyBankToPlugin(int bankIndex, bool synchronous /* = false */)
{
    if (bankIndex < 0 || bankIndex >= (int)banks.size())
        return;
    if (!vstHost)
        return;

    const auto& b = banks[bankIndex];
    auto* currentInst = vstHost->getPluginInstance();
    bool needLoad = false;

    if (!currentInst)
        needLoad = b.pluginId.isNotEmpty();
    else if (b.pluginId.isNotEmpty() && b.pluginId != lastLoadedPluginId)
    {
        vstHost->unloadPlugin();
        needLoad = true;
    }

    if (needLoad)
    {
        juce::File pluginFile(b.pluginId);
        if (pluginFile.existsAsFile())
        {
            DBG("Loading plugin: " << pluginFile.getFullPathName());
            vstHost->loadPlugin(pluginFile);
            lastLoadedPluginId = b.pluginId;
        }
        else
        {
            DBG("Plugin file not found: " << pluginFile.getFullPathName());
            return;
        }
    }

    auto* instNow = vstHost->getPluginInstance();
    if (!instNow)
        return;

    // --- Если есть полный state
    if (b.pluginState.getSize() > 0)
    {
        auto stateCopy = b.pluginState;
        auto targetId = lastLoadedPluginId;
        bool justLoaded = needLoad;

        auto applyState = [this, targetId, stateCopy]()
            {
                if (!vstHost) return;
                if (targetId.isEmpty() || targetId != lastLoadedPluginId) return;

                auto* instCheck = vstHost->getPluginInstance();
                if (!instCheck) return;

                const int sz = (int)stateCopy.getSize();
                if (sz <= 0 || stateCopy.getData() == nullptr) return;

                bool wasOpen = vstHost->isPluginEditorOpen();
                if (wasOpen) vstHost->closePluginEditorIfOpen();

                DBG("Applying full plugin state (" << sz << " bytes)");
                try { instCheck->setStateInformation(stateCopy.getData(), sz); }
                catch (...) { DBG("Exception in setStateInformation — skipped"); }

                if (wasOpen)
                {
                    juce::Timer::callAfterDelay(80, [this, targetId]()
                        {
                            if (!vstHost) return;
                            if (targetId != lastLoadedPluginId) return;
                            if (vstHost->getPluginInstance())
                                vstHost->openPluginEditorIfNeeded();
                        });
                }
            };

        if (synchronous)
        {
            // Применяем state прямо сейчас
            applyState();
        }
        else if (justLoaded)
        {
            juce::MessageManager::callAsync([applyState]()
                {
                    juce::Timer::callAfterDelay(150, applyState);
                });
        }
        else
        {
            if (juce::MessageManager::getInstance()->isThisTheMessageThread())
                applyState();
            else
                juce::MessageManager::callAsync(applyState);
        }

        return;
    }

    // --- Если state пуст — baseline + diffs
    if (b.activeProgram >= 0)
        instNow->setCurrentProgram(b.activeProgram);

    auto params = instNow->getParameters();
    const int n = std::min((int)params.size(), (int)b.pluginParamValues.size());
    for (int i = 0; i < n; ++i)
        params[i]->setValueNotifyingHost(b.pluginParamValues[i]);

    for (const auto& kv : b.paramDiffs)
    {
        int idx = kv.first;
        float val = kv.second;
        if (idx >= 0 && idx < (int)params.size())
            params[idx]->setValueNotifyingHost(val);
    }
}

void BankEditor::snapshotCurrentBank()
{
    if (!vstHost || activeBankIndex < 0 || activeBankIndex >= (int)banks.size())
        return;

    auto* inst = vstHost->getPluginInstance();
    if (!inst)
        return;

    auto& b = banks[activeBankIndex];
    const auto& params = inst->getParameters();
    const int N = (int)params.size();

    // Инициализация/переразмер базовой копии параметров банка
    if ((int)b.pluginParamValues.size() != N)
        b.pluginParamValues.assign(N, 0.0f);

    static constexpr float eps = 1.0e-4f;
    std::unordered_map<int, float> newDiffs;
    newDiffs.reserve((size_t)N / 4); // эвристика, чтобы меньше реаллокаций

    for (int i = 0; i < N; ++i)
    {
        const float v = params[i]->getValue();       // текущее значение в плагине [0..1]
        const float old = b.pluginParamValues[i];       // сохранённый baseline банка

        if (std::abs(v - old) > eps)
            newDiffs[i] = v;                            // зафиксировать как отличающееся

        b.pluginParamValues[i] = v;                     // обновить baseline банка
    }

    b.paramDiffs = std::move(newDiffs);
    b.activeProgram = inst->getCurrentProgram();
}
void BankEditor::unloadPluginEverywhere()
{
    // 1. Выгружаем сам плагин
    if (vstHost)
        vstHost->unloadPlugin();

    // 2. Сбрасываем все банки в дефолт
    for (int i = 0; i < numBanks; ++i)
    {
        banks[i] = Bank(); // пересоздаём структуру
        banks[i].bankName = "BANK" + juce::String(i + 1);
    }

    // 3. Чистим глобальные данные плагина
    globalPluginName.clear();
    globalPluginId.clear();
    globalPluginState.reset();
    globalPluginParamValues.clear();
    globalActiveProgram = -1;

    // 4. Сбрасываем активные индексы
    activeBankIndex = 0;
    activePreset = 0;

    // 5. Обновляем UI
    updateUI();

    // 6. Сохраняем пустой конфиг
    saveSettingsToFile(getDefaultConfigFile());

}
juce::String BankEditor::getCurrentPluginDisplayName() const
{
    if (vstHost != nullptr)
        if (auto* inst = vstHost->getPluginInstance())
            return inst->getName().isEmpty() ? "Plugin: <unnamed>" : inst->getName();

    return "Plugin: None";
}
/////педаль 
void BankEditor::applyPedalValue(int slot, float norm)
{
    if (slot < 0 || slot >= numCCParams || vstHost == nullptr)
        return;

    auto& bank = banks[activeBankIndex];
    CCMapping& m = bank.globalCCMappings[slot];

    // 1. Применение к плагину
    if (m.paramIndex >= 0)
    {
        if (auto* plug = vstHost->getPluginInstance())
        {
            if (auto* param = plug->getParameters()[m.paramIndex])
            {
                float v = m.invert ? (1.0f - norm) : norm;
                param->setValueNotifyingHost(v);
            }
        }
    }

    // 2. Отправка MIDI CC (если нужно)
    if (midiOutput != nullptr)
    {
        int ccValue = juce::jlimit(0, 127, static_cast<int>(norm * 127.0f));
        auto msg = juce::MidiMessage::controllerEvent(1, slot + 1, ccValue);
        midiOutput->sendMessageNow(msg);
    }

    // 3. Обновление модели (если нужно)
    if (activePreset >= 0 && activePreset < (int)bank.ccPresetStates.size())
    {
        bank.ccPresetStates[activePreset][slot] = (norm > 0.0f);
        bank.presetCCMappings[activePreset][slot].enabled = (norm > 0.0f);
    }

    sendChange();
}


void BankEditor::openPedalMappingDialog(int slot)
{
    if (slot < 0 || slot >= numCCParams)
        return;

    CCMapping& m = banks[activeBankIndex].globalCCMappings[slot];

    auto dlg = std::make_unique<SetCCDialog>(
        vstHost,
        m,
        juce::String("Pedal Mapping ") + juce::String(slot),
        [this, slot](CCMapping newMap, bool ok)
        {
            if (ok)
                banks[activeBankIndex].globalCCMappings[slot] = newMap;
        });

    dlg->setVisible(true);
}



