#pragma once
#include <JuceHeader.h>

// ��������� �������� ��� ���������, �� ������ deprecated API.
// maxLen = 64 ���������� ��� �������� � UI.
inline juce::String safeGetParamName(juce::AudioPluginInstance* inst,
    int index,
    int maxLen = 64)
{
    if (inst != nullptr)
        if (auto* p = inst->getParameters()[index])
            return p->getName(maxLen);

    return {};
}

