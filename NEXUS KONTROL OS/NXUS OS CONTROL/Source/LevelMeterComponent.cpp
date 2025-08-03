#include "LevelMeterComponent.h"

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

    // обновляем пик клипа, если уровень в красной зоне
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

void LevelMeterComponent::timerCallback()
{
    auto now = juce::Time::getMillisecondCounterHiRes();

    // сбрасываем глобальный пик
    if (now - peakTs > peakHoldMs)
        peakLinear = levelLinear;

    // сбрасываем пик клипа
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

    // dB-уровни → экранные Y
    auto lvlDb = juce::Decibels::gainToDecibels(levelLinear, minDb);
    auto peakDb = juce::Decibels::gainToDecibels(peakLinear, minDb);
    auto clipDb = juce::Decibels::gainToDecibels(clipPeakLinear, minDb);

    float yZero = juce::jmap(0.0f, maxDb, minDb, yTop, yBot);
    float yLevel = juce::jmap(lvlDb, maxDb, minDb, yTop, yBot);
    float yPeak = juce::jmap(peakDb, maxDb, minDb, yTop, yBot);
    float yClip = juce::jmap(clipDb, maxDb, minDb, yTop, yBot);
    float yGreen = juce::jmap(-12.0f, maxDb, minDb, yTop, yBot);
    float yYellow = juce::jmap(-6.0f, maxDb, minDb, yTop, yBot);

    // градиент для зон < 0 dB
    auto totalH = yBot - yZero;
    auto norm = [&](float y) { return juce::jlimit(0.0f, 1.0f, (yBot - y) / totalH); };
    juce::ColourGradient grad(
        juce::Colours::blue, 0.0f, float(yBot),
        juce::Colours::yellow, 0.0f, float(yZero),
        false
    );
    grad.addColour(norm(yGreen), juce::Colours::green);
    grad.addColour(norm(yYellow), juce::Colours::yellow);
    g.setGradientFill(grad);

    // рисуем заливку до 0 dB или текущего уровня
    float topY = lvlDb <= 0.0f ? yLevel : yZero;
    g.fillRect(x, topY, w, yBot - topY);

    // текущая красная зона клипа (<уровень > 0 dB>)
    if (lvlDb > 0.0f)
    {
        g.setColour(juce::Colours::red);
        g.fillRect(x, yLevel, w, yZero - yLevel);
    }

    // удержанная полупрозрачная красная область пика клипа
    if (clipPeakLinear > 0.0f)
    {
        g.setColour(juce::Colours::red.withAlpha(0.6f));
        g.fillRect(x, yClip, w, yZero - yClip);
    }

    // белая линия глобального пика
    g.setColour(juce::Colours::white);
    g.fillRect(x, yPeak - 1.0f, w, 2.0f);

    // сетка dB
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    for (auto db : scaleMarks)
    {
        float y = juce::jmap(db, maxDb, minDb, yTop, yBot);
        g.drawHorizontalLine(int(y), x, x + w);
    }
}
