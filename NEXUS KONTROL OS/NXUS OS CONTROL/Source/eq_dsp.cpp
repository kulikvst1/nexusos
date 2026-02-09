#include "eq_dsp.h"

EqDsp::EqDsp() noexcept
{
    EqSettings def{};
    settingsBuf[0] = def;
    settingsBuf[1] = def;
}

void EqDsp::prepare(double sampleRate,
    int    samplesPerBlock,
    int    numChannels) noexcept
{
    jassert(sampleRate > 0.0);
    jassert(numChannels > 0);

    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32> (numChannels);

    chains.clear();
    chains.resize((size_t)numChannels);

    for (auto& ch : chains)
        ch.prepare(spec);

    constexpr double smoothingTime = 0.05;
    sLowCutFreq.reset(sampleRate, smoothingTime);
    sLowShelfFreq.reset(sampleRate, smoothingTime);
    sLowShelfGain.reset(sampleRate, smoothingTime);
    sPeakFreq.reset(sampleRate, smoothingTime);
    sPeakGain.reset(sampleRate, smoothingTime);
    sPeakQ.reset(sampleRate, smoothingTime);
    sHighShelfFreq.reset(sampleRate, smoothingTime);
    sHighShelfGain.reset(sampleRate, smoothingTime);
    sHighCutFreq.reset(sampleRate, smoothingTime);

    const auto& s = getActiveSettings();
    sLowCutFreq.setCurrentAndTargetValue(s.lowCutFreq);
    sLowShelfFreq.setCurrentAndTargetValue(s.lowShelfFreq);
    sLowShelfGain.setCurrentAndTargetValue(s.lowShelfGain);
    sPeakFreq.setCurrentAndTargetValue(s.peakFreq);
    sPeakGain.setCurrentAndTargetValue(s.peakGain);
    sPeakQ.setCurrentAndTargetValue(s.peakQ);
    sHighShelfFreq.setCurrentAndTargetValue(s.highShelfFreq);
    sHighShelfGain.setCurrentAndTargetValue(s.highShelfGain);
    sHighCutFreq.setCurrentAndTargetValue(s.highCutFreq);

    prepared.store(true, std::memory_order_release);
}

void EqDsp::publishSettings(const EqSettings& s) noexcept
{
    const int cur = activeIdx.load(std::memory_order_relaxed);
    const int next = cur ^ 1;

    settingsBuf[next] = s;

    activeIdx.store(next, std::memory_order_release);
    dirty.store(true, std::memory_order_release);
}

const EqSettings& EqDsp::getActiveSettings() const noexcept
{
    return settingsBuf[activeIdx.load(std::memory_order_acquire)];
}

void EqDsp::updateSettings(const EqSettings& newSettings) noexcept
{
    publishSettings(newSettings);
}

void EqDsp::process(juce::AudioBuffer<float>& buffer) noexcept
{
    if (shuttingDown.load(std::memory_order_relaxed))
    {
        buffer.clear();
        return;
    }

    if (!prepared.load(std::memory_order_acquire))
        return;

    if (dirty.exchange(false, std::memory_order_acq_rel))
    {
        const auto& s = getActiveSettings();
        for (auto& ch : chains)
            updateChainCoeffs(ch, s, spec.sampleRate);
    }

    juce::dsp::AudioBlock<float> block(buffer);
    for (size_t i = 0; i < chains.size(); ++i)
    {
        auto single = block.getSingleChannelBlock((int)i);
        juce::dsp::ProcessContextReplacing<float> ctx(single);
        chains[i].process(ctx);
    }
}

void EqDsp::updateChainCoeffs(Chain& chain,
    const EqSettings& s,
    double sampleRate) noexcept
{
    constexpr float Q = 0.7071f;

    auto lc = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, s.lowCutFreq, Q);
    auto ls = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, s.lowShelfFreq, Q,
        juce::Decibels::decibelsToGain(s.lowShelfGain));
    auto pk = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, s.peakFreq, s.peakQ,
        juce::Decibels::decibelsToGain(s.peakGain));
    auto hs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, s.highShelfFreq, Q,
        juce::Decibels::decibelsToGain(s.highShelfGain));
    auto hc = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, s.highCutFreq, Q);

    *chain.get<0>().state = *lc;
    *chain.get<1>().state = *ls;
    *chain.get<2>().state = *pk;
    *chain.get<3>().state = *hs;
    *chain.get<4>().state = *hc;
}
