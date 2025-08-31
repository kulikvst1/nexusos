#pragma once

#include <JuceHeader.h>
#include "BinaryData.h"

// ==========================
// Основной LookAndFeel
// ==========================
class InLookAndFeel : public juce::LookAndFeel_V4
{
public:
    InLookAndFeel() = default;

    // Шрифт для обычных TextButton (~20% от высоты кнопки)
    juce::Font getTextButtonFont(juce::TextButton& /*button*/,
        int buttonHeight) override
    {
        juce::Font f(buttonHeight * 0.20f, juce::Font::bold);
        return f;
    }

    // Шрифт для Label (~70% от высоты метки)
    juce::Font getLabelFont(juce::Label& label) override
    {
        juce::Font f(label.getHeight() * 0.70f, juce::Font::bold);
        return f;
    }

    // Шрифт для ComboBox (~60% от высоты)
    juce::Font getComboBoxFont(juce::ComboBox& box) override
    {
        juce::Font f(box.getHeight() * 0.60f, juce::Font::bold);
        return f;
    }

    // Шрифт для PopupMenu
    juce::Font getPopupMenuFont() override
    {
        return juce::Font(14.0f, juce::Font::plain);
    }

    // Кастомная отрисовка рамки и стрелки ComboBox
    void drawComboBox(juce::Graphics& g,
        int width, int height,
        bool /*isButtonDown*/,
        int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
        juce::ComboBox& box) override
    {
        g.fillAll(box.findColour(juce::ComboBox::backgroundColourId));
        g.setColour(box.findColour(juce::ComboBox::outlineColourId));
        g.drawRect(0, 0, width, height, 1);

        juce::Path triangle;
        auto arrowArea = juce::Rectangle<float>(width - height, 0, height, height);

        triangle.addTriangle(
            arrowArea.getX() + 3.0f, arrowArea.getCentreY() - 3.0f,
            arrowArea.getRight() - 3.0f, arrowArea.getCentreY() - 3.0f,
            arrowArea.getCentreX(), arrowArea.getBottom() - 3.0f
        );

        g.setColour(box.findColour(juce::ComboBox::arrowColourId));
        g.strokePath(triangle, juce::PathStrokeType(2.0f));
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InLookAndFeel)
};

// ==========================
// LookAndFeel для кнопок пресетов
// ==========================
class InLookButon : public juce::LookAndFeel_V4
{
public:
    InLookButon() = default;

    // Шрифт для TextButton (~50% от высоты кнопки)
    juce::Font getTextButtonFont(juce::TextButton& /*button*/,
        int buttonHeight) override
    {
        juce::Font f(buttonHeight * 0.50f, juce::Font::bold);
        return f;
    }
};
