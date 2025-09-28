//v2
#pragma once
#include <JuceHeader.h>
#include <vector>
#include <atomic>
#include <cmath>
#include <cstring>

// ====================== FirstOrderSmoother (мягкое тянущееся значение) ======================
class FirstOrderSmoother
{
public:
    void prepare(float smoothingTimeMs, double sr)
    {
        jassert(sr > 0.0 && smoothingTimeMs > 0.0f);
        sampleRate = sr;
        const float twoPi = 6.28318530717958647692f;
        a = std::exp(-twoPi / (smoothingTimeMs * 0.001f * (float)sampleRate));
        b = 1.0f - a;
        z = 0.0f;
    }

    inline float process(float target) noexcept
    {
        z = target * b + z * a;
        return z;
    }

    void reset(float start = 0.0f) noexcept { z = start; }

private:
    double sampleRate{ 44100.0 };
    float a{ 0.0f }, b{ 0.0f }, z{ 0.0f };
};

// ====================== SingleChannelDelay (наш одно-канальный delay с +2 margin) ======================
class SingleChannelDelay
{
public:
    void prepare(double sr, int maximumBlockSize, double maxDelayMs)
    {
        juce::ignoreUnused(maximumBlockSize);
        jassert(sr > 0.0 && maxDelayMs > 0.0);

        sampleRate = sr;
        requestedMaxDelaySamples = (int)std::ceil(sampleRate * (maxDelayMs / 1000.0));
        bufferSize = juce::jmax(1, requestedMaxDelaySamples) + 2; // явный +2 под интерполяцию

        buffer.assign((size_t)bufferSize, 0.0f);
        writePos = 0;

        smoother.prepare(200.0f, sampleRate); // достаточно быстрая реакция на твисты
        smoother.reset(0.0f);

        targetDelaySamples = 0.0f;
        delaySamples = 0.0f;

        prepared = true;

#if JUCE_DEBUG
        DBG(juce::String::formatted("[SCD] prepare: sr=%.1f maxDelayMs=%.1f requestedMax=%d bufSize=%d",
            sampleRate, maxDelayMs, requestedMaxDelaySamples, bufferSize));
#endif
    }

    void reset()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        smoother.reset(0.0f);
        targetDelaySamples = 0.0f;
        delaySamples = 0.0f;
        // prepared остаётся true — буфер готов; если хочешь — можешь опустить в false.
    }

    void setDelayMs(float ms) noexcept
    {
        // FIX: защищаемся от неготового состояния и невалидной SR
        if (!prepared || sampleRate <= 0.0)
            return;

        const float smps = juce::jmax(0.0f, ms) * (float)sampleRate / 1000.0f;

        // FIX: hi не может быть отрицательным
        const int   hiSamples = juce::jmax(0, requestedMaxDelaySamples);
        const float hi = (float)hiSamples;

        targetDelaySamples = juce::jlimit(0.0f, hi, smps);
    }

    // Процессим массив (одно-канальный)
    void processBuffer(const float* in, float* out, int numSamples) noexcept
    {
        if (!prepared || in == nullptr || out == nullptr || numSamples <= 0)
        {
            if (in && out && numSamples > 0)
                std::memcpy(out, in, (size_t)numSamples * sizeof(float));
            return;
        }

        for (int i = 0; i < numSamples; ++i)
        {
            // сглаженная задержка, кламп по нашему явному максимуму
            delaySamples = smoother.process(targetDelaySamples);
            if (!std::isfinite(delaySamples)) delaySamples = 0.0f;

            // FIX: безопасный верхний предел
            const float hi = (float)juce::jmax(0, requestedMaxDelaySamples);
            delaySamples = juce::jlimit(0.0f, hi, delaySamples);

            // запись
            buffer[(size_t)writePos] = in[i];

            // чтение с дробной задержкой
            float readPos = (float)writePos - delaySamples;
            while (readPos < 0.0f) readPos += (float)bufferSize;

            const int   i0 = (int)std::floor(readPos);
            const int   i1 = (i0 + 1) % bufferSize;
            const float frac = readPos - (float)i0;

            const float s0 = buffer[(size_t)i0];
            const float s1 = buffer[(size_t)i1];

            float y = s0 + (s1 - s0) * frac;
            if (std::fabs(y) < 1.0e-30f) y = 0.0f; // денормалы

            out[i] = y;

            writePos = (writePos + 1) % bufferSize;
        }
    }

