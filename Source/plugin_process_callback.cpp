#include "plugin_process_callback.h"
#include "vst_host.h"

//==============================================================================
//  Конструкторы
//==============================================================================
PluginProcessCallback::PluginProcessCallback(juce::AudioPluginInstance* p,
    double rate) noexcept
    : currentSampleRate(rate)
{
    setPlugin(p);
}

//==============================================================================
//  setPlugin  – атомарно подменяем экземпляр
//==============================================================================
void PluginProcessCallback::setPlugin(juce::AudioPluginInstance* p) noexcept
{
    pluginInstance.store(p, std::memory_order_release);
}

//==============================================================================
//  AudioIODeviceCallback
//==============================================================================
void PluginProcessCallback::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
    int  numInputChannels,
    float* const* outputChannelData,
    int  numOutputChannels,
    int  numSamples,
    const juce::AudioIODeviceCallbackContext&)
{
    auto* inst = pluginInstance.load(std::memory_order_acquire);
    if (inst == nullptr) return;

    juce::AudioBuffer<float> buffer(outputChannelData, numOutputChannels, numSamples);
    juce::MidiBuffer         midi;

    const auto t0 = juce::Time::getHighResolutionTicks();
    inst->processBlock(buffer, midi);
    const auto t1 = juce::Time::getHighResolutionTicks();

    // --- опциональная CPU-метрика ------------------------------------------------
    if (hostComponent != nullptr && currentSampleRate > 0.0)
    {
        const double blkSec = static_cast<double> (numSamples) / currentSampleRate;
        const double used = juce::Time::highResolutionTicksToSeconds(t1 - t0);
        hostComponent->updatePluginCpuLoad(blkSec > 0.0 ? used / blkSec : 0.0);
    }
}

void PluginProcessCallback::audioDeviceIOCallback(const float** inputChannelData,
    int            numInputChannels,
    float** outputChannelData,
    int            numOutputChannels,
    int            numSamples)
{
    static const juce::AudioIODeviceCallbackContext dummy{};
    audioDeviceIOCallbackWithContext(inputChannelData,  //  const float* const*
        numInputChannels,
        outputChannelData, //  float*  const*
        numOutputChannels,
        numSamples,
        dummy);
}

void PluginProcessCallback::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    currentSampleRate = device->getCurrentSampleRate();
    if (auto* inst = pluginInstance.load())
        inst->prepareToPlay(currentSampleRate, device->getCurrentBufferSizeSamples());
}

void PluginProcessCallback::audioDeviceStopped()
{
    if (auto* inst = pluginInstance.load())
        inst->releaseResources();
}

//==============================================================================
//  Хелперы
//==============================================================================
void PluginProcessCallback::setHostComponent(VSTHostComponent* host) noexcept
{
    hostComponent = host;
}
