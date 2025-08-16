#pragma once
#include "OutControlComponent.h"
#include <JuceHeader.h>
#include <atomic>
#include <vector>

// Предварительное объявление классов
class VSTHostComponent;
class TunerComponent;
class LooperEngine;
class InputControlComponent;  
//==============================================================================
//  PluginProcessCallback
//==============================================================================
class PluginProcessCallback : public juce::AudioIODeviceCallback
{
public:
    PluginProcessCallback() noexcept = default;
    PluginProcessCallback(juce::AudioPluginInstance* p,
                          double                      rate) noexcept;

    ~PluginProcessCallback() override ;

    // Атомарная установка экземпляра плагина
    void setPlugin(juce::AudioPluginInstance* p) noexcept;

    // Привязка GUI-хоста для обновления CPU-метрики
    void setHostComponent(VSTHostComponent* host) noexcept;

    // Привязка движка лупера
    void setLooperEngine(LooperEngine* engine) noexcept;

    // Регистрирует тюнер для анализа входного аудио
    void setTuner(TunerComponent* t) noexcept;
    void removeTuner(TunerComponent* t) noexcept;

    //==============================================================================  
    //  AudioIODeviceCallback
    //==============================================================================  
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    void audioDeviceIOCallbackWithContext(
        const float* const*                      inputChannelData,
        int                                       numInputChannels,
        float* const*                            outputChannelData,
        int                                       numOutputChannel,
        int                                       numSamples,
        const juce::AudioIODeviceCallbackContext& context) override;

    void audioDeviceIOCallback(
        const float** inputChannelData,
        int            numInputChannels,
        float**        outputChannelData,
        int            numOutputChannels,
        int            numSamples) ;
    void setTuners(const std::vector<TunerComponent*>& newTuners) noexcept
    {
        tuners = newTuners;
    }
    // добавляем указатель и сеттер
    void setOutControl(OutControlComponent* oc) noexcept
    {
        outControl = oc;
    }

    void audioDeviceIOCallback(...)
    {
        // здесь позже вызовем outControl->processAudioBlock()
    }
    void setInputControl(InputControlComponent* ic) noexcept
    {
        inputControl = ic;
    }
private:
    InputControlComponent* inputControl = nullptr;
    OutControlComponent* outControl = nullptr;
    std::atomic<juce::AudioPluginInstance*>   pluginInstance{ nullptr };
    double                                     currentSampleRate{ 44100.0 };
    int                                        bufferSize{ 0 };

    VSTHostComponent*                          hostComponent{ nullptr };
    LooperEngine*                              looper{ nullptr };

    juce::AudioBuffer<float>                   dryBuffer, wetBuffer;

    // Список всех зарегистрированных тюнеров
   // std::vector<TunerComponent*>               tuners;
    std::mutex                           tunersMutex;
    std::vector<TunerComponent*>         tuners;
    
    void stereoizeInput(const float* const* inputChannelData,
        int                  numInputChannels,
        juce::AudioBuffer<float>& dest,
        int                  numSamples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessCallback)
};
