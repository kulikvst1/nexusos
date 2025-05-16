#pragma once

#include <JuceHeader.h>
#include <vector>
#include <functional>

//==============================================================================
// Компонент BankRow представляет одну строку для банка.
// Здесь располагается текстовый редактор для имени банка и 6 редакторов для имён пресетов.
class BankRow : public juce::Component,
    public juce::TextEditor::Listener
{
public:
    BankRow(int bankIndex, const juce::String& defaultBankName, const juce::StringArray& defaultPresetNames)
        : index(bankIndex)
    {
        // Редактор для имени банка
        bankNameEditor.setText(defaultBankName);
        bankNameEditor.addListener(this);
        bankNameEditor.setJustification(juce::Justification::centredLeft);
        addAndMakeVisible(bankNameEditor);

        // Создаем 6 текстовых редакторов для имен пресетов
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
        // Размещаем редактор имени банка фиксированной ширины, а остальные редакторы равномерно
        auto bounds = getLocalBounds().reduced(4);
        int bankNameWidth = 150;
        bankNameEditor.setBounds(bounds.removeFromLeft(bankNameWidth));
        int remaining = bounds.getWidth();
        int each = remaining / presetEditors.size();
        for (auto* editor : presetEditors)
            editor->setBounds(bounds.removeFromLeft(each));
    }

    juce::String getBankName() const { return bankNameEditor.getText(); }
    juce::String getPresetName(int i) const { jassert(i >= 0 && i < presetEditors.size()); return presetEditors[i]->getText(); }

    // При изменении текста в любом редакторе вызываем callback
    void textEditorTextChanged(juce::TextEditor& editor) override
    {
        if (onRowChanged != nullptr)
            onRowChanged(index);
        

    }
    void textEditorReturnKeyPressed(juce::TextEditor& editor) override {}
    void textEditorEscapeKeyPressed(juce::TextEditor& editor) override {}
    void textEditorFocusLost(juce::TextEditor& editor) override {}

    // Этот callback (устанавливаемый родительским компонентом) уведомляет о том, что содержимое банки изменилось.
    std::function<void(int bankIndex)> onRowChanged;

private:
    int index{ 0 };
    juce::TextEditor bankNameEditor;
    juce::OwnedArray<juce::TextEditor> presetEditors;
};

//==============================================================================
// Компонент BankManager хранит 20 банков и сразу отображает их все в виде списка.
// Каждый банк хранится в структуре Bank, а для каждого банка создается BankRow, располагаемая вертикально.
// Внешний компонент может подписаться на onBankManagerChanged, чтобы обновить главный интерфейс (например, метку BANK NAME).
class BankManager : public juce::Component
{
public:
    // Структура, описывающая банк
    struct Bank
    {
        juce::String bankName;
        juce::StringArray presetNames;  // 6 имен пресетов

        Bank()
        {
            bankName = "Untitled";
            presetNames.clear();
            for (int i = 0; i < 6; ++i)
            {
                presetNames.add("Preset " + juce::String(static_cast<char>('A' + i)));
            }
        }
    };

    BankManager()
    {
        // Создаем 20 банков по умолчанию
        banks.resize(15);
        for (int i = 0; i < (int)banks.size(); i++)
        {
            banks[i].bankName = "Bank " + juce::String(i + 1);
            banks[i].presetNames.clear();
            for (int j = 0; j < 6; j++)
                banks[i].presetNames.add("Preset " + juce::String(static_cast<char>('A' + j)));
        }

        // По умолчанию активен банк 1 (индекс 0)
        currentBankIndex = 0;

        // Создаем контейнер для всех строк банков
        rowContainer = std::make_unique<juce::Component>();

        // Для каждого банка создаем BankRow и добавляем его в контейнер
        for (int i = 0; i < banks.size(); i++)
        {
            auto row = std::make_unique<BankRow>(i, banks[i].bankName, banks[i].presetNames);
            // При изменении строки обновляем значения банка в векторе banks
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
                };
            rowContainer->addAndMakeVisible(row.get());
            bankRows.add(std::move(row));
        }

