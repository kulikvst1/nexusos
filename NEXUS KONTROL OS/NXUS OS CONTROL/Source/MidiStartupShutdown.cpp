#include "MidiStartupShutdown.h"
#include <JuceHeader.h>

void MidiStartupShutdown::sendStartupCommands()
{
    // 1. ��������
    sendImpedanceFromSettings();

    // 2. ��������� ��������� �������
    sendOtherStartupCommands();
}

void MidiStartupShutdown::sendShutdownCommands()
{
    // 1. ����� ������� �� ����������/�����
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

    // ������ ������� � �����
    rigControl.sendMidiCC(1, 100, 0); // reset stomp mode

    // ������ � ����� 10 ��
    delay += 10;
    juce::Timer::callAfterDelay(delay, [this]
        {
            rigControl.sendMidiCC(1, 101, 0); // reset shift mode
        });

    // ����� ������ � ��� ����� 10 ��
    delay += 10;
    juce::Timer::callAfterDelay(delay, [this]
        {
            rigControl.sendMidiCC(1, 20, 127); // reset looper
        });

    // �������� ������ � ��� ����� 10 ��
    delay += 10;
    juce::Timer::callAfterDelay(delay, [this]
        {
            rigControl.sendMidiCC(1, 102, 0); // close looper
        });

    // ����� ������ � ��� ����� 10 ��
    delay += 10;
    juce::Timer::callAfterDelay(delay, [this]
        {
            rigControl.sendMidiCC(1, 110, 0); // reset tuner
        });
    // ����� ���� ������ � ��� ����� 10 ��
    delay += 10;
    juce::Timer::callAfterDelay(delay, [this]
        {
            rigControl.sendMidiCC(1, 55, 0); // reset menu input
        });
    //  ����� ����������� ������ 
    
    juce::Timer::callAfterDelay(delay, [this]
        {
            rigControl.sendMidiCC(6, 1, 127);

        });
}

void MidiStartupShutdown::sendOtherShutdownCommands()
{
    // ������:
    // rigControl.sendMidiCC(1, 60, 0); // CC60=0 �� ������ 1
}

