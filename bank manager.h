#pragma once

#include <JuceHeader.h>
#include <vector>
#include <functional>

//==============================================================================
// Компонент BankRow — отображает один банк в виде двух строк:
// • Верхняя строка содержит: поле для имени банка, 6 редакторов для имен пресетов,
//   а также редактор для номера пресета плагина.
// • Нижняя строка содержит 10 редакторов для ввода номеров контроллеров (CC mapping).
class BankRow : public juce::Component,
    public juce::TextEditor::Listener
{
public:
    BankRow(int bankIndex, const juce::String& defaultBankName, const juce::StringArray& defaultPresetNames)
        : index(bankIndex)
    {
        // Поле для имени банка
        bankNameEditor.setText(defaultBankName);
        bankNameEditor.addListener(this);
        bankNameEditor.setJustification(juce::Justification::centredLeft);
        addAndMakeVisible(bankNameEditor);

        // Редакторы для имен пресетов (6 шт.)
        for (int i = 0; i < defaultPresetNames.size(); ++i)
        {
            auto* presetEditor = new juce::TextEditor();
            presetEditor->setText(defaultPresetNames[i]);
            presetEditor->addListener(this);
            presetEditor->setJustification(juce::Justification::centred);
            presetEditors.add(presetEditor);
            addAndMakeVisible(presetEditor);
        }

        // Редактор для номера плагин-пресета
        pluginPresetEditor.setText("0");
        pluginPresetEditor.addListener(this);
        pluginPresetEditor.setJustification(juce::Justification::centred);
        pluginPresetEditor.setInputRestrictions(3, "0123456789");
        addAndMakeVisible(pluginPresetEditor);

        // Редакторы для CC mapping (10 шт.)
        for (int i = 0; i < 10; ++i)
        {
            auto* ccEditor = new juce::TextEditor();
            ccEditor->setText(juce::String(i + 1)); // по умолчанию от 1 до 10
            ccEditor->setInputRestrictions(3, "0123456789");
            ccEditor->addListener(this);
            ccMappingEditors.add(ccEditor);
            addAndMakeVisible(ccEditor);
        }
    }

    ~BankRow() override
    {
        bankNameEditor.removeListener(this);
        for (auto* ed : presetEditors)
            ed->removeListener(this);
        for (auto* ed : ccMappingEditors)
            ed->removeListener(this);
        pluginPresetEditor.removeListener(this);
    }

    /** Выделяет или снимает выделение с данной строки. */
    void setHighlighted(bool shouldHighlight)
    {
        isActiveRow = shouldHighlight;
        repaint();
    }

    /** Включает/отключает редактирование поля номера плагин-пресета.
        Если disabled, в поле устанавливается текст "n/a". */
    void setPluginPresetEnabled(bool enabled)
    {
        pluginPresetEditor.setEnabled(enabled);
        pluginPresetEditor.setWantsKeyboardFocus(enabled);
        pluginPresetEditor.setInterceptsMouseClicks(enabled, false);
        if (!enabled)
            pluginPresetEditor.setText("n/a", juce::dontSendNotification);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        // Если банк активен, фон красный; иначе – белый.
        if (isActiveRow)
            g.setColour(juce::Colours::red.withAlpha(0.9f));
        else
            g.setColour(juce::Colours::white.withAlpha(0.95f));
        g.fillRoundedRectangle(bounds, 8.0f);
        g.setColour(juce::Colours::grey);
        g.drawRoundedRectangle(bounds, 8.0f, 1.0f);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(2);
        int totalHeight = bounds.getHeight();
        int topRowHeight = static_cast<int>(totalHeight * 0.6f);
        int bottomRowHeight = totalHeight - topRowHeight;

        // Верхняя строка: сначала поле имени банка
        auto topArea = bounds.removeFromTop(topRowHeight);
        int bankNameWidth = 150;
        bankNameEditor.setBounds(topArea.removeFromLeft(bankNameWidth));

        // Количество колонок: 6 редакторов для пресетов + 1 редактор для номера плагин-пресета
        int numColumns = presetEditors.size() + 1;
        int remainingWidth = topArea.getWidth();
        int eachColWidth = (numColumns > 0) ? remainingWidth / numColumns : 0;

        // Располагаем редакторы для имен пресетов
        for (auto* editor : presetEditors)
            editor->setBounds(topArea.removeFromLeft(eachColWidth));

        // Оставшуюся область выделяем для редактора номера плагин-пресета
        pluginPresetEditor.setBounds(topArea);

        // Нижняя строка: редакторы для CC mapping
        int numCC = ccMappingEditors.size();
        int eachCCWidth = (numCC > 0) ? bounds.getWidth() / numCC : 0;
        for (int i = 0; i < numCC; ++i)
            ccMappingEditors[i]->setBounds(i * eachCCWidth, bounds.getY(), eachCCWidth, bottomRowHeight);
    }

    // Геттеры данных
    juce::String getBankName() const { return bankNameEditor.getText(); }
    juce::String getPresetName(int i) const { jassert(i >= 0 && i < presetEditors.size()); return presetEditors[i]->getText(); }
    int getPluginPresetNumber() const { return pluginPresetEditor.getText().getIntValue(); }

    std::vector<int> getCurrentMapping() const
    {
        std::vector<int> mapping;
        for (int i = 0; i < ccMappingEditors.size(); ++i)
        {
            int val = ccMappingEditors[i]->getText().getIntValue();
            if (val > 127)
                val = 127;
            mapping.push_back(val);
        }
        return mapping;
    }

    /** Обновляет поля: имя банка, имена пресетов и значение плагин-пресета. */
    void updateFields(const juce::String& newBankName,
        const juce::StringArray& newPresetNames,
        const juce::String& pluginPresetStr)
    {
        bankNameEditor.setText(newBankName, juce::dontSendNotification);
        for (int i = 0; i < presetEditors.size(); ++i)
        {
            if (i < newPresetNames.size())
                presetEditors[i]->setText(newPresetNames[i], juce::dontSendNotification);
        }
        pluginPresetEditor.setText(pluginPresetStr, juce::dontSendNotification);
    }

    /** Обновляет значения редакторов для CC mapping по вектору mapping. */
    void updateMappingEditors(const std::vector<int>& mapping)
    {
        for (int i = 0; i < ccMappingEditors.size(); ++i)
        {
            int val = (i < mapping.size()) ? mapping[i] : 0;
            ccMappingEditors[i]->setText(juce::String(val), juce::dontSendNotification);
        }
    }

    // TextEditor Listener
    void textEditorTextChanged(juce::TextEditor& editor) override
    {
        if (&editor == &bankNameEditor)
        {
            if (onRowChanged)
                onRowChanged(index);
        }
        else
        {
            for (int i = 0; i < presetEditors.size(); ++i)
            {
                if (&editor == presetEditors[i])
                {
                    if (onRowChanged)
                        onRowChanged(index);
                    return;
                }
            }
            if (&editor == &pluginPresetEditor)
            {
                if (onRowChanged)
                    onRowChanged(index);
                return;
            }
            for (int i = 0; i < ccMappingEditors.size(); ++i)
            {
                if (&editor == ccMappingEditors[i])
                {
                    int newMapping = editor.getText().getIntValue();
                    if (newMapping > 127)
                        newMapping = 127;
                    if (onMappingChanged)
                        onMappingChanged(index, i, newMapping);
                    return;
                }
            }
        }
    }

    void textEditorReturnKeyPressed(juce::TextEditor& /*editor*/) override {}
    void textEditorEscapeKeyPressed(juce::TextEditor& /*editor*/) override {}
    void textEditorFocusLost(juce::TextEditor& /*editor*/) override {}

    std::function<void(int bankIndex)> onRowChanged;
    std::function<void(int bankIndex, int ccFieldIndex, int newMapping)> onMappingChanged;

private:
    int index{ 0 };
    juce::TextEditor bankNameEditor;
    juce::OwnedArray<juce::TextEditor> presetEditors;
    juce::OwnedArray<juce::TextEditor> ccMappingEditors;
    juce::TextEditor pluginPresetEditor;
    bool isActiveRow{ false };
};

