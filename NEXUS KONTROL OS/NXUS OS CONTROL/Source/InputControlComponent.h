#pragma once
#include <JuceHeader.h>
#include <atomic> 
#include "in_fount_label.h"
#include "SimpleGate.h"
#include "LevelMeterComponent.h"
#include "LevelScaleComponent.h"
#include "Rig_control.h"
juce::Rectangle<int> getCellBounds(int cellIndex,
    int totalWidth,
    int totalHeight,
    int spanX = 1,
    int spanY = 1);

class InputControlComponent : public juce::Component
{
public:
    InputControlComponent();
    ~InputControlComponent() override;

    void resized() override;
    void prepare(double sampleRate, int blockSize) noexcept;
    void processBlock(float* const* channels, int numSamples) noexcept;
    void saveSettings() const;
    void loadSettings();
    void setRigControl(Rig_control* rc) { rigControl = rc; }
    bool isShuttingDown() const noexcept {
        return shuttingDown.load(std::memory_order_acquire);
    }
    void updateAutoConfigButtonColour(juce::TextButton& b);
    void updateSwitchButtonColour(juce::TextButton& b, bool isOn);
    void updateStatusLabel(const juce::String& text);
    void syncSwitchState(int cc, bool isOn);
    void syncPedalSliderByCC(int cc, int value);
    void syncAutoConfigButton(bool isOn);
    void showPressUp();
    void showPressDown();
    void resetStateLabel();
    void setPedalConnected(bool connected);
    juce::Slider& getPedalMinSlider() { return pedalMinSlider; }
    juce::Slider& getPedalMaxSlider() { return pedalMaxSlider; }

    bool getInvertState() const { return invertButton.getToggleState(); }

private:

    // DSP
    SimpleGate gateL, gateR;
    std::atomic<float> thresholdL{ 0.01f }, thresholdR{ 0.01f };
    std::atomic<float> attackL{ 10.0f }, attackR{ 10.0f };
    std::atomic<float> releaseL{ 100.0f }, releaseR{ 100.0f };
    std::atomic<bool> bypassL{ false }, bypassR{ false };
    std::atomic<bool> inputBypass1State{ false }, inputBypass2State{ false };
    double currentSampleRate = 44100.0;
    int    currentBlockSize = 512;

    InLookAndFeel  inLnF;
    InLookButon    inBtnLnF;

    // Метки
  
    juce::Label input1Label, input2Label;
    juce::Label gateIn1Label, gateIn2Label;
    juce::Label threshold1Label, threshold2Label;
    juce::Label attack1Label, attack2Label;
    juce::Label release1Label, release2Label;
    juce::Label VU1Label, VU2Label;
    // VU
    LevelMeterComponent vuLeft;
    LevelMeterComponent vuRight;
    LevelScaleComponent vuScale;
    LevelScaleComponent vuScaleL;
    LevelScaleComponent vuScaleR;
    // CLIP-индикаторы
    juce::Label clipLabelL, clipLabelR;
    std::atomic<bool> clipL{ false }, clipR{ false };
    double clipTsL = 0.0, clipTsR = 0.0;
    static constexpr double clipHoldMs = 800.0;
   

    // Слайдеры
    juce::Slider gateKnob1, gateKnob2;           // Threshold
    juce::Slider attackSliderL, attackSliderR;   // Attack
    juce::Slider releaseSliderL, releaseSliderR; // Release

    // Кнопки
    juce::TextButton gateBypass1{ "GATE ON" }, gateBypass2{ "GATE ON" };
    juce::TextButton inputBypass1{ "CH 1" }, inputBypass2{ "CH 2" };

    juce::TextButton impBtn33k{ "33k" };
    juce::TextButton impBtn1M{ "1M" };
    juce::TextButton impBtn3M{ "3M" };
    juce::TextButton impBtn10M{ "10M" };
    Rig_control* rigControl = nullptr;
    //
    // ==== PEDAL CONTROL ====
    juce::Label pedalLabel; // Заголовок "Pedal Settings"

    juce::Label stateLabel; // Текущее состояние (SW1, SW2, PRESS FOOT UP/DOWN)

    // Rotary sliders
    juce::Slider pedalStateSlider; // Текущее откалиброванное значение педали
    juce::Slider pedalMinSlider;   // MIN
    juce::Slider pedalMaxSlider;   // MAX
    std::unique_ptr<InLookSliderArrow> minLook;
    std::unique_ptr<InLookSliderArrow> maxLook;
    std::unique_ptr<InLookSliderBar> stateLook;


    // Подписи под слайдерами
    juce::Label pedalStateTextLabel;
    juce::Label pedalMinTextLabel;
    juce::Label pedalMaxTextLabel;

    // Кнопки
    juce::TextButton autoConfigButton{ "AUTO KALL" };
    juce::TextButton sw1Button{ "SW1" };
    juce::TextButton sw2Button{ "SW2" };
    juce::TextButton invertButton{ "INVERT" };

    bool isPedalConnected = false;
    static void updateChannelBypassColour(juce::TextButton& b);
    static void updateBypassButtonColour(juce::TextButton& b);
    std::atomic<bool> shuttingDown{ false };
    int pendingImpedanceCC = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputControlComponent)
};
