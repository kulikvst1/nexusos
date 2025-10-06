#ifndef CUSTOM_AUDIO_PLAYHEAD_H_INCLUDED
#define CUSTOM_AUDIO_PLAYHEAD_H_INCLUDED

#include <JuceHeader.h>
#include <cmath>

class CustomAudioPlayHead : public juce::AudioPlayHead
{
public:
    CustomAudioPlayHead()
        : active(false),
        currentBpm(120.0),
        currentSampleRate(48000.0) // Значение по умолчанию, можно обновить через setSampleRate()
    {
    }

    // Включает или отключает передачу позиции
    void setActive(bool newState) noexcept
    {
        active = newState;
    }

    // Устанавливает темп в BPM
    void setBpm(double newBpm) noexcept
    {
        currentBpm = newBpm;
    }

    // Возвращает текущий темп в BPM
    double getBpm() const noexcept
    {
        return currentBpm;
    }

    // Устанавливает текущую частоту дискретизации
    void setSampleRate(double newSampleRate) noexcept
    {
        currentSampleRate = newSampleRate;
    }

    // Реализация AudioPlayHead::getPosition()
    juce::Optional<juce::AudioPlayHead::PositionInfo> getPosition() const override
    {
        if (!active)
            return {};

        juce::AudioPlayHead::PositionInfo info;

        // Темп
        info.setBpm(currentBpm);

        // Время в секундах с момента запуска системы
        const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        info.setTimeInSeconds(now);

        // Время в сэмплах
        info.setTimeInSamples(static_cast<juce::int64>(now * currentSampleRate));

        // PPQ-позиция
        const double ppq = (now * currentBpm) / 60.0;
        info.setPpqPosition(ppq);

        // Позиция начала такта (4/4)
        info.setPpqPositionOfLastBarStart(ppq - std::fmod(ppq, 4.0));

        // Тактовая сигнатура 4/4
        juce::AudioPlayHead::TimeSignature ts{ 4, 4 };
        info.setTimeSignature(ts);

        // Неопределённый тип кадров
        info.setFrameRate(juce::AudioPlayHead::FrameRateType::fpsUnknown);

        // Воспроизведение активно, запись выключена
        info.setIsPlaying(true);
        info.setIsRecording(false);

        return info;
    }

private:
    bool active;
    double currentBpm;
    double currentSampleRate;
};

#endif // CUSTOM_AUDIO_PLAYHEAD_H_INCLUDED
