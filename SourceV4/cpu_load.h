#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <vector>

inline std::atomic<double> globalCpuLoad{ 0.0 };

class CpuLoadIndicator : public juce::Component, private juce::Timer
{
public:
    // Конструктор: можно оставить тот же таймер, например, 1 Гц
    CpuLoadIndicator(std::atomic<double>& loadValueRef)
        : cpuLoad(loadValueRef)
    {
        startTimerHz(5); // обновление раз в секунду
    }

    ~CpuLoadIndicator() override { stopTimer(); }

    // Метод обновления нового значения CPU загрузки
    void setPluginCpuLoad(double load)
    {
        double newLoad = load / 1.8;
        // Добавляем новое измерение в буфер
        samples.push_back(newLoad);
        // Если буфер превышает нужное количество значений, удаляем самое старое
        if (samples.size() > maxSamples)
            samples.erase(samples.begin());

        // Вычисляем среднее значение из буфера
        double sum = 0.0;
        for (double v : samples)
            sum += v;
        smoothedCpuLoad = sum / samples.size();

        cpuLoad.store(smoothedCpuLoad);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        double load = cpuLoad.load();
        juce::Colour fillColour;
        if (load < 0.55)
            fillColour = juce::Colours::limegreen;
        else if (load < 0.80)
            fillColour = juce::Colours::orange;
        else
            fillColour = juce::Colours::red;

        g.fillAll(juce::Colours::darkgrey);
        float fillWidth = bounds.getWidth() * static_cast<float>(load);
        g.setColour(fillColour);
        g.fillRect(juce::Rectangle<float>(bounds.getX(), bounds.getY(), fillWidth, bounds.getHeight()));
        g.setColour(juce::Colours::black);
        g.drawRect(bounds, 2.0f);

        juce::String text = juce::String((load / 1.5) * 100.0, 1) + "% CPU";
        g.setColour(juce::Colours::white);
        g.setFont(14.0f);
        g.drawText(text, bounds, juce::Justification::centred, true);
    }

private:
    void timerCallback() override { repaint(); }

    std::atomic<double>& cpuLoad;
    double smoothedCpuLoad = 0.0;

    // Буфер для хранения последних измерений
    std::vector<double> samples;
    const size_t maxSamples = 5; // размер окна скользящего среднего - можно подобрать оптимальное значение
};
