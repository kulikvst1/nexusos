// CustomLookAndFeel.h
#pragma once
#include <JuceHeader.h>
#include "BinaryData.h"

class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel()
    {
        // Настройка базовых цветов для слайдеров
        setColour(juce::Slider::backgroundColourId, juce::Colours::darkgrey);
        setColour(juce::Slider::trackColourId, juce::Colours::grey);
        setColour(juce::Slider::thumbColourId, juce::Colours::silver);
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::lightblue);
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::darkblue);
    }

    // Шрифт для обычных TextButton
    juce::Font getTextButtonFont(juce::TextButton& button,
        int buttonHeight) override
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

    // Шрифт для PopupMenu (пунктов меню)
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

    // ================================
    // Секция: кастомизация JUCE-слайдеров
    // ================================

    // Радиус больше не нужен, но оставим минимальный, чтобы ничего не поломалось
    void drawLinearSlider(juce::Graphics& g,
        int x, int y, int width, int height,
        float sliderPos,
        float /*min*/, float /*max*/,
        const juce::Slider::SliderStyle style,
        juce::Slider& slider) override
    {
        // Если стиль не вертикальный — используем стандартную отрисовку
        if (style != juce::Slider::LinearVertical)
        {
            LookAndFeel_V4::drawLinearSlider(
                g, x, y, width, height,
                sliderPos, 0.0f, 0.0f,
                style, slider
            );
            return;
        }

        // 1) Рисуем трек по центру с плавной прозрачностью
        constexpr int trackW = 3;
        int trackX = x + (width - trackW) / 2;
        g.setColour(
            slider.findColour(juce::Slider::backgroundColourId)
            .withAlpha(0.6f)
        );
        g.fillRect(trackX, y + 4, trackW, height - 8);

        // 2) Задаём желаемый размер картинки-ручки
        //    Здесь — 16×16 пикселей, можно менять под свой PNG
        constexpr int thumbW = 26;
        constexpr int thumbH = 26;

        // 3) Загружаем и один раз ресайзим изображение для скорости
        static const auto thumbImage =
            juce::ImageCache::getFromMemory(
                BinaryData::slider_png, BinaryData::slider_pngSize
            ).rescaled(thumbW, thumbH,
                juce::Graphics::highResamplingQuality);

        // 4) Вычисляем позицию так, чтобы центрировать картинку по X
        int thumbX = x + (width - thumbW) / 2;
        //    Смещаем по Y на позицию слайдера и корректируем по половине высоты
        int thumbY = int(sliderPos - thumbH * 0.5f);

        // 5) Создаём прямоугольник для отрисовки вручную
        juce::Rectangle<float> thumbRect{
            float(thumbX),  // левый верхний угол X
            float(thumbY),  // левый верхний угол Y
            float(thumbW),  // ширина
            float(thumbH)   // высота
        };

        // 6) Рисуем картинку в нужном месте и размере
        g.drawImage(thumbImage,
            thumbRect,
            juce::RectanglePlacement::fillDestination);
    


        // 3) Подгружаем картинку из BinaryData и рисуем
        auto img = juce::ImageCache::getFromMemory(BinaryData::slider_png,
            BinaryData::slider_pngSize);

        // Проверяем, действительно ли картинка загрузилась
        if (img.isValid())   // <-- тут уже bool
        {
            g.drawImage(img, thumbRect, juce::RectanglePlacement::fillDestination);
        }
        else
        {
            // fallback
            g.setColour(slider.findColour(juce::Slider::thumbColourId));
            g.fillRect(thumbRect);
        }
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomLookAndFeel)
};

// Отдельный LookAndFeel для кнопок пресетов
class CustomLookButon : public juce::LookAndFeel_V4
{
public:
    CustomLookButon() {}

    juce::Font getTextButtonFont(juce::TextButton& button,
        int buttonHeight) override
    {
        float fontSize = buttonHeight * 0.5f;
        juce::Font f(fontSize);
        f.setBold(true);
        return f;
    }
};
