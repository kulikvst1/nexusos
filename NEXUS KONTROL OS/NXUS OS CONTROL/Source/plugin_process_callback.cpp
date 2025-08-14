#include "plugin_process_callback.h"
#include "vst_host.h"
#include "TunerComponent.h"
#include <vector>
#include <mutex>    // ← добавили для защиты tunerList

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
//  Audio I/O Callback с универсальным маршрутом 🎛️
//  — Если плагин есть: плагин → лупер (post-FX) → OutControl → выход
//  — Если плагина нет: стереоизация → (лупер по желанию) → OutControl → выход
//==============================================================================

void PluginProcessCallback::audioDeviceIOCallbackWithContext(
    const float* const* inputChannelData,
    int                  numInputChannels,
    float* const* outputChannelData,
    int                  numOutputChannels,
    int                  numSamples,
    const juce::AudioIODeviceCallbackContext& /*ctx*/)
{
    // 🔍 0) Отправляем вход в тюнеры (чтобы у них всегда была живая картинка)
    {
        std::lock_guard<std::mutex> lk(tunersMutex);
        for (auto* t : tuners)
            if (t->isVisible())
                t->pushAudioData(inputChannelData[0], numSamples);
    }

    // 🎯 1) Загружаем текущий плагин
    auto* inst = pluginInstance.load(std::memory_order_acquire);

    // 🌀 1a) Нет плагина → готовим стерео-байпасный буфер
    if (inst == nullptr)
    {
        juce::AudioBuffer<float> stereoBuffer;
        stereoBuffer.setSize(2, numSamples, false, false, true);

        auto* L = stereoBuffer.getWritePointer(0);
        auto* R = stereoBuffer.getWritePointer(1);

        if (numInputChannels >= 2)
        {
            juce::FloatVectorOperations::copy(L, inputChannelData[0], numSamples);
            juce::FloatVectorOperations::copy(R, inputChannelData[1], numSamples);
        }
        else if (numInputChannels == 1)
        {
            juce::FloatVectorOperations::copy(L, inputChannelData[0], numSamples);
            juce::FloatVectorOperations::copy(R, inputChannelData[0], numSamples);
        }
        else
        {
            juce::FloatVectorOperations::clear(L, numSamples);
            juce::FloatVectorOperations::clear(R, numSamples);
        }

        // 🔄 Лупер в режиме байпаса (по желанию)
        if (looper != nullptr && looper->isPreparedSuccessfully())
            looper->process(stereoBuffer);

        // 🎚️ Мастер-обработка OutControl
        if (outControl != nullptr)
            outControl->processAudioBlock(stereoBuffer);

        // 🚀 Отправка в выход
        const int copyCh = juce::jmin(numOutputChannels, stereoBuffer.getNumChannels());
        for (int ch = 0; ch < copyCh; ++ch)
            juce::FloatVectorOperations::copy(outputChannelData[ch],
                stereoBuffer.getReadPointer(ch),
                numSamples);
        for (int ch = copyCh; ch < numOutputChannels; ++ch)
            juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
        return; // ⬅️ Выход из функции в no‑plugin режиме
    }

    // 🛠️ 2) Есть плагин → копируем вход в локальный буфер
    const int totalCh = juce::jmax(numInputChannels, numOutputChannels);
    juce::AudioBuffer<float> inputBuffer(totalCh, numSamples);

    for (int ch = 0; ch < numInputChannels; ++ch)
        if (inputChannelData[ch] != nullptr)
            inputBuffer.copyFrom(ch, 0, inputChannelData[ch], numSamples);
        else
            inputBuffer.clear(ch, 0, numSamples);

    for (int ch = numInputChannels; ch < totalCh; ++ch)
        inputBuffer.clear(ch, 0, numSamples);

    // 🎹 3) Запускаем плагин
    juce::MidiBuffer midi;
    const auto t0 = juce::Time::getHighResolutionTicks();
    inst->processBlock(inputBuffer, midi);
    const auto t1 = juce::Time::getHighResolutionTicks();

    // 💾 4) dry = результат плагина
    dryBuffer.makeCopyOf(inputBuffer);

    // 🔊 5) wet = dry + лупер
    wetBuffer.makeCopyOf(dryBuffer);
    if (looper != nullptr && looper->isPreparedSuccessfully())
        looper->process(wetBuffer);

    // 🎚️ 6) Миксуем dry + wet → выход
    for (int ch = 0; ch < numOutputChannels; ++ch)
    {
        auto* out = outputChannelData[ch];
        if (!out) continue;

        const int dryCh = (ch < dryBuffer.getNumChannels()) ? ch : 0;
        const int wetCh = (ch < wetBuffer.getNumChannels()) ? ch : 0;

        juce::FloatVectorOperations::copy(out, dryBuffer.getReadPointer(dryCh), numSamples);

        if (looper != nullptr
            && looper->isPreparedSuccessfully()
            && looper->getState() == LooperEngine::Playing)
        {
            juce::FloatVectorOperations::add(out, wetBuffer.getReadPointer(wetCh), numSamples);
        }
    }

    // 📊 7) CPU метрика
    if (hostComponent != nullptr && currentSampleRate > 0.0)
    {
        const double blkSec = double(numSamples) / currentSampleRate;
        const double used = juce::Time::highResolutionTicksToSeconds(t1 - t0);
        hostComponent->updatePluginCpuLoad(blkSec > 0.0 ? used / blkSec : 0.0);
    }

    // 🏁 8) Финальный мастер — OutControl
    if (outControl != nullptr)
    {
        juce::AudioBuffer<float> masterBuffer(outputChannelData,
            numOutputChannels,
            numSamples);
        outControl->processAudioBlock(masterBuffer);
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
        currentSampleRate = device->getCurrentSampleRate();
        auto bufSize = device->getCurrentBufferSizeSamples();

        std::lock_guard<std::mutex> lk(tunersMutex);
        for (auto* t : tuners)
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
    //
    if (outControl != nullptr)
        outControl->prepare(currentSampleRate, bufSize);

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
    if (!t) return;
    std::lock_guard<std::mutex> lk(tunersMutex);
    if (std::find(tuners.begin(), tuners.end(), t) == tuners.end())
        tuners.push_back(t);

    if (currentSampleRate > 0 && dryBuffer.getNumSamples() > 0)
        t->prepare(dryBuffer.getNumSamples(), currentSampleRate);
}

void PluginProcessCallback::removeTuner(TunerComponent* t) noexcept
{
    std::lock_guard<std::mutex> lk(tunersMutex);
    tuners.erase(std::remove(tuners.begin(), tuners.end(), t),
        tuners.end());
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
    std::lock_guard<std::mutex> lk(tunersMutex);
    tuners.clear();
}
// Приводим вход к стерео, независимо от числа каналов
// ====================== 🎚 Stereo Utils 🎚 ======================
void PluginProcessCallback::stereoizeInput(const float* const* inputChannelData,
    int                  numInputChannels,
    juce::AudioBuffer<float>& dest,
    int                  numSamples)
{
    dest.clear(); // 🧹 На всякий случай обнуляем буфер перед записью

    if (numInputChannels >= 2 && inputChannelData[0] && inputChannelData[1])
    {
        // 🎯 Полноценное стерео — копируем как есть
        dest.copyFrom(0, 0, inputChannelData[0], numSamples); // L
        dest.copyFrom(1, 0, inputChannelData[1], numSamples); // R
    }
    else if (numInputChannels == 1 && inputChannelData[0])
    {
        // 🎯 Моно — дублируем сигнал в оба канала
        dest.copyFrom(0, 0, inputChannelData[0], numSamples); // L
        dest.copyFrom(1, 0, inputChannelData[0], numSamples); // R
    }
    else
    {
        // 🎯 Нет сигнала — оставляем тишину
        dest.clear();
    }
}


