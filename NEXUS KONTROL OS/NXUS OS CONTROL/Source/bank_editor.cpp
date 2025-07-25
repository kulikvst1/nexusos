#include"bank_editor.h"
#include "plugin_process_callback.h"     // ← без лишней точки!
#include "custom_audio_playhead.h"
#include "LearnController.h"
#include "cpu_load.h"
#include <memory>
#include <atomic>

// extern juce::ThreadPool pluginScanPool;   // объявлен в vst_host.cpp////////////////////////////////////////////////////////////////////////////////


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
//───────────────────────────  Фоновая job для автосейва  ─────────────────────
class SaveJob : public juce::ThreadPoolJob
{
public:
    explicit SaveJob(BankEditor& owner)
        : juce::ThreadPoolJob("autosave"), ref(owner) {}

    JobStatus runJob() override
    {
        DBG("[SaveJob] started  @" << juce::Time::getCurrentTime().toString(true, true));
        ref.saveSettings();
        DBG("[SaveJob] finished @" << juce::Time::getCurrentTime().toString(true, true));
        return jobHasFinished;
    }
private:
    BankEditor& ref;
};

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
        ccNameEditors[i].onTextChange = [this, i]() {
            banks[activeBankIndex].globalCCMappings[i].name = ccNameEditors[i].getText();
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
            juce::Colour(0, 80, 0));          // тёмно-зелёный
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
    for (int i = 0; i < numBanks; ++i)
    {
        banks[i].bankName = "BANK" + juce::String(i + 1);
    }
    // запустить автосохранение каждые 30 секунд
    startTimer(30 * 1000);

    // установить UI по умолчанию
    setActiveBankIndex(0);
    setActivePreset(0);
    // vstButton.onClick = [this] { showVSTDialog(); };

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
        pluginLabel.setFont(juce::Font(sH * 0.45f, juce::Font::plain));
        pluginLabel.setJustificationType(juce::Justification::centredLeft);
        pluginLabel.setMinimumHorizontalScale(0.7f);
        pluginLabel.setBounds(7 * fw, fy, fw, sH);
    }
    // —————————— Row 3: selectedPresetLabel ——————————
    selectedPresetLabel.setBounds(0, 3 * sH, W, sH);
    // —————————— Row 4: SET CC buttons ——————————
    for (int i = 0; i < numCCParams; ++i)
        setCCButtons[i].setBounds(i * 2 * sW, 5 * sH, 2 * sW, sH);
    // —————————— Row 5: CC name editors ——————————
    for (int i = 0; i < numCCParams; ++i)
        ccNameEditors[i].setBounds(i * 2 * sW, 6 * sH, 2 * sW, sH);
    // —————————— Row 6: CC toggle buttons ——————————
    for (int i = 0; i < numCCParams; ++i)
        ccToggleButtons[i].setBounds(i * 2 * sW, 7 * sH, 2 * sW, sH);
    // ── Row 7: Learn buttons (2 сектора на кнопку) ───────────────────────
    for (int i = 0; i < numCCParams; ++i)
        learnButtons[i].setBounds(i * 2 * sW, 4 * sH, 2 * sW, sH);
    // —————————— Row 11: Default/Load/Store/Save/Cancel ——————————
    defaultButton.setBounds(0 * sW, 11 * sH, 2 * sW, sH);
    loadButton.setBounds(6 * sW, 11 * sH, 2 * sW, sH);
    storeButton.setBounds(9 * sW, 11 * sH, 2 * sW, sH);
    saveButton.setBounds(12 * sW, 11 * sH, 2 * sW, sH);
    cancelButton.setBounds(18 * sW, 11 * sH, 2 * sW, sH);
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
    if (b == &defaultButton) resetAllDefaults();
    else if (b == &loadButton)    loadFromDisk();
    else if (b == &storeButton)   storeToBank();
    else if (b == &saveButton)    saveToDisk();
    else if (b == &cancelButton)  cancelChanges();



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
    setActiveBank(newIdx);      // обновили activeBankIndex + UI
    setActivePreset(0);         // сбросили пресет на 0
    if (onBankChanged)          // уведомляем външний Rig_control
        onBankChanged();
    restartAutoSaveTimer();
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

    for (int i = 0; i < numCCParams; ++i)
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
    // Обновляем текст для каждого редактора имен пресетов:
    for (int i = 0; i < numPresets; ++i)
        presetEditors[i].setText(banks[activeBankIndex].presetNames[i], juce::dontSendNotification);
    // Обновляем метку плагина. Если имя плагина в банке пустое, метка покажет пустое значение.
    pluginLabel.setText(banks[activeBankIndex].pluginName, juce::dontSendNotification);
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
    {
        banks[i] = Bank(); // пересоздаём банк
        banks[i].bankName = "BANK" + juce::String(i + 1);
    }
    vstHost->unloadPlugin();          // плагин ушёл
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
    // Закрываем любые активные PopupMenu
    juce::PopupMenu::dismissAllActiveMenus();
    if (vstHost == nullptr)
        return;

    // A: плагин ещё не загружен — строим меню из PluginManager
    if (vstHost->getPluginInstance() == nullptr)
    {
        // 1) Берём «снепшот» списка плагинов под защитой lock’а
        auto entries = vstHost->getPluginManager().getPluginsSnapshot();

        // 2) Фильтруем только включённые и строим PopupMenu
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

        // 3) Если нет ни одного плагина — предупредим и выйдем
        if (indexMap.empty())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "No Plugins Enabled",
                "Please enable the plugin(s) in the Plugin Manager.");
            return;
        }


        // 4) Показать меню и загрузить выбранный плагин
        menu.showMenuAsync(
            juce::PopupMenu::Options().withTargetComponent(&vstButton),
            [this, entries, indexMap](int choice)
            {
                // Игнорируем отмену или неверный выбор
                if (choice < 1 || choice >(int)indexMap.size())
                    return;

                // «реальный» индекс в entries[]
                int realIdx = indexMap[choice - 1];
                auto& desc = entries[realIdx].desc;

                // Текущие аудио-настройки
                auto& adm = vstHost->getAudioDeviceManagerRef();
                auto* dev = adm.getCurrentAudioDevice();
                double sr = dev ? dev->getCurrentSampleRate() : 44100.0;
                int    bs = dev ? dev->getCurrentBufferSizeSamples() : 512;

                // Загружаем плагин
                vstHost->loadPlugin(juce::File(desc.fileOrIdentifier), sr, bs);

                // Обновляем CC-маппинг на случай нового количества параметров
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

                // Обновляем UI
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
                    vstHost->unloadPlugin();
                    clearCCMappingsForActiveBank();
                    setActiveBankIndex(0);
                    setActivePreset(0);
                    saveSettings();

                    banks[activeBankIndex].pluginName = "Plugin: None";
                    pluginLabel.setText(banks[activeBankIndex].pluginName,
                        juce::dontSendNotification);
                    vstButton.setButtonText("Load Plugin");

                    // Сбрасываем все CC-имена в UI
                    for (int i = 0; i < numCCParams; ++i)
                    {
                        banks[activeBankIndex].globalCCMappings[i].name = "<none>";
                        ccNameEditors[i].setText("<none>",
                            juce::dontSendNotification);
                    }
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
void BankEditor::updateCCParameter(int index, bool /*ignoredState*/)
{
    auto& globalMapping = banks[activeBankIndex].globalCCMappings[index];
    auto& presetMapping = banks[activeBankIndex]
        .presetCCMappings[activePreset][index];

    uint8_t effective = 0;

    // ───────────── НОВАЯ ЛОГИКА ─────────────
    if (!presetMapping.invert)
    {
        // обычный режим: ON → ccValue, OFF → 0
        effective = presetMapping.enabled ? presetMapping.ccValue : 0;
    }
    else
    {
        // инвертированный: ON → 0, OFF → ccValue
        effective = presetMapping.enabled ? 0 : presetMapping.ccValue;
    }
    if (midiOutput != nullptr)
    {
        auto msg = juce::MidiMessage::controllerEvent(1, index + 1, effective);
        midiOutput->sendMessageNow(msg);
    }
    if (vstHost != nullptr && globalMapping.paramIndex >= 0)
        vstHost->setPluginParameter(globalMapping.paramIndex, effective);
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
    learnButtons[slot].setColour(juce::TextButton::buttonColourId,
        oldLearnColour);
    updateSelectedPresetLabel();
    if (onLearnToggled) onLearnToggled(slot, false);
}
void BankEditor::learnFinished(int slot, int paramIdx, const juce::String& pname)
{
    learnCancelled(slot);                          // вернуть кнопку

    // сообщаем хосту, что Learn завершился — он выключит свою кнопку
    if (vstHost)
        vstHost->setExternalLearnState(slot, false);

    auto& map = banks[activeBankIndex].globalCCMappings[slot];
    map.paramIndex = paramIdx;
    map.name = pname.isEmpty() ? "<none>" : pname;
    ccNameEditors[slot].setText(map.name, juce::dontSendNotification);
    saveSettings();
}

void BankEditor::toggleLearnFromHost(int cc, bool on)
{
    if (cc < 0 || cc >= numCCParams) return;
    if (on)  learn.begin(cc);
    else     learn.cancel();
}
