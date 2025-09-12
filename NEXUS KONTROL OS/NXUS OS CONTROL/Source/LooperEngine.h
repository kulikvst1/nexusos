#pragma once
#include <JuceHeader.h>
#include <cstdint>

class LooperEngine : private juce::AsyncUpdater
{
public:
    enum class Mode { Player, Looper };
    enum State { Clean, Recording, Stopped, Playing };

    struct Listener
    {
        virtual ~Listener() = default;
        virtual void engineChanged() = 0;
    };

    LooperEngine()
        : ioThread("LooperEngine IO")
    {
        formatManager.registerBasicFormats();
        ioThread.startThread();
    }

    ~LooperEngine() override
    {
        transport.stop();
        transport.setSource(nullptr);
        readerSource.reset();
        ioThread.stopThread(200);
    }

    // === Mode ===
    void setMode(Mode m) noexcept { mode = m; notify(); }
    Mode getMode() const noexcept { return mode; }

    // === Player (встроено) ===
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
        loadedFile = file; // ← сохранить для отображения
        notify();
        return true;
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
        playerTriggerArmed = true;
        notify();
    }

    void onSignalDetected()
    {
        if (!isReady() || !playerTriggerArmed || !waitingForTrigger || transport.isPlaying())
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

    // Состояние плеера (отдельные геттеры, чтобы не ломать логику лупера)
    bool isReady()                 const noexcept { return readerSource != nullptr; }
    bool isPlaying()               const noexcept { return transport.isPlaying(); }
    bool isPlayerTriggerArmed()    const noexcept { return playerTriggerArmed; }
    bool isPlayerWaitingForTrigger() const noexcept { return playerTriggerArmed && waitingForTrigger; }

    double getCurrentTime() const noexcept { return transport.getCurrentPosition(); }
    double getTotalTime()   const noexcept
    {
        if (sr > 0.0 && totalLength > 0)
            return static_cast<double>(totalLength) / sr;
        return 0.0;
    }
    void setTriggerArmed(bool t) noexcept
    {
        playerTriggerArmed = t;
        notify();
    }

    // === Общие параметры ===
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

    void setTriggerEnabled(bool t) noexcept
    {
        if (isPrepared) triggerEnabled = t;
        notify();
    }
    bool getTriggerEnabled() const noexcept { return triggerEnabled; }

    // === Looper (без изменений API) ===
    void prepare(double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;
        maxLen = int(sr * getMaxRecordSeconds());
        buffer.setSize(2, maxLen);

        // важно: в transport передаём ожидаемый размер блока, а не maxLen
        transport.prepareToPlay(maxBlockSize, sampleRate);

        isPrepared = true;
        reset();
    }

    void reset()
    {
        if (!isPrepared) return;

        // looper
        buffer.clear();
        recPos = playPos = loopStart = loopEnd = 0;
        hasData = recTriggered = false;
        level = 1.0f;
        state = Clean;
        if (onCleared)
            onCleared();

        // player
        transport.stop();
        transport.setSource(nullptr);
        readerSource.reset();
        totalLength = 0;
        waitingForTrigger = false;
        playerTriggerArmed = false;

        notify();
    }
    void controlButtonPressed()
    {
        if (!isPrepared) return;

        State oldState = state;

        switch (state)
        {
        case Clean:
            recTriggered = !triggerEnabled;
            recPos = 0;
            state = Recording;
            break;

        case Recording:
            if (recPos > 0)
            {
                state = Stopped;
                hasData = true;
                loopStart = 0;
                loopEnd = recPos;
                playPos = loopStart;
                checkInvariants();
            }
            else
            {
                state = Clean;
            }
            break;

        case Stopped:
            if (hasData)
            {
                state = Playing;
                playPos = loopStart;
            }
            break;

        case Playing:
            state = Stopped;
            playPos = loopStart;
            break;
        }

        // Уведомляем, если состояние изменилось
        if (onStateChanged && state != oldState)
            onStateChanged(state);

        notify();
    }

    void process(juce::AudioBuffer<float>& inOut)
    {
        if (!isPrepared) return;

        if (mode == Mode::Looper)
        {
            switch (state)
            {
            case Recording: processRecording(inOut); return;
            case Playing:   processPlaying(inOut);   return;
            default:        processBypass(inOut);    return;
            }
        }
        else
        {
            // === Триггер в режиме Player — как в лупере ===
            if (playerTriggerArmed && waitingForTrigger)
            {
                const int nSamps = inOut.getNumSamples();
                const int nCh = juce::jmin(inOut.getNumChannels(), 2);

                for (int i = 0; i < nSamps; ++i)
                {
                    for (int ch = 0; ch < nCh; ++ch)
                    {
                        float x = inOut.getReadPointer(ch)[i];
                        if (std::abs(x) >= triggerThreshold)
                        {
                            onSignalDetected(); // ⬅️ как в лупере
                            goto skipDetection;
                        }
                    }
                }
            skipDetection:;
            }

            processPlayer(inOut);
        }
    }
    juce::String getLoadedFileName() const noexcept
    {
        return loadedFile.exists() ? loadedFile.getFileName() : juce::String{};
    }
    // looper getters
    State  getState()                  const noexcept { return state; }
    int    getRecordedSamples()        const noexcept { return recPos; }
    int    getLoopLengthSamples()      const noexcept { return loopEnd - loopStart; }
    double getRecordedLengthSeconds()  const noexcept { return sr > 0 ? recPos / sr : 0.0; }
    double getLoopLengthSeconds()      const noexcept { return sr > 0 ? (loopEnd - loopStart) / sr : 0.0; }
    double getPlayPositionSeconds()    const noexcept { return sr > 0 ? (playPos - loopStart) / sr : 0.0; }
    static constexpr double getMaxRecordSeconds() noexcept { return 300.0; }

    bool isPreparedSuccessfully() const noexcept { return isPrepared; }
    bool isTriggerArmed()         const noexcept { return triggerEnabled && state == Recording && !recTriggered; } // оригинальная семантика
    bool isRecordingLive()        const noexcept { return state == Recording && recTriggered; }

    // listeners
    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }
    // Уведомления наружу (например, в Rig_control)
    std::function<void(State)> onStateChanged; // смена состояния (Recording/Stopped/Playing)
    std::function<void()> onCleared;           // очистка (Clean)

