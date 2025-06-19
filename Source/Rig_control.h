#pragma once

#include <JuceHeader.h>

class Rig_control : public juce::Component,
    public juce::Button::Listener
{
public:
    Rig_control();
    ~Rig_control() override;

    void resized() override;
    void buttonClicked(juce::Button* button) override;

private:
    // ��������� ��� ���� ��������� �������
    juce::Component mainTab;

    // ������� ������ ��� CC � ��������
    juce::OwnedArray<juce::TextButton> ccButtons;
    juce::OwnedArray<juce::TextButton> presetButtons;

    // ����� ��� ����������� �������� �����
    juce::Label bankNameLabel;

    // ������ ���������� (SHIFT, TEMPO, UP, DOWN)
    std::unique_ptr<juce::TextButton> shiftButton, tempoButton, upButton, downButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Rig_control)
};
