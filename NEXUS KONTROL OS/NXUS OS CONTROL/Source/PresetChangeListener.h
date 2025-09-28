#pragma once
#include <JuceHeader.h>

class PresetChangeListener
{
public:
    virtual ~PresetChangeListener() {}

    // ���������� ��� ��������� ����� �������
    virtual void presetChanged(int presetIndex, const juce::String& newName) = 0;

    // ���������� ��� ��������� ��������� �������
    virtual void activePresetChanged(int presetIndex) = 0;
};

