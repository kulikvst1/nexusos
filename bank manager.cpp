/*
  ==============================================================================

    bank manager.cpp
    Created: 27 May 2025 1:32:49pm
    Author:  111

  ==============================================================================
*/

#include "bank manager.h"




void  BankManager::saveSettingsToFile(const juce::File& configFile)
{
    juce::PropertiesFile::Options options;
    options.applicationName = "MyBankManagerApp";
    juce::PropertiesFile propertiesFile(configFile, options);

    for (int i = 0; i < (int)banks.size(); i++)
    {
        // Записываем имя банка
        propertiesFile.setValue("bankName_" + juce::String(i), banks[i].bankName);

        for (int j = 0; j < banks[i].presetNames.size(); j++)
        {
            // Записываем имя пресета
            propertiesFile.setValue("bank_" + juce::String(i) + "_preset_" + juce::String(j),
                banks[i].presetNames[j]);

            // Записываем состояния CC (сериализуем в строку с разделителем запятых)
            juce::String ccStatesStr;
            for (int k = 0; k < (int)banks[i].ccPresetStates[j].size(); k++)
            {
                ccStatesStr += banks[i].ccPresetStates[j][k] ? "1" : "0";
                if (k < banks[i].ccPresetStates[j].size() - 1)
                    ccStatesStr += ",";
            }
            propertiesFile.setValue("bank_" + juce::String(i) + "_ccStates_" + juce::String(j), ccStatesStr);

            // Записываем уровень громкости для пресета
            propertiesFile.setValue("bank_" + juce::String(i) + "_volume_" + juce::String(j),
                banks[i].presetVolume[j]);
        }

        // Записываем CC mapping для 10 контроллеров
        juce::String mappingStr;
        for (int m = 0; m < (int)banks[i].ccMapping.size(); m++)
        {
            mappingStr += juce::String(banks[i].ccMapping[m]);
            if (m < banks[i].ccMapping.size() - 1)
                mappingStr += ",";
        }
        propertiesFile.setValue("bank_" + juce::String(i) + "_ccMapping", mappingStr);

        // Записываем номер плагин-пресета
        propertiesFile.setValue("bank_" + juce::String(i) + "_pluginPreset", banks[i].pluginPreset);
    }
    propertiesFile.saveIfNeeded();
}

void  BankManager::loadSettingsFromFile(const juce::File& configFile)
{
    if (!configFile.existsAsFile())
        return;

    juce::PropertiesFile::Options options;
    options.applicationName = "MyBankManagerApp";
    juce::PropertiesFile propertiesFile(configFile, options);

    for (int i = 0; i < (int)banks.size(); i++)
    {
        // Восстанавливаем имя банка
        banks[i].bankName = propertiesFile.getValue("bankName_" + juce::String(i), banks[i].bankName);

        for (int j = 0; j < banks[i].presetNames.size(); j++)
        {
            // Восстанавливаем имя пресета
            banks[i].presetNames.set(j,
                propertiesFile.getValue("bank_" + juce::String(i) + "_preset_" + juce::String(j),
                    banks[i].presetNames[j]));

            // Восстанавливаем состояния CC
            juce::String stateStr = propertiesFile.getValue("bank_" + juce::String(i) + "_ccStates_" + juce::String(j), "");
            if (stateStr.isNotEmpty())
            {
                juce::StringArray tokens;
                tokens.addTokens(stateStr, ",", "");
                for (int k = 0; k < juce::jmin(tokens.size(), (int)banks[i].ccPresetStates[j].size()); k++)
                {
                    banks[i].ccPresetStates[j][k] = (tokens[k].getIntValue() != 0);
                }
            }

            // Восстанавливаем уровень громкости для пресета
            banks[i].presetVolume[j] = propertiesFile.getIntValue("bank_" + juce::String(i) + "_volume_" + juce::String(j),
                banks[i].presetVolume[j]);
        }

        // Восстанавливаем CC mapping
        juce::String mappingStr = propertiesFile.getValue("bank_" + juce::String(i) + "_ccMapping", "");
        if (mappingStr.isNotEmpty())
        {
            juce::StringArray tokens;
            tokens.addTokens(mappingStr, ",", "");
            banks[i].ccMapping.resize(10);
            for (int m = 0; m < juce::jmin(tokens.size(), 10); m++)
                banks[i].ccMapping[m] = tokens[m].getIntValue();
        }

        // Восстанавливаем номер плагин-пресета
        banks[i].pluginPreset = propertiesFile.getIntValue("bank_" + juce::String(i) + "_pluginPreset", banks[i].pluginPreset);
    }

    // Обновляем визуальные компоненты (например, BankRow)
    for (int i = 0; i < bankRows.size(); i++)
    {
        bankRows[i]->updateFields(banks[i].bankName, banks[i].presetNames, juce::String(banks[i].pluginPreset));
        bankRows[i]->updateMappingEditors(banks[i].ccMapping);
    }

    if (onBankManagerChanged)
        onBankManagerChanged();

    updateRowHighlighting();
}

