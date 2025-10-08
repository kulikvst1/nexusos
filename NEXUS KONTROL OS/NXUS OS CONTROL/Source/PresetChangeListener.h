#pragma once
#include <JuceHeader.h>

class PresetChangeListener
{
public:
    virtual ~PresetChangeListener() {}

    // Âûçûâàåòñÿ ïðè èçìåíåíèè èìåíè ïðåñåòà
    virtual void presetChanged(int presetIndex, const juce::String& newName) = 0;

    // Âûçûâàåòñÿ ïðè èçìåíåíèè àêòèâíîãî ïðåñåòà
    virtual void activePresetChanged(int presetIndex) = 0;
};

