#pragma once
#include <cmath>
#include <juce_dsp/juce_dsp.h>

struct SimpleGate
{
    void prepare(double newSampleRate)
    {
        sampleRate = newSampleRate;
        setTimesMs(2.0f, 80.0f);
        reset();
    }

    void setThresholdDb(float dB)
    {
        thresholdDb = dB;
        thresholdLin = std::pow(10.0f, thresholdDb / 20.0f);
    }

    void setTimesMs(float attackMs, float releaseMs)
    {
        aEnvAttack = coefFromMs(attackMs);
        aEnvRelease = coefFromMs(releaseMs);
        aGainAttack = coefFromMs(attackMs);
        aGainRelease = coefFromMs(releaseMs);
    }

    void reset()
    {
        ch = Chan{};
    }

    void process(float* data, int numSamples, bool bypass)
    {
        if (bypass || data == nullptr)
            return;

        for (int n = 0; n < numSamples; ++n)
        {
            const float xAbs = std::abs(data[n]);
            stepEnvelope(xAbs, ch);
            const float g = stepGain(ch);
            data[n] *= g;
        }
    }

    float getEnv() const noexcept { return ch.env; }
    float getGain() const noexcept { return ch.gain; }

private:
    struct Chan { float env = 0.0f; float gain = 1.0f; } ch;

    double sampleRate = 44100.0;
    float thresholdDb = -40.0f;
    float thresholdLin = 0.01f;

    float aEnvAttack = 0.0f, aEnvRelease = 0.0f;
    float aGainAttack = 0.0f, aGainRelease = 0.0f;

    float coefFromMs(float ms) const
    {
        const float t = ms * 0.001f;
        return std::exp(-1.0f / (t * (float)sampleRate));
    }

    void stepEnvelope(float xAbs, Chan& c) noexcept
    {
        const bool rising = xAbs > c.env;
        const float a = rising ? aEnvAttack : aEnvRelease;
        c.env = a * c.env + (1.0f - a) * xAbs;
    }

    float stepGain(Chan& c) noexcept
    {
        const float target = (c.env >= thresholdLin) ? 1.0f : 0.0f;
        const bool rising = target > c.gain;
        const float a = rising ? aGainAttack : aGainRelease;
        c.gain = a * c.gain + (1.0f - a) * target;
        return c.gain;
    }
};

