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
        currentSampleRate(48000.0) // Значение по умолчанию, его можно обновить через setSampleRate()
    {
    }

    // Включает или отключает передачу позиции
    void setActive(bool newState)
    {
        active = newState;
    }

    // Устанавливает темп в BPM
    void setBpm(double newBpm)
    {
        currentBpm = newBpm;
    }

    // Устанавливает текущую частоту дискретизации
    void setSampleRate(double newSampleRate)
    {
        currentSampleRate = newSampleRate;
    }

    juce::Optional<juce::AudioPlayHead::PositionInfo> getPosition() const override
    {
        // Если позиционная информация не активна, возвращаем пустое значение.
        if (!active)
            return {};

        juce::AudioPlayHead::PositionInfo info;

        // Устанавливаем темп
        info.setBpm(currentBpm);

        // Получаем текущее время в секундах (время с момента запуска системы)
        const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        info.setTimeInSeconds(now);

        // Вычисляем время в сэмплах по формуле: timeInSamples = timeInSeconds * sampleRate
        info.setTimeInSamples(static_cast<juce::int64>(now * currentSampleRate));

        // Рассчитываем PPQ-позицию: (time in seconds * BPM) / 60
        const double ppq = (now * currentBpm) / 60.0;
        info.setPpqPosition(ppq);

        // Определяем позицию начала такта по схеме 4/4.
        info.setPpqPositionOfLastBarStart(ppq - std::fmod(ppq, 4.0));

        // Устанавливаем тактовую сигнатуру 4/4
        juce::AudioPlayHead::TimeSignature ts{ 4, 4 };
        info.setTimeSignature(ts);

        // Указываем, что тип кадров не определён
        info.setFrameRate(juce::AudioPlayHead::FrameRateType::fpsUnknown);

        // Сигнализируем, что воспроизведение активно, а запись выключена
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
