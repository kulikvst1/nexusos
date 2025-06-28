#pragma once
#include <JuceHeader.h>

class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel() {}


    juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override
    {
        // Размер шрифта равен 70% от высоты кнопки
        float fontSize = buttonHeight * 0.2f;
        juce::Font f(fontSize);
        f.setBold(true);
        return f;
    }


};
// Определяем класс в глобальном пространстве имён (или в вашем пространстве, если нужно) //для кнопок пресет
class CustomLookButon : public juce::LookAndFeel_V4
{
public:
    CustomLookButon() {}


    juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override
    {
        // Размер шрифта равен 70% от высоты кнопки
        float fontSize = buttonHeight * 0.5f;
        juce::Font f(fontSize);
        f.setBold(true);
        return f;
    }


};