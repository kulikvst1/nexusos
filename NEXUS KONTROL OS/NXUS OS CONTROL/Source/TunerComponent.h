// TunerComponent.h
#pragma once

#include <JuceHeader.h>
#include "PitchEngine.h"
#include "Grid.h"

class TunerComponent : public juce::Component,
    private juce::AsyncUpdater,
    private juce::Button::Listener
{
public:
    TunerComponent();
    ~TunerComponent() override;

    void prepare(int blockSize, double sampleRate) noexcept;
    void pushAudioData(const float* data, int numSamples) noexcept;

    float getLastFrequency() const noexcept { return lastFreq.load(std::memory_order_relaxed); }
    const std::string& getCurrentString() const noexcept { return currentString; }

    void setReferenceA4(double a4Hz) noexcept { referenceA4.store(a4Hz, std::memory_order_relaxed); updateRefLabel(); }
    double getReferenceA4() const noexcept { return referenceA4.load(std::memory_order_relaxed); }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // AsyncUpdater callback
    void handleAsyncUpdate() override
    {
        updateString();
        repaint();
    }

    // Button::Listener
    void buttonClicked(juce::Button* b) override
    {
        auto a4 = getReferenceA4();
        if (b == &minusButton)
            a4 = std::max(400.0, a4 - 1.0);  // нижний предел 400 Hz
        else if (b == &plusButton)
            a4 = std::min(480.0, a4 + 1.0);  // верхний предел 480 Hz

        setReferenceA4(a4);
    }

    void updateRefLabel()
    {
        referenceLabel.setText("A4: " + juce::String(getReferenceA4(), 0) + " Hz",
            juce::dontSendNotification);
    }

    void detectionThreadFunction();
    void updateString();

    PitchEngine                      pitchEngine;
    std::vector<float>               ringBuffer, tempBuffer, diffBuffer;
    std::atomic<int>                 writePos{ 0 };
    int                              bufSize = 0, halfSize = 0;
    std::thread                      detectionThread;
    std::atomic<bool>                hasNewData{ false }, shouldTerminate{ true };

    std::atomic<float>               lastFreq{ -1.0f };
    float                            lastFreqSmooth{ -1.0f }, alphaSmooth{ 0.2f };
    float                            rmsThreshold{ 0.005f };

    std::atomic<double>              referenceA4{ 440.0 };

    const std::array<const char*, 6> stringNames{ { "E2","A2","D3","G3","B3","E4" } };
    const std::array<float, 6>       stringFreqs{ { 82.4f,110.0f,146.8f,196.0f,246.9f,329.6f } };
    std::string                      currentString;

    // UI
    juce::TextButton                 minusButton{ "-" };
    juce::TextButton                 plusButton{ "+" };
    juce::Label                      referenceLabel;

    // Zones
    juce::Rectangle<int>             noteArea, scaleArea, stringArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TunerComponent)
};
