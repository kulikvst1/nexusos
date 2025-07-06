// LooperEngine.h
#pragma once

#include <JuceHeader.h>

class LooperEngine
{
public:
    enum State { Clean, Recording, Stopped, Playing };

    LooperEngine() = default;

    // Вызывать из AudioProcessor::prepareToPlay
    void prepare(double sampleRate, int /*maxBlockSize*/)
    {
        sr = sampleRate;
        maxLen = int(sr * getMaxRecordSeconds());
        buffer.setSize(2, maxLen);
        isPrepared = true;
        reset();
    }

    // Сброс (не трогаем triggerEnabled)
    void reset()
    {
        if (!isPrepared) return;
        buffer.clear();
        recPos = playPos = loopStart = loopEnd = 0;
        hasData = recTriggered = false;
        level = 1.0f;
        state = Clean;
    }

    // GUI-тред
    void controlButtonPressed()
    {
        if (!isPrepared) return;
        switch (state)
        {
        case Clean:
            recTriggered = !triggerEnabled;
            recPos = 0;
            state = Recording;
            break;

        case Recording:
            if (recPos > 0)
            {
                state = Stopped;
                hasData = true;
                loopStart = 0;
                loopEnd = recPos;
                playPos = loopStart;
                checkInvariants();
            }
            else
            {
                state = Clean; // пустая запись
            }
            break;

        case Stopped:
            if (hasData)
            {
                state = Playing;
                playPos = loopStart;
            }
            break;

        case Playing:
            state = Stopped;
            playPos = loopStart;
            break;
        }
    }

    // Параметры
    void setTriggerEnabled(bool t)   noexcept { if (isPrepared) triggerEnabled = t; }
    void setLevel(float v) noexcept { if (isPrepared) level = v; }
    void setTriggerThreshold(float t) noexcept { triggerThreshold = juce::jlimit(0.0f, 1.0f, t); }

    bool  getTriggerEnabled() const noexcept { return triggerEnabled; }
    float getTriggerThreshold() const noexcept { return triggerThreshold; }

    // Главный аудиокаллбэк
    void process(juce::AudioBuffer<float>& inOut)
    {
        if (!isPrepared) return;

        // быстрый dispatch по состоянию
        switch (state)
        {
        case Recording: processRecording(inOut); return;
        case Playing:   processPlaying(inOut); return;
        default:        processBypass(inOut); return;
        }
    }

    // UI-метаданные
    State getState()                  const noexcept { return state; }
    int   getRecordedSamples()        const noexcept { return recPos; }
    int   getLoopLengthSamples()      const noexcept { return loopEnd - loopStart; }
    double getRecordedLengthSeconds() const noexcept { return sr > 0 ? recPos / (double)sr : 0.0; }
    double getLoopLengthSeconds()     const noexcept { return sr > 0 ? (loopEnd - loopStart) / sr : 0.0; }
    double getPlayPositionSeconds()   const noexcept { return sr > 0 ? (playPos - loopStart) / sr : 0.0; }
    static constexpr double getMaxRecordSeconds() noexcept { return 300.0; }

    bool isPreparedSuccessfully() const noexcept { return isPrepared; }
    bool isTriggerArmed()         const noexcept { return triggerEnabled && state == Recording && !recTriggered; }
    bool isRecordingLive()        const noexcept { return state == Recording && recTriggered; }

private:
    //====================================================================
    void processRecording(juce::AudioBuffer<float>& io)
    {
        const int nSamps = io.getNumSamples();
        const int nCh = juce::jmin(io.getNumChannels(), buffer.getNumChannels());

        for (int i = 0; i < nSamps; ++i)
        {
            // сначала проверяем триггер и пишем все каналы
            bool justStarted = false;
            for (int ch = 0; ch < nCh; ++ch)
            {
                float x = io.getReadPointer(ch)[i];
                if (!recTriggered && std::abs(x) >= triggerThreshold)
                {
                    recTriggered = true; justStarted = true;
                }

                if (recTriggered && recPos < maxLen)
                    buffer.getWritePointer(ch)[recPos] = x;

                // passthrough
                io.getWritePointer(ch)[i] = x;
            }

            if (recTriggered)
                ++recPos;

            if (recPos >= maxLen)
            {
                recPos = maxLen;
                controlButtonPressed(); // закончить запись
                break;
            }
        }
    }

    void processPlaying(juce::AudioBuffer<float>& io)
    {
        const int nSamps = io.getNumSamples();
        const int nCh = juce::jmin(io.getNumChannels(), buffer.getNumChannels());

        for (int i = 0; i < nSamps; ++i)
        {
            // читаем один раз sample из буфера
            float gain = level;
            for (int ch = 0; ch < nCh; ++ch)
            {
                io.getWritePointer(ch)[i] = buffer.getSample(ch, playPos) * gain;
            }

            if (++playPos >= loopEnd)
                playPos = loopStart;
        }
    }

    void processBypass(juce::AudioBuffer<float>& io)
    {
        const int nSamps = io.getNumSamples();
        const int nCh = io.getNumChannels();

        for (int ch = 0; ch < nCh; ++ch)
            std::memcpy(io.getWritePointer(ch),
                io.getReadPointer(ch),
                sizeof(float) * nSamps);
    }

    void checkInvariants() const
    {
        if (hasData)
            jassert(loopEnd > loopStart);
    }

    //====================================================================
    bool                     isPrepared = false;
    juce::AudioBuffer<float> buffer;
    int recPos = 0, playPos = 0, loopStart = 0, loopEnd = 0, maxLen = 0;
    double sr = 44100.0;
    bool hasData = false, triggerEnabled = false, recTriggered = false;
    float level = 1.0f, triggerThreshold = 0.001f;
    State state = Clean;
};
