#include "MidiStartupShutdown.h"
#include <JuceHeader.h>

void MidiStartupShutdown::sendStartupCommands()
{
    // 1. »мпеданс
    sendImpedanceFromSettings();

    // 2. ќстальные стартовые команды
    sendOtherStartupCommands();
}

void MidiStartupShutdown::sendShutdownCommands()
{
    // 1. Ћюбые команды на выключение/сброс
    sendOtherShutdownCommands();
}

void MidiStartupShutdown::sendImpedanceFromSettings()
{
    juce::File settingsFile = juce::File::getSpecialLocation(
        juce::File::userApplicationDataDirectory
    ).getChildFile("InputControlSettings.xml");

    if (settingsFile.existsAsFile())
    {
        if (auto xml = juce::XmlDocument::parse(settingsFile))
        {
            if (xml->hasTagName("InputControl"))
            {
                int activeCC = xml->getIntAttribute("activeImpedanceCC", -1);
                if (activeCC != -1)
                {
                    rigControl.sendImpedanceCC(activeCC, true);
                }
            }
        }
    }
}

void MidiStartupShutdown::sendOtherStartupCommands()
{
    int delay = 0;

    // ѕерва€ команда Ч сразу
    rigControl.sendMidiCC(1, 100, 0); // reset stomp mode

    // ¬тора€ Ч через 10 мс
    delay += 10;
    juce::Timer::callAfterDelay(delay, [this]
        {
            rigControl.sendMidiCC(1, 101, 0); // reset shift mode
        });

    // —брос лупера Ч ещЄ через 10 мс
    delay += 10;
    juce::Timer::callAfterDelay(delay, [this]
        {
            rigControl.sendMidiCC(1, 20, 127); // reset looper
        });

    // «акрытие лупера Ч ещЄ через 10 мс
    delay += 10;
    juce::Timer::callAfterDelay(delay, [this]
        {
            rigControl.sendMidiCC(1, 102, 0); // close looper
        });

    // —брос тюнера Ч ещЄ через 10 мс
    delay += 10;
    juce::Timer::callAfterDelay(delay, [this]
        {
            rigControl.sendMidiCC(1, 110, 0); // reset tuner
        });
    // —брос меню инпута Ч ещЄ через 10 мс
    delay += 10;
    juce::Timer::callAfterDelay(delay, [this]
        {
            rigControl.sendMidiCC(1, 55, 0); // reset menu input
        });
    //  ќпрос подключени€ педали 
    
    juce::Timer::callAfterDelay(delay, [this]
        {
            rigControl.sendMidiCC(6, 1, 127);

        });
}

void MidiStartupShutdown::sendOtherShutdownCommands()
{
    // ѕример:
    // rigControl.sendMidiCC(1, 60, 0); // CC60=0 на канале 1
}

