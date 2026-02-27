#pragma once
#include <JuceHeader.h>

// Кликабельная метка (для встроенного TextBox слайдера)
struct ClickableLabel : public juce::Label
{
    std::function<void()> onClick;

    void mouseDown(const juce::MouseEvent& e) override
    {
        juce::Label::mouseDown(e);
        if (onClick) onClick();
    }
};

// ---------------- DigitalKeyboard ----------------
class DigitalKeyboard : public juce::Component
{
public:
    // Конструктор принимает метку и callback для применения значения
    DigitalKeyboard(juce::Label& targetLabel,
        std::function<void(double)> onValueEntered);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::Label& externalLabel;
    std::function<void(double)> onValueEnteredCallback;

    juce::TextEditor editor;
    juce::OwnedArray<juce::TextButton> digitButtons;
    juce::TextButton backButton, clearButton, pointButton, okButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DigitalKeyboard)
};

// --- LookAndFeel для слайдеров с кликабельным TextBox ---
class SliderTextBoxLNF : public juce::LookAndFeel_V4
{
public:
    juce::Label* createSliderTextBox(juce::Slider& slider) override;
};
