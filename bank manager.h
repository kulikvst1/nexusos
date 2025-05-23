#pragma once

#include <JuceHeader.h>
#include <vector>
#include <functional>

//==============================================================================
// Компонент BankRow – отображает для одного банка две строки:
//  • Верхняя строка: поле для имени банка и 6 текстовых редакторов для пресетов.
//  • Нижняя строка: 10 текстовых редакторов для ввода номера контроллера (CC mapping).
// При этом компонент отрисовывается как «карточка» с закруглённым фоном и рамкой.
class BankRow : public juce::Component,
    public juce::TextEditor::Listener
{
public:
    BankRow(int bankIndex,
        const juce::String& defaultBankName,
        const juce::StringArray& defaultPresetNames)
        : index(bankIndex)
    {
        // Верхняя строка: поле для имени банка
        bankNameEditor.setText(defaultBankName);
        bankNameEditor.addListener(this);
        bankNameEditor.setJustification(juce::Justification::centredLeft);
        addAndMakeVisible(bankNameEditor);

        // Создаем 6 текстовых редакторов для названий пресетов
        for (int i = 0; i < defaultPresetNames.size(); ++i)
        {
            auto* presetEditor = new juce::TextEditor();
            presetEditor->setText(defaultPresetNames[i]);
            presetEditor->addListener(this);
            presetEditor->setJustification(juce::Justification::centred);
            presetEditors.add(presetEditor);
            addAndMakeVisible(presetEditor);
        }

        // Нижняя строка: создаем 10 текстовых редакторов для ввода номеров контроллеров
        for (int i = 0; i < 10; ++i)
        {
            auto* ccEditor = new juce::TextEditor();
            // По умолчанию задаём номер равный i (или любое другое значение)
            ccEditor->setText(juce::String(i));
            // Ограничиваем ввод: максимум 3 символа, только цифры
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
    }

    // Отрисовка компонента BankRow как карточки с мягким фоном и рамкой
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        // Фон – почти белый, с небольшой прозрачностью
        g.setColour(juce::Colours::white.withAlpha(0.95f));
        g.fillRoundedRectangle(bounds, 8.0f);

        // Рамка – светло-серая, толщина 1 пиксель
        g.setColour(juce::Colours::grey);
        g.drawRoundedRectangle(bounds, 8.0f, 1.0f);
    }

    // Разбиваем компонент на две строки:
    //  • Верхняя строка (60% от общей высоты): имя банка + 6 редакторов пресетов.
    //  • Нижняя строка (40%): 10 редакторов CC mapping.
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(4);
        int totalHeight = bounds.getHeight();
        int topRowHeight = static_cast<int>(totalHeight * 0.6f);
        int bottomRowHeight = totalHeight - topRowHeight;

        // Верхняя строка:
        auto topArea = bounds.removeFromTop(topRowHeight);
        int bankNameWidth = 150;
        bankNameEditor.setBounds(topArea.removeFromLeft(bankNameWidth));
        int presetCount = presetEditors.size();
        int remainingWidth = topArea.getWidth();
        int eachPresetWidth = (presetCount > 0) ? remainingWidth / presetCount : 0;
        for (auto* editor : presetEditors)
            editor->setBounds(topArea.removeFromLeft(eachPresetWidth));

        // Нижняя строка: 10 редакторов для ввода номеров контроллеров
        int numCC = ccMappingEditors.size();
        int eachCCWidth = (numCC > 0) ? bounds.getWidth() / numCC : 0;
        for (int i = 0; i < numCC; ++i)
            ccMappingEditors[i]->setBounds(i * eachCCWidth, bounds.getY(), eachCCWidth, bottomRowHeight);
    }

    // Геттеры для данных BankRow
    juce::String getBankName() const { return bankNameEditor.getText(); }
    juce::String getPresetName(int i) const
    {
        jassert(i >= 0 && i < presetEditors.size());
        return presetEditors[i]->getText();
    }

    // Обновление содержимого верхней строки (имя банка и названия пресетов)
    void updateFields(const juce::String& newBankName,
        const juce::StringArray& newPresetNames)
    {
        bankNameEditor.setText(newBankName, juce::dontSendNotification);
        for (int i = 0; i < presetEditors.size(); ++i)
        {
            if (i < newPresetNames.size())
                presetEditors[i]->setText(newPresetNames[i], juce::dontSendNotification);
        }
    }

    // Обновление редакторов для CC mapping по вектору mapping
    void updateMappingEditors(const std::vector<int>& mapping)
    {
        for (int i = 0; i < ccMappingEditors.size(); ++i)
        {
            int val = (i < mapping.size()) ? mapping[i] : 0;
            ccMappingEditors[i]->setText(juce::String(val), juce::dontSendNotification);
        }
    }

    // Считывание текущих номеров контроллеров из редакторов
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

    // Обработчик изменений текстовых редакторов.
    // Если изменяется имя банка или один из пресетов – вызываем onRowChanged.
    // Если изменяется один из редакторов для CC mapping – вызываем onMappingChanged.
    void textEditorTextChanged(juce::TextEditor& editor) override
    {
        if (&editor == &bankNameEditor)
        {
            if (onRowChanged != nullptr)
                onRowChanged(index);
        }
        else
        {
            for (int i = 0; i < presetEditors.size(); ++i)
            {
                if (&editor == presetEditors[i])
                {
                    if (onRowChanged != nullptr)
                        onRowChanged(index);
                    return;
                }
            }
            for (int i = 0; i < ccMappingEditors.size(); ++i)
            {
                if (&editor == ccMappingEditors[i])
                {
                    int newMapping = editor.getText().getIntValue();
                    if (newMapping > 127)
                        newMapping = 127;
                    if (onMappingChanged != nullptr)
                        onMappingChanged(index, i, newMapping);
                    return;
                }
            }
        }
    }
    void textEditorReturnKeyPressed(juce::TextEditor& /*editor*/) override {}
    void textEditorEscapeKeyPressed(juce::TextEditor& /*editor*/) override {}
    void textEditorFocusLost(juce::TextEditor& /*editor*/) override {}

    // Callback для уведомления об изменениях:
    // onRowChanged – для верхней строки (банк и пресеты).
    // onMappingChanged – для изменения номера контроллера.
    std::function<void(int bankIndex)> onRowChanged;
    std::function<void(int bankIndex, int ccFieldIndex, int newMapping)> onMappingChanged;

