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
    // ������� ������ �� ��������� ����������, � ������� �������� �������� (�������� �� 0.0 �� 1.0)
    CpuLoadIndicator(std::atomic<double>& loadValueRef)
        : cpuLoad(loadValueRef)
    {
        // ��������� ��������� 10 ��� � ������� (������ 100 ��)
        startTimerHz(10);
    }

    ~CpuLoadIndicator() override
    {
        stopTimer();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        double load = cpuLoad.load(); // �������� �������� �� 0.0 �� 1.0

        // �������� ���� � ����������� �� ��������:
        // - ���� 33% � ������
        // - �� 33% �� 66% � ���������
        // - ���� 66% � �������
        juce::Colour fillColour;
        if (load < 0.33)
            fillColour = juce::Colours::limegreen;
        else if (load < 0.66)
            fillColour = juce::Colours::orange;
        else
            fillColour = juce::Colours::red;

        // ������ ��� ���������� (��������, ����-�����)
        g.fillAll(juce::Colours::darkgrey);

        // ��������� ����� �� ����������� ��������������� ��������
        float fillWidth = bounds.getWidth() * static_cast<float>(load);
        g.setColour(fillColour);
        g.fillRect(bounds.getX(), bounds.getY(), fillWidth, bounds.getHeight());

        // ������ ����� ������ ����������
        g.setColour(juce::Colours::black);
        g.drawRect(bounds, 2.0f);

        // ���������� �������� �������� ���� "xx.x% CPU"
        juce::String text = juce::String(load * 100.0, 1) + "% CPU";
        g.setColour(juce::Colours::white);
        g.setFont(14.0f); // ������ ������ ����� ������������
        g.drawText(text, bounds, juce::Justification::centred, true);
    }

private:
    // �� ������� �������� repaint() ��� ���������� UI
    void timerCallback() override
    {
        repaint();
    }

    std::atomic<double>& cpuLoad;
};
