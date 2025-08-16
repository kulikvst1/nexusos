#pragma once
#include <JuceHeader.h>
#include <atomic> 
#include "in_fount_label.h"
#include "SimpleGate.h"

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
    void prepare(double sampleRate, int blockSize) noexcept;
    void processBlock(float* const* channels, int numSamples) noexcept;

private:
    class VUMeter : public juce::Component
    {
    public:
        void setValue(float newValue) noexcept
        {
            value = juce::jlimit(0.0f, 1.0f, newValue);
            repaint();
        }
        void paint(juce::Graphics& g) override;
    private:
        float value = 0.0f;
    };

    // DSP
    SimpleGate gateL, gateR;

    std::atomic<float> thresholdL{ 0.01f }, thresholdR{ 0.01f };
    std::atomic<float> attackL{ 10.0f }, attackR{ 10.0f };
    std::atomic<float> releaseL{ 100.0f }, releaseR{ 100.0f };
    std::atomic<bool> bypassL{ false }, bypassR{ false };

    double currentSampleRate = 44100.0;
    int    currentBlockSize = 512;

    InLookAndFeel  inLnF;
    InLookButon    inBtnLnF;

    // Метки
    juce::Label titleLabel;
    juce::Label input1Label, input2Label;
    juce::Label gateIn1Label, gateIn2Label;
    juce::Label threshold1Label, threshold2Label;
    juce::Label attack1Label, attack2Label;
    juce::Label release1Label, release2Label;

    // VU
    VUMeter vuLeft, vuRight;

    // Слайдеры
    juce::Slider gateKnob1, gateKnob2;           // Threshold
    juce::Slider attackSliderL, attackSliderR;   // Attack
    juce::Slider releaseSliderL, releaseSliderR; // Release

    // Кнопки
    juce::TextButton gateBypass1{ "GATE ON" }, gateBypass2{ "GATE ON" };

    static void updateBypassButtonColour(juce::TextButton& b);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputControlComponent)
};
