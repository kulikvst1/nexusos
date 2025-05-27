#pragma once
#include <JuceHeader.h>
#include <vector>
#include <functional>

//==============================================================================
// Компонент BankRow для отображения и редактирования имени банка и 6 пресетов.
class BankRow : public juce::Component, public juce::TextEditor::Listener
{
public:
    BankRow(int bankIndex, const juce::String& defaultBankName, const juce::StringArray& defaultPresetNames)
        : index(bankIndex)
    {
        bankNameEditor.setText(defaultBankName);
        bankNameEditor.addListener(this);
        bankNameEditor.setJustification(juce::Justification::centredLeft);
        addAndMakeVisible(bankNameEditor);

        // Создание 6 пресет-редакторов
        for (int i = 0; i < 6; ++i)
        {
            auto* editor = new juce::TextEditor();
            editor->setText(defaultPresetNames[i]);
            editor->addListener(this);
            editor->setJustification(juce::Justification::centred);
            presetEditors.add(editor);
            addAndMakeVisible(editor);
        }
    }

    ~BankRow() override
    {
        bankNameEditor.removeListener(this);
        for (auto* editor : presetEditors)
            editor->removeListener(this);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(4);
        int bankNameWidth = 150;
        bankNameEditor.setBounds(bounds.removeFromLeft(bankNameWidth));
        int remaining = bounds.getWidth();
        int each = remaining / presetEditors.size();
        for (auto* editor : presetEditors)
            editor->setBounds(bounds.removeFromLeft(each));
    }

    juce::String getBankName() const { return bankNameEditor.getText(); }
    juce::String getPresetName(int i) const
    {
        jassert(i >= 0 && i < presetEditors.size());
        return presetEditors[i]->getText();
    }
    void updateFields(const juce::String& newBankName, const juce::StringArray& newPresetNames)
    {
        bankNameEditor.setText(newBankName, juce::dontSendNotification);
        for (int i = 0; i < presetEditors.size(); ++i)
        {
            if (i < newPresetNames.size())
                presetEditors[i]->setText(newPresetNames[i], juce::dontSendNotification);
        }
    }

    // Вызывается при любом изменении текста.
    void textEditorTextChanged(juce::TextEditor& /*editor*/) override
    {
        if (onRowChanged != nullptr)
            onRowChanged(index);
    }
    void textEditorReturnKeyPressed(juce::TextEditor& /*editor*/) override {}
    void textEditorEscapeKeyPressed(juce::TextEditor& /*editor*/) override {}
    void textEditorFocusLost(juce::TextEditor& /*editor*/) override {}

    // Callback для оповещения об изменениях (например, изменения BANK NAME)
    std::function<void(int bankIndex)> onRowChanged;

private:
    int index{ 0 };
    juce::TextEditor bankNameEditor;
    juce::OwnedArray<juce::TextEditor> presetEditors;
};

//==============================================================================
// Класс BankManager для управления списком банков (15 штук) и их пресетов.
class BankManager : public juce::Component, public juce::Timer
{
public:
    // --- Переместите объявление структуры Bank вперед, прежде чем использовать его в методах ---
    struct Bank
    {
        juce::String       bankName;
        juce::StringArray  presetNames;            // 6 пресетов
        std::vector<std::vector<bool>> ccPresetStates; // Элементы CC для каждого пресета
        std::vector<int>   presetVolume;


        Bank()
        {
            // Инициализируем 6 пресетов – для каждого 10 CC-кнопок (состояния по умолчанию: false)
            ccPresetStates.resize(6);
            for (auto& preset : ccPresetStates)
                preset.resize(10, false);
            bankName = "Untitled";
            presetNames.clear();
            for (int i = 0; i < 6; ++i)
                presetNames.add("Preset " + juce::String(static_cast<char>('A' + i)));
            // Инициализируем значение Volume для каждого пресета (например, 64 по умолчанию)
            presetVolume.resize(6, 64);
        }
    };

    // Теперь можно использовать Bank в методах-аксессорах
    const std::vector<Bank>& getBanks() const { return banks; }
    std::vector<Bank>& getBanks() { return banks; }

    BankManager()
    {
        // Инициализация 15 банков дефолтными данными
        banks.resize(15);
        for (int i = 0; i < (int)banks.size(); i++)
        {
            banks[i].bankName = "Bank " + juce::String(i + 1);
            banks[i].presetNames.clear();
            for (int j = 0; j < 6; j++)
                banks[i].presetNames.add("Preset " + juce::String(static_cast<char>('A' + j)));
        }
        currentBankIndex = 0;

        // Создаём контейнер для BankRow и добавляем их
        rowContainer = std::make_unique<juce::Component>();
        for (int i = 0; i < banks.size(); i++)
        {
            auto row = std::make_unique<BankRow>(i, banks[i].bankName, banks[i].presetNames);
            // При изменении данных обновляем соответствующий банк в banks
            row->onRowChanged = [this](int bankIndex)
                {
                    if (bankIndex >= 0 && bankIndex < banks.size() && bankIndex < bankRows.size())
                    {
                        banks[bankIndex].bankName = bankRows[bankIndex]->getBankName();
                        for (int j = 0; j < 6; j++)
                            banks[bankIndex].presetNames.set(j, bankRows[bankIndex]->getPresetName(j));
                    }
                    if (onBankManagerChanged != nullptr)
                        onBankManagerChanged();

                    // При изменениях перезапускаем таймер для автосохранения (debounce 2 сек)
                    restartAutoSaveTimer();
                };

            rowContainer->addAndMakeVisible(row.get());
            bankRows.add(std::move(row));
        }

        addAndMakeVisible(viewport);
        viewport.setViewedComponent(rowContainer.get(), false);

        // Автоматическая загрузка настроек при запуске
        loadSettings();
    }

