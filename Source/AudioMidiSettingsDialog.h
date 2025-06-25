#pragma once
#include <JuceHeader.h>

class AudioMidiSettingsDialog : public juce::Component
{
public:
    AudioMidiSettingsDialog(juce::AudioDeviceManager& adm)
        : deviceManager(adm)
    {
        deviceSelector.reset(new juce::AudioDeviceSelectorComponent(
            deviceManager,
            0, 256,    // min/max inputs
            0, 256,    // min/max outputs
            true,      // show input selector
            true,      // show output selector
            true,      // show stereo pairs
            false));   // hide advanced options

        addAndMakeVisible(deviceSelector.get());
        setSize(600, 400);
    }

    ~AudioMidiSettingsDialog() override
    {
        // вызывается при закрытии окна и удалении диалога
        if (auto xml = deviceManager.createStateXml())
        {
            auto f = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                .getChildFile("MyPluginAudioSettings.xml");
            xml->writeTo(f, {});
        }
    }

    void resized() override
    {
        deviceSelector->setBounds(getLocalBounds());
    }

private:
    juce::AudioDeviceManager& deviceManager;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioMidiSettingsDialog)
};
