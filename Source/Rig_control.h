#pragma once

#include <JuceHeader.h>

class Rig_control : public juce::Component,
    public juce::Button::Listener
{
public:
    Rig_control();
    ~Rig_control() override;

    void resized() override;
    void buttonClicked(juce::Button* button) override;

private:
    // Контейнер для всех элементов дизайна
    juce::Component mainTab;

    // Массивы кнопок для CC и пресетов
    juce::OwnedArray<juce::TextButton> ccButtons;
    juce::OwnedArray<juce::TextButton> presetButtons;

    // Метка для отображения названия банка
    juce::Label bankNameLabel;

    // Кнопки управления (SHIFT, TEMPO, UP, DOWN)
    std::unique_ptr<juce::TextButton> shiftButton, tempoButton, upButton, downButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Rig_control)
};