    ~BankManager() override
    {
        stopTimer();
    }

    // Реализация дебаунс-таймера: при изменениях перезапускаем таймер
    void restartAutoSaveTimer()
    {
        stopTimer();
        startTimer(2000); // 2 секунды
    }

    // Вызывается через 2 секунды после последнего изменения
    void timerCallback() override
    {
        saveSettings();
        stopTimer();
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        viewport.setBounds(bounds);
        int rowHeight = 40;  // высота строки
        rowContainer->setSize(bounds.getWidth(), bankRows.size() * rowHeight);
        int y = 0;
        for (auto& row : bankRows)
        {
            row->setBounds(0, y, rowContainer->getWidth(), rowHeight);
            y += rowHeight;
        }
    }

    const Bank& getBank(int index) const
    {
        jassert(index >= 0 && index < banks.size());
        return banks[index];
    }

    int getActiveBankIndex() const { return currentBankIndex; }

    void setActiveBankIndex(int newIndex)
    {
        if (newIndex >= 0 && newIndex < banks.size() && newIndex != currentBankIndex)
        {
            currentBankIndex = newIndex;
            if (onBankManagerChanged != nullptr)
                onBankManagerChanged();
        }

    }

    // Callback, вызываемый при обновлении данных (например, изменения BANK NAME)
    std::function<void()> onBankManagerChanged;

    // Сохранение настроек в INI-файл
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

                // Сохраняем состояние CC для пресета j как строку типа "1,0,1,0,..." 
                const auto& ccStates = banks[i].ccPresetStates[j];
                juce::String stateStr;
                for (int k = 0; k < ccStates.size(); ++k)
                {
                    stateStr += (ccStates[k] ? "1" : "0");
                    if (k < ccStates.size() - 1)
                        stateStr += ",";
                }
                propertiesFile.setValue("bank_" + juce::String(i) + "_ccStates_" + juce::String(j), stateStr);
                // Сохраняем значение Volume для пресета j
                propertiesFile.setValue("bank_" + juce::String(i) + "_volume_" + juce::String(j),
                    banks[i].presetVolume[j]);
            }
        }
        propertiesFile.saveIfNeeded();
    }


    // Загрузка настроек из INI-файла
    void loadSettings()
    {
        // Получаем файл настроек (расположение – userApplicationDataDirectory)
        juce::File configFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("BankManagerSettings.ini");
        if (!configFile.existsAsFile())
            return; // Если файл не существует, выходим

        juce::PropertiesFile::Options options;
        options.applicationName = "MyBankManagerApp";
        juce::PropertiesFile propertiesFile(configFile, options);

        // Проходим по всем банкам
        for (int i = 0; i < banks.size(); ++i)
        {
            // Загружаем имя для банка i
            banks[i].bankName = propertiesFile.getValue("bankName_" + juce::String(i),
                banks[i].bankName);

            // Для каждого пресета в банке восстанавливаем имя и состояние CC-кнопок
            for (int j = 0; j < banks[i].presetNames.size(); ++j)
            {
                // Восстанавливаем имя пресета
                banks[i].presetNames.set(j,
                    propertiesFile.getValue("bank_" + juce::String(i) + "_preset_" + juce::String(j),
                        banks[i].presetNames[j]));

                // Считываем строку, описывающую состояния CC-кнопок для пресета j
                juce::String stateStr = propertiesFile.getValue("bank_" + juce::String(i) + "_ccStates_" + juce::String(j), "");
                if (stateStr.isNotEmpty())
                {
                    juce::StringArray tokens;
                    tokens.addTokens(stateStr, ",", "");
                    // Обновляем вектор ccPresetStates для данного пресета
                    for (int k = 0; k < juce::jmin(tokens.size(), (int)banks[i].ccPresetStates[j].size()); ++k)
                    {
                        banks[i].ccPresetStates[j][k] = (tokens[k].getIntValue() != 0);
                    }
                }
                // Загружаем сохранённое значение Volume для пресета j:
                banks[i].presetVolume[j] = propertiesFile.getIntValue("bank_" + juce::String(i) + "_volume_" + juce::String(j),
                    banks[i].presetVolume[j]);
            }
        }

        // Обновляем визуальные компоненты для банков
        for (int i = 0; i < bankRows.size(); ++i)
        {
            bankRows[i]->updateFields(banks[i].bankName, banks[i].presetNames);
        }

        // Вызываем callback, чтобы обновить и остальные UI-элементы (например, показать текущий банк)
        if (onBankManagerChanged != nullptr)
            onBankManagerChanged();
    }


    

private:
    std::vector<Bank> banks;               // Список банков
    int currentBankIndex = 0;
    juce::OwnedArray<BankRow> bankRows;    // Компоненты для отображения банков
    std::unique_ptr<juce::Component> rowContainer;
    juce::Viewport viewport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BankManager)
};
