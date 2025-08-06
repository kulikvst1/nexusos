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
    auto r = getLocalBounds().toFloat();
    auto x = r.getX();
    auto w = r.getWidth();
    auto yTop = r.getY();
    auto yBot = r.getBottom();

    // фон
    g.fillAll(juce::Colours::darkgrey.darker(0.2f));

    // лямбда для конвертации линейного в dB + калибровка
    auto toDb = [&](float lin)
        {
            float db = juce::Decibels::gainToDecibels(lin, minDb);
            db += calibrationDb;                       // применяем смещение
            return juce::jlimit(minDb, maxDb, db);
        };

    // пересчитываем уровни с учётом калибровки
    auto lvlDb = toDb(levelLinear);
    auto peakDb = toDb(peakLinear);
    auto clipDb = toDb(clipPeakLinear);

    // экранные Y для 0 dB, уровня, пиков и зон
    float yZero = juce::jmap(0.0f, maxDb, minDb, yTop, yBot);
    float yLevel = juce::jmap(lvlDb, maxDb, minDb, yTop, yBot);
    float yPeak = juce::jmap(peakDb, maxDb, minDb, yTop, yBot);
    float yClip = juce::jmap(clipDb, maxDb, minDb, yTop, yBot);
    float yGreen = juce::jmap(-12.0f, maxDb, minDb, yTop, yBot);
    float yYellow = juce::jmap(-6.0f, maxDb, minDb, yTop, yBot);

    // создаём градиент для зон < 0 dB
    auto totalH = yBot - yZero;
    auto norm = [&](float yy) { return juce::jlimit(0.0f, 1.0f, (yBot - yy) / totalH); };
    float yMinus40 = juce::jmap(-40.0f, maxDb, minDb, yTop, yBot);

    juce::ColourGradient grad(
        juce::Colours::darkblue, 0.0f, float(yBot),
        juce::Colours::yellow, 0.0f, float(yZero),
        false
    );
    grad.addColour(norm(yMinus40), juce::Colours::blue);
    grad.addColour(norm(yGreen), juce::Colours::green);
    grad.addColour(norm(yYellow), juce::Colours::yellow);

    g.setGradientFill(grad);

    // заливаем до 0 dB или до текущего уровня
    float topY = lvlDb <= 0.0f ? yLevel : yZero;
    g.fillRect(x, topY, w, yBot - topY);

    // красная зона клипа, если > 0 dB
    if (lvlDb > 0.0f)
    {
        g.setColour(juce::Colours::red);
        g.fillRect(x, yLevel, w, yZero - yLevel);
    }

    // полупрозрачная область удержанного клипа
    if (clipPeakLinear > 0.0f)
    {
        auto clipRectF = juce::Rectangle<float>(x, yClip, w, yZero - yClip)
            .getIntersection(r);
        auto clipRectI = clipRectF.getSmallestIntegerContainer();
        g.setColour(juce::Colours::red.withAlpha(0.6f));
        g.fillRect(clipRectI);
    }

    // белая линия глобального пика
    g.setColour(juce::Colours::white);
    g.fillRect(x, yPeak - 1.0f, w, 2.0f);

    // рисуем dB-сетку
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    for (auto db : scaleMarks)
    {
        float y = juce::jmap(db, maxDb, minDb, yTop, yBot);
        g.drawHorizontalLine(int(y), x, x + w);
    }
}
