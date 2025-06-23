#pragma once
#include <JuceHeader.h>

class AudioMidiSettingsDialog : public juce::Component
{
public:
  
    AudioMidiSettingsDialog(juce::AudioDeviceManager& adm) : deviceManager(adm)
    {
       
        deviceSelector.reset(new juce::AudioDeviceSelectorComponent(deviceManager,
            0, 256,   // ìèíèìàëüíîå è ìàêñèìàëüíîå ÷èñëî âõîäíûõ êàíàëîâ
            0, 256,   // ìèíèìàëüíîå è ìàêñèìàëüíîå ÷èñëî âûõîäíûõ êàíàëîâ
            true,     // ïîêàçûâàòü íàñòðîéêè àóäèî
            true,    
            true,     
            false));  
        addAndMakeVisible(deviceSelector.get());
        
        setSize(600, 400);
    }
    
    void resized() override
    {
        deviceSelector->setBounds(getLocalBounds());
    }
    
private:
    juce::AudioDeviceManager& deviceManager;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;
};
