#pragma once

#include <JuceHeader.h>
#include "FilePlayerEngine.h"
#include "Grid.h"
#include "fount_label.h"

class FilePlayerComponent
    : public juce::Component,
    private juce::Button::Listener,
    private juce::Slider::Listener,
    private juce::Timer
{
public:
    explicit FilePlayerComponent(FilePlayerEngine& eng);
    ~FilePlayerComponent() override;

    void setScale(float newScale) noexcept;
    void paint(juce::Graphics& g) override;
    void resized() override;

    void prepareToPlay(int spb, double sr);
    void releaseResources();
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info);
    void onExternalSignal();

private:
    void buttonClicked(juce::Button* b) override;
    void sliderValueChanged(juce::Slider* s) override;
    void timerCallback() override;

    void updateStateLabel();
    static juce::String formatTime(double seconds);

    FilePlayerEngine& engine;
    CustomLookAndFeel buttonLnf;

    // ** ROW 0 **
    juce::Label   stateLabel, levelTextLabel, mixTextLabel;
    juce::Slider  levelSlider, mixSlider;
  

    // ** ROW 1 **
    juce::Slider  progressSlider;
    juce::Label   currentTimeLabel, totalTimeLabel;

    // ** ROW 2 **
    juce::TextButton resetBtn, controlBtn, triggerBtn, fileSelectBtn;

    float  scale = 1.0f;
    double lastTotal = -1.0;

    bool blinkOn = false;
    int  blinkCounter = 0;
    static constexpr int blinkPeriodTicks = 10;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilePlayerComponent)
};
