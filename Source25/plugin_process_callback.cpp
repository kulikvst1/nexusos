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
    int numInputChannels,
    float* const* outputChannelData,
    int numOutputChannels,
    int numSamples,
    const juce::AudioIODeviceCallbackContext&)
{
    auto* inst = pluginInstance.load(std::memory_order_acquire);
    if (inst == nullptr)
    {
        // Если плагин не загружен, очищаем выходные буферы
        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            if (outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
        }
        return;
    }
    
    // Если число входных каналов меньше или равно 0, очищаем выходные буферы и выходим.
    if (numInputChannels <= 0)
    {
        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            if (outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
        }
        return;
    }
    
    // Создаем временный аудиобуфер из входных данных
    juce::AudioBuffer<float> inputBuffer(numInputChannels, numSamples);
    for (int ch = 0; ch < numInputChannels; ++ch)
    {
        if (inputChannelData[ch] != nullptr)
            inputBuffer.copyFrom(ch, 0, inputChannelData[ch], numSamples);
        else
            inputBuffer.clear(ch, 0, numSamples);
    }
    
    // Выводим информацию о буфере и текущей частоте
    DBG("Before processBlock: Input channels = " << numInputChannels
        << ", Output channels = " << numOutputChannels
        << ", numSamples = " << numSamples
        << ", Buffer channels = " << inputBuffer.getNumChannels()
        << ", Sample Rate = " << currentSampleRate);

    juce::MidiBuffer midi;

    const auto t0 = juce::Time::getHighResolutionTicks();
    // Передаем входной сигнал в плагин для обработки
    inst->processBlock(inputBuffer, midi);
    const auto t1 = juce::Time::getHighResolutionTicks();

    // Выводим информацию после обработки (опционально)
    DBG("After processBlock: Buffer channels = " << inputBuffer.getNumChannels());

    // Копируем обработанные данные в выходные буферы.
    // Если число выходных каналов больше числа входных, для лишних каналов копируем данные из первого.
    for (int ch = 0; ch < numOutputChannels; ++ch)
    {
        int copyChannel = (ch < inputBuffer.getNumChannels()) ? ch : 0;
        if (outputChannelData[ch] != nullptr)
            juce::FloatVectorOperations::copy(outputChannelData[ch],
                inputBuffer.getReadPointer(copyChannel),
                numSamples);
    }

    // --- опциональная CPU-метрика ------------------------------------------------
    if (hostComponent != nullptr && currentSampleRate > 0.0)
    {
        const double blkSec = static_cast<double>(numSamples) / currentSampleRate;
        const double used = juce::Time::highResolutionTicksToSeconds(t1 - t0);
        hostComponent->updatePluginCpuLoad(blkSec > 0.0 ? used / blkSec : 0.0);
        // Выводим также измеренный CPU load
        DBG("CPU load: " << (blkSec > 0.0 ? used / blkSec : 0.0));
    }
}

void PluginProcessCallback::audioDeviceIOCallback(const float** inputChannelData,
    int numInputChannels,
    float** outputChannelData,
    int numOutputChannels,
    int numSamples)
{
    static const juce::AudioIODeviceCallbackContext dummy{};
    audioDeviceIOCallbackWithContext(inputChannelData,  // const float* const*
        numInputChannels,
        outputChannelData, // float* const*
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
