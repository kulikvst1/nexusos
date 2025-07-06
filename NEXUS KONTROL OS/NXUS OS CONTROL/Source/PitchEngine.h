#pragma once

#include <vector>
#include <complex>
#include <JuceHeader.h>

class PitchEngine
{
public:
    PitchEngine() = default;

    /**
      * Обязательно вызывать перед process()/processHybrid().
      * bufferSize — размер аудиоблока (желательно степень двойки),
      * sampleRate — частота дискретизации.
      */
    void prepare(int bufferSize, double sampleRate);

    /**
      * Чистый YIN (CMND) в диапазоне tauMin..tauMax.
      * diffOut должен иметь длину как минимум tauMax+1.
      * Возвращает Hz или -1.0f.
      */
    float process(const float* audioData,
        int size,
        float* diffOut) const;

    /**
      * Гибрид: 1) FFT → грубая fCoarse  2) выставляем tauRange вокруг fCoarse
      * 3) вызываем process() для уточнения.
      * Возвращает Hz или -1.0f.
      */
    float processHybrid(const float* audioData,
        int size,
        float* diffOut);

    /** YIN-порог (обычно 0.10…0.20) */
    void setThreshold(float t) noexcept { threshold = t; }

    /** Задаёт абсолютный диапазон поиска [fMin…fMax] перед processHybrid */
    void setSearchRange(float fMinHz,
        float fMaxHz) noexcept;

    /** Порог спектрального пика при FFT-этапе (0.0…1.0) */
    void setMagnitudeThreshold(float magThresh) noexcept { magnitudeThreshold = magThresh; }

private:
    // Настроечные величины
    int    m_bufferSize = 0;
    int    m_halfSize = 0;
    double m_sampleRate = 0.0;
    int    tauMin = 1;
    int    tauMax = 0;
    float  threshold = 0.15f;
    float  fMin = 50.0f;
    float  fMax = 2000.0f;
    float  magnitudeThreshold = 0.01f;

    // Для FFT-этапа
    int    fftOrder = 0;
    int    fftSize = 0;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<std::complex<float>> spectrum;
    std::vector<float>              window;
};
