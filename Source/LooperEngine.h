//==============================================================================
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
        isPrepared = true;       // теперь движок «готов»
        reset();                 // сбрасываем состояния
    }

    // Сброс лупа и состояния
    void reset()
    {
        if (!isPrepared)       // если ещё не prepared — выходим
            return;

        buffer.clear();
        recPos = 0;
        playPos = 0;
        loopStart = 0;
        loopEnd = 0;
        hasData = false;
        recTriggered = false;
        triggerEnabled = false;
        level = 1.0f;
        mix = 0.5f;
        state = Clean;
    }

    // «CTRL» кнопка (GUI-тред)
    void controlButtonPressed()
    {
        if (!isPrepared)       // защита до prepare()
            return;

        switch (state)
        {
        case Clean:
            recTriggered = !triggerEnabled;
            recPos = 0;
            state = Recording;
            break;

        case Recording:
            state = Stopped;
            hasData = (recPos > 0);
            loopStart = 0;
            loopEnd = recPos;
            playPos = loopStart;
            checkInvariants();
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

    // on/off «триггер по уровню»
    void setTriggerEnabled(bool t)
    {
        if (!isPrepared) return;
        triggerEnabled = t;
    }

    // уровень лупа
    void setLevel(float v)
    {
        if (!isPrepared) return;
        level = v;
    }

    // баланс микса (0…1)
    void setMix(float v)
    {
        if (!isPrepared) return;
        mix = v;
    }
    float getMix() const noexcept
    {
        return isPrepared ? mix : 0.0f;
    }

    // Основной аудиокаллбэк — поток Audio
    void process(juce::AudioBuffer<float>& inOut)
    {
        if (!isPrepared)   // ничего не делаем до prepare()
            return;

        jassert(sr > 0.0);
        const int numSamps = inOut.getNumSamples();
        const int chans = juce::jmin(inOut.getNumChannels(),
            buffer.getNumChannels());

        for (int ch = 0; ch < chans; ++ch)
        {
            auto* inPtr = inOut.getReadPointer(ch);
            auto* outPtr = inOut.getWritePointer(ch);
            auto* bufPtr = buffer.getWritePointer(ch);

            for (int i = 0; i < numSamps; ++i)
            {
                float inS = inPtr[i];
                float loopS = 0.0f;

                if (state == Recording)
                {
                    if (!recTriggered && std::abs(inS) > 0.001f)
                        recTriggered = true;

                    if (recTriggered && recPos < maxLen)
                        bufPtr[recPos++] = inS;

                    if (recPos >= maxLen)
                    {
                        recPos = maxLen;
                        state = Stopped;
                        hasData = true;
                        loopStart = 0;
                        loopEnd = recPos;
                        checkInvariants();
                    }
                }

                if (state == Playing && hasData)
                {
                    jassert(loopEnd > loopStart);
                    loopS = buffer.getSample(ch, playPos) * level;
                }

                outPtr[i] = inS * (1.0f - mix) + loopS * mix;

                if (state == Playing && hasData)
                {
                    if (++playPos >= loopEnd)
                        playPos = loopStart;
                }
            }
        }
    }

    // get-методы для UI
    State  getState()                const noexcept { return state; }
    int    getRecordedSamples()      const noexcept { return recPos; }
    int    getLoopLengthSamples()    const noexcept { return loopEnd - loopStart; }
    double getRecordedLengthSeconds()const noexcept { return sr > 0.0 ? recPos / sr : 0.0; }
    double getLoopLengthSeconds()    const noexcept { return sr > 0.0 ? (loopEnd - loopStart) / sr : 0.0; }
    double getPlayPositionSeconds()  const noexcept { return sr > 0.0 ? (playPos - loopStart) / sr : 0.0; }

    static constexpr double getMaxRecordSeconds() noexcept { return 300.0; }

    // Проверить, что движок подготовлен  
    bool isPreparedSuccessfully() const noexcept { return isPrepared; }
   
private:
   
    
    void checkInvariants() const
    {
        if (hasData)
            jassert(loopEnd > loopStart);
    }

    bool                     isPrepared = false;
    juce::AudioBuffer<float> buffer;
    int                      recPos = 0,
        playPos = 0,
        loopStart = 0,
        loopEnd = 0,
        maxLen = 0;
    double                   sr = 44100.0;
    bool                     hasData = false,
        triggerEnabled = false,
        recTriggered = false;
    float                    level = 1.0f,
        mix = 0.5f;
    State                    state = Clean;
};
