#pragma once

#include <JuceHeader.h>
#include "PitchEngine.h"
#include "Grid.h"                  // ← ваш Grid

class TunerComponent : public juce::Component,
    private juce::AsyncUpdater
{
public:
    TunerComponent();
    ~TunerComponent() override;

    void prepare(int blockSize, double sampleRate) noexcept;
    void pushAudioData(const float* data, int numSamples) noexcept;

    void paint(juce::Graphics& g) override;
    void resized() override;        // ← теперь реализуем

private:
    void handleAsyncUpdate() override
    {
        updateString();
        repaint();
    }

    void detectionThreadFunction();
    void updateString();

    PitchEngine                  pitchEngine;
    std::vector<float>           ringBuffer, tempBuffer, diffBuffer;
    std::atomic<int>             writePos{ 0 };
    int                          bufSize = 0, halfSize = 0;
    std::thread                  detectionThread;
    std::atomic<bool>            hasNewData{ false };
    std::atomic<bool>            shouldTerminate{ true };

    std::atomic<float>           lastFreq{ -1.0f };
    float                        lastFreqSmooth{ -1.0f };
    float                        alphaSmooth{ 0.2f };
    float                        rmsThreshold{ 0.005f };

    const std::array<const char*, 6> stringNames{ {
        "E2","A2","D3","G3","B3","E4"
    } };
    const std::array<float, 6>   stringFreqs{ {
        82.4069f, 110.0000f,
        146.8324f,195.9977f,
        246.9417f,329.6276f
    } };
    std::string                  currentString;

    // ← Новые поля для областей сетки 5×4
    juce::Rectangle<int> noteArea,
        scaleArea,
        stringArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TunerComponent)
};
