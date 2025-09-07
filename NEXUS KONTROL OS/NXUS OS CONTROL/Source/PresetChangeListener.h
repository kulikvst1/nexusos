#pragma once
#include <JuceHeader.h>

class PresetChangeListener
{
public:
    virtual ~PresetChangeListener() {}

    // Вызывается при изменении имени пресета
    virtual void presetChanged(int presetIndex, const juce::String& newName) = 0;

    // Вызывается при изменении активного пресета
    virtual void activePresetChanged(int presetIndex) = 0;
};

