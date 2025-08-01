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
    //levelLinear = juce::jlimit(0.0f, 1.0f, newLevel);
    levelLinear = juce::jmax(0.0f, newLevel);
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
    auto area = getLocalBounds().toFloat();
    auto x = area.getX();
    auto width = area.getWidth();
    auto yTop = area.getY();
    auto yBottom = area.getBottom();

    // перевод уровня в dB
    auto levelDb = juce::Decibels::gainToDecibels(levelLinear, minDb);
    const float yYellowDb = -6.0f, yRedDb = 0.0f;

    // экранные координаты
    auto yYellow = juce::jmap(yYellowDb, maxDb, minDb, yTop, yBottom);
    auto yZero = juce::jmap(yRedDb, maxDb, minDb, yTop, yBottom);
    auto yLevel = juce::jmap(levelDb, maxDb, minDb, yTop, yBottom);

    // фон
    g.fillAll(juce::Colours::darkgrey.darker(0.2f));

    if (levelDb < yYellowDb)
    {
        // только зелёная зона до –6 dB
        g.setColour(juce::Colours::green);
        g.fillRect(x, yLevel, width, yBottom - yLevel);
    }
    else if (levelDb < yRedDb)
    {
        // зелёная от –6 dB вниз
        g.setColour(juce::Colours::green);
        g.fillRect(x, yYellow, width, yBottom - yYellow);

        // + жёлтая от уровня до –6 dB
        g.setColour(juce::Colours::yellow);
        g.fillRect(x, yLevel, width, yYellow - yLevel);
    }
    else
    {
        // зелёная от –6 dB вниз
        g.setColour(juce::Colours::green);
        g.fillRect(x, yYellow, width, yBottom - yYellow);

        // жёлтая от 0 dB до –6 dB
        g.setColour(juce::Colours::yellow);
        g.fillRect(x, yZero, width, yYellow - yZero);

        // и наконец красная от пика до 0 dB (включая ровно 0 dB)
        g.setColour(juce::Colours::red);
        g.fillRect(x, yLevel, width, yZero - yLevel);
    }

    // шкала
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    for (auto dB : scaleMarks)
    {
        float y = juce::jmap(dB, maxDb, minDb, yTop, yBottom);
        g.drawHorizontalLine(int(y), x, x + width);
    }
}



