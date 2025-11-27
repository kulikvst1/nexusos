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
    juce::Font getTextButtonFont(juce::TextButton& /*button*/, int buttonHeight) override
    {
        return juce::Font(buttonHeight * 0.20f, juce::Font::bold);
    }

    // Шрифт для Label (~70% от высоты метки)
    juce::Font getLabelFont(juce::Label& label) override
    {
        return juce::Font(label.getHeight() * 0.70f, juce::Font::bold);
    }

    // Шрифт для ComboBox (~60% от высоты)
    juce::Font getComboBoxFont(juce::ComboBox& box) override
    {
        return juce::Font(box.getHeight() * 0.60f, juce::Font::bold);
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
    juce::Font getTextButtonFont(juce::TextButton& /*button*/, int buttonHeight) override
    {
        return juce::Font(buttonHeight * 0.50f, juce::Font::bold);
    }
};

// ==========================
// LookAndFeel для MIN/MAX слайдеров со стрелками
// ==========================
class InLookSliderArrow : public juce::LookAndFeel_V4
{
public:
    enum class Direction { ToStateFromLeft, ToStateFromRight };

    explicit InLookSliderArrow(Direction dir) : arrowDir(dir) {}

    int   trackThickness = 24;
    float arrowLength = 44.0f;   // длина в сторону STATE
    float arrowBaseWidth = 22.0f;   // “толщина” стрелки по Y
    float arrowInset = 1.0f;    // база вплотную к треку
    juce::Colour arrowColour = juce::Colours::blue; // ТОЛЬКО цвет стрелки

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float, float,
        const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearVertical)
            return;

        const float cx = (float)x + (float)width * 0.5f;
        const float y0 = (float)y;
        const float y1 = (float)(y + height);
        const float half = (float)trackThickness * 0.5f;

        // Разбиение трека на верх/низ относительно ручки — используем твои fill/outline из initPedalSlider
        const auto bottomColour = slider.findColour(juce::Slider::trackColourId);  // fill
        const auto topColour = slider.findColour(juce::Slider::thumbColourId);  // outline

        // Нижняя часть (от ручки вниз)
        g.setColour(bottomColour);
        g.fillRect(juce::Rectangle<float>(cx - half, sliderPos, (float)trackThickness, y1 - sliderPos));

        // Верхняя часть (от верха до ручки)
        g.setColour(topColour);
        g.fillRect(juce::Rectangle<float>(cx - half, y0, (float)trackThickness, sliderPos - y0));

        // Стрелка поверх
        drawLinearSliderThumb(g, x, y, width, height, sliderPos, 0.0f, 0.0f, style, slider);
    }

    void drawLinearSliderThumb(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float, float,
        const juce::Slider::SliderStyle style, juce::Slider&) override
    {
        if (style != juce::Slider::LinearVertical)
            return;

        const float cx = (float)x + (float)width * 0.5f;
        const float baseTop = sliderPos - arrowBaseWidth * 0.5f;
        const float baseBot = sliderPos + arrowBaseWidth * 0.5f;

        juce::Path p;

        if (arrowDir == Direction::ToStateFromLeft)
        {
            // MIN (слева): ВЕРШИНА справа от трека → смотрит В СТОРОНУ STATE
            const float apexX = cx + arrowLength;   // кончик к центру
            const float baseX = cx - arrowInset;    // база у трека
            p.addTriangle(apexX, sliderPos, baseX, baseTop, baseX, baseBot);
        }
        else
        {
            // MAX (справа): ВЕРШИНА слева от трека → смотрит В СТОРОНУ STATE
            const float apexX = cx - arrowLength;   // кончик к центру
            const float baseX = cx + arrowInset;    // база у трека
            p.addTriangle(apexX, sliderPos, baseX, baseTop, baseX, baseBot);
        }

        g.setColour(arrowColour); // Цвет ТОЛЬКО стрелки
        g.fillPath(p);
    }

private:
    Direction arrowDir;
};


class InLookSliderBar : public juce::LookAndFeel_V4
{
public:
    int trackThickness = 80; // ширина STATE

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                          const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearVertical)
            return;

        const float cx = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
        const float half = static_cast<float>(trackThickness) * 0.5f;

        // Фон
        juce::Rectangle<float> bg(
            cx - half,
            static_cast<float>(y),
            static_cast<float>(trackThickness),
            static_cast<float>(height)
        );
        g.setColour(slider.findColour(juce::Slider::backgroundColourId));
        g.fillRect(bg);

        // Заполненная часть (снизу вверх или сверху вниз — тут от sliderPos вниз)
        juce::Rectangle<float> fill(
            cx - half,
            sliderPos,
            static_cast<float>(trackThickness),
            static_cast<float>(y + height) - sliderPos
        );

        g.setColour(slider.findColour(juce::Slider::trackColourId));
        g.fillRect(fill);
    }

    void drawLinearSliderThumb(juce::Graphics&, int, int, int, int,
                               float, float, float,
                               const juce::Slider::SliderStyle, juce::Slider&) override
    {
        // thumb не рисуем — чистый бар
    }
};
