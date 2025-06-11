#pragma once
#include <JuceHeader.h>
#include "vst_host.h"        // для VSTHostComponent, ParameterInfo
#include "bank manager.h"    // для Bank, CCMapArray
#include <functional>

//------------------------------------------------------------------------------
// 1. CCMappingRow — одна строка: выбор параметра, слайдер 0–127, чек-бокс On/Off
//------------------------------------------------------------------------------
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
        int                               slotIdx)
        : parameters(params), mapping(initial), slotIndex(slotIdx)
    {
        // Label “CC 1”, … “CC 10”
        label.setText("CC " + juce::String(slotIndex + 1), juce::dontSendNotification);
        addAndMakeVisible(label);

        // ComboBox: пункт “– none –” с id=1, далее параметры с id=i+2
        combo.addItem("– none –", 1);
        for (int i = 0; i < parameters.size(); ++i)
            combo.addItem(parameters[i].name, i + 2);

        combo.addListener(this);
        addAndMakeVisible(combo);

        // Slider 0..127
        slider.setRange(0, 127, 1);
        slider.addListener(this);
        addAndMakeVisible(slider);

        // Toggle On/Off
        toggle.setButtonText("On");
        toggle.addListener(this);
        addAndMakeVisible(toggle);

        updateUI();
    }

    // Отдаёт в CCDialog обновлённый mapping
    CCMapping getMapping() const { return mapping; }
    // Сбрасывает mapping в дефолт
    void setMapping(CCMapping m) { mapping = m; updateUI(); }

    void resized() override
    {
        auto r = getLocalBounds().reduced(4);
        label.setBounds(r.removeFromLeft(50));
        combo.setBounds(r.removeFromLeft(150));
        slider.setBounds(r.removeFromLeft(100));
        toggle.setBounds(r);
    }

private:
    void comboBoxChanged(juce::ComboBox*) override
    {
        int id = combo.getSelectedId();
        mapping.paramIndex = (id <= 1 ? -1 : id - 2);
    }
    void sliderValueChanged(juce::Slider*) override
    {
        mapping.ccValue = static_cast<uint8_t>(slider.getValue());
    }
    void buttonClicked(juce::Button*) override
    {
        mapping.enabled = toggle.getToggleState();
    }

    void updateUI()
    {
        // ComboBox
        if (mapping.paramIndex < 0)
            combo.setSelectedId(1, juce::dontSendNotification);
        else
            combo.setSelectedId(mapping.paramIndex + 2, juce::dontSendNotification);
        slider.setValue(mapping.ccValue, juce::dontSendNotification);
        toggle.setToggleState(mapping.enabled, juce::dontSendNotification);
    }

    const std::vector<ParameterInfo>& parameters;
    CCMapping mapping;
    int slotIndex;

    juce::Label    label;
    juce::ComboBox combo;
    juce::Slider   slider;
    juce::ToggleButton toggle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CCMappingRow)
};

//------------------------------------------------------------------------------
// 2. CCDialog — компонент с 10 строками CCMappingRow + кнопки Save/Default
// (переделанный из модального окна в обычный компонент для использования во вкладке)
//------------------------------------------------------------------------------
class CCDialog : public juce::Component,
    private juce::Button::Listener
{
public:
    /**
        ctor
        host          — указатель на VSTHostComponent (для запроса списка параметров)
        initialMap    — массив из 10 CCMapping (из Bank.ccMappings)
        onSave        — callback, отдаёт новый CCMapArray в BankManager
    */
    CCDialog(VSTHostComponent* host,
        CCMapArray                   initialMap,
        std::function<void(CCMapArray)> onSave)
        : hostComponent(host),
        parameters(host->getCurrentPluginParameters()),
        mapping(initialMap),
        onSaveCallback(std::move(onSave))
    {
        // Создаём 10 строк
        for (int i = 0; i < 10; ++i)
        {
            auto* row = new CCMappingRow(parameters, mapping[i], i);
            rows.add(row);
            addAndMakeVisible(row);
        }

        // Кнопки Save и Default
        saveButton.setButtonText("Save");
        resetButton.setButtonText("Default");
        saveButton.addListener(this);
        resetButton.addListener(this);
        addAndMakeVisible(saveButton);
        addAndMakeVisible(resetButton);

        // Устанавливаем фиксированный размер компонента
        setSize(450, 10 * 40 + 60);
    }

    ~CCDialog() override = default;

    void resized() override
    {
        auto area = getLocalBounds();
        for (int i = 0; i < rows.size(); ++i)
            rows[i]->setBounds(area.removeFromTop(40));

        auto btns = area.removeFromBottom(40).reduced(10);
        saveButton.setBounds(btns.removeFromRight(btns.getWidth() / 2));
        resetButton.setBounds(btns);
    }

private:
    // Button::Listener
    void buttonClicked(juce::Button* b) override
    {
        if (b == &saveButton)
            applyAndNotify();
        else if (b == &resetButton)
            resetToDefaults();
    }

    // Собираем данные из строк и вызываем callback onSave
    void applyAndNotify()
    {
        for (int i = 0; i < rows.size(); ++i)
            mapping[i] = rows[i]->getMapping();

        if (onSaveCallback)
            onSaveCallback(mapping);
    }

    // Сбрасываем все маппинги к дефолтным настройкам: paramIndex = -1, enabled = false, ccValue = 64
    void resetToDefaults()
    {
        CCMapping def{ -1, false, 64 };
        for (auto* r : rows)
            r->setMapping(def);
    }

    VSTHostComponent* hostComponent = nullptr;
    std::vector<ParameterInfo> parameters;
    CCMapArray mapping;
    juce::OwnedArray<CCMappingRow> rows; // 10 строк
    juce::TextButton saveButton, resetButton;
    std::function<void(CCMapArray)> onSaveCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CCDialog)
};
