// eq_dsp.h
#pragma once

#include <JuceHeader.h>

class EqDsp
{
public:
    EqDsp() noexcept
        : currentSampleRate(0.0),
        prepared(false)
    {}

    ~EqDsp() noexcept = default;

    /**
     * Подготовка фильтров: задаёт sampleRate, очищает и создаёт
     * IIRFilter-объекты по числу каналов.
     */
    void prepare(double sampleRate,
        int samplesPerBlock,
        int numChannels) noexcept;

    /**
     * Обновляет целевые коэффициенты всех 5-ти фильтров:
     * Low-Cut, LowShelf, Peak, HighShelf, High-Cut.
     * Реальные фильтры переключатся в process().
     */
    void updateCoefficients(float lowCutFreq,
        float lowFreq, float lowGain,
        float midFreq, float midGain, float midQ,
        float highFreq, float highGain,
        float highCutFreq) noexcept;

    /**
     * Обрабатывает буфер: проходит по каждому каналу и
     * последовательно прогоняет через 5 фильтров.
     */
    void process(juce::AudioBuffer<float>& buffer) noexcept;

    /** Возвращает true после вызова prepare(). */
    bool isPrepared() const noexcept { return prepared; }

private:
    juce::OwnedArray<juce::IIRFilter> lowCutFilters;
    juce::OwnedArray<juce::IIRFilter> lowShelfFilters;
    juce::OwnedArray<juce::IIRFilter> peakFilters;
    juce::OwnedArray<juce::IIRFilter> highShelfFilters;
    juce::OwnedArray<juce::IIRFilter> highCutFilters;

    double currentSampleRate;
    bool   prepared;
};
