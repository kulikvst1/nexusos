#include "LevelMeterComponent.h"
#include <JuceHeader.h>

LevelMeterComponent::LevelMeterComponent()
{
    startTimerHz(30);
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
    // единое область рисования
    auto area = getLocalBounds().toFloat();

    // фон
    g.fillAll(juce::Colours::darkgrey.darker(0.2f));

    // лямбда для конвертации линейного уровня в dB + калибровка
    auto toDb = [&](float lin)
        {
            float db = juce::Decibels::gainToDecibels(lin, minDb);
            return juce::jlimit(minDb, maxDb, db + calibrationDb);
        };

    // рассчитываем текущие значения dB
    float lvlDb = toDb(levelLinear);
    float peakDb = toDb(peakLinear);
    float clipDb = toDb(clipPeakLinear);

    // маппинг dB в Y-координаты через утилиту
    float yZero = LevelUtils::mapDbToY(0.0f, maxDb, minDb, area);
    float yLevel = LevelUtils::mapDbToY(lvlDb, maxDb, minDb, area);
    float yPeak = LevelUtils::mapDbToY(peakDb, maxDb, minDb, area);
    float yClip = LevelUtils::mapDbToY(clipDb, maxDb, minDb, area);
    float yGreen = LevelUtils::mapDbToY(-12.0f, maxDb, minDb, area);
    float yYellow = LevelUtils::mapDbToY(-6.0f, maxDb, minDb, area);
    float yMinus40 = LevelUtils::mapDbToY(-40.0f, maxDb, minDb, area);

    // построение градиента зон < 0 dB
    float totalH = area.getBottom() - yZero;
    auto norm = [&](float yy)
        {
            return juce::jlimit(0.0f, 1.0f, (area.getBottom() - yy) / totalH);
        };

    juce::ColourGradient grad(
        juce::Colours::darkblue, area.getCentreX(), area.getBottom(),
        juce::Colours::yellow, area.getCentreX(), yZero,
        false
    );
    grad.addColour(norm(yMinus40), juce::Colours::blue);
    grad.addColour(norm(yGreen), juce::Colours::green);
    grad.addColour(norm(yYellow), juce::Colours::yellow);
    g.setGradientFill(grad);

    // основной fill: до текущего уровня или до 0 dB
    float fillTop = (lvlDb <= 0.0f) ? yLevel : yZero;
    g.fillRect(area.getX(), fillTop,
        area.getWidth(), area.getBottom() - fillTop);

    // красная клип-зона выше 0 dB
    if (lvlDb > 0.0f)
    {
        g.setColour(juce::Colours::red);
        g.fillRect(area.getX(), yLevel,
            area.getWidth(), yZero - yLevel);
    }

    // удержанный клип-пик
    if (clipPeakLinear > 0.0f)
    {
        auto clipArea = juce::Rectangle<float>(area.getX(), yClip,
            area.getWidth(), yZero - yClip)
            .getIntersection(area);
        g.setColour(juce::Colours::red.withAlpha(0.6f));
        g.fillRect(clipArea);
    }

    // белая линия пикового уровня
    g.setColour(juce::Colours::white);
    g.fillRect(area.getX(), yPeak - 1.0f,
        area.getWidth(), 2.0f);

    // dB-сетка по тем же меткам, что и LevelScaleComponent
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    for (float db : scaleMarks)
    {
        float y = LevelUtils::mapDbToY(db, maxDb, minDb, area);
        g.drawHorizontalLine(int(y), area.getX(), area.getRight());
    }
}
