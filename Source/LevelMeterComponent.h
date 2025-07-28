#pragma once

#include <JuceHeader.h>
#include <vector>

class LevelMeterComponent : public juce::Component,
    private juce::Timer
{
public:
    LevelMeterComponent();
    ~LevelMeterComponent() override;

    // ������������� �������� ������� (0..1)
    void setLevel(float newLevel) noexcept;

    // ������ ����� � dB: �������� {-60, -20, -12, -6, -3, 0}
    void setScaleMarks(const std::vector<float>& marks) noexcept;

    void paint(juce::Graphics& g) override;
    void resized() override {}

private:
    void timerCallback() override;

    float levelLinear = 0.0f;
    std::vector<float> scaleMarks;

    // ������� �����
    static constexpr float maxDb = 6.0f;
    static constexpr float minDb = -60.0f;
};

