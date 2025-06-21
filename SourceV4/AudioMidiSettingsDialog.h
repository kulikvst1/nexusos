#pragma once
#include <JuceHeader.h>

class AudioMidiSettingsDialog : public juce::Component
{
public:
    // Ïðèíèìàåì ññûëêó íà óæå èíèöèàëèçèðîâàííûé AudioDeviceManager
    AudioMidiSettingsDialog(juce::AudioDeviceManager& adm) : deviceManager(adm)
    {
        // Ñîçäàåì ñòàíäàðòíûé ñåëåêòîð äëÿ íàñòðîåê àóäèî (à òàêæå MIDI, åñëè ïîòðåáóåòñÿ)
        deviceSelector.reset(new juce::AudioDeviceSelectorComponent(deviceManager,
            0, 256,   // ìèíèìàëüíîå è ìàêñèìàëüíîå ÷èñëî âõîäíûõ êàíàëîâ
            0, 256,   // ìèíèìàëüíîå è ìàêñèìàëüíîå ÷èñëî âûõîäíûõ êàíàëîâ
            true,     // ïîêàçûâàòü íàñòðîéêè àóäèî
            true,     // ïîêàçûâàòü íàñòðîéêè MIDI
            true,     // ïîêàçûâàòü íàñòðîéêè áóôåðà è sample rate
            false));  // íå ïîêàçûâàòü ðàñøèðåííûå íàñòðîéêè
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
