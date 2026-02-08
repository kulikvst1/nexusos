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

    // Устанавливаем текущий уровень [0..1]
    void setLevel(float newLevel) noexcept;

    // Задаём отметки шкалы в dB
    void setScaleMarks(const std::vector<float>& marks) noexcept;

    // Новый метод для установки смещения калибровки в dB
    void setCalibrationDb(float dbOffset) noexcept;

    void paint(juce::Graphics& g) override;
    void resized() override {}

    /** Возвращает true, если накопился пик > 0 dB (клиппинг) */
    bool isClipping() const noexcept
    {
        // clipPeakLinear хранит максимальный уровень > 0 dB, сбрасывается по таймауту
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

    float levelLinear = 0.0f;      // текущий уровень
    float peakLinear = 0.0f;       // глобальный пик
    double peakTs = 0.0;           // время фиксации глобального пика
    static constexpr double peakHoldMs = 800.0;

    float clipPeakLinear = 0.0f;   // пик в красной зоне (>0 dB)
    double clipPeakTs = 0.0;       // время фиксации клип-пика
    static constexpr double clipHoldMs = 800.0;

    std::vector<float> scaleMarks; // отметки шкалы в dB
    float calibrationDb = 0.0f;    // dB-пристройка (0 = без калибровки)
    float minDb = -60.0f, maxDb = 6.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeterComponent)
};
