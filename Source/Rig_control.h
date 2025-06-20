#include <JuceHeader.h>
#include"fount_label.h"
#include <functional> // для std::ref
#include <vector>
#include <string>
#include "custom_audio_playhead.h"

class Rig_control : public juce::Component,
    public juce::Button::Listener,
    public juce::Slider::Listener,
    public juce::MidiInputCallback,
    public juce::Timer
{
public:
    Rig_control();
    ~Rig_control() override;

    // Переопределённые методы JUCE компонентов
    void resized() override;
    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;
    void timerCallback() override;

private:
    // Контейнер для элементов интерфейса
    std::unique_ptr<juce::Component> mainTab;

    // Кнопки-пресеты
    juce::OwnedArray<juce::TextButton> presetButtons;

    // Метка с названием банка
    juce::Label bankNameLabel;

    // Кнопки: SHIFT, TEMPO, UP, DOWN
    std::unique_ptr<juce::TextButton> shiftButton, tempoButton, upButton, downButton;

    // Rotary‑слайдеры и их метки
    std::unique_ptr<juce::Slider> gainSlider, volumeSlider;
    juce::Label gainLabel, volumeLabel;

    // Объект кастомного LookAndFeel
    CustomLookAndFeel customLF;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Rig_control)
};
