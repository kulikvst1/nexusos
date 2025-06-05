#include "CCDialog.h"

//==============================================================================
// CCMappingRow
//==============================================================================
CCMappingRow::CCMappingRow(const std::vector<ParameterInfo>& params,
    CCMapping                         initial,
    int                               slotIdx)
    : parameters(params),
    mapping(initial),
    slotIndex(slotIdx)
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

void CCMappingRow::resized()
{
    auto r = getLocalBounds().reduced(4);
    label.setBounds(r.removeFromLeft(50));
    combo.setBounds(r.removeFromLeft(150));
    slider.setBounds(r.removeFromLeft(100));
    toggle.setBounds(r);
}

CCMapping CCMappingRow::getMapping() const { return mapping; }
void      CCMappingRow::setMapping(CCMapping m) { mapping = m; updateUI(); }

void CCMappingRow::comboBoxChanged(juce::ComboBox*)
{
    int id = combo.getSelectedId();
    mapping.paramIndex = (id <= 1 ? -1 : id - 2);
}

void CCMappingRow::sliderValueChanged(juce::Slider*)
{
    mapping.ccValue = static_cast<uint8_t> (slider.getValue());
}

void CCMappingRow::buttonClicked(juce::Button*)
{
    mapping.enabled = toggle.getToggleState();
}

void CCMappingRow::updateUI()
{
    // Combo
    if (mapping.paramIndex < 0)
        combo.setSelectedId(1, juce::dontSendNotification);
    else
        combo.setSelectedId(mapping.paramIndex + 2, juce::dontSendNotification);

    slider.setValue(mapping.ccValue, juce::dontSendNotification);
    toggle.setToggleState(mapping.enabled, juce::dontSendNotification);
}

//==============================================================================
// CCDialog
//==============================================================================
CCDialog::CCDialog(VSTHostComponent* host,
    CCMapArray                   initialMap,
    std::function<void(CCMapArray)> onSave)
    : hostComponent(host),
    parameters(host->getCurrentPluginParameters()),
    mapping(initialMap),
    onSaveCallback(std::move(onSave))
{
    // создаём 10 строк
    for (int i = 0; i < 10; ++i)
    {
        auto* row = new CCMappingRow(parameters, mapping[i], i);
        rows.add(row);
        addAndMakeVisible(row);
    }

    // Save / Default
    saveButton.setButtonText("Save");
    resetButton.setButtonText("Default");
    saveButton.addListener(this);
    resetButton.addListener(this);
    addAndMakeVisible(saveButton);
    addAndMakeVisible(resetButton);

    setSize(450, 10 * 40 + 60);
}

void CCDialog::resized()
{
    auto area = getLocalBounds();
    for (int i = 0; i < rows.size(); ++i)
        rows[i]->setBounds(area.removeFromTop(40));

    auto btns = area.removeFromBottom(40).reduced(10);
    saveButton.setBounds(btns.removeFromRight(btns.getWidth() / 2));
    resetButton.setBounds(btns);
}

void CCDialog::buttonClicked(juce::Button* b)
{
    if (b == &saveButton)      applyAndClose();
    else if (b == &resetButton) resetToDefaults();
}

void CCDialog::applyAndClose()
{
    // собираем из строк новый массив
    for (int i = 0; i < rows.size(); ++i)
        mapping[i] = rows[i]->getMapping();

    // отдадим банку новые настройки
    onSaveCallback(mapping);

    // закроем окно
    if (auto* w = findParentComponentOfClass<juce::DialogWindow>())
        w->exitModalState(0);
}

void CCDialog::resetToDefaults()
{
    CCMapping def{ -1, false, 64 };
    for (auto* r : rows)
        r->setMapping(def);
}
