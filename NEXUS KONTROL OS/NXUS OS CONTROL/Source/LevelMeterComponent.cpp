#include "LevelMeterComponent.h"
#include <JuceHeader.h>
#include <cmath> // для std::exp

LevelMeterComponent::LevelMeterComponent()
{
    startTimerHz(60);
    scaleMarks = { -60.0f, -20.0f, -12.0f, -6.0f, -3.0f, 0.0f };
}

LevelMeterComponent::~LevelMeterComponent()
{
    stopTimer();
}

void LevelMeterComponent::setLevel(float newLevel) noexcept
{
    levelLinear = juce::jmax(0.0f, newLevel);

    // обновляем глобальный пик
    if (levelLinear > peakLinear)
    {
        peakLinear = levelLinear;
        peakTs = juce::Time::getMillisecondCounterHiRes();
    }

    // обновляем пик клипа, если уровень в красной зоне (> 0 dB)
    auto lvlDb = juce::Decibels::gainToDecibels(levelLinear, minDb);
    if (lvlDb > 0.0f && levelLinear > clipPeakLinear)
    {
        clipPeakLinear = levelLinear;
        clipPeakTs = juce::Time::getMillisecondCounterHiRes();
    }
}

void LevelMeterComponent::setScaleMarks(const std::vector<float>& marks) noexcept
{
    scaleMarks = marks;
}

// реализация метода калибровки
void LevelMeterComponent::setCalibrationDb(float dbOffset) noexcept
{
    calibrationDb = dbOffset;
}

void LevelMeterComponent::timerCallback()
{
    auto now = juce::Time::getMillisecondCounterHiRes();

    // dt в секундах (защита при первом вызове)
    double dt = (lastTimerTs > 0.0) ? (now - lastTimerTs) * 0.001 : 0.0;
    lastTimerTs = now;

    // --- сглаживание displayLevelLinear (экспоненциальный фильтр) ---
    if (dt > 0.0)
    {
        // выбираем tau в секундах: attack для роста, release для спада
        double tau = (levelLinear > displayLevelLinear) ? (attackMs * 0.001) : (releaseMs * 0.001);

        if (tau <= 0.0)
            displayLevelLinear = levelLinear;
        else
        {
            double alpha = std::exp(-dt / tau);
            displayLevelLinear = static_cast<float>(alpha * displayLevelLinear + (1.0 - alpha) * levelLinear);
        }
    }
    else
    {
        // первый вызов таймера
        displayLevelLinear = levelLinear;
    }

    // --- логика пиков (оставляем по реальному линейному уровню) ---
    // сбрасываем глобальный пик по таймауту
    if (now - peakTs > peakHoldMs)
        peakLinear = levelLinear;

    // сбрасываем пик клипа по таймауту
    if (now - clipPeakTs > clipHoldMs)
        clipPeakLinear = 0.0f;

    repaint();
}

void LevelMeterComponent::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.fillAll(juce::Colours::black);

    auto toDb = [&](float lin)
        {
            float db = juce::Decibels::gainToDecibels(lin, minDb);
            return juce::jlimit(minDb, maxDb, db + calibrationDb);
        };

    // real dB для логики (без calibration) — по реальному уровню
    float realDb = juce::Decibels::gainToDecibels(levelLinear, minDb);
    float realPeakDb = juce::Decibels::gainToDecibels(peakLinear, minDb);
    float realClipDb = juce::Decibels::gainToDecibels(clipPeakLinear, minDb);

    // display dB для визуализации (используем сглаженный displayLevelLinear)
    float lvlDb = toDb(displayLevelLinear);
    float peakDb = toDb(peakLinear);
    float clipDb = toDb(clipPeakLinear);

    float yZero = LevelUtils::mapDbToY(0.0f + calibrationDb, maxDb, minDb, area);
    float yLevel = LevelUtils::mapDbToY(lvlDb, maxDb, minDb, area);
    float yPeak = LevelUtils::mapDbToY(peakDb, maxDb, minDb, area);
    float yClip = LevelUtils::mapDbToY(clipDb, maxDb, minDb, area);
    float yGreen = LevelUtils::mapDbToY(-12.0f + calibrationDb, maxDb, minDb, area);

    // зелёная зона
    juce::ColourGradient gradGreen(
        juce::Colours::darkgreen, area.getCentreX(), area.getBottom(),
        juce::Colours::green, area.getCentreX(), yGreen,
        false
    );
    g.setGradientFill(gradGreen);

    if (lvlDb <= -12.0f)
    {
        g.fillRect(area.getX(), yLevel,
            area.getWidth(), area.getBottom() - yLevel);
    }
    else
    {
        g.fillRect(area.getX(), yGreen,
            area.getWidth(), area.getBottom() - yGreen);
    }

    // жёлтая зона (-12 dB до 0 dB)
    if (realDb > -12.0f && realDb < 0.0f)
    {
        g.setColour(juce::Colours::yellow);
        if (yGreen > yLevel)
            g.fillRect(area.getX(), yLevel, area.getWidth(), yGreen - yLevel);
        else
            g.fillRect(area.getX(), yGreen, area.getWidth(), yLevel - yGreen);
    }
    else if (realDb > 0.0f)
    {
        g.setColour(juce::Colours::yellow);
        if (yGreen > yZero)
            g.fillRect(area.getX(), yZero, area.getWidth(), yGreen - yZero);
        else
            g.fillRect(area.getX(), yGreen, area.getWidth(), yZero - yGreen);
    }

    // красная зона (>= 0 dB)
    if (realDb >= 0.0f)
    {
        g.setColour(juce::Colours::red);

        float topY = std::min(yLevel, yZero);
        float bottomY = std::max(yLevel, yZero);

        if (bottomY - topY > 0.0f)
            g.fillRect(area.getX(), topY,
                area.getWidth(), bottomY - topY);
    }

    // удержанный клип-пик
    if (clipPeakLinear >= 0.0f)
    {
        auto clipArea = juce::Rectangle<float>(area.getX(), yClip,
            area.getWidth(), yZero - yClip)
            .getIntersection(area);
        g.setColour(juce::Colours::red.withAlpha(0.6f));
        g.fillRect(clipArea);
    }

    // белая линия пика
    g.setColour(juce::Colours::white);
    g.fillRect(area.getX(), yPeak - 1.0f,
        area.getWidth(), 2.0f);

    // сетка по меткам
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    for (float db : scaleMarks)
    {
        float y = LevelUtils::mapDbToY(db + calibrationDb, maxDb, minDb, area);
        g.drawHorizontalLine((int)y, area.getX(), area.getRight());
    }
}
