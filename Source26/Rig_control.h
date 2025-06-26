#include <JuceHeader.h>
#include"fount_label.h"
#include <functional> // для std::ref
#include <vector>
#include <string>
#include "custom_audio_playhead.h"
#include "tap_tempo.h" 
#include"vst_host.h"

// Предварительное объявление BankEditor
class BankEditor;

class Rig_control : public juce::Component,
    public juce::Button::Listener,
    public juce::Slider::Listener,
    public juce::MidiInputCallback,
    public juce::Timer
{
public:
    Rig_control();
    ~Rig_control() override;

    void resized() override;
    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;
    void timerCallback() override;

    void setVstHostComponent(VSTHostComponent* host) { hostComponent = host; }

    // Новая функция для установки BankEditor
    void setBankEditor(BankEditor* editor);

    // Метод для обновления имен пресетов на кнопках и метках
    void updatePresetDisplays();

private:
    std::unique_ptr<juce::Component> mainTab;
    juce::OwnedArray<juce::TextButton> presetButtons;
    juce::Label bankNameLabel;
    std::unique_ptr<juce::TextButton> shiftButton, tempoButton, upButton, downButton;
    std::unique_ptr<juce::Slider> gainSlider, volumeSlider;
    juce::Label gainLabel, volumeLabel;

    TapTempo tapTempo;

    CustomLookAndFeel   custom;
    CustomLookButon   presetLF;
    juce::LookAndFeel_V4 customLF;
    VSTHostComponent* hostComponent = nullptr;

    // Указатель на BankEditor для получения списка имён пресетов
    BankEditor* bankEditor = nullptr;

    // Новые метки для пресетов (отображают альтернативную группу)
    juce::Label presetLabel1_4, presetLabel2_5, presetLabel3_6;

    

   

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Rig_control)
};