private:
    int index{ 0 };
    juce::TextEditor bankNameEditor;
    juce::OwnedArray<juce::TextEditor> presetEditors;
    juce::OwnedArray<juce::TextEditor> ccMappingEditors;
};

//==============================================================================
// Класс BankManager для управления списком из 15 банков и их пресетов/CC mapping.
// Каждый банк представляется компонентом BankRow (с двумя строками).
class BankManager : public juce::Component,
    public juce::Timer
{
public:
    // Структура Bank содержит имя банка, 6 пресетов, CC-состояния (для кнопок, не затрагиваем здесь),
    // Volume и вектор ccMapping для 10 кнопок.
    struct Bank
    {
        juce::String bankName;
        juce::StringArray presetNames;   // 6 пресетов
        std::vector<std::vector<bool>> ccPresetStates; // (оставляем без изменений)
        std::vector<int> presetVolume;
        std::vector<int> ccMapping;        // CC mapping для 10 кнопок

        Bank()
        {
            ccPresetStates.resize(6);
            for (auto& preset : ccPresetStates)
                preset.resize(10, false);
            bankName = "Untitled";
            presetNames.clear();
            for (int i = 0; i < 6; ++i)
                presetNames.add("Preset " + juce::String(static_cast<char>('A' + i)));
            presetVolume.resize(6, 64);
            ccMapping.resize(10);
            for (int i = 0; i < 10; ++i)
                ccMapping[i] = i; // По умолчанию: номер контроллера равен индексу
        }
    };

    const std::vector<Bank>& getBanks() const { return banks; }
    std::vector<Bank>& getBanks() { return banks; }

    BankManager()
    {
        // Инициализируем 15 банков дефолтными данными.
        banks.resize(15);
        for (int i = 0; i < (int)banks.size(); ++i)
        {
            banks[i].bankName = "Bank " + juce::String(i + 1);
            banks[i].presetNames.clear();
            for (int j = 0; j < 6; ++j)
                banks[i].presetNames.add("Preset " + juce::String(static_cast<char>('A' + j)));
        }
        currentBankIndex = 0;

        // Создаем контейнер для BankRow и добавляем BankRow для каждого банка.
        rowContainer = std::make_unique<juce::Component>();
        for (int i = 0; i < banks.size(); ++i)
        {
            auto row = std::make_unique<BankRow>(i, banks[i].bankName, banks[i].presetNames);
            // Обработка изменения верхних данных банка (имя, пресеты)
            row->onRowChanged = [this](int bankIndex)
                {
                    if (bankIndex >= 0 && bankIndex < banks.size() && bankIndex < bankRows.size())
                    {
                        banks[bankIndex].bankName = bankRows[bankIndex]->getBankName();
                        for (int j = 0; j < 6; ++j)
                            banks[bankIndex].presetNames.set(j, bankRows[bankIndex]->getPresetName(j));
                    }
                    if (onBankManagerChanged)
                        onBankManagerChanged();
                    restartAutoSaveTimer();
                };
            // Обработка изменения CC mapping: сохраняем введенное значение в массив.
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

        // Автоматическая загрузка настроек при запуске.
        loadSettings();
    }

    ~BankManager() override
    {
        stopTimer();
    }

    // Перезапуск debounce-таймера для автосохранения (2 секунды).
    void restartAutoSaveTimer()
    {
        stopTimer();
        startTimer(2000);
    }

    // Таймер вызывает автосохранение через 2 секунды после последних изменений.
    void timerCallback() override
    {
        saveSettings();
        stopTimer();
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        viewport.setBounds(bounds);

        // Определим высоту каждой BankRow с учетом отступа между банками.
        int rowHeight = 100;  // Например, 100 пикселей высоты для двух рядов внутри каждой карточки
        int gap = 8;          // Отступ между банками
        rowContainer->setSize(bounds.getWidth(), bankRows.size() * (rowHeight + gap));
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
        if (newIndex >= 0 && newIndex < banks.size() && newIndex != currentBankIndex)
        {
            currentBankIndex = newIndex;
            if (onBankManagerChanged)
                onBankManagerChanged();
        }
    }

    // Callback, вызываемый при обновлении данных.
    std::function<void()> onBankManagerChanged;

    // Сохранение настроек в INI-файл (включая данные о CC mapping).
    void saveSettings()
    {
        juce::File configFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("BankManagerSettings.ini");
        juce::PropertiesFile::Options options;
        options.applicationName = "MyBankManagerApp";
        juce::PropertiesFile propertiesFile(configFile, options);
        for (int i = 0; i < banks.size(); ++i)
        {
            propertiesFile.setValue("bankName_" + juce::String(i), banks[i].bankName);
            for (int j = 0; j < banks[i].presetNames.size(); ++j)
            {
                propertiesFile.setValue("bank_" + juce::String(i) + "_preset_" + juce::String(j),
                    banks[i].presetNames[j]);

                // Сохраняем ccPresetStates и volume не изменяем в данном примере.
                const auto& ccStates = banks[i].ccPresetStates[j];
                juce::String stateStr;
                for (int k = 0; k < ccStates.size(); ++k)
                {
                    stateStr += (ccStates[k] ? "1" : "0");
                    if (k < ccStates.size() - 1)
                        stateStr += ",";
                }
                propertiesFile.setValue("bank_" + juce::String(i) + "_ccStates_" + juce::String(j), stateStr);
                propertiesFile.setValue("bank_" + juce::String(i) + "_volume_" + juce::String(j),
                    banks[i].presetVolume[j]);
            }
            // Сохраняем ccMapping как строку чисел, разделённых запятыми.
            juce::String mappingStr;
            for (int m = 0; m < banks[i].ccMapping.size(); ++m)
            {
                mappingStr += juce::String(banks[i].ccMapping[m]);
                if (m < banks[i].ccMapping.size() - 1)
                    mappingStr += ",";
            }
            propertiesFile.setValue("bank_" + juce::String(i) + "_ccMapping", mappingStr);
        }
        propertiesFile.saveIfNeeded();
    }

    // Загрузка настроек из INI-файла.
    void loadSettings()
    {
        juce::File configFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("BankManagerSettings.ini");
        if (!configFile.existsAsFile())
            return;
        juce::PropertiesFile::Options options;
        options.applicationName = "MyBankManagerApp";
        juce::PropertiesFile propertiesFile(configFile, options);
        for (int i = 0; i < banks.size(); ++i)
        {
            banks[i].bankName = propertiesFile.getValue("bankName_" + juce::String(i), banks[i].bankName);
            for (int j = 0; j < banks[i].presetNames.size(); ++j)
            {
                banks[i].presetNames.set(j, propertiesFile.getValue("bank_" + juce::String(i)
                    + "_preset_" + juce::String(j), banks[i].presetNames[j]));
                juce::String stateStr = propertiesFile.getValue("bank_" + juce::String(i)
                    + "_ccStates_" + juce::String(j), "");
                if (stateStr.isNotEmpty())
                {
                    juce::StringArray tokens;
                    tokens.addTokens(stateStr, ",", "");
                    for (int k = 0; k < juce::jmin(tokens.size(), (int)banks[i].ccPresetStates[j].size()); ++k)
                        banks[i].ccPresetStates[j][k] = (tokens[k].getIntValue() != 0);
                }
                banks[i].presetVolume[j] = propertiesFile.getIntValue("bank_" + juce::String(i)
                    + "_volume_" + juce::String(j), banks[i].presetVolume[j]);
            }
            juce::String mappingStr = propertiesFile.getValue("bank_" + juce::String(i) + "_ccMapping", "");
            if (mappingStr.isNotEmpty())
            {
                juce::StringArray tokens;
                tokens.addTokens(mappingStr, ",", "");
                banks[i].ccMapping.resize(10);
                for (int m = 0; m < juce::jmin(tokens.size(), 10); ++m)
                    banks[i].ccMapping[m] = tokens[m].getIntValue();
            }
        }
        // Обновляем визуальную часть: задаем для каждого BankRow актуальные данные.
        for (int i = 0; i < bankRows.size(); ++i)
        {
            bankRows[i]->updateFields(banks[i].bankName, banks[i].presetNames);
            bankRows[i]->updateMappingEditors(banks[i].ccMapping);
        }
        if (onBankManagerChanged)
            onBankManagerChanged();
    }

private:
    std::vector<Bank> banks;
    int currentBankIndex = 0;
    juce::OwnedArray<BankRow> bankRows;
    std::unique_ptr<juce::Component> rowContainer;
    juce::Viewport viewport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BankManager)
};
