
#include "plugin_process_callback.h"
#include "vst_host.h"

/*──────── глобальный FIFO ────────*/
EventBuffer        gEventBuf{};
juce::AbstractFifo gEventFifo{ static_cast<int>(gEventBuf.size()) };

/*──────── запасной атомик для «старого» конструктора ────────*/
static std::atomic<double> dummyCpu{ 0.0 };

/*──────── новые конструкторы ────────*/
PluginProcessCallback::PluginProcessCallback(juce::AudioPluginInstance* p,
    double                     sr,
    std::atomic<double>& cpuTarget) noexcept
    : pluginInstance(p),
    currentSR(sr),
    cpuLoadExt(cpuTarget)
{}

PluginProcessCallback::PluginProcessCallback(juce::AudioPluginInstance* p,
    double                     sr) noexcept
    : PluginProcessCallback(p, sr, dummyCpu) {}          // делегируем

/*──────── JUCE-8 колбэк с контекстом ────────*/
void PluginProcessCallback::audioDeviceIOCallbackWithContext(
    const float* const* inCh,
    int                                 nIn,
    float* const* outCh,
    int                                 nOut,
    int                                 numSamples,
    const juce::AudioIODeviceCallbackContext&)
{
    if (!pluginInstance) return;
    juce::ScopedNoDenormals _noDenormals;

    /* 1. GUI → Audio события */
    while (true)
    {
        int s1 = 0, sz1 = 0, s2 = 0, sz2 = 0;

        // ─── СТАРАЯ СТРОКА, которая даёт C2120 ───
        // if (gEventFifo.prepareToRead (1, s1, sz1, s2, sz2) == 0)
        //     break;

        // ─── НОВАЯ, совместимая и с JUCE-7, и с 8.0.7 ───
        gEventFifo.prepareToRead(1, s1, sz1, s2, sz2); // метод void
        if (sz1 == 0)                                   // очередь пуста
            break;

        const Event& e = gEventBuf[s1];
        if (e.type == EventType::Param)
            pluginInstance->setParameterNotifyingHost(e.param.index, e.param.value);
        else
            midi.addEvent(e.midi.data, e.midi.size,
                juce::jlimit(0, numSamples - 1, e.midi.sampleOffset));

        gEventFifo.finishedRead(1);
    }

    /* 2. оборачиваем AudioBuffer */
    const int ch = std::max(nIn, nOut);
    juce::AudioBuffer<float> buffer{ const_cast<float**>(nIn ? inCh : outCh),
                                      ch, numSamples };
    if (nIn == 0) buffer.clear();

    /* 3. processBlock + CPU-метрика */
    const auto t0 = juce::Time::getHighResolutionTicks();
    pluginInstance->processBlock(buffer, midi);
    const auto t1 = juce::Time::getHighResolutionTicks();
    midi.clear();

    if (++cpuCounter == 256)
    {
        cpuCounter = 0;
        const double blk = numSamples / currentSR;
        const double spent = juce::Time::highResolutionTicksToSeconds(t1 - t0);
        cpuLoadExt.store(blk > 0.0 ? spent / blk : 0.0,
            std::memory_order_relaxed);

        if (hostComponent)
            hostComponent->updatePluginCpuLoad(cpuLoadExt.load(
                std::memory_order_relaxed));
    }
}

/*──────── прокси-колбэк (без override) ────────*/
void PluginProcessCallback::audioDeviceIOCallback(const float* const* in,
    int                  nIn,
    float* const* out,
    int                  nOut,
    int                  numSamples)
{
    static const juce::AudioIODeviceCallbackContext dummy{};
    audioDeviceIOCallbackWithContext(in, nIn, out, nOut, numSamples, dummy);
}

/*──────── prepare / release ────────*/
void PluginProcessCallback::audioDeviceAboutToStart(juce::AudioIODevice* d)
{
    currentSR = d->getCurrentSampleRate();
    pluginInstance->prepareToPlay(currentSR, d->getCurrentBufferSizeSamples());
}
void PluginProcessCallback::audioDeviceStopped()
{
    if (pluginInstance) pluginInstance->releaseResources();
}
