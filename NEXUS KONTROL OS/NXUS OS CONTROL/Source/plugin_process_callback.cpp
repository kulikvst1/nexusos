//==============================================================================
//  PluginProcessCallback.cpp
//==============================================================================

#include "plugin_process_callback.h"
#include "vst_host.h"
#include "TunerComponent.h"
#include <vector>
#include <mutex>    // ← добавили для защиты tunerList

// статический список всех зарегистрированных тюнеров
static std::vector<TunerComponent*> tunerList;
// мьютекс для защиты от одновременного чтения/записи
static std::mutex                 tunerListMutex;

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
    int                                       numInputChannels,
    float* const* outputChannelData,
    int                                       numOutputChannels,
    int                                       numSamples,
    const juce::AudioIODeviceCallbackContext& /*ctx*/)
{
    // 0) отправляем вход во все видимые тюнеры
    if (numInputChannels > 0 && inputChannelData[0] != nullptr)
    {
        std::lock_guard<std::mutex> lk(tunerListMutex);    // ← блокируем доступ
        for (auto* t : tunerList)
            if (t->isVisible())
                t->pushAudioData(inputChannelData[0], numSamples);
    }

    // 1) получаем плагин
    auto* inst = pluginInstance.load(std::memory_order_acquire);
    if (inst == nullptr)
    {
        // bypass: копируем вход на выход
        const int channels = juce::jmin(numInputChannels, numOutputChannels);
        for (int ch = 0; ch < channels; ++ch)
        {
            if (inputChannelData[ch] != nullptr && outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::copy(
                    outputChannelData[ch],
                    inputChannelData[ch],
                    numSamples);
            else if (outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::clear(
                    outputChannelData[ch], numSamples);
        }
        for (int ch = channels; ch < numOutputChannels; ++ch)
            if (outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::clear(
                    outputChannelData[ch], numSamples);

        return;
    }

    if (numInputChannels <= 0)
    {
        for (int ch = 0; ch < numOutputChannels; ++ch)
            if (outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::clear(
                    outputChannelData[ch], numSamples);
        return;
    }

    // 2) копируем вход в локальный буфер и прогоняем плагин
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

    // 3) dry = результат плагина
    dryBuffer.makeCopyOf(inputBuffer);

    // 4) wet = dry, затем лупер
    wetBuffer.makeCopyOf(dryBuffer);
    if (looper != nullptr && looper->isPreparedSuccessfully())
        looper->process(wetBuffer);

    // 5) микс dry+wet в выходные
    for (int ch = 0; ch < numOutputChannels; ++ch)
    {
        auto* out = outputChannelData[ch];
        if (!out) continue;

        int dryCh = (ch < dryBuffer.getNumChannels()) ? ch : 0;
        int wetCh = (ch < wetBuffer.getNumChannels()) ? ch : 0;

        juce::FloatVectorOperations::copy(
            out,
            dryBuffer.getReadPointer(dryCh),
            numSamples);

        if (looper != nullptr
            && looper->isPreparedSuccessfully()
            && looper->getState() == LooperEngine::Playing)
        {
            juce::FloatVectorOperations::add(
                out,
                wetBuffer.getReadPointer(wetCh),
                numSamples);
        }
    }

    // 6) CPU-метрика
    if (hostComponent != nullptr && currentSampleRate > 0.0)
    {
        const double blkSec = double(numSamples) / currentSampleRate;
        const double used = juce::Time::highResolutionTicksToSeconds(t1 - t0);
        hostComponent->updatePluginCpuLoad(
            blkSec > 0.0 ? used / blkSec : 0.0);
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
//  Начало устройства – prepareToPlay + prepare тюнеров и лупера
//==============================================================================
void PluginProcessCallback::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    currentSampleRate = device->getCurrentSampleRate();
    int bufSize = device->getCurrentBufferSizeSamples();

    // готовим всех тюнеров под блокировкой
    {
        std::lock_guard<std::mutex> lk(tunerListMutex);
        for (auto* t : tunerList)
            t->prepare(bufSize, currentSampleRate);
    }

    // готовим плагин
    if (auto* inst = pluginInstance.load(std::memory_order_acquire))
        inst->prepareToPlay(currentSampleRate, bufSize);

    auto maxCh = juce::jmax(device->getActiveInputChannels().countNumberOfSetBits(),
        device->getActiveOutputChannels().countNumberOfSetBits());
    dryBuffer.setSize(maxCh, bufSize, false, false, true);
    wetBuffer.setSize(maxCh, bufSize, false, false, true);

    if (looper)
        looper->prepare(currentSampleRate, bufSize);
}

//==============================================================================
//  Стоп устройства – releaseResources + лупер reset
//==============================================================================
void PluginProcessCallback::audioDeviceStopped()
{
    if (auto* inst = pluginInstance.load(std::memory_order_acquire))
        inst->releaseResources();

    if (looper)
        looper->reset();
}

//==============================================================================
//  setHostComponent – обычная установка
//==============================================================================
void PluginProcessCallback::setHostComponent(VSTHostComponent* host) noexcept
{
    hostComponent = host;
}

//==============================================================================
//  setTuner – регистрируем новый тюнер
//==============================================================================
void PluginProcessCallback::setTuner(TunerComponent* t) noexcept
{
    if (t != nullptr)
    {
        // потокобезопасно добавляем в список
        {
            std::lock_guard<std::mutex> lk(tunerListMutex);
            tunerList.push_back(t);
        }

        // если аудио уже запущено — сразу готовим его
        if (currentSampleRate > 0.0 && dryBuffer.getNumSamples() > 0)
            t->prepare(dryBuffer.getNumSamples(), currentSampleRate);
    }
}
void PluginProcessCallback::removeTuner(TunerComponent* t) noexcept
{
    std::lock_guard<std::mutex> lk(tunerListMutex);

    auto it = std::find(tunerList.begin(), tunerList.end(), t);
    if (it != tunerList.end())
        tunerList.erase(it);
}
//==============================================================================  
//  setLooperEngine – связываем движок лупера  
//==============================================================================  
void PluginProcessCallback::setLooperEngine(LooperEngine* engine) noexcept
{
    looper = engine;
}
//==============================================================================  
//  Деструктор – очищаем список тюнеров под защитой мьютекса  
//==============================================================================
PluginProcessCallback::~PluginProcessCallback()
 {
    std::lock_guard<std::mutex> lk(tunerListMutex);
    tunerList.clear();
    }
