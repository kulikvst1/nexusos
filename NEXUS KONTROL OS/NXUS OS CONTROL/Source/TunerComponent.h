// TunerComponent.h
#pragma once

#include <JuceHeader.h>
#include "PitchEngine.h"
#include "Grid.h"

class TunerComponent : public juce::Component,
    private juce::AsyncUpdater,
    private juce::Button::Listener
{
public:
    TunerComponent();
    ~TunerComponent() override;

    void prepare(int blockSize, double sampleRate) noexcept;
    void pushAudioData(const float* data, int numSamples) noexcept;

    float getLastFrequency() const noexcept { return lastFreq.load(std::memory_order_relaxed); }
    const std::string& getCurrentString() const noexcept { return currentString; }

    std::function<void(double)> onReferenceA4Changed;

    void setReferenceA4(double a4Hz, bool notifyCallback = true) noexcept
    {
        referenceA4.store(a4Hz, std::memory_order_relaxed);
        updateRefLabel();

        if (notifyCallback && onReferenceA4Changed)
            onReferenceA4Changed(a4Hz);
    }

    double getReferenceA4() const noexcept { return referenceA4.load(std::memory_order_relaxed); }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // AsyncUpdater callback
    void handleAsyncUpdate() override
    {
        updateString();
        repaint();
    }

    // Button::Listener
    void buttonClicked(juce::Button* b) override
    {
        auto a4 = getReferenceA4();
        if (b == &minusButton)
            a4 = std::max(400.0, a4 - 1.0);  // нижний предел 400 Hz
        else if (b == &plusButton)
            a4 = std::min(480.0, a4 + 1.0);  // верхний предел 480 Hz

        // notifyCallback == true by default → fires onReferenceA4Changed
        setReferenceA4(a4);
    }

    void updateRefLabel()
    {
        referenceLabel.setText("A4: " + juce::String(getReferenceA4(), 0) + " Hz",
            juce::dontSendNotification);
    }

    void detectionThreadFunction();
    void updateString();

    PitchEngine                      pitchEngine;
    std::vector<float>               ringBuffer, tempBuffer, diffBuffer;
    std::atomic<int>                 writePos{ 0 };
    int                              bufSize = 0, halfSize = 0;
    std::thread                      detectionThread;
    std::atomic<bool>                hasNewData{ false }, shouldTerminate{ true };

    // Последняя вычисленная частота (в герцах), хранится потокобезопасно.
    // Изначально = –1, чтобы указать на отсутствие валидных данных.
    std::atomic<float> lastFreq{ -1.0f };

    // Вспомогательная переменная для экспоненциального сглаживания частоты:
    // хранит предыдущий «сглаженный» результат
    float lastFreqSmooth{ -1.0f },

        // Коэффициент сглаживания α для формулы:
        //   smoothNew = α * detectedFreq + (1 – α) * smoothOld
        // α = 0.2 → 20% новое измерение, 80% накопленное значение
        alphaSmooth{ 0.2f };

    // Порог RMS-уровня (корень из среднего квадрата амплитуд),
    // при котором сигнал считается «достаточно громким» для анализа.
    // Низкие значения → ловит тихие ноты (и шумы),
    // большие → отбрасывает слабые шумы, но может пропустить тихие ноты.
    float rmsThreshold{ 0.090f };


    std::atomic<double>              referenceA4{ 440.0 };

    const std::array<const char*, 6> stringNames{ { "E2","A2","D3","G3","B3","E4" } };
    const std::array<float, 6>       stringFreqs{ { 82.4f,110.0f,146.8f,196.0f,246.9f,329.6f } };
    std::string                      currentString;
    float currentCents{ 0.0f };

    // UI
    juce::TextButton                 minusButton{ "-" };
    juce::TextButton                 plusButton{ "+" };
    juce::Label                      referenceLabel;

    // Zones
    juce::Rectangle<int>             noteArea, scaleArea, stringArea;

    // пороги в центах
    float perfectRange = 10.0f;    // до ±5¢ — в «зеленой» зоне
    float warningRange = 30.0f;   // до ±20¢ — в «желтой»
    float dangerRange = 45.0f;   // до ±60¢ — в «красной»

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TunerComponent)
};
