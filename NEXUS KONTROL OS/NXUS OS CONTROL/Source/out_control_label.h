#pragma once

#include <JuceHeader.h>
#include "BinaryData.h"

class OutLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OutLookAndFeel()
    {
        // �������� � ���������� ����������� �������� ���� ���
        constexpr int thumbW = 26, thumbH = 45;
        thumbImage = juce::ImageCache::getFromMemory(BinaryData::slider_png,
            BinaryData::slider_pngSize)
            .rescaled(thumbW, thumbH,
                juce::Graphics::highResamplingQuality);

    }

    // ����� ��� TextButton
    juce::Font getTextButtonFont(juce::TextButton& button,
        int buttonHeight) override
    {
        return { buttonHeight * 0.5f, juce::Font::bold };
    }

    // ����� ��� ComboBox
    juce::Font getComboBoxFont(juce::ComboBox& box) override
    {
        return { box.getHeight() * 0.6f, juce::Font::bold };
    }

    // ����� ��� PopupMenu
    juce::Font getPopupMenuFont() override
    {
        return { 14.0f, juce::Font::plain };
    }

    // ��������� ��������� ����� � ������� ComboBox
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

    // ���������� ������������ ������������ ���������
    void drawLinearSlider(juce::Graphics& g,

        int x, int y, int width, int height,
        float sliderPos,
        float /*minValue*/, float /*maxValue*/,
        const juce::Slider::SliderStyle style,
        juce::Slider& slider) override
    {
        // 1) ������ ������������ ����������� ���� (��� + ����������� �����)
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height,
            sliderPos, 0.0f, 0.0f,
            style, slider);

        // 2) ���� ��� ������������ ������� � �������� ��������� � ������ ���� thumb
        if (style == juce::Slider::LinearVertical && thumbImage.isValid())
        {
            const int thumbW = thumbImage.getWidth();
            const int thumbH = thumbImage.getHeight();

            // ���������� �� ����������� � ����������� �� ���������
            float thumbX = (float)x + (width - thumbW) * 0.5f;
            float thumbY = sliderPos - thumbH * 0.5f;

            g.drawImage(thumbImage,
                juce::Rectangle<float>(thumbX, thumbY, thumbW, thumbH),
                juce::RectanglePlacement::fillDestination);
        }
    }
    /// ���������� ������ thumb-����������� (��� ���������� ���� ����� � hit-�����)
    int getSliderThumbRadius(juce::Slider& slider) override
    {
        if (thumbImage.isValid())
        {
            // ���������� �������� ������ ��������
            return thumbImage.getHeight() / 1.8;
        }

        // ����� � ��������� ������ �� �������� LookAndFeel
        return LookAndFeel_V4::getSliderThumbRadius(slider);
    }
private:
    juce::Image thumbImage;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutLookAndFeel)
};
