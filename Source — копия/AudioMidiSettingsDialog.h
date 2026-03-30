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
        okButton.setButtonText(juce::String::fromUTF8("✔️ OK"));
        okButton.addListener(this);
        addAndMakeVisible(okButton);
        cancelButton.setButtonText(juce::String::fromUTF8("❌ Close"));
        cancelButton.addListener(this);
        addAndMakeVisible(cancelButton);
        setSize(600, 600);
    }

    ~AudioMidiSettingsDialog() override = default;

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);

        // область под кнопки
        auto buttonsRow = area.removeFromBottom(60); // увеличили высоту строки

        int btnWidth = 80;   // немного меньше по ширине
        int btnHeight = 40;  // немного больше по высоте
        int gap = 20;        // расстояние между кнопками

        int totalWidth = btnWidth * 2 + gap; // ширина ряда из двух кнопок
        auto centeredRow = buttonsRow.withSizeKeepingCentre(totalWidth, btnHeight);

        // Cancel слева
        cancelButton.setBounds(centeredRow.removeFromLeft(btnWidth).reduced(4).withHeight(btnHeight));

        centeredRow.removeFromLeft(gap);

        // OK справа
        okButton.setBounds(centeredRow.removeFromLeft(btnWidth).reduced(4).withHeight(btnHeight));

        // селектор занимает оставшуюся область
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
                juce::File::userApplicationDataDirectory)
                .getChildFile("NEXUS_KONTROL_OS")
                .getChildFile("AudioSettings.xml");

            settingsFile.getParentDirectory().createDirectory();

            // 🔹 добавляем bounds окна
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            {
                xml->setAttribute("DialogBounds", dw->getBounds().toString());
            }

            xml->writeTo(settingsFile);
        }
    }

    juce::AudioDeviceManager& deviceManager;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;
    juce::TextButton okButton, cancelButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioMidiSettingsDialog)
};