private:
    double sampleRate{ 44100.0 };
    int requestedMaxDelaySamples{ 1 };
    int bufferSize{ 1 };

    std::vector<float> buffer;
    int writePos{ 0 };

    FirstOrderSmoother smoother;
    float targetDelaySamples{ 0.0f };
    float delaySamples{ 0.0f };

    bool prepared{ false };
};

// ====================== SimpleLFO ======================
class SimpleLFO
{
public:
    void prepare(double sr) noexcept
    {
        sampleRate = (sr > 0.0 ? sr : 44100.0);
        setFrequencyHz(freqHz);
        phase = 0.0;
    }

    void setFrequencyHz(float f) noexcept
    {
        freqHz = juce::jmax(0.0f, f);
        const double fclamped = juce::jlimit(0.0, 20.0, (double)freqHz);
        incr = (sampleRate > 0.0 ? juce::MathConstants<double>::twoPi * fclamped / sampleRate : 0.0);
    }

    inline float current() const noexcept { return std::sin((float)phase); }

    inline void advance(int samples) noexcept
    {
        phase += incr * (double)samples;
        // держим фазу в [0, 2π) без частых ветвлений
        if (phase >= juce::MathConstants<double>::twoPi)
            phase = std::fmod(phase, juce::MathConstants<double>::twoPi);
    }

    void setPhaseRadians(double p) noexcept
    {
        // нормализуем
        phase = std::fmod(std::fmod(p, juce::MathConstants<double>::twoPi) + juce::MathConstants<double>::twoPi,
            juce::MathConstants<double>::twoPi);
    }

private:
    double sampleRate{ 44100.0 };
    double phase{ 0.0 };
    double incr{ 0.0 };
    float  freqHz{ 0.35f };
};

// ====================== Doubler: L=dry, R=wet (true crossfade только в R) ======================
class Doubler
{
public:
    // Предпочтительная сигнатура под твой вызов: doubler.prepare(spec);
    void prepare(const juce::dsp::ProcessSpec& spec, double maxDelayMs = 200.0)
    {
        jassert(spec.sampleRate > 0.0);
        jassert(spec.maximumBlockSize > 0);

        sampleRate = spec.sampleRate;

        // DRY-буфер размером под текущий spec
        dry.setSize((int)spec.numChannels, (int)spec.maximumBlockSize, false, false, true);

        // Одно-канальный delay под WET-канал
        delay.prepare(sampleRate, (int)spec.maximumBlockSize, maxDelayMs);

        // LFO подготовка
        lfo.prepare(sampleRate);
        lfo.setFrequencyHz(lfoRateHz.load(std::memory_order_relaxed));

        // FIX: prepared ставим в true только после полностью валидной подготовки
        prepared.store(true, std::memory_order_release);

        setDelayMs(defaultDelayMsMs);
        setMix(1.0f);
        setBypass(false);

#if JUCE_DEBUG
        DBG(juce::String::formatted("[Doubler] prepare: sr=%.1f maxBlock=%u inCh=%u",
            sampleRate, spec.maximumBlockSize, spec.numChannels));
#endif
    }

    void reset()
    {
        delay.reset();
        dry.clear();
        lfo.setPhaseRadians(0.0);
    }

    // Управление
    void setBypass(bool b)           noexcept { bypass.store(b, std::memory_order_relaxed); }
    void setMix(float v)             noexcept { mix.store(juce::jlimit(0.0f, 1.0f, v), std::memory_order_relaxed); }
    void setLFOEnabled(bool e)       noexcept { lfoEnabled.store(e, std::memory_order_relaxed); }
    void setLFORateHz(float hz)      noexcept
    {
        lfoRateHz.store(juce::jlimit(0.0f, 20.0f, hz), std::memory_order_relaxed);
        if (prepared.load(std::memory_order_acquire))
            lfo.setFrequencyHz(lfoRateHz.load(std::memory_order_relaxed));
    }
    void setLFODepthMs(float ms)     noexcept { lfoDepthMs.store(juce::jmax(0.0f, ms), std::memory_order_relaxed); }

    // FIX: безопасный setDelayMs (никаких клампов с “ломаными” границами)
    void setDelayMs(float ms) noexcept
    {
        // Если уже сваливаемся — игнор, чтобы не трогать внутреннее состояние
        if (shuttingDown.load(std::memory_order_relaxed))
            return;

        // В момент закрытия prepare мог уже не действовать — не обращаемся в delay
        if (!prepared.load(std::memory_order_acquire))
        {
            delayMs.store(juce::jmax(0.0f, ms), std::memory_order_relaxed);
            return;
        }

        const float safeMs = juce::jmax(0.0f, ms);
        delayMs.store(safeMs, std::memory_order_relaxed);
        delay.setDelayMs(safeMs); // внутри SCD — безопасный кламп
    }

