#pragma once

#include <JuceHeader.h>
#include "BinaryData.h"

// ==========================
// Îñíîâíîé LookAndFeel
// ==========================
class InLookAndFeel : public juce::LookAndFeel_V4
{
public:
    InLookAndFeel() = default;

    // Øðèôò äëÿ îáû÷íûõ TextButton (~20% îò âûñîòû êíîïêè)
    juce::Font getTextButtonFont(juce::TextButton& /*button*/,
        int buttonHeight) override
    {
        juce::Font f(buttonHeight * 0.20f, juce::Font::bold);
        return f;
    }

    // Øðèôò äëÿ Label (~70% îò âûñîòû ìåòêè)
    juce::Font getLabelFont(juce::Label& label) override
    {
        juce::Font f(label.getHeight() * 0.70f, juce::Font::bold);
        return f;
    }

    // Øðèôò äëÿ ComboBox (~60% îò âûñîòû)
    juce::Font getComboBoxFont(juce::ComboBox& box) override
    {
        juce::Font f(box.getHeight() * 0.60f, juce::Font::bold);
        return f;
    }

    // Øðèôò äëÿ PopupMenu
    juce::Font getPopupMenuFont() override
    {
        return juce::Font(14.0f, juce::Font::plain);
    }

    // Êàñòîìíàÿ îòðèñîâêà ðàìêè è ñòðåëêè ComboBox
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
// LookAndFeel äëÿ êíîïîê ïðåñåòîâ
// ==========================
class InLookButon : public juce::LookAndFeel_V4
{
public:
    InLookButon() = default;

    // Øðèôò äëÿ TextButton (~50% îò âûñîòû êíîïêè)
    juce::Font getTextButtonFont(juce::TextButton& /*button*/,
        int buttonHeight) override
    {
        juce::Font f(buttonHeight * 0.50f, juce::Font::bold);
        return f;
    }
};
