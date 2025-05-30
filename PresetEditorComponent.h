#pragma once

#include <JuceHeader.h>
#include <map>

// Компонент для редактирования пресета плагина.
// Для каждого параметра создаются слайдер, метка и чекбокс.
// В нижней части располагаются две кнопки: Save Preset и Cancel.
class PresetEditorComponent : public juce::Component,
    public juce::Slider::Listener,
    public juce::Button::Listener
{
public:
    // Callback, вызываемый при нажатии кнопки Save Preset.
    // Передаются номер пресета и карта сохранённых значений (индекс параметра → значение).
    std::function<void(int, const std::map<int, float>&)> onPresetSaved;

    PresetEditorComponent(juce::AudioPluginInstance* plugin, int presetIndex)
        : pluginInstance(plugin), presetIndex(presetIndex)
    {
        if (pluginInstance != nullptr)
        {
            auto& params = pluginInstance->getParameters();
            const int numParams = params.size();

            // Для отладки можно вывести число параметров:
            DBG("PresetEditorComponent: numParams = " << numParams);

            // Кэшируем имена параметров.
            for (int i = 0; i < numParams; ++i)
            {
                juce::String name = params[i]->getName(100).trim();
                if (name.isEmpty())
                    name = "Param " + juce::String(i);
                parameterNames.add(name);
            }

            // Создаем элементы управления для каждого параметра.
            for (int i = 0; i < numParams; ++i)
            {
                // Слайдер.
                auto* slider = new juce::Slider();
                slider->setRange(0.0, 1.0, 0.001);
                slider->setValue(params[i]->getValue(), juce::dontSendNotification);
                slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
                slider->addListener(this);
                addAndMakeVisible(slider);
                parameterSliders.add(slider);

                // Метка.
                auto* label = new juce::Label();
                label->setText(parameterNames[i], juce::dontSendNotification);
                addAndMakeVisible(label);
                parameterLabels.add(label);

                // Чекбокс для выбора сохранения данного параметра.
                auto* checkBox = new juce::ToggleButton("Save");
                checkBox->setToggleState(false, juce::dontSendNotification);
                addAndMakeVisible(checkBox);
                parameterCheckBoxes.add(checkBox);
            }

            // Создаем кнопки "Save Preset" и "Cancel".
            saveButton.setButtonText("Save Preset");
            saveButton.addListener(this);
            addAndMakeVisible(saveButton);

            cancelButton.setButtonText("Cancel");
            cancelButton.addListener(this);
            addAndMakeVisible(cancelButton);

            // Устанавливаем размер компонента: ширина фиксирована (500px),
            // высота = (число параметров * 40) + 70 пикселей для кнопок.
            setSize(500, numParams * 40 + 70);
        }
    }

    ~PresetEditorComponent() override {}

    void sliderValueChanged(juce::Slider* slider) override
    {
        auto& params = pluginInstance->getParameters();
        for (int i = 0; i < parameterSliders.size(); ++i)
        {
            if (parameterSliders[i] == slider)
            {
                if (parameterCheckBoxes[i]->getToggleState())
                {
                    float newValue = static_cast<float>(slider->getValue());
                    pluginInstance->setParameterNotifyingHost(i, newValue);
                    DBG("Setting parameter " << i << " to " << newValue);
                }
                break;
            }
        }
    }

    void buttonClicked(juce::Button* button) override
    {
        if (button == &saveButton)
        {
            savedPreset.clear();
            for (int i = 0; i < parameterSliders.size(); ++i)
            {
                if (parameterCheckBoxes[i]->getToggleState())
                {
                    float value = static_cast<float>(parameterSliders[i]->getValue());
                    savedPreset[i] = value;
                    DBG("Saved parameter " << i << " value " << value << " for preset " << presetIndex);
                }
            }
            if (onPresetSaved)
                onPresetSaved(presetIndex, savedPreset);
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState(0);
        }
        else if (button == &cancelButton)
        {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState(0);
        }
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        // Предполагаем фиксированную высоту для каждого ряда параметра: 40 пикселей,
        // и фиксированную высоту для области кнопок: 70 пикселей.
        auto bounds = getLocalBounds(); // Например, (0, 0, 500, 190) при 3 параметрах.
        int totalHeight = bounds.getHeight();
        int buttonHeight = 70;
        int paramAreaHeight = totalHeight - buttonHeight; // Ожидается paramAreaHeight = numParams * 40.
        auto buttonArea = bounds.removeFromBottom(buttonHeight);
        auto paramArea = bounds; // Теперь paramArea имеет высоту, равную numParams*40.

        // Для каждого ряда задаем высоту 40 пикселей.
        for (int i = 0; i < parameterSliders.size(); ++i)
        {
            // Если у вас задано фиксированное значение 40 px, то
            auto row = paramArea.removeFromTop(40).reduced(0, 2);
            // Метка занимает 120 пикселей слева.
            parameterLabels[i]->setBounds(row.removeFromLeft(120));
            // Чекбокс – справа, 60 пикселей.
            parameterCheckBoxes[i]->setBounds(row.removeFromRight(60));
            // Остаток отдан слайдеру.
            parameterSliders[i]->setBounds(row);
        }
        saveButton.setBounds(buttonArea.removeFromLeft(buttonArea.getWidth() / 2 - 5).reduced(4));
        cancelButton.setBounds(buttonArea.reduced(4));
    }

    const std::map<int, float>& getSavedPreset() const { return savedPreset; }

private:
    juce::AudioPluginInstance* pluginInstance = nullptr;
    int presetIndex = 0;

    juce::OwnedArray<juce::Slider>       parameterSliders;
    juce::OwnedArray<juce::Label>        parameterLabels;
    juce::OwnedArray<juce::ToggleButton> parameterCheckBoxes;
    juce::TextButton                     saveButton, cancelButton;

    juce::StringArray parameterNames;
    std::map<int, float> savedPreset;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetEditorComponent)
};
