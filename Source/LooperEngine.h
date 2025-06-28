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
        maxLen = int(sr * 300.0);   // 5 минут
        buffer.setSize(2, maxLen);
        reset();
    }

    // Сброс лупа и состояния
    void reset()
    {
        buffer.clear();
        recPos = 0;
        playPos = 0;
        loopLen = 0;           // ← теперь обнуляем зафиксированную длину петли
        hasData = false;
        recArmed = false;
        recTriggered = false;
        state = Clean;
    }

    // Нажата «контрольная» кнопка
    void controlButtonPressed()
    {
        switch (state)
        {
        case Clean:
            // переходим в режим записи
            state = Recording;
            hasData = false;
            recArmed = triggerEnabled;   // если триггер
            recTriggered = !triggerEnabled;
            recPos = 0;
            break;

        case Recording:
            // останавливаем запись
            state = Stopped;
            hasData = (recPos > 0);
            loopLen = recPos;
            recArmed = recTriggered = false;
            break;

        case Stopped:
            if (hasData)
                state = Playing;   // запускаем воспроизведение
            break;

        case Playing:
            state = Stopped;       // останавливаем воспроизведение
            break;
        }
    }

    // Включить/выключить «триггер»
    void setTriggerEnabled(bool t) { triggerEnabled = t; }

    // Уровень громкости лупа
    void setLevel(float v) { level = v; }

    // Основной аудиокалл-бек
    void process(juce::AudioBuffer<float>& inOut)
    {
        auto numSamps = inOut.getNumSamples();
        auto chans = juce::jmin(inOut.getNumChannels(), buffer.getNumChannels());

        for (int ch = 0; ch < chans; ++ch)
        {
            auto* src = inOut.getReadPointer(ch);
            auto* dst = inOut.getWritePointer(ch);

            for (int i = 0; i < numSamps; ++i)
            {
                float inS = src[i];
                float outS = inS;

                // запись
                if (state == Recording)
                {
                    if (!recTriggered)
                    {
                        // ждём появления сигнала
                        if (std::abs(inS) > 0.001f)
                            recTriggered = true;
                    }

                    if (recTriggered && recPos < maxLen)
                    {
                        buffer.setSample(ch, recPos, buffer.getSample(ch, recPos) + inS);
                        recPos++;
                    }
                    // лимит 5 мин
                    if (recPos >= maxLen)
                    {
                        state = Stopped;
                        hasData = true;
                        loopLen = recPos;
                    }
                }

                // воспроизведение
                if (state == Playing && hasData)
                {
                    outS = buffer.getSample(ch, playPos) * level;
                }

                // пишем на выход
                dst[i] = outS;

                // обновляем playPos только когда есть данные
                if (hasData && ++playPos >= loopLen)
                    playPos = 0;
            }
        }
    }

    State getState() const { return state; }

    // Сколько сэмплов уже записано в текущую запись (или во всеми время записи)
    int   getRecordedSamples() const noexcept { return recPos; }
    // Какая длина зафиксирована при остановке записи
    int   getLoopLengthSamples() const noexcept { return loopLen; }

    // Переводим в секунды:
    double getRecordedLengthSeconds() const noexcept { return sr > 0.0 ? recPos / sr : 0.0; }
    double getLoopLengthSeconds() const noexcept { return sr > 0.0 ? loopLen / sr : 0.0; }

    // вспомогательный геттер playPos в секундах
    int   getPlayPositionSamples() const noexcept { return playPos; }
    double getPlayPositionSeconds() const noexcept { return sr > 0.0 ? playPos / sr : 0.0; }

    // можно вернуть макс. длину записи (5 мин) в секундах
    static constexpr double getMaxRecordSeconds() noexcept { return 300.0; }

private:
    juce::AudioBuffer<float> buffer;
    int recPos = 0, playPos = 0, loopLen = 0, maxLen = 0;
    double sr = 44100.0;
    bool hasData = false, triggerEnabled = false;
    bool recArmed = false, recTriggered = false;
    float level = 1.0f;
    State state = Clean;
};
