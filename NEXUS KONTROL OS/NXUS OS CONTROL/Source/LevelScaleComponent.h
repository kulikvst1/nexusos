#pragma once

#include <JuceHeader.h>
#include <vector>

class LevelScaleComponent : public juce::Component
{
public:
    LevelScaleComponent();
    ~LevelScaleComponent() override {}

    // задаёт метки в dB: например { -60, -20, -12, -6, -3, 0 }
    void setScaleMarks(const std::vector<float>& marks) noexcept;

    void paint(juce::Graphics& g) override;

private:
    std::vector<float> scaleMarks;

    static constexpr float maxDb = 6.0f;    // верх шкалы
    static constexpr float minDb = -60.0f;  // низ шкалы

    int textMargin = 2;           // отступ слева от левого края компонента
    juce::Colour lineColour = juce::Colours::white.withAlpha(0.4f);
    juce::Colour textColour = juce::Colours::white;
    juce::Font   font = juce::Font(12.0f);  // шрифт для цифр
};
