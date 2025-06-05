#pragma once
#include <JuceHeader.h>
#include "vst_host.h"        // для VSTHostComponent, ParameterInfo
#include "bank manager.h"    // для Bank, CCMapArray
//#include "CCDialog.h"
// ——————————————————————————————————————————————————————————————————————————————
// 1. CCMappingRow — одна строка: выбор параметра, слайдер 0–127, чек-бокс On/Off
// ——————————————————————————————————————————————————————————————————————————————
class CCMappingRow : public juce::Component,
    private juce::ComboBox::Listener,
    private juce::Slider::Listener,
    private juce::Button::Listener
{
public:
    // params   — список всех параметров из плагина
    // initial  — текущее состояние (Bank.ccMappings[i])
    // slotIdx  — от 0 до 9
    CCMappingRow(const std::vector<ParameterInfo>& params,
        CCMapping                         initial,
        int                               slotIdx);

    // отдаёт в CCDialog обновлённый mapping
    CCMapping getMapping() const;
    // сбрасывает mapping в дефолт
    void setMapping(CCMapping);

    void resized() override;

private:
    void comboBoxChanged(juce::ComboBox*) override;
    void sliderValueChanged(juce::Slider*) override;
    void buttonClicked(juce::Button*) override;

    void updateUI(); // применить mapping → ComboBox/Slider/Toggle

    const std::vector<ParameterInfo>& parameters;
    CCMapping   mapping;
    int         slotIndex;

    juce::Label        label;
    juce::ComboBox     combo;
    juce::Slider       slider;
    juce::ToggleButton toggle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CCMappingRow)
};

// ——————————————————————————————————————————————————————————————————————————————
// 2. CCDialog — окно с 10 строками CCMappingRow + кнопки Save/Default
// ——————————————————————————————————————————————————————————————————————————————
class CCDialog : public juce::Component,
    private juce::Button::Listener
{
public:
    /** ctor
        host          — твой VSTHostComponent, чтобы запросить список params
        initialMap    — массив из 10 CCMapping (из Bank.ccMappings)
        onSave        — callback, отдаёт новый CCMapArray в твой BankManager
    */
    CCDialog(VSTHostComponent* host,
        CCMapArray                   initialMap,
        std::function<void(CCMapArray)> onSave);

    ~CCDialog() override = default;
    void resized() override;

private:
    // Button::Listener
    void buttonClicked(juce::Button*) override;

    void applyAndClose();     // Save → вызов onSave + exitModal
    void resetToDefaults();   // Default → сброс всех строк в {-1,false,64}

    VSTHostComponent* hostComponent;
    std::vector<ParameterInfo>          parameters;
    CCMapArray                          mapping;      // локальная копия
    juce::OwnedArray<CCMappingRow>      rows;         // 10 строк

    juce::TextButton                    saveButton, resetButton;
    std::function<void(CCMapArray)>     onSaveCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CCDialog)
};
