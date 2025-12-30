#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>

// Параметры 5‑полосного EQ
struct EqSettings
{
    float lowCutFreq = 20.0f;
    float lowShelfFreq = 100.0f;
    float lowShelfGain = 0.0f;
    float peakFreq = 1000.0f;
    float peakGain = 0.0f;
    float peakQ = 1.0f;
    float highShelfFreq = 5000.0f;
    float highShelfGain = 0.0f;
    float highCutFreq = 20000.0f;
};

class EqDsp
{
public:
    EqDsp() noexcept;
    ~EqDsp() noexcept = default;

    void prepare(double sampleRate, int samplesPerBlock, int numChannels) noexcept;
    void updateSettings(const EqSettings& newSettings) noexcept;
    void process(juce::AudioBuffer<float>& buffer) noexcept;

    bool isPrepared() const noexcept { return prepared.load(std::memory_order_relaxed); }
    void setShuttingDown(bool b) noexcept { shuttingDown.store(b, std::memory_order_relaxed); }

private:
    using Band = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;
    using Chain = juce::dsp::ProcessorChain<Band, Band, Band, Band, Band>;

    std::vector<Chain>     chains;
    juce::dsp::ProcessSpec spec;

    juce::SmoothedValue<float> sLowCutFreq,
        sLowShelfFreq, sLowShelfGain,
        sPeakFreq, sPeakGain, sPeakQ,
        sHighShelfFreq, sHighShelfGain,
        sHighCutFreq;

    // --- RCU двухбуферный снапшот настроек ---
    EqSettings       settingsBuf[2]{};
    std::atomic<int> activeIdx{ 0 };

    std::atomic<bool> dirty{ false };
    std::atomic<bool> prepared{ false };
    std::atomic<bool> shuttingDown{ false };

    void publishSettings(const EqSettings& s) noexcept;
    const EqSettings& getActiveSettings() const noexcept;

    static void updateChainCoeffs(Chain& chain, const EqSettings& s, double sampleRate) noexcept;
};
