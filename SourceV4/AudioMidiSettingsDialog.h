#pragma once
#include <JuceHeader.h>

/*
    ����� AudioMidiSettingsDialog ������������� ��������� ��� ��������� ����� � MIDI,
    ��������� ����������� AudioDeviceSelectorComponent. ��� �������� ������� ���������� ���������� ��������.
*/
class AudioMidiSettingsDialog : public juce::Component
{
public:
    AudioMidiSettingsDialog(juce::AudioDeviceManager& dm)
        : deviceManager(dm),
        audioSelector(dm,
            /* minInputChannels  */ 0,
            /* maxInputChannels  */ 2,
            /* minOutputChannels */ 0,
            /* maxOutputChannels */ 2,
            /* selectDefaultDeviceOnFailure */ true,
            /* showAdvancedOptions */ true,
            /* includeMidiInputs */ true,
            /* includeMidiOutputs */ true)
    {
        addAndMakeVisible(audioSelector);
        setSize(600, 400);
    }

    ~AudioMidiSettingsDialog() override
    {
        saveSettings();
    }

    void resized() override
    {
        audioSelector.setBounds(getLocalBounds());
    }

private:
    void saveSettings()
    {
        // �������� ������� ��������� �������� �����/ MIDI � ���� XML
        std::unique_ptr<juce::XmlElement> state(deviceManager.createStateXml());
        if (state != nullptr)
        {
            // ��������� ��������� � ����� � �������� �������� ������������
            juce::File configFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile("NEXUS_OS_AudioMidiSettings.xml");
            state->writeToFile(configFile, {});
        }
    }

    juce::AudioDeviceManager& deviceManager;
    juce::AudioDeviceSelectorComponent audioSelector;
};
