#pragma once
#include <JuceHeader.h>
#include <atomic>
//#include"plugin_process_callback.h"
// Объявляем глобальную переменную как inline (с C++17) 
inline std::atomic<double> globalCpuLoad{ 0.0 };

class CpuLoadIndicator : public juce::Component, private juce::Timer
{
public:
    void setPluginCpuLoad(double load)
    {
        cpuLoad.store(load / 1.8);
        
        repaint();
    }
    // Конструктор теперь принимает указатель
    CpuLoadIndicator(std::atomic<double>& loadValueRef) : cpuLoad(loadValueRef)
    {
        startTimerHz(10);
    }

    ~CpuLoadIndicator() override { stopTimer(); }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        double load = cpuLoad.load();
        juce::Colour fillColour;
        if (load < 0.55)
            fillColour = juce::Colours::limegreen;
        else if (load < 0.90)
            fillColour = juce::Colours::orange;
        else
            fillColour = juce::Colours::red;

        g.fillAll(juce::Colours::darkgrey);
        float fillWidth = bounds.getWidth() * static_cast<float>(load);
        g.setColour(fillColour);
        g.fillRect(juce::Rectangle<float>(bounds.getX(), bounds.getY(), fillWidth, bounds.getHeight()));
        g.setColour(juce::Colours::black);
        g.drawRect(bounds, 2.0f);
        juce::String text = juce::String((load / 1.8) * 100.0, 1) + "% CPU";
        g.setColour(juce::Colours::white);
        g.setFont(14.0f);
        g.drawText(text, bounds, juce::Justification::centred, true);
    }

private:
  void timerCallback() override { repaint(); }
    
    std::atomic<double>& cpuLoad;
};
