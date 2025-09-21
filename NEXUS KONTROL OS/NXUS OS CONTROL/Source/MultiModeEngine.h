#pragma once

#include <JuceHeader.h>
#include "LooperEngine.h"
#include "FilePlayerEngine.h"

class MultiModeEngine
{
public:
    enum class Mode { Looper, Player };

    struct Listener
    {
        virtual ~Listener() = default;
        virtual void engineChanged() = 0;
    };

    explicit MultiModeEngine(Mode startMode = Mode::Looper) noexcept
        : mode(startMode)
    {
    }

    void prepareToPlay(int blockSize, double sampleRate) noexcept
    {
        currentBlockSize = blockSize;
        currentSampleRate = sampleRate;

        looper.prepare(sampleRate, blockSize);
        player.prepareToPlay(blockSize, sampleRate);
    }

    void releaseResources() noexcept
    {
        looper.reset();
        player.releaseResources();
    }

    void setMode(Mode m) noexcept
    {
        mode = m;
        notifyListeners();
    }

    Mode getMode() const noexcept { return mode; }

    void setLevel(float v) noexcept
    {
        if (mode == Mode::Looper)
            looper.setLevel(v);
        else
            player.setLevel(v);
        notifyListeners();
    }

    void setTriggerThreshold(float t) noexcept
    {
        if (mode == Mode::Looper)
            looper.setTriggerThreshold(t);
        else
            player.setTriggerThreshold(t);
        notifyListeners();
    }

    void processAudio(juce::AudioBuffer<float>& buffer) noexcept
    {
        switch (mode)
        {
        case Mode::Looper:
            looper.process(buffer);
            break;
        case Mode::Player:
            player.processInputBuffer(buffer);
            break;
        }
    }

    void processTriggerInput(const float* const* inputChannelData,
        int numInputChannels,
        int numSamples) noexcept
    {
        if (mode != Mode::Player)
            return;

        if (!player.isTriggerArmed() || !player.isReady())
            return;

        const int chs = juce::jmax(1, numInputChannels);
        juce::AudioBuffer<float> inView(const_cast<float**>(inputChannelData), chs, numSamples);
        player.processInputBuffer(inView);
    }

    // === Looper: управление ===
    void controlButtonPressed() noexcept
    {
        looper.controlButtonPressed();
        notifyListeners();
    }

    void resetLooper() noexcept
    {
        looper.reset();
        notifyListeners();
    }

    float getLooperProgress() const noexcept
    {
        const double len = looper.getLoopLengthSeconds();
        if (len <= 0.0) return 0.0f;
        const double pos = looper.getPlayPositionSeconds();
        return (float)juce::jlimit(0.0, 1.0, pos / len);
    }

    double getPlayPositionSeconds() const noexcept
    {
        return looper.getPlayPositionSeconds();
    }

    // === Player: управление ===
    void togglePlayerPlayback() noexcept
    {
        if (player.isPlaying())
            player.stop();
        else
            player.startFromTop();
        notifyListeners();
    }

    void openFileDialog()
    {
        // Заглушка — если файл выбирается извне
        notifyListeners();
    }

    float getPlayerProgress() const noexcept
    {
        const double len = player.getTotalTime();
        if (len <= 0.0) return 0.0f;
        const double pos = player.getCurrentTime();
        return (float)juce::jlimit(0.0, 1.0, pos / len);
    }

    double getPlayerPositionSeconds() const noexcept
    {
        return player.getCurrentTime();
    }

    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

private:
    void notifyListeners() { listeners.call([](Listener& l) { l.engineChanged(); }); }

    Mode mode{ Mode::Looper };
    int currentBlockSize{ 0 };
    double currentSampleRate{ 0.0 };

    LooperEngine     looper;
    FilePlayerEngine player;

    juce::ListenerList<Listener> listeners;
};
