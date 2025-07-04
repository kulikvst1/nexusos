#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "LooperEngine.h"   // ← добавлено

// — forward, чтобы избежать циклической зависимости
class VSTHostComponent;

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

    /*-------------------  AudioIODeviceCallback  ------------------*/
    // метод с контекстом — override тут и только тут
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
        int                                numInputChannels,
        float* const* outputChannelData,
        int                                numOutputChannels,
        int                                numSamples,
        const juce::AudioIODeviceCallbackContext& ctx) override;

    // «старая» версия без контекста – прокси (без override)
    void audioDeviceIOCallback(const float** inputChannelData,
        int            numInputChannels,
        float** outputChannelData,
        int            numOutputChannels,
        int            numSamples);

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    // LOOPER
    void setLooperEngine(LooperEngine* engine) noexcept
    {
        looper = engine;
    }

private:
    std::atomic<juce::AudioPluginInstance*> pluginInstance{ nullptr };
    double                                   currentSampleRate{ 44100.0 };
    VSTHostComponent* hostComponent{ nullptr };

    LooperEngine* looper{ nullptr };          // ← добавлено

    // для dry/wet микширования
    juce::AudioBuffer<float> dryBuffer, wetBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessCallback)
};
