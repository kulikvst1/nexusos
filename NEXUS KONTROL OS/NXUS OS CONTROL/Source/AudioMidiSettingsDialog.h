#pragma once

#include <JuceHeader.h>

class AudioMidiSettingsDialog : public juce::Component,
    private juce::Button::Listener
{
public:
    AudioMidiSettingsDialog(juce::AudioDeviceManager& adm)
        : deviceManager(adm),
        shouldSave(false)
    {
        // Селектор аудио-устройств
        deviceSelector.reset(new juce::AudioDeviceSelectorComponent(
            deviceManager, 0, 256, 0, 256,
            true, true, true, false));
        addAndMakeVisible(deviceSelector.get());

        // Кнопка OK
        okButton.setButtonText("OK");
        okButton.addListener(this);
        addAndMakeVisible(okButton);

        // Кнопка Cancel
        cancelButton.setButtonText("Cancel");
        cancelButton.addListener(this);
        addAndMakeVisible(cancelButton);

        setSize(600, 600);
    }

    ~AudioMidiSettingsDialog() override
    {
        // Сохраняем только при OK
        if (shouldSave)
        {
            if (auto xml = deviceManager.createStateXml())
            {
                auto file = juce::File::getSpecialLocation(
                    juce::File::userDocumentsDirectory)
                    .getChildFile("MyPluginAudioSettings.xml");
                xml->writeToFile(file, {});
            }
        }
    }

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
            shouldSave = true;

        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            // первый параметр — код возврата: 1 при OK, 0 при Cancel
            dw->exitModalState(shouldSave ? 1 : 0);
    }

    juce::AudioDeviceManager& deviceManager;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;
    juce::TextButton                                   okButton, cancelButton;
    bool                                               shouldSave;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioMidiSettingsDialog)
};
