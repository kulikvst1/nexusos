#include <JuceHeader.h>

class FirstOrderSmoother
{
public:
    FirstOrderSmoother() {}

    ~FirstOrderSmoother() {}

    void prepare(float smoothingTimeInMs, float samplingRate)
    {
        const float c_twoPi = 6.283185307179586476925286766559f;

        a = exp(-c_twoPi / (smoothingTimeInMs * 0.001f * samplingRate));
        b = 1.0f - a;
        z = 0.0f;
    };

    inline float process(float in)
    {
        z = (in * b) + (z * a);
        return z;
    }

private:
    float a;
    float b;
    float z;
};

template <typename SampleType>
class DigitalDelay
{
public:
    DigitalDelay() {}

    void setDelayMs(SampleType newDelayMs) { delayTime = newDelayMs * this->sampleRate / 1000.0; }

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        jassert(spec.sampleRate > 0);
        jassert(spec.numChannels > 0);

        sampleRate = spec.sampleRate;

        delay.prepare(spec);

        smoother.prepare(850.0, sampleRate);

        reset();
    }

    void reset() { delay.reset(); }

    template <typename ProcessContext>
    void process(const ProcessContext& context) noexcept
    {
        const auto& inputBlock = context.getInputBlock();
        auto& outputBlock = context.getOutputBlock();
        const auto numChannels = outputBlock.getNumChannels();
        const auto numSamples = outputBlock.getNumSamples();

        jassert(inputBlock.getNumChannels() == numChannels);
        jassert(inputBlock.getNumSamples() == numSamples);

        if (context.isBypassed)
        {
            outputBlock.copyFrom(inputBlock);
            return;
        }

        auto* inputSamples = inputBlock.getChannelPointer(1);
        auto* outputSamples = outputBlock.getChannelPointer(1);

        for (size_t i = 0; i < numSamples; ++i)
        {
            auto input = inputSamples[i];
            auto output = input;

            delay.setDelay(smoother.process(this->delayTime));
            delay.pushSample((int)1, output);
            output = delay.popSample((int)1);

            outputSamples[i] = output;
        }
    }


private:
    juce::dsp::DelayLine<SampleType, juce::dsp::DelayLineInterpolationTypes::Linear> delay{ 2 * 44100 };
    double sampleRate = 44100.0;
    SampleType delayTime = 7.0;
    FirstOrderSmoother smoother;
};

class Doubler
{
public:
    Doubler() {}

    ~Doubler() {}

    void prepare(juce::dsp::ProcessSpec& _spec)
    {
        spec = _spec;
        delayModule.prepare(spec);
        delayModule.setDelayMs(0.0);
        lastDelayValue = 0.0;
        bypassed.store(true);
        mix.store(1.0f, std::memory_order_relaxed);  
    }
    void setBypass(bool shouldBypass) noexcept
    {
        bypassed.store(shouldBypass);
    }
    void setDelayMs(float newDelay)
    {
        delayModule.setDelayMs(newDelay);
        lastDelayValue = newDelay;
    }
    void setMix(float newMix) noexcept
    {
        mix.store(juce::jlimit(0.0f, 1.0f, newMix),
            std::memory_order_relaxed);
    }
    void Doubler::process(juce::AudioBuffer<float>& buffer)
    {
        if (bypassed.load()) return;

        // 1) Сохраняем сухой сигнал в отдельный буфер (deep copy)
        juce::AudioBuffer<float> dryBuffer(buffer.getNumChannels(),
            buffer.getNumSamples());
        dryBuffer.makeCopyOf(buffer);

        // 2) Обрабатываем delayModule «на месте»
        juce::dsp::AudioBlock<float> block(buffer);
        delayModule.process(juce::dsp::ProcessContextReplacing<float>(block));

        // 3) Смешиваем dry/wet по формуле out = wet*mix + dry*(1-mix)
        const float wet = mix.load(std::memory_order_relaxed);
        const float dry = 1.0f - wet;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* dest = buffer.getWritePointer(ch);
            auto* srcDry = dryBuffer.getReadPointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                dest[i] = dest[i] * wet + srcDry[i] * dry;
        }
    }

    double lastDelayValue;

private:
    juce::dsp::ProcessSpec spec;
    DigitalDelay<float> delayModule;
    std::atomic<bool>  bypassed{ true };
    std::atomic<float>            mix{ 1.0f };
};