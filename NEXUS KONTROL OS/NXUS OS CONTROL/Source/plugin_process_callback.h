#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>               // ← добавлено
#include "LooperEngine.h"       // ← оставлено

// — forward, чтобы избежать циклических include’ов
class VSTHostComponent;
class TunerComponent;

//==============================================================================
//  PluginProcessCallback
//==============================================================================
class PluginProcessCallback : public juce::AudioIODeviceCallback
{
public:
    PluginProcessCallback() noexcept = default;
    PluginProcessCallback(juce::AudioPluginInstance* p,
        double                      rate) noexcept;
    ~PluginProcessCallback() override = default;

    void setPlugin(juce::AudioPluginInstance* p) noexcept;
    void setHostComponent(VSTHostComponent* host) noexcept;

    /** Регистрирует тюнер для анализа входного аудио */
    void setTuner(TunerComponent* t) noexcept;

    /* AudioIODeviceCallback: */
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
        int                                     numInputChannels,
        float* const* outputChannelData,
        int                                     numOutputChannels,
        int                                     numSamples,
        const juce::AudioIODeviceCallbackContext& ctx) override;

    void audioDeviceIOCallback(const float** inputChannelData,
        int            numInputChannels,
        float** outputChannelData,
        int            numOutputChannels,
        int            numSamples) ;

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    // LOOPER
    void setLooperEngine(LooperEngine* engine) noexcept { looper = engine; }

private:
    std::atomic<juce::AudioPluginInstance*> pluginInstance{ nullptr };
    double                                   currentSampleRate{ 44100.0 };
    int                                      bufferSize{ 0 };        // ← добавлено
    VSTHostComponent* hostComponent{ nullptr };
    LooperEngine* looper{ nullptr };

    // для dry/wet микширования
    juce::AudioBuffer<float>                 dryBuffer, wetBuffer;

    // вместо одного указателя — список всех зарегистрированных тюнеров
    std::vector<TunerComponent*>             tuners;                        // ← изменено

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessCallback)
};
