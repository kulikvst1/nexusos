// CustomLookAndFeel.h
#pragma once
#include <JuceHeader.h>
#include "BinaryData.h"

class CustomLookAndFeelA : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeelA() {}

    // Шрифт для Label
    juce::Font getLabelFont(juce::Label& label) override
    {
        if (label.getName() == "volumeLabel")
            return juce::Font(25.0f, juce::Font::plain); // 🔹 уменьшенный размер

        return juce::Font(14.0f, juce::Font::plain); // дефолт для остальных
    }

    // Шрифт для обычных TextButton
    juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override
    {
        float fontSize = buttonHeight * 0.2f;
        juce::Font f(fontSize);
        f.setBold(true);
        return f;
    }

    // Шрифт для ComboBox
    juce::Font getComboBoxFont(juce::ComboBox& box) override
    {
        float fontSize = box.getHeight() * 0.6f;
        juce::Font f(fontSize);
        f.setBold(true);
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
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomLookAndFeelA)
};


// Отдельный LookAndFeelA для кнопок пресетов
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

// Для BankName
class BankNameKomboBox : public juce::LookAndFeel_V4
{
public:
    BankNameKomboBox()
    {
        // Цвет подсветки выделенного пункта меню
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colours::blue);
        // Цвет текста в меню
        setColour(juce::PopupMenu::textColourId, juce::Colours::white);
    }

    // Шрифт в самом ComboBox
    juce::Font getComboBoxFont(juce::ComboBox& box) override
    {
        juce::Font f(juce::jmax(16.0f, box.getHeight() * 0.75f));
        f.setBold(true);
        return f;
    }

    // Шрифт во всплывающем меню
    juce::Font getPopupMenuFont() override
    {
        return juce::Font(28.0f, juce::Font::plain);
    }

    // Размеры пунктов меню
    void getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator,
        int /*standardMenuItemHeight*/, int& idealWidth, int& idealHeight) override
    {
        auto font = getPopupMenuFont();
        // === Здесь регулируется высота строки меню ===
        idealHeight = isSeparator ? juce::roundToInt(font.getHeight() * 0.6f)
            : juce::roundToInt(font.getHeight() * 1.4f);
        idealWidth = font.getStringWidth(text) + 10; // определяет минимальную ширину выпадающего меню
    }

    // Центрирование текста и отрисовка голубой подсветки
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
/*
// Для SW кнопок
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
*/
// --- для кнопок с иконками ---
class CustomLookAndFeelFS : public juce::LookAndFeel_V4
{
public:
    juce::Drawable* iconFS = nullptr; // просто указатель, без unique_ptr
    float textScale = 0.15f;

    juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override
    {
        float fontSize = buttonHeight * textScale;
        juce::Font f(fontSize);
        f.setBold(true);
        return f;
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
        const juce::Colour& backgroundColour,
        bool, bool) override
    {
        LookAndFeel_V4::drawButtonBackground(g, button, backgroundColour, false, false);

        if (iconFS != nullptr)
        {
            auto bounds = button.getLocalBounds().toFloat();
            auto iconArea = bounds.withSizeKeepingCentre(bounds.getWidth() * 0.3f,
                bounds.getHeight() * 0.3f);

            juce::Colour iconColour = button.getToggleState() ? juce::Colours::white
                : juce::Colours::black;

            // если это SVG → перекрашиваем пути
            if (auto* comp = dynamic_cast<juce::DrawableComposite*>(iconFS))
            {
                auto& children = comp->getChildren();
                for (auto* child : children)
                {
                    if (auto* path = dynamic_cast<juce::DrawablePath*>(child))
                    {
                        path->setFill(button.getToggleState() ? juce::Colours::white
                            : juce::Colours::black);
                    }
                }
            }
            else if (auto* path = dynamic_cast<juce::DrawablePath*>(iconFS))
            {
                path->setFill(button.getToggleState() ? juce::Colours::white
                    : juce::Colours::black);
            }


            iconFS->drawWithin(g, iconArea, juce::RectanglePlacement::centred, 1.0f);
        }
    }

   
};
