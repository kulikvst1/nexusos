#include "MidiStartupShutdown.h"
#include <JuceHeader.h>

void MidiStartupShutdown::sendStartupCommands()
{
    // 1. Импеданс
    sendImpedanceFromSettings();

    // 2. Остальные стартовые команды
    sendOtherStartupCommands();
}

void MidiStartupShutdown::sendShutdownCommands()
{
    // 1. Любые команды на выключение/сброс
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

    // Первая команда — сразу
    rigControl.sendMidiCC(1, 100, 0); // reset stomp mode

    // Вторая — через 10 мс
    delay += 10;
    juce::Timer::callAfterDelay(delay, [this]
        {
            rigControl.sendMidiCC(1, 101, 0); // reset shift mode
        });

    // Сброс лупера — ещё через 10 мс
    delay += 10;
    juce::Timer::callAfterDelay(delay, [this]
        {
            rigControl.sendMidiCC(1, 20, 127); // reset looper
        });

    // Закрытие лупера — ещё через 10 мс
    delay += 10;
    juce::Timer::callAfterDelay(delay, [this]
        {
            rigControl.sendMidiCC(1, 102, 0); // close looper
        });

    // Сброс тюнера — ещё через 10 мс
    delay += 10;
    juce::Timer::callAfterDelay(delay, [this]
        {
            rigControl.sendMidiCC(1, 110, 0); // reset tuner
        });

    // Сброс меню инпута — ещё через 10 мс
    delay += 10;
    juce::Timer::callAfterDelay(delay, [this]
        {
            rigControl.sendMidiCC(1, 55, 0); // reset menu input
        });

    // Опрос подключения педали — через 500 мс
    delay += 500;
    juce::Timer::callAfterDelay(delay, [this]
        {
            rigControl.sendMidiCC(6, 1, 127); // pedal check
        });
}

void MidiStartupShutdown::sendOtherShutdownCommands()
{
    // Пример:
    // rigControl.sendMidiCC(1, 60, 0); // CC60=0 на канале 1
}
