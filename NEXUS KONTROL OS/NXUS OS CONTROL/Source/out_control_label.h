#pragma once

#include <JuceHeader.h>
#include "BinaryData.h"

class OutLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OutLookAndFeel()
    {
        // Загрузка и рескейлинг изображения ползунка один раз
        constexpr int thumbW = 36, thumbH = 55;
        thumbImage = juce::ImageCache::getFromMemory(BinaryData::slider2_png,
            BinaryData::slider2_pngSize)
            .rescaled(thumbW, thumbH,
                juce::Graphics::highResamplingQuality);
    }

    // Шрифт для TextButton
    juce::Font getTextButtonFont(juce::TextButton& button,
        int buttonHeight) override
    {
        return { buttonHeight * 0.3f, juce::Font::bold };
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
    // Упрощённая кастомизация вертикальных слайдеров + дБ-теги
    void OutLookAndFeel::drawLinearSlider(juce::Graphics& g,
        int x, int y, int width, int height,
        float sliderPos,
        float /*minValue*/, float /*maxValue*/,
        const juce::Slider::SliderStyle style,
        juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearVertical)
        {
            LookAndFeel_V4::drawLinearSlider(g, x, y, width, height,
                sliderPos, 0.0f, 0.0f,
                style, slider);
            return;
        }

        // 1) СЕТКА: короткие тики + цифровые метки по 10 дБ
        {
            auto range = slider.getRange();               // –60…+12
            float tickLen = width * 0.2f;                    // 30% ширины
            float xs = x + (width - tickLen) * 0.5f;
            float xe = xs + tickLen;

            bool  isRight = slider.getName() == "right";
            int   labelW = 40;
            float labelX = isRight
                ? xe + 4.0f     // справа
                : xs - labelW - 4.0f; // слева
            auto  justif = isRight
                ? juce::Justification::centredLeft
                : juce::Justification::centredRight;

            g.setFont(12.0f);
            g.setColour(juce::Colours::white.withAlpha(0.6f));

            for (float db = std::ceil(range.getStart() / 10.0f) * 10.0f;
                db <= range.getEnd();
                db += 10.0f)
            {
                float p = 1.0f - (db - range.getStart())
                    / (range.getEnd() - range.getStart());
                float yPos = y + p * height;

                g.drawLine(xs, yPos, xe, yPos, 1.0f);

                g.setColour(juce::Colours::white);
                auto text = (db >= 0 ? "+" : "") + juce::String((int)db);
                g.drawText(text,
                    int(labelX), int(yPos - 8),
                    labelW, 16,
                    justif);

                g.setColour(juce::Colours::white.withAlpha(0.6f));
            }
        }

        // 2) СЛАЙДЕР: фон трека
        const int trackW = 4;
        float trackX = x + (width - trackW) * 0.5f;
        juce::Rectangle<float> trackArea(trackX, float(y), float(trackW), float(height));
        g.setColour(slider.findColour(juce::Slider::backgroundColourId));
        g.fillRect(trackArea);

        // 2b) СЛАЙДЕР: заполненная часть
        juce::Rectangle<float> fillArea = trackArea;
        fillArea.setY(sliderPos);
        fillArea.setHeight(float(y + height) - sliderPos);
        g.setColour(slider.findColour(juce::Slider::trackColourId));
        g.fillRect(fillArea);

        // 3) SKIN-THUMB: ваш PNG над всем
        if (thumbImage.isValid())
        {
            float tw = float(thumbImage.getWidth());
            float th = float(thumbImage.getHeight());
            float tx = x + (width - tw) * 0.5f;
            float ty = sliderPos - th * 0.5f;

            g.drawImage(thumbImage,
                juce::Rectangle<float>(tx, ty, tw, th),
                juce::RectanglePlacement::fillDestination);
        }
    }

    //--------------------------------------------------- 2
    // Корректная зона клика под thumb
    int getSliderThumbRadius(juce::Slider& slider) override
    {
        if (thumbImage.isValid())
            return thumbImage.getHeight() / 2;

        return LookAndFeel_V4::getSliderThumbRadius(slider);
    }

private:
    juce::Image thumbImage;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutLookAndFeel)
};
