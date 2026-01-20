#pragma once
#include <JuceHeader.h>

namespace LevelUtils
{
    inline float mapDbToY(float dB,
        float maxDb, float minDb,
        juce::Rectangle<float> area) noexcept
    {
        return juce::jmap<float>(dB, maxDb, minDb,
            area.getY(), area.getBottom());
    }
}

