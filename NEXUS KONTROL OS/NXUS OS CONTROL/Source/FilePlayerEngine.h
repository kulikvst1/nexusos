#pragma once

#include <JuceHeader.h>
#include <cstdint>

class FilePlayerEngine : private juce::AsyncUpdater
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void engineChanged() = 0;
    };

    FilePlayerEngine()
        : ioThread("FilePlayerEngine IO")
    {
        formatManager.registerBasicFormats();
        ioThread.startThread();
    }

    ~FilePlayerEngine() override
    {
        transport.stop();
        transport.setSource(nullptr);
        readerSource.reset();
        ioThread.stopThread(200);
    }

    // === Audio ===
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate)
    {
        sr = sampleRate;
        transport.prepareToPlay(samplesPerBlockExpected, sampleRate);
        isPrepared = true;
        notify();
    }

    void releaseResources()
    {
        transport.releaseResources();
    }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
    {
        transport.getNextAudioBlock(info);
        if (info.buffer != nullptr && level != 1.0f)
            info.buffer->applyGain(info.startSample, info.numSamples, level);
    }

    void processInputBuffer(const juce::AudioBuffer<float>& input)
    {
        if (!isReady() || !triggerArmed || !waitingForTrigger || transport.isPlaying())
            return;

        const int nCh = input.getNumChannels();
        const int nSamps = input.getNumSamples();

        for (int ch = 0; ch < nCh; ++ch)
        {
            const float* data = input.getReadPointer(ch);
            for (int i = 0; i < nSamps; ++i)
            {
                if (std::abs(data[i]) >= triggerThreshold)
                {
                    onSignalDetected();
                    return;
                }
            }
        }
    }

    // === Control ===
    bool loadFile(const juce::File& file)
    {
        transport.stop();

        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
        if (reader == nullptr)
            return false;

        auto newSource = std::make_unique<juce::AudioFormatReaderSource>(reader.release(), true);
        totalLength = newSource->getTotalLength();

        transport.setSource(newSource.get(), 0, &ioThread, sr);
        readerSource = std::move(newSource);

        transport.setPosition(0.0);
        transport.setLooping(true);

        notify();
        return true;
    }

    void reset()
    {
        transport.stop();
        transport.setSource(nullptr);
        readerSource.reset();
        totalLength = 0;
        waitingForTrigger = false;
        notify();
    }

    void startFromTop()
    {
        if (!isReady())
            return;

        transport.setPosition(0.0);
        transport.start();
        waitingForTrigger = false;
        notify();
    }

    void armTriggerAndWait()
    {
        if (!isReady() || isPlaying())
            return;

        waitingForTrigger = true;
        triggerArmed = true;
        notify();
    }

    void onSignalDetected()
    {
        if (!isReady() || !triggerArmed || !waitingForTrigger || transport.isPlaying())
            return;

        waitingForTrigger = false;
        transport.setPosition(0.0);
        transport.start();
        notify();
    }

    void stop()
    {
        transport.stop();
        waitingForTrigger = false;
        notify();
    }

    // === Params ===
    void setLevel(float v) noexcept
    {
        level = juce::jlimit(0.0f, 1.0f, v);
        notify();
    }

    float getLevel() const noexcept { return level; }

    void setTriggerThreshold(float t) noexcept
    {
        triggerThreshold = juce::jlimit(0.0f, 1.0f, t);
        notify();
    }

    float getTriggerThreshold() const noexcept { return triggerThreshold; }

    void setTriggerArmed(bool t) noexcept
    {
        triggerArmed = t;
        notify();
    }

    bool isTriggerArmed() const noexcept { return triggerArmed; }
    bool isWaitingForTrigger() const noexcept { return triggerArmed && waitingForTrigger; }

    // === State ===
    bool isReady()   const noexcept { return readerSource != nullptr; }
    bool isPlaying() const noexcept { return transport.isPlaying(); }

    double getCurrentTime() const noexcept { return transport.getCurrentPosition(); }

    double getTotalTime() const noexcept
    {
        if (sr > 0.0 && totalLength > 0)
            return static_cast<double>(totalLength) / sr;
        return 0.0;
    }

    // === Listeners ===
    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

private:
    void handleAsyncUpdate() override
    {
        listeners.call([](Listener& L) { L.engineChanged(); });
    }

    void notify() noexcept { triggerAsyncUpdate(); }

    juce::AudioFormatManager formatManager;
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::TimeSliceThread ioThread;

    double sr = 44100.0;
    std::int64_t totalLength = 0;

    float level = 1.0f;
    float triggerThreshold = 0.02f;
    bool  triggerArmed = false;
    bool  waitingForTrigger = false;
    bool  isPrepared = false;

    juce::ListenerList<Listener> listeners;
};
