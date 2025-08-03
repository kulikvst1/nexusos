#pragma once

#include <JuceHeader.h>
#include <vector>

class LevelMeterComponent : public juce::Component,
    private juce::Timer
{
public:
    LevelMeterComponent();
    ~LevelMeterComponent() override;

    // ”станавливаем текущий уровень [0..1]
    void setLevel(float newLevel) noexcept;

    // «адаЄм отметки шкалы в dB
    void setScaleMarks(const std::vector<float>& marks) noexcept;

    void paint(juce::Graphics& g) override;
    void resized() override {}

private:
    void timerCallback() override;

    float levelLinear = 0.0f;   // текущий уровень
    float peakLinear = 0.0f;   // глобальный пик
    double peakTs = 0.0;    // врем€ фиксации глобального пика
    static constexpr double peakHoldMs = 800.0;

    float clipPeakLinear = 0.0f;   // пик в красной зоне (>0 dB)
    double clipPeakTs = 0.0;    // врем€ фиксации клип-пика
    static constexpr double clipHoldMs = 800.0;

    std::vector<float> scaleMarks;
    static constexpr float maxDb = 6.0f, minDb = -60.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeterComponent)
};
