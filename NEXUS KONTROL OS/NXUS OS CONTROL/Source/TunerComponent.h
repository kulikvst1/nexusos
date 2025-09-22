#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <thread>
#include <array>
#include <chrono>
#include <limits>
#include "IPitchDetector.h"
#include "MPMDetector.h"

class TunerComponent : public juce::Component,
    private juce::AsyncUpdater,
    private juce::Button::Listener
{
public:
    TunerComponent();
    ~TunerComponent() override;

    void prepare(int blockSize, double sampleRate) noexcept;
    void pushAudioData(const float* data, int numSamples) noexcept;

    float  getLastFrequency() const noexcept { return lastFreq.load(std::memory_order_acquire); }
    double getReferenceA4() const noexcept { return referenceA4.load(std::memory_order_acquire); }

    std::function<void(double)> onReferenceA4Changed;
    void setReferenceA4(double a4Hz, bool notify = true) noexcept;

    void paint(juce::Graphics& g) override;
    void resized() override;

    enum class TunerVisualStyle { Classic, Triangles };

    void setVisualStyle(TunerVisualStyle style) noexcept;

private:
    // AsyncUpdater
    void handleAsyncUpdate() override;

    // Buttons
    void buttonClicked(juce::Button*) override;

    // DSP loop
    void detectionThreadFunction();

    void updateRefLabel();
    void updateString();


    // DSP
    std::vector<float>         ringBuffer, tempBuffer, diffBuffer;
    std::atomic<int>           writePos{ 0 };
    int                        bufSize = 0, halfSize = 0;
    std::thread                detectionThread;
    std::atomic<bool>          hasNewData{ false }, shouldTerminate{ false };

    // State
    std::atomic<float>         lastFreq{ -1.0f };
    float                      lastFreqSmooth{ -1.0f };
    float                      alphaSmooth{ 0.2f };
    std::array<float, 3>       freqHist{ { -1.0f, -1.0f, -1.0f } };
    int                        freqHistCount{ 0 };

    // Хроматический режим
    bool                       chromaticMode{ true };

    // RMS gate (гистерезис + счётчики + hold)
    float rmsOn{ 0.006f };
    float rmsOff{ 0.003f };
    int   onBlocksNeeded{ 2 };
    int   offBlocksNeeded{ 6 };
    int   onCount{ 0 }, offCount{ 0 };
    int   holdMs{ 250 };
    std::chrono::steady_clock::time_point lastAudibleTime{};
    bool  audible{ false };

    // Игнор атаки после открытия гейта
    int   attackIgnoreMs{ 150 };
    std::chrono::steady_clock::time_point gateOpenTime{};
    bool  gateJustOpened{ false };

    // Доп. hold для частоты
    int   freqHoldMs{ 120 };
    std::chrono::steady_clock::time_point lastFreqOkTime{};
    float lastGoodFreq{ -1.0f };

    // Reference
    std::atomic<double>        referenceA4{ 440.0 };

    // Для режима «6 струн»
    const std::array<const char*, 6> stringNames{ { "E","A","D","G","B","E" } };
    const std::array<float, 6>       stringFreqs{ { 82.4f,110.0f,146.8f,196.0f,246.9f,329.6f } };

    // Output labels
    std::string                currentString;
    float                      currentCents{ 0.0f };

    // UI
    juce::TextButton           minusButton{ "-" };
    juce::TextButton           plusButton{ "+" };
    juce::Label                referenceLabel;
    TunerVisualStyle visualStyle = TunerVisualStyle::Triangles;

    void drawClassicStyle(juce::Graphics& g);
    void drawTriangleStyle(juce::Graphics& g);


    // Layout
    juce::Rectangle<int>       noteArea, scaleArea, stringArea;

    // Пороги зон (¢)
    float perfectRange = 5.0f;
    float warningRange = 20.0f;
    float dangerRange = 60.0f;

    // накопление для фиксированного окна анализа
    std::atomic<int> totalWritten{ 0 };   // сколько сэмплов всего записано
    int lastProcessed{ 0 };               // сколько было обработано на предыдущем детекте
    int analysisSize{ 0 };                // фиксированный размер окна анализа в сэмплах
    std::unique_ptr<IPitchDetector> detector; // вместо прямого PitchEngine вызова

    // Авто-струна: фиксатор ближайшей струны на время, чтобы не прыгать
   
    int   stringLockMs = 500;             // длительность фиксации (мс)
    int   lockedString = -1;              // индекс залоченной струны (-1 = нет)
    std::chrono::steady_clock::time_point stringLockTime{}; // момент фиксации

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TunerComponent)
};