    // Позволяет хосту “погасить” обработчик на финальном тике
    void setShuttingDown(bool b) noexcept { shuttingDown.store(b, std::memory_order_release); }

    // Главный процесс
    void process(juce::AudioBuffer<float>& buffer)
    {
        if (shuttingDown.load(std::memory_order_relaxed))
            return;

        if (!prepared.load(std::memory_order_acquire) || bypass.load(std::memory_order_relaxed))
            return;

        const int nCh = buffer.getNumChannels();
        const int nSm = buffer.getNumSamples();
        if (nCh <= 0 || nSm <= 0) return;

        // Гарантируем размер DRY-буфера под вход (безопасно при изменении блоков/каналов)
        if (dry.getNumChannels() != nCh || dry.getNumSamples() < nSm)
            dry.setSize(nCh, nSm, false, false, true);

        // DRY-копия всего входа
        dry.makeCopyOf(buffer, true);

        const bool stereo = (nCh >= 2);
        const int  leftOutCh = 0;
        const int  desiredWetCh = stereo ? 1 : 0;   // куда кладём WET

        // FIX: индексы безопасны (но у нас уже есть ранний return при nCh<=0)
        const int  safeWetCh = juce::jlimit(0, nCh - 1, desiredWetCh);

        // 1) Левый канал — всегда DRY (если стерео)
        if (stereo)
            buffer.copyFrom(leftOutCh, 0, dry, leftOutCh, 0, nSm);

        // 2) Правый (или единственный) канал — прогоняем через delay, с LFO‑модуляцией (чанки)
        const float* inWet = dry.getReadPointer(safeWetCh);
        float* outWet = buffer.getWritePointer(safeWetCh);

        if (inWet != nullptr && outWet != nullptr)
        {
            const float tBaseMs = delayMs.load(std::memory_order_relaxed);
            const float depthMs = lfoDepthMs.load(std::memory_order_relaxed);
            const bool  useLFO = lfoEnabled.load(std::memory_order_relaxed) && depthMs > 0.0f;

            if (!useLFO)
            {
                // без модуляции — быстрый путь
                delay.setDelayMs(tBaseMs);
                delay.processBuffer(inWet, outWet, nSm);
            }
            else
            {
                // медленный плавный дрейф — обновляем целевую задержку порционно
                constexpr int chunk = 32; // баланс точности и стоимости
                int pos = 0;
                while (pos < nSm)
                {
                    const int len = juce::jmin(chunk, nSm - pos);
                    const float lfoNow = lfo.current();           // [-1..1]
                    const float tNowMs = juce::jmax(0.0f, tBaseMs + lfoNow * depthMs);

                    delay.setDelayMs(tNowMs);
                    delay.processBuffer(inWet + pos,
                        outWet + pos,
                        len);

                    lfo.advance(len);
                    pos += len;
                }
            }
        }

        // 3) MIX: true crossfade ТОЛЬКО в выбранном wet-канале
        const float w = mix.load(std::memory_order_relaxed);
        const float d = 1.0f - w;

        {
            float* out = buffer.getWritePointer(safeWetCh);
            const float* dr = dry.getReadPointer(safeWetCh);
            if (out && dr)
            {
                for (int i = 0; i < nSm; ++i)
                    out[i] = out[i] * w + dr[i] * d;
            }
        }

        // Прочие каналы (кроме safeWetCh) уже DRY (мы их не трогали)
    }

private:
    double sampleRate{ 44100.0 };

    SingleChannelDelay       delay;
    juce::AudioBuffer<float> dry;

    // Параметры задержки/микса/жизненного цикла
    std::atomic<float> delayMs{ 18.0f };      // по умолчанию слышимый Haas
    std::atomic<float> mix{ 1.0f };           // 1 = только wet в правом, левый всегда dry
    std::atomic<bool>  bypass{ false };
    std::atomic<bool>  prepared{ false };     // FIX: сделал атомарным
    std::atomic<bool>  shuttingDown{ false }; // FIX: мягкое гашение на выходе

    // LFO-параметры
    SimpleLFO           lfo;
    std::atomic<bool>   lfoEnabled{ true };
    std::atomic<float>  lfoRateHz{ 0.35f };
    std::atomic<float>  lfoDepthMs{ 2.0f };

    static constexpr float defaultDelayMsMs = 18.0f;
};
