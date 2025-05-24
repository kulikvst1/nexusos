/*
  ==============================================================================

    cpu_load.h
    Created: 23 May 2025 7:04:44pm
    Author:  111

  ==============================================================================
*/

#pragma once
#pragma once
#include <JuceHeader.h>
#include <atomic>
std::atomic<double> globalCpuLoad{ 0.0 };
class CpuLoadIndicator : public juce::Component, private juce::Timer
{
public:
    // Передаём ссылку на атомарную переменную, в которой хранится загрузка (значение от 0.0 до 1.0)
    CpuLoadIndicator(std::atomic<double>& loadValueRef)
        : cpuLoad(loadValueRef)
    {
        // Обновляем компонент 10 раз в секунду (каждые 100 мс)
        startTimerHz(10);
    }

    ~CpuLoadIndicator() override
    {
        stopTimer();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        double load = cpuLoad.load(); // значение загрузки от 0.0 до 1.0

        // Выбираем цвет в зависимости от загрузки:
        // - Ниже 33% – зелёный
        // - От 33% до 66% – оранжевый
        // - Выше 66% – красный
        juce::Colour fillColour;
        if (load < 0.33)
            fillColour = juce::Colours::limegreen;
        else if (load < 0.66)
            fillColour = juce::Colours::orange;
        else
            fillColour = juce::Colours::red;

        // Рисуем фон компонента (например, тёмно-серый)
        g.fillAll(juce::Colours::darkgrey);

        // Заполняем часть по горизонтали пропорционально загрузке
        float fillWidth = bounds.getWidth() * static_cast<float>(load);
        g.setColour(fillColour);
        g.fillRect(bounds.getX(), bounds.getY(), fillWidth, bounds.getHeight());

        // Рисуем рамку вокруг компонента
        g.setColour(juce::Colours::black);
        g.drawRect(bounds, 2.0f);

        // Отображаем цифровое значение вида "xx.x% CPU"
        juce::String text = juce::String(load * 100.0, 1) + "% CPU";
        g.setColour(juce::Colours::white);
        g.setFont(14.0f); // размер шрифта можно регулировать
        g.drawText(text, bounds, juce::Justification::centred, true);
    }

private:
    // По таймеру вызываем repaint() для обновления UI
    void timerCallback() override
    {
        repaint();
    }

    std::atomic<double>& cpuLoad;
};
