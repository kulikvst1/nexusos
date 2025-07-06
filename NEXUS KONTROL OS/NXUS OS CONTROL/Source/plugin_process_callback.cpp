//==============================================================================
// PluginProcessCallback.cpp
//==============================================================================

#include "plugin_process_callback.h"
#include "vst_host.h"
#include "TunerComponent.h"    // ← подключаем TunerComponent

//==============================================================================
//  Конструкторы
//==============================================================================
PluginProcessCallback::PluginProcessCallback(juce::AudioPluginInstance* p,
    double                      rate) noexcept
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
//  AudioIODeviceCallbackWithContext
//==============================================================================
void PluginProcessCallback::audioDeviceIOCallbackWithContext(
    const float* const* inputChannelData,
    int                                 numInputChannels,
    float* const* outputChannelData,
    int                                 numOutputChannels,
    int                                 numSamples,
    const juce::AudioIODeviceCallbackContext&)
{
    // — шлём входной сигнал в тюнер (даже если плагина нет)
    if (tuner != nullptr && numInputChannels > 0 && inputChannelData[0] != nullptr)
        tuner->pushAudioData(inputChannelData[0], numSamples);

    // 0) забираем плагин
    auto* inst = pluginInstance.load(std::memory_order_acquire);
    if (inst == nullptr)
    {
        // если плагина нет — bypass: копируем вход на выход
        const int channels = juce::jmin(numInputChannels, numOutputChannels);
        for (int ch = 0; ch < channels; ++ch)
        {
            if (inputChannelData[ch] != nullptr && outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::copy(outputChannelData[ch],
                    inputChannelData[ch],
                    numSamples);
            else if (outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
        }
        // зачищаем лишние выходные каналы
        for (int ch = channels; ch < numOutputChannels; ++ch)
            if (outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);

        return;
    }

    if (numInputChannels <= 0)
    {
        // если каналов нет — затираем выход
        for (int ch = 0; ch < numOutputChannels; ++ch)
            if (outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
        return;
    }

    // 1) копируем вход в локальный буфер и прогоняем плагином
    juce::AudioBuffer<float> inputBuffer(numInputChannels, numSamples);
    for (int ch = 0; ch < numInputChannels; ++ch)
    {
        if (inputChannelData[ch] != nullptr)
            inputBuffer.copyFrom(ch, 0, inputChannelData[ch], numSamples);
        else
            inputBuffer.clear(ch, 0, numSamples);
    }

    juce::MidiBuffer midi;
    const auto t0 = juce::Time::getHighResolutionTicks();

    inst->processBlock(inputBuffer, midi);
    const auto t1 = juce::Time::getHighResolutionTicks();

    // 2) сохраняем dry — результат работы плагина
    dryBuffer.makeCopyOf(inputBuffer);

    // 3) подготавливаем wetBuffer и копируем в него dryBuffer
    wetBuffer.makeCopyOf(dryBuffer);

    // 4) лупер обрабатывает wetBuffer (если готов)
    if (looper != nullptr && looper->isPreparedSuccessfully())
        looper->process(wetBuffer);

    // 5) миксуем dry + wet в выходной буфер
    for (int ch = 0; ch < numOutputChannels; ++ch)
    {
        auto* out = outputChannelData[ch];
        if (!out) continue;

        int dryCh = (ch < dryBuffer.getNumChannels()) ? ch : 0;
        int wetCh = (ch < wetBuffer.getNumChannels()) ? ch : 0;

        // сначала dry
        juce::FloatVectorOperations::copy(out,
            dryBuffer.getReadPointer(dryCh),
            numSamples);

        // потом wet только в режиме Playing
        if (looper != nullptr
            && looper->isPreparedSuccessfully()
            && looper->getState() == LooperEngine::Playing)
        {
            juce::FloatVectorOperations::add(out,
                wetBuffer.getReadPointer(wetCh),
                numSamples);
        }
    }

    // --- опциональная CPU-метрика (без изменений) ---
    if (hostComponent != nullptr && currentSampleRate > 0.0)
    {
        const double blkSec = double(numSamples) / currentSampleRate;
        const double used = juce::Time::highResolutionTicksToSeconds(t1 - t0);
        hostComponent->updatePluginCpuLoad(blkSec > 0.0 ? used / blkSec : 0.0);
    }
}

//==============================================================================
//  AudioIODeviceCallback (без контекста) – прокси
//==============================================================================
void PluginProcessCallback::audioDeviceIOCallback(const float** inputChannelData,
    int            numInputChannels,
    float** outputChannelData,
    int            numOutputChannels,
    int            numSamples)
{
    static const juce::AudioIODeviceCallbackContext dummy{};
    audioDeviceIOCallbackWithContext(inputChannelData,
        numInputChannels,
        outputChannelData,
        numOutputChannels,
        numSamples,
        dummy);
}

//==============================================================================
//  Начало устройства – prepareToPlay + looper.prepare
//==============================================================================
void PluginProcessCallback::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    currentSampleRate = device->getCurrentSampleRate();
    int bufSize = device->getCurrentBufferSizeSamples();

    // готовим тюнер
    if (tuner != nullptr)
        tuner->prepare(bufSize, currentSampleRate);

    if (auto* inst = pluginInstance.load())
        inst->prepareToPlay(currentSampleRate, bufSize);

    auto maxCh = juce::jmax(device->getActiveInputChannels().countNumberOfSetBits(),
        device->getActiveOutputChannels().countNumberOfSetBits());
    dryBuffer.setSize(maxCh, bufSize, false, false, true);
    wetBuffer.setSize(maxCh, bufSize, false, false, true);

    if (looper)
        looper->prepare(currentSampleRate, bufSize);
}

//==============================================================================
//  Стоп устройства – releaseResources + looper.reset
//==============================================================================
void PluginProcessCallback::audioDeviceStopped()
{
    if (auto* inst = pluginInstance.load())
        inst->releaseResources();

    if (looper)
        looper->reset();
}

//==============================================================================
//  Хелперы
//==============================================================================
void PluginProcessCallback::setHostComponent(VSTHostComponent* host) noexcept
{
    hostComponent = host;
}
