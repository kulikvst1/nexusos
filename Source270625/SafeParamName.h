#pragma once
#include <JuceHeader.h>

// Áåçîïàñíî ïîëó÷àåò èìÿ ïàðàìåòðà, íå òðîãàÿ deprecated API.
// maxLen = 64 äîñòàòî÷íî äëÿ ïîäïèñåé â UI.
inline juce::String safeGetParamName(juce::AudioPluginInstance* inst,
    int index,
    int maxLen = 64)
{
    if (inst != nullptr)
        if (auto* p = inst->getParameters()[index])
            return p->getName(maxLen);

    return {};
}

