#include "LevelScaleComponent.h"
using namespace juce;
LevelScaleComponent::LevelScaleComponent()
{
    // по умолчанию студийные метки
    scaleMarks = { -60.0f, -20.0f, -12.0f, -6.0f, -3.0f, 0.0f };
}

void LevelScaleComponent::setScaleMarks(const std::vector<float>& marks) noexcept
{
    scaleMarks = marks;
    repaint();
}

void LevelScaleComponent::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.fillAll(juce::Colours::transparentBlack);

    if (b.getWidth() <= 0 || b.getHeight() <= 0)
        return;

    // большие цифры: 8% от высоты
    float fontH = b.getHeight() * 0.06f;
    g.setFont(juce::Font(fontH, juce::Font::bold));

    // линии полупрозрачные
    g.setColour(juce::Colours::white.withAlpha(0.4f));

    for (auto dB : scaleMarks)
    {
        float y = juce::jmap<float>(dB, maxDb, minDb,
            b.getY(), b.getBottom());
        g.drawHorizontalLine(int(y), b.getX(), b.getRight());

        // крупный текст по центру
        juce::String txt = juce::String((int)std::round(dB)) + " dB";
        auto textArea = juce::Rectangle<int>(
            (int)b.getX(),
            int(y - fontH * 0.5f),
            (int)b.getWidth(),
            int(fontH));
        g.setColour(juce::Colours::white);
        g.drawFittedText(txt, textArea, juce::Justification::centred, 1);
    }
}


