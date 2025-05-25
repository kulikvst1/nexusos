#include "plugin_process_callback.h"
#include "vst_host.h" // Здесь доступно полное определение VSTHostComponent

// Реализация конструктора
PluginProcessCallback::PluginProcessCallback(juce::AudioPluginInstance* p, double rate)
    : pluginInstance(p), currentSampleRate(rate), hostComponent(nullptr)
{
    // Дополнительная инициализация, если нужна
}

void PluginProcessCallback::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
    int numInputChannels,
    float* const* outputChannelData,
    int numOutputChannels,
    int numSamples,
    const juce::AudioIODeviceCallbackContext& context)
{
    if (pluginInstance == nullptr)
        return;

    juce::AudioBuffer<float> buffer(outputChannelData, numOutputChannels, numSamples);
    juce::MidiBuffer midiBuffer;

    auto startTicks = juce::Time::getHighResolutionTicks();
    pluginInstance->processBlock(buffer, midiBuffer);
    auto endTicks = juce::Time::getHighResolutionTicks();

    auto elapsedTicks = endTicks - startTicks;
    double elapsedSeconds = juce::Time::highResolutionTicksToSeconds(elapsedTicks);

    // Защита от деления на ноль:
    double blockDuration = 0.0;
    if (currentSampleRate > 0.0)
        blockDuration = numSamples / currentSampleRate;
    else
        DBG("Current sample rate is zero!");

    double pluginCPULoad = (blockDuration > 0.0) ? (elapsedSeconds / blockDuration) : 0.0;

    if (hostComponent != nullptr)
        hostComponent->updatePluginCpuLoad(pluginCPULoad);

    DBG("Plugin CPU load: " << pluginCPULoad * 100.0 << "%");
}

void PluginProcessCallback::audioDeviceIOCallback(const float** inputChannelData,
    int numInputChannels,
    float** outputChannelData,
    int numOutputChannels,
    int numSamples)
{
    // Можно оставить пустую реализацию, если данная версия не используется.
}

void PluginProcessCallback::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    // Реализация по необходимости
}

void PluginProcessCallback::audioDeviceStopped()
{
    // Реализация по необходимости
}

void PluginProcessCallback::setHostComponent(VSTHostComponent* host)
{
    hostComponent = host;
}
