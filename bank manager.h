#pragma once

#include <JuceHeader.h>
#include <vector>
#include <functional>

//==============================================================================
// ��������� BankRow ������������ ���� ������ ��� �����.
// ����� ������������� ��������� �������� ��� ����� ����� � 6 ���������� ��� ��� ��������.
class BankRow : public juce::Component,
    public juce::TextEditor::Listener
{
public:
    BankRow(int bankIndex, const juce::String& defaultBankName, const juce::StringArray& defaultPresetNames)
        : index(bankIndex)
    {
        // �������� ��� ����� �����
        bankNameEditor.setText(defaultBankName);
        bankNameEditor.addListener(this);
        bankNameEditor.setJustification(juce::Justification::centredLeft);
        addAndMakeVisible(bankNameEditor);

        // ������� 6 ��������� ���������� ��� ���� ��������
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
        // ��������� �������� ����� ����� ������������� ������, � ��������� ��������� ����������
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

    // ��� ��������� ������ � ����� ��������� �������� callback
    void textEditorTextChanged(juce::TextEditor& editor) override
    {
        if (onRowChanged != nullptr)
            onRowChanged(index);
        

    }
    void textEditorReturnKeyPressed(juce::TextEditor& editor) override {}
    void textEditorEscapeKeyPressed(juce::TextEditor& editor) override {}
    void textEditorFocusLost(juce::TextEditor& editor) override {}

    // ���� callback (��������������� ������������ �����������) ���������� � ���, ��� ���������� ����� ����������.
    std::function<void(int bankIndex)> onRowChanged;

private:
    int index{ 0 };
    juce::TextEditor bankNameEditor;
    juce::OwnedArray<juce::TextEditor> presetEditors;
};

//==============================================================================
// ��������� BankManager ������ 20 ������ � ����� ���������� �� ��� � ���� ������.
// ������ ���� �������� � ��������� Bank, � ��� ������� ����� ��������� BankRow, ������������� �����������.
// ������� ��������� ����� ����������� �� onBankManagerChanged, ����� �������� ������� ��������� (��������, ����� BANK NAME).
class BankManager : public juce::Component
{
public:
    // ���������, ����������� ����
    struct Bank
    {
        juce::String bankName;
        juce::StringArray presetNames;  // 6 ���� ��������

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
        // ������� 20 ������ �� ���������
        banks.resize(15);
        for (int i = 0; i < (int)banks.size(); i++)
        {
            banks[i].bankName = "Bank " + juce::String(i + 1);
            banks[i].presetNames.clear();
            for (int j = 0; j < 6; j++)
                banks[i].presetNames.add("Preset " + juce::String(static_cast<char>('A' + j)));
        }

        // �� ��������� ������� ���� 1 (������ 0)
        currentBankIndex = 0;

        // ������� ��������� ��� ���� ����� ������
        rowContainer = std::make_unique<juce::Component>();

        // ��� ������� ����� ������� BankRow � ��������� ��� � ���������
        for (int i = 0; i < banks.size(); i++)
        {
            auto row = std::make_unique<BankRow>(i, banks[i].bankName, banks[i].presetNames);
            // ��� ��������� ������ ��������� �������� ����� � ������� banks
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

        // ������� �������������� ��������� (Viewport) ��� ������ ������
        addAndMakeVisible(viewport);
        viewport.setViewedComponent(rowContainer.get(), false);
    }

    ~BankManager() override {}

    void resized() override
    {
        auto bounds = getLocalBounds();
        viewport.setBounds(bounds);
        // ������ ������ ������ (��������, 40 ��������)
        int rowHeight = 40;
        rowContainer->setSize(bounds.getWidth(), bankRows.size() * rowHeight);
        int y = 0;
        for (auto& row : bankRows)
        {
            row->setBounds(0, y, rowContainer->getWidth(), rowHeight);
            y += rowHeight;
        }
    }

    // ��� �������� �������: ��������� ����� �� �������
    const Bank& getBank(int index) const
    {
        jassert(index >= 0 && index < banks.size());
        return banks[index];
    }

    // ���������� ����� ��������� �����
    int getActiveBankIndex() const { return currentBankIndex; }

    // ��������� ���������� �������� ���� ����� (��������, ��� ������������ �������� UP/DOWN)
    void setActiveBankIndex(int newIndex)
    {
        if (newIndex >= 0 && newIndex < banks.size() && newIndex != currentBankIndex)
        {
            currentBankIndex = newIndex;
            // ��������, ����� ������� � ������ ������ �� viewport (���� ���� ��������������)
            // � �������� callback ��� ���������� �������� ����������
            if (onBankManagerChanged != nullptr)
                onBankManagerChanged();
        }
    }

    // Callback, ������� ���������� ������� ��������� � ���, ��� ������ ������ ����������.
    std::function<void()> onBankManagerChanged;

    // ����� �������� ������� ����������/�������� ������ � ����.
    void saveSettings()
    {
        // ���������� ���� �������� � ����������� ���������� ��� ���������������� ������  
        juce::File configFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("BankManagerSettings.ini");

        juce::PropertiesFile::Options options;
        options.applicationName = "MyBankManagerApp";
        juce::PropertiesFile propertiesFile(configFile, options);

        // ��������� ������ ��� ���� ������  
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
            return; // ���� ���� �� ������, ��������� �������� �� ���������

        juce::PropertiesFile::Options options;
        options.applicationName = "MyBankManagerApp";
        juce::PropertiesFile propertiesFile(configFile, options);

        // ��������� ������ ��� ���� ������  
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

        // ����� �������� ����� �������� ����������� BankRows, ���� ��� ��� �������  
        for (int i = 0; i < bankRows.size(); ++i)
        {
            bankRows[i]->setVisible(true); // ��� ������� setText() ��� ������� ���������
        }

        // ���� � �������� ���������� ���������� callback onBankManagerChanged, �������� ���:
        if (onBankManagerChanged != nullptr)
            onBankManagerChanged();
    }

private:
    std::vector<Bank> banks;  // 20 ������
    int currentBankIndex = 0; // �������� ���� (�� ��������� 0, �� ���� Bank 1)
    juce::OwnedArray<BankRow> bankRows;  // ������ ��� ����������� ������
    std::unique_ptr<juce::Component> rowContainer;
    juce::Viewport viewport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BankManager)
};
