// CustomLookAndFeel.h
#pragma once
#include <JuceHeader.h>
#include "BinaryData.h"

class CustomLookAndFeelA : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeelA(){}
    
    // Øðèôò äëÿ îáû÷íûõ TextButton
    juce::Font getTextButtonFont(juce::TextButton& button,
        int buttonHeight) override
    {
        float fontSize = buttonHeight * 0.2f;
        juce::Font f(fontSize);
        f.setBold(true);
        return f;
    }

    // Øðèôò äëÿ ComboBox
    juce::Font getComboBoxFont(juce::ComboBox& box) override
    {
        float fontSize = box.getHeight() * 0.6f;
        juce::Font f(fontSize);
        f.setBold(true);
        return f;
    }

    // Øðèôò äëÿ PopupMenu (ïóíêòîâ ìåíþ)
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
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomLookAndFeelA)
};

// Îòäåëüíûé LookAndFeelA äëÿ êíîïîê ïðåñåòîâ
class CustomLookPresetButtons : public juce::LookAndFeel_V4
{
public:
    CustomLookPresetButtons() {}

    juce::Font getTextButtonFont(juce::TextButton& button,
        int buttonHeight) override
    {
        float fontSize = buttonHeight * 0.5f;
        juce::Font f(fontSize);
        f.setBold(true);
        return f;
    }
};
// Äëÿ BankName
class BankNameKomboBox : public juce::LookAndFeel_V4
{
public:
    BankNameKomboBox()
    {
        // Öâåò ïîäñâåòêè âûäåëåííîãî ïóíêòà ìåíþ
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colours::blue);
        // Öâåò òåêñòà â ìåíþ
        setColour(juce::PopupMenu::textColourId, juce::Colours::white);
    }

    // Øðèôò â ñàìîì ComboBox
    juce::Font getComboBoxFont(juce::ComboBox& box) override
    {
        juce::Font f(juce::jmax(16.0f, box.getHeight() * 0.75f));
        f.setBold(true);
        return f;
    }

    // Øðèôò âî âñïëûâàþùåì ìåíþ
    juce::Font getPopupMenuFont() override
    {
        return juce::Font(28.0f, juce::Font::plain);
    }

    // Ðàçìåðû ïóíêòîâ ìåíþ
    void getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator,
        int /*standardMenuItemHeight*/, int& idealWidth, int& idealHeight) override
    {
        auto font = getPopupMenuFont();
        // === Çäåñü ðåãóëèðóåòñÿ âûñîòà ñòðîêè ìåíþ ===
        idealHeight = isSeparator ? juce::roundToInt(font.getHeight() * 0.6f)
            : juce::roundToInt(font.getHeight() * 1.4f);
        idealWidth = font.getStringWidth(text) + 10;//îïðåäåëÿåò ìèíèìàëüíóþ øèðèíó âûïàäàþùåãî ìåíþ
    }

    // Öåíòðèðîâàíèå òåêñòà è îòðèñîâêà ãîëóáîé ïîäñâåòêè
    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
        bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
        bool hasSubMenu, const juce::String& text,
        const juce::String& shortcutKeyText,
        const juce::Drawable* icon, const juce::Colour* textColour) override
    {
        if (isSeparator)
        {
            g.setColour(juce::Colours::grey);
            g.fillRect(area.reduced(5, area.getHeight() / 2 - 1).withHeight(2));
            return;
        }

        if (isHighlighted)
            g.fillAll(findColour(juce::PopupMenu::highlightedBackgroundColourId));

        g.setColour(textColour != nullptr ? *textColour : findColour(juce::PopupMenu::textColourId));
        g.setFont(getPopupMenuFont());
        g.drawFittedText(text, area, juce::Justification::centred, 1);
    }
};

//Äëÿ SW êíîïîê
class CustomLookSWButtons : public juce::LookAndFeel_V4
{
public:
    CustomLookSWButtons() {}

    juce::Font getTextButtonFont(juce::TextButton& button,
        int buttonHeight) override
    {
        float fontSize = buttonHeight * 0.13f;
        juce::Font f(fontSize);
        f.setBold(true);
        return f;
    }
};