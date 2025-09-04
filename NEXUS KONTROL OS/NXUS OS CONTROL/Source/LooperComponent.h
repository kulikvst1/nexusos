#pragma once

#include <JuceHeader.h>
#include "LooperEngine.h"
#include "Grid.h"
#include "fount_label.h"

class LooperComponent : public juce::Component,
    public juce::Button::Listener,  // ← исправлено
    private juce::Slider::Listener,
    private juce::Timer,
    private LooperEngine::Listener

{
public:
    explicit LooperComponent(LooperEngine& eng);
    ~LooperComponent() override;

    void setScale(float newScale) noexcept;
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // Button::Listener
    void buttonClicked(juce::Button* b) override;
    // Slider::Listener
    void sliderValueChanged(juce::Slider* s) override;
    // Timer
    void timerCallback() override;

    // Перестроить тексты/цвета stateLabel + надпись на controlBtn
    void updateStateLabel();
    // Формат mm:ss
    static juce::String formatTime(double seconds);

    LooperEngine& engine;
    CustomLookAndFeel  buttonLnf;

    // ** ROW 0 **
    juce::Label   stateLabel, levelTextLabel, mixTextLabel;
    juce::Slider  levelSlider, mixSlider;
    juce::Label   mixValueLabel, mixMinLabel, mixMaxLabel;

    // ** ROW 1 **
    juce::Slider  progressSlider;
    juce::Label   currentTimeLabel, totalTimeLabel;
    juce::Label modeLabel;
    juce::Label fileNameLabel;

    // ** ROW 2 **
    juce::TextButton resetBtn, controlBtn, triggerBtn;
    juce::TextButton modeSwitchBtn, fileSelectBtn;
    std::unique_ptr<juce::FileChooser> fileChooser;

    float               scale = 1.0f;
    double              lastTotal = -1.0;
    LooperEngine::State lastState{ LooperEngine::Clean };


    bool     blinkOn = false;       // текущий «пик» мигания
    int      blinkCounter = 0;           // счётчик тиков таймера
    static constexpr int blinkPeriodTicks = 10; // полный цикл = 10 * 30ms = 300ms

    // вспомогательные:
    bool isArmed() const { return engine.isTriggerArmed(); }
    bool isLive()  const { return engine.isRecordingLive(); }

    void engineChanged() override;         // ← вызывается при изменении движка
    void updateFromEngine() noexcept;     // ← синхронизация UI с движком


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LooperComponent)
};