        // Создаем прокручиваемый контейнер (Viewport) для списка банков
        addAndMakeVisible(viewport);
        viewport.setViewedComponent(rowContainer.get(), false);
    }

    ~BankManager() override {}

    void resized() override
    {
        auto bounds = getLocalBounds();
        viewport.setBounds(bounds);
        // Высота каждой строки (например, 40 пикселей)
        int rowHeight = 40;
        rowContainer->setSize(bounds.getWidth(), bankRows.size() * rowHeight);
        int y = 0;
        for (auto& row : bankRows)
        {
            row->setBounds(0, y, rowContainer->getWidth(), rowHeight);
            y += rowHeight;
        }
    }

    // Для внешнего доступа: получение банка по индексу
    const Bank& getBank(int index) const
    {
        jassert(index >= 0 && index < banks.size());
        return banks[index];
    }

    // Возвращает номер активного банка
    int getActiveBankIndex() const { return currentBankIndex; }

    // Позволяет установить активный банк извне (например, при переключении кнопками UP/DOWN)
    void setActiveBankIndex(int newIndex)
    {
        if (newIndex >= 0 && newIndex < banks.size() && newIndex != currentBankIndex)
        {
            currentBankIndex = newIndex;
            // Например, можно перейти к нужной строке во viewport (если окно прокручивается)
            // И вызывать callback для обновления внешнего интерфейса
            if (onBankManagerChanged != nullptr)
                onBankManagerChanged();
        }
    }

    // Callback, который уведомляет внешний интерфейс о том, что данные банков изменились.
    std::function<void()> onBankManagerChanged;

    // Можно добавить функции сохранения/загрузки банков в файл.
    void saveSettings()
    {
        // Определяем файл настроек в специальной директории для пользовательских данных  
        juce::File configFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("BankManagerSettings.ini");

        juce::PropertiesFile::Options options;
        options.applicationName = "MyBankManagerApp";
        juce::PropertiesFile propertiesFile(configFile, options);

        // Сохраняем данные для всех банков  
        for (int i = 0; i < banks.size(); ++i)
        {
            propertiesFile.setValue("bankName_" + juce::String(i), banks[i].bankName);
            for (int j = 0; j < banks[i].presetNames.size(); ++j)
            {
                propertiesFile.setValue("bank_" + juce::String(i) + "_preset_" + juce::String(j),
                    banks[i].presetNames[j]);
            }
        }

        propertiesFile.saveIfNeeded();
    }

    void loadSettings()
    {
        juce::File configFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("BankManagerSettings.ini");

        if (!configFile.existsAsFile())
            return; // Если файл не найден, оставляем значения по умолчанию

        juce::PropertiesFile::Options options;
        options.applicationName = "MyBankManagerApp";
        juce::PropertiesFile propertiesFile(configFile, options);

        // Считываем данные для всех банков  
        for (int i = 0; i < banks.size(); ++i)
        {
            banks[i].bankName = propertiesFile.getValue("bankName_" + juce::String(i), banks[i].bankName);
            for (int j = 0; j < banks[i].presetNames.size(); ++j)
            {
                banks[i].presetNames.set(j,
                    propertiesFile.getValue("bank_" + juce::String(i) + "_preset_" + juce::String(j),
                        banks[i].presetNames[j]));
            }
        }

        // После загрузки можно обновить отображение BankRows, если они уже созданы  
        for (int i = 0; i < bankRows.size(); ++i)
        {
            bankRows[i]->setVisible(true); // или вызвать setText() для каждого редактора
        }

        // Если у внешнего интерфейса установлен callback onBankManagerChanged, вызовите его:
        if (onBankManagerChanged != nullptr)
            onBankManagerChanged();
    }

private:
    std::vector<Bank> banks;  // 20 банков
    int currentBankIndex = 0; // активный банк (по умолчанию 0, то есть Bank 1)
    juce::OwnedArray<BankRow> bankRows;  // строки для отображения банков
    std::unique_ptr<juce::Component> rowContainer;
    juce::Viewport viewport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BankManager)
};
