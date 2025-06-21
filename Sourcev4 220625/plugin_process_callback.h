#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>

// — forward, чтобы избежать циклической зависимости
class VSTHostComponent;

//==============================================================================
//  PluginProcessCallback
//==============================================================================
class PluginProcessCallback : public juce::AudioIODeviceCallback
{
public:
    // два удобных конструктора
    PluginProcessCallback() noexcept = default;
    PluginProcessCallback(juce::AudioPluginInstance* p, double rate) noexcept;

    ~PluginProcessCallback() override = default;

    /*-------------------  главное нововведение  -------------------*/
    void setPlugin(juce::AudioPluginInstance* p) noexcept;

    /*-------------------  AudioIODeviceCallback  ------------------*/
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
        int  numInputChannels,
        float* const* outputChannelData,
        int  numOutputChannels,
        int  numSamples,
        const juce::AudioIODeviceCallbackContext& ctx) override;

    // «старая» версия без контекста – просто прокси
    void audioDeviceIOCallback(const float** inputChannelData,
        int            numInputChannels,
        float** outputChannelData,
        int            numOutputChannels,
        int            numSamples);

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    void setHostComponent(VSTHostComponent* host) noexcept;

private:
    std::atomic<juce::AudioPluginInstance*> pluginInstance{ nullptr };   // <- атомарно!
    double               currentSampleRate = 44100.0;

    VSTHostComponent* hostComponent = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessCallback)
};
