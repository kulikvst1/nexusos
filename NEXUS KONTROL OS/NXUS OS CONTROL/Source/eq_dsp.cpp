#include "eq_dsp.h"

EqDsp::EqDsp() noexcept
{
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

    // По умолчанию сглаживание 50 ms
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

    // Начальные значения из targetSettings
    auto s = targetSettings.load();
    sLowCutFreq.setCurrentAndTargetValue(s.lowCutFreq);
    sLowShelfFreq.setCurrentAndTargetValue(s.lowShelfFreq);
    sLowShelfGain.setCurrentAndTargetValue(s.lowShelfGain);
    sPeakFreq.setCurrentAndTargetValue(s.peakFreq);
    sPeakGain.setCurrentAndTargetValue(s.peakGain);
    sPeakQ.setCurrentAndTargetValue(s.peakQ);
    sHighShelfFreq.setCurrentAndTargetValue(s.highShelfFreq);
    sHighShelfGain.setCurrentAndTargetValue(s.highShelfGain);
    sHighCutFreq.setCurrentAndTargetValue(s.highCutFreq);

    prepared.store(true);
}

void EqDsp::updateSettings(const EqSettings& newSettings) noexcept
{
    targetSettings.store(newSettings);
    dirty.store(true);
}

void EqDsp::process(juce::AudioBuffer<float>& buffer) noexcept
{
    if (!prepared.load())
        return;

    // Если пришли новые цели — сразу пересчитаем «сырые» coeffs
    if (dirty.exchange(false))
    {
        auto s = targetSettings.load();
        for (auto& ch : chains)
            updateChainCoeffs(ch, s, spec.sampleRate);
    }

    // Получаем AudioBlock по каналам и прогоняем Chain
    juce::dsp::AudioBlock<float>  block(buffer);
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
    auto ls = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, s.lowShelfFreq, Q, juce::Decibels::decibelsToGain(s.lowShelfGain));
    auto pk = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, s.peakFreq, s.peakQ, juce::Decibels::decibelsToGain(s.peakGain));
    auto hs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, s.highShelfFreq, Q, juce::Decibels::decibelsToGain(s.highShelfGain));
    auto hc = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, s.highCutFreq, Q);

    *chain.get<0>().state = *lc;
    *chain.get<1>().state = *ls;
    *chain.get<2>().state = *pk;
    *chain.get<3>().state = *hs;
    *chain.get<4>().state = *hc;
}
