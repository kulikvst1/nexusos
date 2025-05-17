

#pragma once
#include <JuceHeader.h>

class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel() {}


    juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override
    {
        // ������ ������ ����� 70% �� ������ ������
        float fontSize = buttonHeight * 0.5f;
        juce::Font f(fontSize);
        f.setBold(true);
        return f;
    }


};
// ���������� ����� � ���������� ������������ ��� (��� � ����� ������������, ���� �����) //��� ������ ������
class CustomLookButon : public juce::LookAndFeel_V4
{
public:
    CustomLookButon() {}


    juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override
    {
        // ������ ������ ����� 70% �� ������ ������
        float fontSize = buttonHeight * 0.2f;
        juce::Font f(fontSize);
        f.setBold(true);
        return f;
    }


};
