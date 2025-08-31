#pragma once

#include <JuceHeader.h>
#include <vector>
#include "LevelUtils.h"
class LevelMeterComponent : public juce::Component,
    private juce::Timer
{
public:
    LevelMeterComponent();
    ~LevelMeterComponent() override;

    // ������������� ������� ������� [0..1]
    void setLevel(float newLevel) noexcept;

    // ����� ������� ����� � dB
    void setScaleMarks(const std::vector<float>& marks) noexcept;

    // <-- ����� ����� ��� ��������� �������� ���������� � dB
    void setCalibrationDb(float dbOffset) noexcept;

    void paint(juce::Graphics& g) override;
    void resized() override {}
    /** ���������� true, ���� ��������� ��� > 0 dB (��������) */
    bool isClipping() const noexcept
    {
        // clipPeakLinear ������ ������������ ������� > 0 dB, ������������ �� ��������
        return clipPeakLinear > 0.0f;
    }
    void setDbRange(float newMinDb, float newMaxDb) noexcept
    {
        minDb = newMinDb;
        maxDb = newMaxDb;
        repaint();
    }
private:
    void timerCallback() override;

    float levelLinear = 0.0f;    // ������� �������
    float peakLinear = 0.0f;    // ���������� ���
    double peakTs = 0.0;     // ����� �������� ����������� ����
    static constexpr double peakHoldMs = 800.0;

    float clipPeakLinear = 0.0f;    // ��� � ������� ���� (>0 dB)
    double clipPeakTs = 0.0;     // ����� �������� ����-����
    static constexpr double clipHoldMs = 800.0;

    std::vector<float> scaleMarks;
    // <-- ���� ��� �������� ����������

    float calibrationDb = 0.0f;    // dB-���������� (0 = ��� ����������)
    float minDb = -60.0f, maxDb = 6.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeterComponent)
};
