#pragma once

#include <JuceHeader.h>
#include "BinaryData.h"

class OutLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OutLookAndFeel()
    {
        // Загрузка и рескейлинг изображения ползунка один раз
        constexpr int thumbW = 26, thumbH = 45;
        thumbImage = juce::ImageCache::getFromMemory(BinaryData::slider_png,
            BinaryData::slider_pngSize)
            .rescaled(thumbW, thumbH,
                juce::Graphics::highResamplingQuality);

    }

    // Шрифт для TextButton
    juce::Font getTextButtonFont(juce::TextButton& button,
        int buttonHeight) override
    {
        return { buttonHeight * 0.5f, juce::Font::bold };
    }

    // Шрифт для ComboBox
    juce::Font getComboBoxFont(juce::ComboBox& box) override
    {
        return { box.getHeight() * 0.6f, juce::Font::bold };
    }

    // Шрифт для PopupMenu
    juce::Font getPopupMenuFont() override
    {
        return { 14.0f, juce::Font::plain };
    }

    // Кастомная отрисовка рамки и стрелки ComboBox
    void drawComboBox(juce::Graphics& g,
        int width, int height,
        bool /*isButtonDown*/,
        int /*buttonX*/, int /*buttonY*/,
        int /*buttonW*/, int /*buttonH*/,
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

    // Упрощённая кастомизация вертикальных слайдеров
    void drawLinearSlider(juce::Graphics& g,

        int x, int y, int width, int height,
        float sliderPos,
        float /*minValue*/, float /*maxValue*/,
        const juce::Slider::SliderStyle style,
        juce::Slider& slider) override
    {
        // 1) Всегда отрисовываем стандартный трек (фон + заполненную часть)
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height,
            sliderPos, 0.0f, 0.0f,
            style, slider);

        // 2) Если это вертикальный слайдер и картинка загружена — рисуем свой thumb
        if (style == juce::Slider::LinearVertical && thumbImage.isValid())
        {
            const int thumbW = thumbImage.getWidth();
            const int thumbH = thumbImage.getHeight();

            // Центрируем по горизонтали и выстраиваем по вертикали
            float thumbX = (float)x + (width - thumbW) * 0.5f;
            float thumbY = sliderPos - thumbH * 0.5f;

            g.drawImage(thumbImage,
                juce::Rectangle<float>(thumbX, thumbY, thumbW, thumbH),
                juce::RectanglePlacement::fillDestination);
        }
    }
    /// Возвращает радиус thumb-контроллера (для корректной зоны клика и hit-теста)
    int getSliderThumbRadius(juce::Slider& slider) override
    {
        if (thumbImage.isValid())
        {
            // возвращаем половину высоты картинки
            return thumbImage.getHeight() / 1.8;
        }

        // иначе — дефолтный радиус из базового LookAndFeel
        return LookAndFeel_V4::getSliderThumbRadius(slider);
    }
private:
    juce::Image thumbImage;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutLookAndFeel)
};
