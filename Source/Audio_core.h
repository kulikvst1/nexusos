#pragma once
#include <JuceHeader.h>
//#include "vst_host.h" // Для доступа к VSTHostComponent::getDefaultAudioDeviceManager()

class Audio_core
{
public:
    Audio_core()
        : deviceManager(VSTHostComponent::getDefaultAudioDeviceManager())
    {
        DBG("Audio_core (старый стиль): Используем единый AudioDeviceManager из VSTHostComponent.");

        // Инициализируем ADM с 2 входами и 2 выходами.
        auto error = deviceManager.initialiseWithDefaultDevices(2, 2);
        jassert(error.isEmpty());
        DBG("Audio_core (старый стиль): ADM инициализирован с 2 входами и 2 выходами.");

        // Если настройки ADM уже корректны, ручную перенастройку не делаем.

        // Создаем AudioProcessorPlayer и регистрируем его в качестве аудио callback.
        audioPlayer = std::make_unique<juce::AudioProcessorPlayer>();
        deviceManager.addAudioCallback(audioPlayer.get());
        DBG("Audio_core (старый стиль): AudioProcessorPlayer создан и добавлен как аудио callback.");
    }

    ~Audio_core()
    {
        deviceManager.removeAudioCallback(audioPlayer.get());
        DBG("Audio_core (старый стиль): Деструктор вызван, аудио callback удален.");
    }

    void setProcessor(juce::AudioProcessor* processor)
    {
        DBG("setProcessor() вызван с указателем: " + juce::String((uintptr_t)processor));
        if (currentProcessor != processor)
        {
            currentProcessor = processor;
            audioPlayer->setProcessor(processor);
            DBG("Audio_core (старый стиль): Плагин обновлен в AudioProcessorPlayer.");
        }
        else
        {
            DBG("Audio_core (старый стиль): Плагин не изменился, обновление не производится.");
        }
    }

    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }

private:
    // Используем ADM, полученный через VSTHostComponent.
    juce::AudioDeviceManager& deviceManager;
    std::unique_ptr<juce::AudioProcessorPlayer> audioPlayer;
    juce::AudioProcessor* currentProcessor = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Audio_core)
};
