#pragma once
#include <JuceHeader.h>
#include "vst_host.h"   // Äëÿ äîñòóïà ê VSTHostComponent::getDefaultAudioDeviceManager()

class Audio_core : public juce::Timer
{
public:
    // Âìåñòî ñîçäàíèÿ íîâîãî AudioDeviceManager ìû ïîëó÷àåì åãî ññûëêó èç VSTHostComponent.
    Audio_core()
    : deviceManager(VSTHostComponent::getDefaultAudioDeviceManager())
    {
        DBG("Audio_core: Èñïîëüçóåì åäèíûé AudioDeviceManager èç VSTHostComponent.");

        // Åñëè òðåáóåòñÿ, ìîæíî íå ïåðåèíèöèàëèçèðîâàòü, òàê êàê â getDefaultAudioDeviceManager() óæå ñäåëàëè initialise.
        // Ñîçäàåì AudioProcessorPlayer äëÿ ïåðåäà÷è àóäèîñèãíàëà â ïëàãèí.
        audioPlayer = std::make_unique<juce::AudioProcessorPlayer>();
        deviceManager.addAudioCallback(audioPlayer.get());
        DBG("Audio_core: AudioProcessorPlayer ñîçäàí è äîáàâëåí êàê audio callback.");

        // Çàïóñêàåì òàéìåð äëÿ ïåðèîäè÷åñêèõ ïðîâåðîê (â îòëàäêå èëè äëÿ îáíîâëåíèÿ)
        startTimer(500);
        DBG("Audio_core: Òàéìåð çàïóùåí (500 ìñ).");
    }

    ~Audio_core() override
    {
        stopTimer();
        deviceManager.removeAudioCallback(audioPlayer.get());
        DBG("Audio_core: Äåñòðóêòîð âûçâàí, òàéìåð îñòàíîâëåí, callback óäàëåí.");
    }

    // Ìåòîä äëÿ íàçíà÷åíèÿ ïëàãèíà â àóäèîöåïî÷êó.
    void setProcessor(juce::AudioProcessor* processor)
    {
        DBG("setProcessor() âûçâàí ñ óêàçàòåëåì: " + juce::String((uintptr_t)processor));
        if (currentProcessor != processor)
        {
            currentProcessor = processor;
            audioPlayer->setProcessor(processor);
            DBG("Audio_core: Processor îáíîâëåí â AudioProcessorPlayer.");
        }
        else
        {
            DBG("Audio_core: Processor íå èçìåíèëñÿ, îáíîâëåíèå íå ïðîèçâîäèòñÿ.");
        }
    }

    // Äîñòóï ê AudioDeviceManager äëÿ äðóãèõ ìîäóëåé (íàïðèìåð, äëÿ äèàëîãà Audio/MIDI íàñòðîåê).
    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }

    // Òàéìåð äëÿ ïåðèîäè÷åñêîé îòëàäêè.
    void timerCallback() override
    {
        // Çäåñü ìîæíî äîáàâèòü äîïîëíèòåëüíûå ïðîâåðêè èëè äðóãîå ëîãèðîâàíèå.
        DBG("Audio_core: timerCallback âûçâàí.");
    }

private:
    // Òåïåðü deviceManager – ýòî ññûëêà íà îáùèé ýêçåìïëÿð.
    juce::AudioDeviceManager& deviceManager;
    std::unique_ptr<juce::AudioProcessorPlayer> audioPlayer;
    juce::AudioProcessor* currentProcessor = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Audio_core)
};