protected:
    void handleAsyncUpdate() override
    {
        listeners.call([](Listener& L) { L.engineChanged(); });
    }

private:
    void notify() noexcept { triggerAsyncUpdate(); }

    // looper processing
    void processRecording(juce::AudioBuffer<float>& io)
    {
        const int nSamps = io.getNumSamples();
        const int nCh = juce::jmin(io.getNumChannels(), buffer.getNumChannels());

        for (int i = 0; i < nSamps; ++i)
        {
            for (int ch = 0; ch < nCh; ++ch)
            {
                float x = io.getReadPointer(ch)[i];

                if (!recTriggered && std::abs(x) >= triggerThreshold)
                    recTriggered = true;

                if (recTriggered && recPos < maxLen)
                    buffer.getWritePointer(ch)[recPos] = x;

                io.getWritePointer(ch)[i] = x;
            }

            if (recTriggered)
                ++recPos;

            if (recPos >= maxLen)
            {
                recPos = maxLen;
                controlButtonPressed(); // завершить запись
                break;
            }
        }
    }

    void processPlaying(juce::AudioBuffer<float>& io)
    {
        const int nSamps = io.getNumSamples();
        const int nCh = juce::jmin(io.getNumChannels(), buffer.getNumChannels());

        for (int i = 0; i < nSamps; ++i)
        {
            const float gain = level;
            for (int ch = 0; ch < nCh; ++ch)
                io.getWritePointer(ch)[i] = buffer.getSample(ch, playPos) * gain;

            if (++playPos >= loopEnd)
                playPos = loopStart;
        }
    }

    void processBypass(juce::AudioBuffer<float>& io)
    {
        const int nSamps = io.getNumSamples();
        const int nCh = io.getNumChannels();

        for (int ch = 0; ch < nCh; ++ch)
            std::memcpy(io.getWritePointer(ch),
                io.getReadPointer(ch),
                sizeof(float) * nSamps);
    }

    // player processing
    void processPlayer(juce::AudioBuffer<float>& bufferRef)
    {
        if (!readerSource || !transport.isPlaying())
        {
            // если плеер не играет, просто пропускаем вход
            processBypass(bufferRef);
            return;
        }

        juce::AudioSourceChannelInfo info(&bufferRef, 0, bufferRef.getNumSamples());
        transport.getNextAudioBlock(info);

        if (level != 1.0f)
            bufferRef.applyGain(0, bufferRef.getNumSamples(), level);
    }

    void checkInvariants() const
    {
        if (hasData)
            jassert(loopEnd > loopStart);
    }

    // === player fields ===
    Mode mode{ Mode::Looper };
    juce::AudioFormatManager formatManager;
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::TimeSliceThread ioThread;

    std::int64_t totalLength = 0;
    bool  playerTriggerArmed = false;
    bool  waitingForTrigger = false;

    // === looper fields ===
    bool                     isPrepared = false;
    juce::AudioBuffer<float> buffer;
    int recPos = 0, playPos = 0, loopStart = 0, loopEnd = 0, maxLen = 0;
    double sr = 44100.0;
    bool hasData = false, triggerEnabled = false, recTriggered = false;
    float level = 1.0f, triggerThreshold = 0.001f; // default оставил от лупера
    State state = Clean;

    // listeners
    juce::ListenerList<Listener> listeners;
    juce::File loadedFile;

};
