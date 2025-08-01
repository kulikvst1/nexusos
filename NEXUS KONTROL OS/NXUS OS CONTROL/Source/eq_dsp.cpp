// eq_dsp.cpp
#include "eq_dsp.h"

//==============================================================================
void EqDsp::prepare(double sampleRate,
    int /*samplesPerBlock*/,
    int numChannels) noexcept
{
    currentSampleRate = sampleRate;

    // Очистим старые фильтры и создадим новый IIRFilter на каждый канал
    lowCutFilters.clear();
    lowShelfFilters.clear();
    peakFilters.clear();
    highShelfFilters.clear();
    highCutFilters.clear();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        lowCutFilters.add(new juce::IIRFilter());
        lowShelfFilters.add(new juce::IIRFilter());
        peakFilters.add(new juce::IIRFilter());
        highShelfFilters.add(new juce::IIRFilter());
        highCutFilters.add(new juce::IIRFilter());
    }

    prepared = true;
}

//==============================================================================
void EqDsp::updateCoefficients(float lowCutFreq,
    float lowFreq, float lowGain,
    float midFreq, float midGain, float midQ,
    float highFreq, float highGain,
    float highCutFreq) noexcept
{
    if (!prepared)
        return;

    constexpr float defaultQ = 0.7071f;

    // 1) Конвертируем дБ → линейный
    auto lowGainLin = juce::Decibels::decibelsToGain(lowGain);
    auto midGainLin = juce::Decibels::decibelsToGain(midGain);
    auto highGainLin = juce::Decibels::decibelsToGain(highGain);

    // 2) Генерируем коэффициенты
    auto lowCutCoefs = juce::IIRCoefficients::makeHighPass(currentSampleRate, lowCutFreq, defaultQ);
    auto lowShelfCoefs = juce::IIRCoefficients::makeLowShelf(currentSampleRate, lowFreq, defaultQ, lowGainLin);
    auto peakCoefs = juce::IIRCoefficients::makePeakFilter(currentSampleRate, midFreq, midQ, midGainLin);
    auto highShelfCoefs = juce::IIRCoefficients::makeHighShelf(currentSampleRate, highFreq, defaultQ, highGainLin);
    auto highCutCoefs = juce::IIRCoefficients::makeLowPass(currentSampleRate, highCutFreq, defaultQ);

    // 3) Применяем их ко всем каналам
    const int n = lowCutFilters.size();
    for (int ch = 0; ch < n; ++ch)
    {
        lowCutFilters[ch]->setCoefficients(lowCutCoefs);
        lowShelfFilters[ch]->setCoefficients(lowShelfCoefs);
        peakFilters[ch]->setCoefficients(peakCoefs);
        highShelfFilters[ch]->setCoefficients(highShelfCoefs);
        highCutFilters[ch]->setCoefficients(highCutCoefs);
    }
}

//==============================================================================
void EqDsp::process(juce::AudioBuffer<float>& buffer) noexcept
{
    if (!prepared || lowCutFilters.size() == 0)
        return;

    const int numCh = buffer.getNumChannels();
    const int numSamps = buffer.getNumSamples();

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);

        // Последовательно прогоняем через все пять фильтров
        lowCutFilters[ch]->processSamples(data, numSamps);
        lowShelfFilters[ch]->processSamples(data, numSamps);
        peakFilters[ch]->processSamples(data, numSamps);
        highShelfFilters[ch]->processSamples(data, numSamps);
        highCutFilters[ch]->processSamples(data, numSamps);
    }
}
////////////////////////////////////////////////////////////////////////////////////
