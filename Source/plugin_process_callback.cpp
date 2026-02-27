#include "plugin_process_callback.h"
#include "InputControlComponent.h"
#include "vst_host.h"
#include "TunerComponent.h"
#include <vector>
#include <mutex>    // ← добавили для защиты tunerList
//#include "FilePlayerEngine.h" // ADD: нужен полный тип для вызова методов

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
    // 0) Снимок указателей в начале
    auto* ic = inputControl.load(std::memory_order_acquire);
    auto* oc = outControl.load(std::memory_order_acquire);
    auto* lp = looper.load(std::memory_order_acquire);
    auto* inst = pluginInstance.load(std::memory_order_acquire);

    // 1) Отправляем вход в тюнеры (моно микс)
    {
        juce::AudioBuffer<float> monoMix(1, numSamples);
        monoMix.clear();

        if (numInputChannels > 0 && inputChannelData[0])
            monoMix.addFrom(0, 0, inputChannelData[0], numSamples);
        if (numInputChannels > 1 && inputChannelData[1])
            monoMix.addFrom(0, 0, inputChannelData[1], numSamples);

        std::lock_guard<std::mutex> lk(tunersMutex);
        for (auto* t : tuners)
            if (t && t->isVisible())
                t->pushAudioData(monoMix.getReadPointer(0), numSamples);
    }

    // 2) InputControl — гейт первым
    if (ic && !ic->isShuttingDown())
    {
        float* chans[2] = { nullptr, nullptr };
        if (numInputChannels > 0 && inputChannelData[0])
            chans[0] = const_cast<float*>(inputChannelData[0]);
        if (numInputChannels > 1 && inputChannelData[1])
            chans[1] = const_cast<float*>(inputChannelData[1]);

        ic->processBlock(chans, numSamples);
    }

    // 3) Если плагина нет — байпас со стереоизацией
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

        // Лупер (без раннего return, просто пропускаем, если нет)
        if (lp && lp->isPreparedSuccessfully() && !lp->isShuttingDown())
            lp->process(stereoBuffer);

        // OutControl
        if (oc && !oc->isShuttingDown())
            oc->processAudioBlock(stereoBuffer);

        // Выход
        const int copyCh = juce::jmin(numOutputChannels, stereoBuffer.getNumChannels());
        for (int ch = 0; ch < copyCh; ++ch)
            juce::FloatVectorOperations::copy(outputChannelData[ch], stereoBuffer.getReadPointer(ch), numSamples);
        for (int ch = copyCh; ch < numOutputChannels; ++ch)
            juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
        return;
    }

    // 4) Есть плагин — собираем входной буфер
    const int totalCh = juce::jmax(numInputChannels, numOutputChannels);
    juce::AudioBuffer<float> inputBuffer(totalCh, numSamples);

    for (int ch = 0; ch < numInputChannels; ++ch)
        if (inputChannelData[ch])
            inputBuffer.copyFrom(ch, 0, inputChannelData[ch], numSamples);
        else
            inputBuffer.clear(ch, 0, numSamples);

    for (int ch = numInputChannels; ch < totalCh; ++ch)
        inputBuffer.clear(ch, 0, numSamples);

    // 5) Плагин
    juce::MidiBuffer midi;
    const auto t0 = juce::Time::getHighResolutionTicks();
    inst->processBlock(inputBuffer, midi);
    const auto t1 = juce::Time::getHighResolutionTicks();

    // 6) dry/wet
    dryBuffer.makeCopyOf(inputBuffer);
    wetBuffer.makeCopyOf(dryBuffer);

    if (lp && lp->isPreparedSuccessfully() && !lp->isShuttingDown())
        lp->process(wetBuffer);

    // 7) Микс и вывод
    for (int ch = 0; ch < numOutputChannels; ++ch)
    {
        auto* out = outputChannelData[ch];
        if (!out) continue;

        const int dryCh = (ch < dryBuffer.getNumChannels()) ? ch : 0;
        const int wetCh = (ch < wetBuffer.getNumChannels()) ? ch : 0;

        // базовый сигнал: плагин (dry)
        juce::FloatVectorOperations::copy(out, dryBuffer.getReadPointer(dryCh), numSamples);

        // добавляем лупер/плеер (wet)
        if (lp && lp->isPreparedSuccessfully() && !lp->isShuttingDown())
        {
            const bool looperIsPlaying = (lp->getMode() == LooperEngine::Mode::Looper)
                ? (lp->getState() == LooperEngine::Playing)
                : false;

            const bool playerIsPlaying = (lp->getMode() == LooperEngine::Mode::Player)
                ? lp->isPlaying() // транспорт плеера
                : false;

            if (looperIsPlaying || playerIsPlaying)
                juce::FloatVectorOperations::add(out, wetBuffer.getReadPointer(wetCh), numSamples);
        }
    }
    // 8) CPU метрика
    if (hostComponent != nullptr && currentSampleRate > 0.0)
    {
        const double blkSec = double(numSamples) / currentSampleRate;
        const double used = juce::Time::highResolutionTicksToSeconds(t1 - t0);
        hostComponent->updatePluginCpuLoad(blkSec > 0.0 ? used / blkSec : 0.0);
    }

    // 9) Финальный мастер — OutControl
    if (oc && !oc->isShuttingDown())
    {
        juce::AudioBuffer<float> masterBuffer(outputChannelData, numOutputChannels, numSamples);
        oc->processAudioBlock(masterBuffer);
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
    // 1) Снимаем параметры один раз
    currentSampleRate = device->getCurrentSampleRate();
    const int bufSize = device->getCurrentBufferSizeSamples();

    // 2) InputControl
    if (auto* ic = inputControl.load(std::memory_order_acquire))
    {
        if (!ic->isShuttingDown())
            ic->prepare(currentSampleRate, bufSize);
    }

    // 3) Тюнеры (под защитой)
    {
        std::lock_guard<std::mutex> lk(tunersMutex);
        for (auto* t : tuners)
            if (t) t->prepare(bufSize, currentSampleRate);
    }

    // 4) Плагин
    if (auto* inst = pluginInstance.load(std::memory_order_acquire))
        inst->prepareToPlay(currentSampleRate, bufSize);

    // 5) Вычисляем количество каналов и готовим внутренние буферы
    const int inCh = device->getActiveInputChannels().countNumberOfSetBits();
    const int outCh = device->getActiveOutputChannels().countNumberOfSetBits();
    const int maxCh = juce::jmax(inCh, outCh, 1); // минимум 1, чтобы не получить 0-канальные буферы

    dryBuffer.setSize(maxCh, bufSize, false, false, true);
    wetBuffer.setSize(maxCh, bufSize, false, false, true);
    dryBuffer.clear();
    wetBuffer.clear();

    // 6) Лупер
    if (auto* lp = looper.load(std::memory_order_acquire))
    {
        if (!lp->isShuttingDown())
            lp->prepare(currentSampleRate, bufSize);
    }

    // 7) OutControl (финальный мастер)
    if (auto* oc = outControl.load(std::memory_order_acquire))
    {
        if (!oc->isShuttingDown())
            oc->prepare(currentSampleRate, bufSize);
    }
}

void PluginProcessCallback::audioDeviceStopped()
{
    // 1) Плагин
    if (auto* inst = pluginInstance.load(std::memory_order_acquire))
        inst->releaseResources();

    // 2) Лупер (мягкий сброс состояния)
    if (auto* lp = looper.load(std::memory_order_acquire))
    {
        if (!lp->isShuttingDown())
            lp->reset();
    }

    // 3) Подчистим внутренние буферы (не обязательно, но безопасно)
    dryBuffer.clear();
    wetBuffer.clear();
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
//  setPlaerEngine – связываем движок плеера
//==============================================================================  
void PluginProcessCallback::setFilePlayerEngine(FilePlayerEngine* engine) noexcept
{
    filePlayerEngine = engine; // не владеем
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


