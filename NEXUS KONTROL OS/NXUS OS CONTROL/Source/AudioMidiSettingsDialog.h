#pragma once
#include <JuceHeader.h>

class AudioMidiSettingsDialog : public juce::Component,
    private juce::Button::Listener
{
public:
    AudioMidiSettingsDialog(juce::AudioDeviceManager& adm)
        : deviceManager(adm)
    {
        deviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent>(
            deviceManager,
            0, 256,   // min/max inputs
            0, 256,   // min/max outputs
            true,     // show MIDI inputs
            true,     // show MIDI outputs
            false,    // stereo pairs
            false     // advanced options
        );
        addAndMakeVisible(deviceSelector.get());

        okButton.setButtonText("OK");
        okButton.addListener(this);
        addAndMakeVisible(okButton);

        cancelButton.setButtonText("Cancel");
        cancelButton.addListener(this);
        addAndMakeVisible(cancelButton);

        setSize(600, 600);
    }

    ~AudioMidiSettingsDialog() override = default;

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        auto buttons = area.removeFromBottom(32);

        cancelButton.setBounds(buttons.removeFromRight(100).reduced(4));
        okButton.setBounds(buttons.removeFromRight(100).reduced(4));
        deviceSelector->setBounds(area);
    }

private:
    void buttonClicked(juce::Button* btn) override
    {
        if (btn == &okButton)
        {
            saveSettings(); // сохраняем явно при OK
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState(1);
        }
        else if (btn == &cancelButton)
        {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState(0);
        }
    }

    void saveSettings()
    {
        if (auto xml = deviceManager.createStateXml())
        {
            auto settingsFile = juce::File::getSpecialLocation(
                juce::File::userApplicationDataDirectory) // ✅ кроссплатформенный путь
                .getChildFile("NEXUS_OS_AUDIO_SET")
                .getChildFile("AudioSettings.xml");

            settingsFile.getParentDirectory().createDirectory(); // гарантируем, что папка есть
            xml->writeTo(settingsFile);
        }
    }

    juce::AudioDeviceManager& deviceManager;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;
    juce::TextButton okButton, cancelButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioMidiSettingsDialog)
};
