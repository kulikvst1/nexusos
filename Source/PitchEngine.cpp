#include "PitchEngine.h"
#include <algorithm>
#include <numeric>
#include <cmath>

void PitchEngine::prepare(int bufferSize, double sampleRate)
{
    jassert(bufferSize > 0 && sampleRate > 0.0);

    m_bufferSize = bufferSize;
    m_halfSize = bufferSize / 2;
    m_sampleRate = sampleRate;

    // По-умолчанию разрешаем максимальный YIN-диапазон
    tauMin = 1;
    tauMax = m_halfSize;

    // Готовим FFT: ищем порядок, ≥ bufferSize
    fftOrder = int(std::ceil(std::log2(m_bufferSize)));
    fftSize = 1 << fftOrder;
    fft = std::make_unique<juce::dsp::FFT>(fftOrder);

    spectrum.assign(fftSize, { 0.0f, 0.0f });
    window.assign(m_bufferSize, 0.0f);
    // Генерация Hann-окна
    for (int i = 0; i < m_bufferSize; ++i)
        2.0f * juce::MathConstants<float>::pi * float(i) / float(m_bufferSize - 1);

}

void PitchEngine::setSearchRange(float fMinHz, float fMaxHz) noexcept
{
    if (m_sampleRate <= 0.0) return;
    fMin = std::max(1.0f, fMinHz);
    fMax = std::min(float(m_sampleRate / 2.0), fMaxHz);

    // Переводим в tau-диапазон для будущего YIN
    int maxTau = std::lround(m_sampleRate / fMin);
    int minTau = std::lround(m_sampleRate / fMax);
    tauMin = std::clamp(minTau, 1, m_halfSize);
    tauMax = std::clamp(maxTau, tauMin, m_halfSize);
}

float PitchEngine::processHybrid(const float* audioData,
    int size,
    float* diffOut)
{
    // 1) Проверка
    if (!audioData || size != m_bufferSize || !diffOut)
        return -1.0f;

    // 2) Окно + копирование в комплексный буфер
    for (int i = 0; i < m_bufferSize; ++i)
        spectrum[i].real(window[i] * audioData[i]), spectrum[i].imag(0.0f);
    // нули для оставшихся
    for (int i = m_bufferSize; i < fftSize; ++i)
        spectrum[i] = { 0.0f, 0.0f };

    // 3) FFT-прямое преобразование
    fft->performRealOnlyForwardTransform(reinterpret_cast<float*>(spectrum.data()));

    // 4) Спектральный пик в bin-диапазоне [binMin..binMax]
    int binMin = int(fMin * fftSize / m_sampleRate);
    int binMax = int(fMax * fftSize / m_sampleRate);
    binMin = std::clamp(binMin, 1, fftSize / 2);
    binMax = std::clamp(binMax, 1, fftSize / 2);

    float  peakMag = 0.0f;
    int    peakBin = binMin;
    for (int b = binMin; b <= binMax; ++b)
    {
        float mag = std::abs(spectrum[b]);
        if (mag > peakMag) { peakMag = mag; peakBin = b; }
    }
    // Если спектральный пик слабый — нет тона
    if (peakMag < magnitudeThreshold)
        return -1.0f;

    float fCoarse = peakBin * m_sampleRate / fftSize;

    // 5) Устанавливаем узкий диапазон tau вокруг fCoarse (±30%)
    int tauCoarse = std::max(1, int(std::round(m_sampleRate / fCoarse)));
    int delta = int(tauCoarse * 0.3f);
    tauMin = std::clamp(tauCoarse - delta, 1, m_halfSize);
    tauMax = std::clamp(tauCoarse + delta, 1, m_halfSize);

    // 6) Уточняем YIN (CMND)
    return process(audioData, size, diffOut);
}

float PitchEngine::process(const float* audioData,
    int size,
    float* diffOut) const
{
    // Валидация
    if (!audioData || !diffOut || size != m_bufferSize || tauMax < tauMin)
        return -1.0f;

    // 1) CMND – разностная функция
    for (int tau = tauMin; tau <= tauMax; ++tau)
    {
        double sum = 0.0;
        int limit = m_bufferSize - tau;
        for (int i = 0; i < limit; ++i)
        {
            double d = double(audioData[i]) - double(audioData[i + tau]);
            sum += d * d;
        }
        diffOut[tau] = float(sum);
    }

    // 2) Нормализация CMND
    double runningSum = 0.0;
    diffOut[tauMin] = 1.0f;
    for (int tau = tauMin + 1; tau <= tauMax; ++tau)
    {
        runningSum += diffOut[tau];
        diffOut[tau] = float(diffOut[tau] * tau / runningSum);
    }

    // 3) Поиск первой точки ниже threshold
    int bestTau = -1;
    for (int tau = tauMin; tau <= tauMax; ++tau)
    {
        if (diffOut[tau] < threshold)
        {
            bestTau = tau;
            break;
        }
    }

    // 4) Если не нашли – берём минимальное значение
    if (bestTau < 0)
    {
        bestTau = tauMin;
        float minVal = diffOut[tauMin];
        for (int tau = tauMin + 1; tau <= tauMax; ++tau)
        {
            if (diffOut[tau] < minVal)
            {
                minVal = diffOut[tau];
                bestTau = tau;
            }
        }
    }

    // 5) Параболическая интерполяция
    float betterTau = float(bestTau);
    if (bestTau > tauMin && bestTau < tauMax)
    {
        float s0 = diffOut[bestTau - 1];
        float s1 = diffOut[bestTau];
        float s2 = diffOut[bestTau + 1];
        float denom = (2 * s1 - s2 - s0);
        if (std::abs(denom) > 1e-6f)
            betterTau = bestTau + 0.5f * (s2 - s0) / denom;
    }

    // 6) Возвращаем частоту (или -1)
    return (betterTau > 0.0f)
        ? float(m_sampleRate / betterTau)
        : -1.0f;
}
