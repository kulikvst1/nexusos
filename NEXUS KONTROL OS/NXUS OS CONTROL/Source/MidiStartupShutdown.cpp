#include "MidiStartupShutdown.h"
#include <JuceHeader.h>

void MidiStartupShutdown::sendStartupCommands()
{
    // 1. Èìïåäàíñ
    sendImpedanceFromSettings();

    // 2. Îñòàëüíûå ñòàðòîâûå êîìàíäû
    sendOtherStartupCommands();
}

void MidiStartupShutdown::sendShutdownCommands()
{
    // 1. Ëþáûå êîìàíäû íà âûêëþ÷åíèå/ñáðîñ
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

    // Ïåðâàÿ êîìàíäà — ñðàçó
    rigControl.sendMidiCC(1, 100, 0); // reset stomp mode

    // Âòîðàÿ — ÷åðåç 10 ìñ
    delay += 10;
    juce::Timer::callAfterDelay(delay, [this]
        {
            rigControl.sendMidiCC(1, 101, 0); // reset shift mode
        });

    // Ñáðîñ ëóïåðà — åù¸ ÷åðåç 10 ìñ
    delay += 10;
    juce::Timer::callAfterDelay(delay, [this]
        {
            rigControl.sendMidiCC(1, 20, 127); // reset looper
        });

    // Çàêðûòèå ëóïåðà — åù¸ ÷åðåç 10 ìñ
    delay += 10;
    juce::Timer::callAfterDelay(delay, [this]
        {
            rigControl.sendMidiCC(1, 102, 0); // close looper
        });

    // Ñáðîñ òþíåðà — åù¸ ÷åðåç 10 ìñ
    delay += 10;
    juce::Timer::callAfterDelay(delay, [this]
        {
            rigControl.sendMidiCC(1, 110, 0); // reset tuner
        });
    // Ñáðîñ ìåíþ èíïóòà — åù¸ ÷åðåç 10 ìñ
    delay += 10;
    juce::Timer::callAfterDelay(delay, [this]
        {
            rigControl.sendMidiCC(1, 55, 0); // reset menu input
        });
    //  Îïðîñ ïîäêëþ÷åíèÿ ïåäàëè 
    delay += 500;
    juce::Timer::callAfterDelay(delay, [this]
        {
            rigControl.sendMidiCC(6, 1, 127);

        });
   
}

void MidiStartupShutdown::sendOtherShutdownCommands()
{
    // Ïðèìåð:
    // rigControl.sendMidiCC(1, 60, 0); // CC60=0 íà êàíàëå 1
}

