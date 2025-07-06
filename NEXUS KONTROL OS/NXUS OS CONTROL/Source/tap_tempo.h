#ifndef TAP_TEMPO_H
#define TAP_TEMPO_H

#include <JuceHeader.h>

/*
    Класс TapTempo реализует механизм «тап-темпо»: при каждом вызове метода tap() запоминается текущее время.
    Если это не первое нажатие, вычисляется интервал между текущим и предыдущим тапом.
    Затем берётся среднее значение по последним нескольким интервалам (например, 4) и на его основе считается BPM.
    Формула: BPM = (60 секунд * 1000 мс) / средний интервал в мс.
*/
class TapTempo
{
public:
    TapTempo() : lastTapTime(0.0), averageInterval(0.0), currentBpm(120.0) {}

    // Вызывайте этот метод при каждом нажатии кнопки tap tempo.
    void tap()
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (lastTapTime > 0.0)
        {
            double interval = now - lastTapTime; // интервал в миллисекундах
            intervals.add(interval);

            // Ограничиваем число интервалов, по которым считается среднее (максимум maxIntervals)
            if (intervals.size() > maxIntervals)
                intervals.remove(0);

            double sum = 0.0;
            for (auto i : intervals)
                sum += i;

            averageInterval = sum / intervals.size();

            // Вычисляем BPM: 60000 мс / средний интервал (мс)
            double calculatedBpm = 60000.0 / averageInterval;

            // Ограничиваем BPM в диапазоне от 40 до 350
            currentBpm = juce::jlimit(40.0, 350.0, calculatedBpm);
        }

        lastTapTime = now;
    }

    // Возвращает вычисленный BPM
    double getBpm() const { return currentBpm; }

    // Сброс состояния (например, при длинном промежутке без нажатий)
    void reset()
    {
        intervals.clear();
        lastTapTime = 0.0;
        averageInterval = 0.0;
        currentBpm = 120.0;
    }

private:
    double lastTapTime;
    juce::Array<double> intervals;
    double averageInterval;
    double currentBpm;
    const int maxIntervals = 4; // среднее по последним 4 интервалам
};

#endif // TAP_TEMPO_H