//==============================================================================
// Класс BankManager управляет списком из 15 банков.
// Каждый банк содержит:
// • Имя банка,
// • 6 имен пресетов,
// • CC mapping для 10 контроллеров,
// • Номер плагин-пресета.
class BankManager : public juce::Component,
    public juce::Timer
{
public:
    struct Bank
    {
        juce::String bankName;
        juce::StringArray presetNames;  // 6 пресетов
        std::vector<std::vector<bool>> ccPresetStates; // не меняются
        std::vector<int> presetVolume;
        std::vector<int> ccMapping;  // для 10 контроллеров
        int pluginPreset = 0;

        Bank()
        {
            ccPresetStates.resize(6);
            for (auto& preset : ccPresetStates)
                preset.resize(10, false);
            bankName = "Untitled";
            presetNames.clear();
            for (int i = 0; i < 6; ++i)
                presetNames.add("PRESET" + juce::String(i + 1));
            presetVolume.resize(6, 64);
            ccMapping.resize(10);
            for (int i = 0; i < 10; ++i)
                ccMapping[i] = i + 1;
            pluginPreset = 1; // По умолчанию устанавливаем 1 (если плагин поддерживает до 15)
        }
    };

    const std::vector<Bank>& getBanks() const { return banks; }
    std::vector<Bank>& getBanks() { return banks; }

    BankManager()
    {
        banks.resize(15);
        for (int i = 0; i < (int)banks.size(); ++i)
        {
            banks[i].bankName = "BANK " + juce::String(i + 1);
            banks[i].presetNames.clear();
            for (int j = 0; j < 6; ++j)
                banks[i].presetNames.add("PRESET" + juce::String(j + 1));
            // ccMapping и pluginPreset уже установлены в конструкторе Bank
        }
        currentBankIndex = 0;

        // Создаем контейнер для строк
        rowContainer = std::make_unique<juce::Component>();
        for (int i = 0; i < (int)banks.size(); ++i)
        {
            auto row = std::make_unique<BankRow>(i, banks[i].bankName, banks[i].presetNames);
            row->onRowChanged = [this](int bankIndex)
                {
                    if (bankIndex >= 0 && bankIndex < banks.size() && bankIndex < bankRows.size())
                    {
                        banks[bankIndex].bankName = bankRows[bankIndex]->getBankName();
                        for (int j = 0; j < 6; ++j)
                            banks[bankIndex].presetNames.set(j, bankRows[bankIndex]->getPresetName(j));
                        banks[bankIndex].pluginPreset = bankRows[bankIndex]->getPluginPresetNumber();
                    }
                    if (onBankManagerChanged)
                        onBankManagerChanged();
                    restartAutoSaveTimer();
                };
            row->onMappingChanged = [this](int bankIndex, int ccFieldIndex, int newMapping)
                {
                    if (bankIndex >= 0 && bankIndex < banks.size())
                    {
                        banks[bankIndex].ccMapping[ccFieldIndex] = newMapping;
                        if (onBankManagerChanged)
                            onBankManagerChanged();
                        restartAutoSaveTimer();
                    }
                };
            rowContainer->addAndMakeVisible(row.get());
            bankRows.add(std::move(row));
        }

        addAndMakeVisible(viewport);
        viewport.setViewedComponent(rowContainer.get(), false);

        // Добавляем кнопку сброса дефолтных настроек
        resetDefaultsButton.setButtonText("Reset Defaults");
        resetDefaultsButton.onClick = [this]()
            {
                // Показываем окно подтверждения с шестым аргументом (nullptr)
              if (juce::AlertWindow::showOkCancelBox(
                juce::MessageBoxIconType::WarningIcon,  // Значок окна (предполагается, что это доступное значение)
              "Reset Defaults",                        // Заголовок окна
                  "Are you sure you want to reset all banks to default?", // Текст сообщения
               "OK",                                    // Текст кнопки OK
                 "Cancel",                                // Текст кнопки Cancel
               nullptr,                                 // Associated component (нет компонента)
               nullptr))                                // Callback (нет callback-функции)
                {
                    resetAllDefaults();
                }
            };
        addAndMakeVisible(resetDefaultsButton);

        loadSettings();
        updateRowHighlighting();
    }

    ~BankManager() override { stopTimer(); }

    void restartAutoSaveTimer()
    {
        stopTimer();
        startTimer(2000);
    }

    void timerCallback() override
    {
        saveSettings();
        stopTimer();
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        // Выделяем в нижней части место для кнопки сброса
        int buttonHeight = 40;
        resetDefaultsButton.setBounds(bounds.removeFromBottom(buttonHeight).reduced(10));
        viewport.setBounds(bounds);

        // Размеры rowContainer внутри viewport:
        int rowHeight = 100;
        int gap = 8;
        rowContainer->setSize(viewport.getWidth(), bankRows.size() * (rowHeight + gap));

        int y = 0;
        for (auto& row : bankRows)
        {
            row->setBounds(0, y, rowContainer->getWidth(), rowHeight);
            y += rowHeight + gap;
        }
    }

    const Bank& getBank(int index) const { jassert(index >= 0 && index < banks.size()); return banks[index]; }
    int getActiveBankIndex() const { return currentBankIndex; }

    void setActiveBankIndex(int newIndex)
    {
        if (newIndex >= 0 && newIndex < (int)banks.size() && newIndex != currentBankIndex)
        {
            currentBankIndex = newIndex;
            if (onBankManagerChanged)
                onBankManagerChanged();
            updateRowHighlighting();
        }
    }

    /** Обновляет подсветку активного банка. */
    void updateRowHighlighting()
    {
        for (int i = 0; i < bankRows.size(); ++i)
            bankRows[i]->setHighlighted(i == currentBankIndex);
    }

    /** Обновляет состояние поля ввода номера плагина-пресета во всех банках.
        Если supported == false, поле блокируется и вместо числа отображается "n/a". */
    void setPluginPresetSupported(bool supported)
    {
        for (auto* row : bankRows)
            row->setPluginPresetEnabled(supported);
    }

    std::function<void()> onBankManagerChanged;

    void saveSettings()
    {
        juce::File configFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("BankManagerSettings.ini");
        juce::PropertiesFile::Options options;
        options.applicationName = "MyBankManagerApp";
        juce::PropertiesFile propertiesFile(configFile, options);
        for (int i = 0; i < (int)banks.size(); ++i)
        {
            propertiesFile.setValue("bankName_" + juce::String(i), banks[i].bankName);
            for (int j = 0; j < banks[i].presetNames.size(); ++j)
                propertiesFile.setValue("bank_" + juce::String(i) + "_preset_" + juce::String(j),
                    banks[i].presetNames[j]);
            juce::String mappingStr;
            for (int m = 0; m < banks[i].ccMapping.size(); ++m)
            {
                mappingStr += juce::String(banks[i].ccMapping[m]);
                if (m < banks[i].ccMapping.size() - 1)
                    mappingStr += ",";
            }
            propertiesFile.setValue("bank_" + juce::String(i) + "_ccMapping", mappingStr);
            propertiesFile.setValue("bank_" + juce::String(i) + "_pluginPreset", banks[i].pluginPreset);
        }
        propertiesFile.saveIfNeeded();
    }

    void loadSettings()
    {
        juce::File configFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("BankManagerSettings.ini");
        if (!configFile.existsAsFile())
            return;
        juce::PropertiesFile::Options options;
        options.applicationName = "MyBankManagerApp";
        juce::PropertiesFile propertiesFile(configFile, options);
        for (int i = 0; i < (int)banks.size(); ++i)
        {
            banks[i].bankName = propertiesFile.getValue("bankName_" + juce::String(i), banks[i].bankName);
            for (int j = 0; j < banks[i].presetNames.size(); ++j)
                banks[i].presetNames.set(j,
                    propertiesFile.getValue("bank_" + juce::String(i) + "_preset_" + juce::String(j),
                        banks[i].presetNames[j]));
            juce::String mappingStr = propertiesFile.getValue("bank_" + juce::String(i) + "_ccMapping", "");
            if (mappingStr.isNotEmpty())
            {
                juce::StringArray tokens;
                tokens.addTokens(mappingStr, ",", "");
                banks[i].ccMapping.resize(10);
                for (int m = 0; m < juce::jmin(tokens.size(), 10); ++m)
                    banks[i].ccMapping[m] = tokens[m].getIntValue();
            }
            banks[i].pluginPreset = propertiesFile.getIntValue("bank_" + juce::String(i) + "_pluginPreset", banks[i].pluginPreset);
        }
        for (int i = 0; i < bankRows.size(); ++i)
        {
            bankRows[i]->updateFields(banks[i].bankName, banks[i].presetNames, juce::String(banks[i].pluginPreset));
            bankRows[i]->updateMappingEditors(banks[i].ccMapping);
        }
        if (onBankManagerChanged)
            onBankManagerChanged();
        updateRowHighlighting();
    }

private:
    /** Сбрасывает все банки к дефолтным настройкам с предварительным подтверждением. */
    void resetAllDefaults()
    {
        // Для каждого банка сбрасываем:
        // - Имя банка -> "BANK " + (индекс + 1)
        // - Имена пресетов -> "PRESET1" ... "PRESET6"
        // - CC mapping -> числа от 1 до 10
        // - Номер плагин-пресета -> 1 (дефолтное значение)
        for (int i = 0; i < banks.size(); i++)
        {
            banks[i].bankName = "BANK " + juce::String(i + 1);
            banks[i].presetNames.clear();
            for (int j = 0; j < 6; j++)
            {
                banks[i].presetNames.add("PRESET" + juce::String(j + 1));
            }
            banks[i].ccMapping.resize(10);
            for (int j = 0; j < 10; j++)
            {
                banks[i].ccMapping[j] = j + 1;
            }
            banks[i].pluginPreset = 1;
        }

        // Обновляем все BankRow
        for (int i = 0; i < bankRows.size(); i++)
        {
            bankRows[i]->updateFields(banks[i].bankName, banks[i].presetNames, juce::String(banks[i].pluginPreset));
            bankRows[i]->updateMappingEditors(banks[i].ccMapping);
        }

        if (onBankManagerChanged)
            onBankManagerChanged();
        saveSettings();
    }

    std::vector<Bank> banks;
    int currentBankIndex = 0;
    juce::OwnedArray<BankRow> bankRows;
    std::unique_ptr<juce::Component> rowContainer;
    juce::Viewport viewport;

    juce::TextButton resetDefaultsButton; // Кнопка сброса дефолтных настроек

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BankManager)
};
