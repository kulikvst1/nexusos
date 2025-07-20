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
    levelLinear = juce::jlimit(0.0f, 1.0f, newLevel);
}

void LevelMeterComponent::setScaleMarks(const std::vector<float>& marks) noexcept
{
    scaleMarks = marks;
}

void LevelMeterComponent::timerCallback()
{
    repaint();
}

void LevelMeterComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(juce::Colours::darkgrey.darker(0.2f));

    if (bounds.isEmpty())
        return;

    // перевод линейного уровн€ в дЅ (отсечка minDb)
    float levelDb = juce::Decibels::gainToDecibels(levelLinear, minDb);

    // Y дл€ 0 dB и дл€ текущего уровн€:
    float yZero = juce::jmap<float>(0.0f, maxDb, minDb,
        bounds.getY(), bounds.getBottom());
    float yLevel = juce::jmap<float>(levelDb, maxDb, minDb,
        bounds.getY(), bounds.getBottom());

    // рисуем поверхность
    // Ч красна€ шапка выше 0 dB
    if (yZero > bounds.getY())
    {
        g.setColour(juce::Colours::red);
        g.fillRect(bounds.removeFromTop(yZero - bounds.getY()));
    }

    // Ч зелЄна€ часть ниже 0 dB
    g.setColour(juce::Colours::green);
    g.fillRect(bounds.removeFromTop(bounds.getBottom() - std::max(yLevel, yZero)));

    // рисуем шкалу меток
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    for (auto markDb : scaleMarks)
    {
        auto y = juce::jmap<float>(markDb, maxDb, minDb,
            getLocalBounds().getY(),
            getLocalBounds().getBottom());
        g.drawHorizontalLine(int(y),
            float(getLocalBounds().getX()),
            float(getLocalBounds().getRight()));
    }
}
