#pragma once

#include <JuceHeader.h>
#include <atomic> 
#include "in_fount_label.h"
#include "SimpleGate.h"

// Прямоугольник по индексу ячейки 20-секторной сетки (4 строки × 5 столбцов)
juce::Rectangle<int> getCellBounds(int cellIndex,
    int totalWidth,
    int totalHeight,
    int spanX = 1,
    int spanY = 1);

class InputControlComponent : public juce::Component
{
public:
    InputControlComponent();
    ~InputControlComponent() override;

    void resized() override;

    /** Инициализация перед началом аудио */
    void prepare(double sampleRate, int blockSize) noexcept;

    /** Вызывается из аудиопотока */
    void processBlock(float* const* channels, int numSamples) noexcept;
        
    void setGateProcessor(SimpleGate* newGate) noexcept
    {
        gateProcessor.store(newGate, std::memory_order_release);
    }

private:
    /** Визуальный VU-метр */
    class VUMeter : public juce::Component
    {
    public:
        void setValue(float newValue) noexcept
        {
            value = juce::jlimit(0.0f, 1.0f, newValue);
            repaint();
        }

        void paint(juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour(juce::Colours::black.withAlpha(0.85f));
            g.fillRoundedRectangle(r, 3.0f);

            g.setColour(juce::Colours::white.withAlpha(0.15f));
            constexpr int ticks = 6;
            for (int i = 1; i < ticks; ++i)
            {
                float y = r.getY() + r.getHeight() * (1.0f - float(i) / ticks);
                g.drawLine(r.getX() + 3.0f, y, r.getRight() - 3.0f, y, 1.0f);
            }

            auto fill = r;
            fill.removeFromTop(r.getHeight() * (1.0f - value));
            juce::ColourGradient grad(
                juce::Colours::green, fill.getBottomLeft(),
                juce::Colours::red, fill.getTopLeft(),
                false
            );
            grad.addColour(0.6, juce::Colours::yellow);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(fill, 2.5f);

            g.setColour(juce::Colours::white.withAlpha(0.25f));
            g.drawRoundedRectangle(r, 3.0f, 1.0f);
        }

    private:
        float value = 0.0f;
    };

    // потоки-неопасные ссылки на DSP и параметры
    std::atomic<SimpleGate*> gateProcessor{ nullptr };
    std::atomic<float>       thresholdL{ 0.01f }, thresholdR{ 0.01f };
    std::atomic<bool>        bypassL{ false }, bypassR{ false };

    double currentSampleRate = 44100.0;
    int    currentBlockSize = 512;

    InLookAndFeel  inLnF;
    InLookButon    inBtnLnF;

    juce::Label    titleLabel;
    juce::Label    input1Label, input2Label;
    juce::Label    gateIn1Label, gateIn2Label;

    VUMeter        vuLeft, vuRight;

    juce::Slider   gateKnob1, gateKnob2;
    juce::TextButton gateBypass1{ "BYPASS GATE 1" },
        gateBypass2{ "BYPASS GATE 2" };

    static void updateBypassButtonColour(juce::TextButton& b);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputControlComponent)
};
