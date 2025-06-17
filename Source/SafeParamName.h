#pragma once
#include <JuceHeader.h>

// Безопасно получает имя параметра, не трогая deprecated API.
// maxLen = 64 достаточно для подписей в UI.
inline juce::String safeGetParamName(juce::AudioPluginInstance* inst,
    int index,
    int maxLen = 64)
{
    if (inst != nullptr)
        if (auto* p = inst->getParameters()[index])
            return p->getName(maxLen);

    return {};
}

