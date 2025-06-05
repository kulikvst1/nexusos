#pragma once
#include <JuceHeader.h>

// AudioDSP отвечает за передачу аудиобуфера в плагин и корректное очищение неактивных каналов
class AudioDSP : public juce::AudioAppComponent
{
public:
    // Принимаем ссылку на AudioDeviceManager, чтобы узнать актуальную конфигурацию устройства.
    AudioDSP(juce::AudioDeviceManager& dm)
        : deviceManager(dm)
    {
        // НЕ вызываем здесь setAudioChannels – пусть AudioDeviceManager управляет конфигурацией согласно настройкам пользователя.
    }

    ~AudioDSP() override
    {
        shutdownAudio();
    }

    // Стандартный метод prepareToPlay, вызываемый перед началом аудио-процессинга.
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override
    {
        if (pluginInstance != nullptr)
        {
            pluginInstance->prepareToPlay(sampleRate, samplesPerBlockExpected);
            DBG("AudioDSP: Plugin prepared with sampleRate = " << sampleRate
                << " and block size = " << samplesPerBlockExpected);
        }
        else
        {
            DBG("AudioDSP: Plugin not loaded in prepareToPlay.");
        }
    }

    // Основной метод обработки аудиоблока.
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        if (pluginInstance != nullptr)
        {
            juce::MidiBuffer midiBuffer;
            pluginInstance->processBlock(*bufferToFill.buffer, midiBuffer);
            DBG("AudioDSP: processBlock called on plugin.");

            // После обработки проверяем активные выходные каналы из AudioDeviceManager.
            if (auto* currentDevice = deviceManager.getCurrentAudioDevice())
            {
                auto activeChannels = currentDevice->getActiveOutputChannels();
                for (int channel = 0; channel < bufferToFill.buffer->getNumChannels(); ++channel)
                {
                    // Если для данного канала активен флаг false – очищаем его область.
                    if (!activeChannels[channel])
                    {
                        bufferToFill.buffer->clear(channel, bufferToFill.startSample, bufferToFill.numSamples);
                        DBG("AudioDSP: Cleared inactive output channel " << channel);
                    }
                }
            }
        }
        else
        {
            // Если плагин не загружен, очищаем весь буфер.
            bufferToFill.clearActiveBufferRegion();
        }
    }

    // Освобождает ресурсы плагина.
    void releaseResources() override
    {
        if (pluginInstance != nullptr)
            pluginInstance->releaseResources();
    }

    // Устанавливает указатель на плагин. Этот метод вызывается из внешнего кода (например, MainComponent).
    void setPluginInstance(juce::AudioPluginInstance* instance)
    {
        pluginInstance = instance;
    }

private:
    // Ссылка на AudioDeviceManager для доступа к настройкам устройства.
    juce::AudioDeviceManager& deviceManager;
    // Указатель на плагин, который используется для обработки аудиосигнала.
    juce::AudioPluginInstance* pluginInstance = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioDSP)
};
