#pragma once
#include <JuceHeader.h>
#include <cstdint>
#include <atomic>

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
        // Блокируем процессинг и уведомления
        shuttingDown.store(true, std::memory_order_release);
        notificationsEnabled.store(false, std::memory_order_release);
        cancelPendingUpdate();

        // Обнуляем внешние коллбэки и чистим слушателей
        onCleared = {};
        onStateChanged = {};
        listeners.clear();

        // Остановить транспорт и отвязать источники
        transport.stop();
        transport.setSource(nullptr);

        // Снять ресурсы транспорта, если был подготовлен
        if (transportPrepared)
            transport.releaseResources();

        readerSource.reset();

        // Остановить IO-поток
        ioThread.stopThread(200);
    }
    void setMode(Mode newMode)
    {
        if (mode != newMode) {
            mode = newMode;
            if (mode == Mode::Looper && !hasData)
                state = Clean;
            if (onModeChanged)
                onModeChanged(mode);
            notify();
        }
    }

    Mode getMode() const noexcept
    {
        return mode;
    }
    // === Загрузка файла для плеера ===
    bool loadFile(const juce::File& file)
    {
        if (shuttingDown.load(std::memory_order_acquire))
            return false;

        transport.stop();

        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
        if (!reader) return false;

        auto newSource = std::make_unique<juce::AudioFormatReaderSource>(reader.release(), true);
        totalLength = newSource->getTotalLength();
        readerSource = std::move(newSource);
        loadedFile = file;

        if (transportPrepared && sr > 0.0)
        {
            transport.setSource(readerSource.get(), 32768, &ioThread, sr);
            transport.setPosition(0.0);
            transport.setLooping(true);
        }

        // --- Автоматическая отправка Stop в MIDI ---
        if (onModeChanged && mode == Mode::Player)
        {
            // Файл только что загружен, но не играет → Stop
            if (onPlayerStateChanged)
                onPlayerStateChanged(false);
        }

        notify();
        return true;
    }

    // === Старт плеера с начала ===
    void startFromTop()
    {
        if (shuttingDown.load(std::memory_order_acquire)
            || !isReady()             // есть ли источник
            || !transportPrepared)    // подготовлен ли транспорт
            return;

        transport.setPosition(0.0);
        transport.start();
        waitingForTrigger = false;
        if (onPlayerStateChanged) onPlayerStateChanged(true);
        notify();
    }

    void armTriggerAndWait()
    {
        if (shuttingDown.load(std::memory_order_acquire) || !isReady() || isPlaying())
            return;

        waitingForTrigger = true;
        playerTriggerArmed = true;
        notify();
    }

    void onSignalDetected()
    {
        if (shuttingDown.load(std::memory_order_acquire)
            || !isReady()
            || !playerTriggerArmed
            || !waitingForTrigger
            || transport.isPlaying())
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
        if (onPlayerStateChanged)
            onPlayerStateChanged(false);

        notify();
    }

    // Состояние плеера (отдельные геттеры, чтобы не ломать логику лупера)
    bool isReady()                   const noexcept { return readerSource != nullptr; }
    bool isPlaying()                 const noexcept { return transport.isPlaying(); }
    bool isPlayerTriggerArmed()      const noexcept { return playerTriggerArmed; }
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

    // === Подготовка движка ===
    void prepare(double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;
        maxLen = int(sr * getMaxRecordSeconds());
        buffer.setSize(2, maxLen);

        // Готовим транспорт под заданный блок
        transport.prepareToPlay(maxBlockSize, sampleRate);
        transportPrepared = true;

        isPrepared = true;

        // Сбрасываем ТОЛЬКО луперную часть, плеер не трогаем
        buffer.clear();
        recPos = playPos = loopStart = loopEnd = 0;
        hasData = recTriggered = false;
        level = 1.0f;
        state = Clean;

        // Если файл уже был загружен до prepare — привязываем источник сейчас
        if (readerSource && sr > 0.0)
        {
            transport.setSource(readerSource.get(), 32768 /*readAhead*/, &ioThread, sr);
            transport.setPosition(0.0);
            transport.setLooping(true);
        }

        notify();
    }


    void reset()
    {
        if (!isPrepared || shuttingDown.load(std::memory_order_acquire))
            return;

        // looper
        buffer.clear();
        recPos = playPos = loopStart = loopEnd = 0;
        hasData = recTriggered = false;
        level = 1.0f;
        state = Clean;

        if (onCleared) {
            auto cb = onCleared;
            if (cb) cb();
        }

        // player
        transport.stop();
        transport.setSource(nullptr);
        totalLength = 0;
        readerSource.reset();
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

        if (onStateChanged && state != oldState)
            onStateChanged(state);

        notify();
    }

    void process(juce::AudioBuffer<float>& inOut)
    {
        if (!isPrepared || shuttingDown.load(std::memory_order_acquire))
            return;

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
                            onSignalDetected();
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
    bool isTriggerArmed()         const noexcept { return triggerEnabled && state == Recording && !recTriggered; }
    bool isRecordingLive()        const noexcept { return state == Recording && recTriggered; }

    // listeners
    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

    // Уведомления наружу (например, в Rig_control)
    std::function<void(bool isPlaying)> onPlayerStateChanged;
    std::function<void(State)> onStateChanged; // смена состояния (Recording/Stopped/Playing)
    std::function<void()> onCleared;           // очистка (Clean)
    bool isShuttingDown() const noexcept {
        return shuttingDown.load(std::memory_order_acquire);
    }
    std::function<void(Mode)> onModeChanged;

protected:
    void handleAsyncUpdate() override
    {
        if (shuttingDown.load(std::memory_order_acquire))
            return;

        listeners.call([](Listener& L) { L.engineChanged(); });
    }

private:
    void notify() noexcept
    {
        if (notificationsEnabled.load(std::memory_order_acquire)
            && !shuttingDown.load(std::memory_order_acquire))
        {
            triggerAsyncUpdate();
        }
    }

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
        if (shuttingDown.load(std::memory_order_acquire)
            || !readerSource
            || !transport.isPlaying())
        {
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
    bool                     transportPrepared = false;
    juce::AudioBuffer<float> buffer;
    int recPos = 0, playPos = 0, loopStart = 0, loopEnd = 0, maxLen = 0;
    double sr = 44100.0;
    bool hasData = false, triggerEnabled = false, recTriggered = false;
    float level = 1.0f, triggerThreshold = 0.001f; // default оставил от лупера
    State state = Clean;

    // listeners
    juce::ListenerList<Listener> listeners;
    juce::File loadedFile;

    // флаги завершения/уведомлений
    std::atomic<bool> shuttingDown{ false };
    std::atomic<bool> notificationsEnabled{ true };


};
