#pragma once

#include <JuceHeader.h>
#include "LooperEngine.h"
#include "fount_label.h"   // ваш глобальный LookAndFeel

class LooperComponent : public juce::Component,
    private juce::Button::Listener,
    private juce::Slider::Listener,
    private juce::Timer
{
public:
    LooperComponent(LooperEngine& eng)
        : engine(eng)
    {
        // Look&Feel для кнопок
        resetBtn.setLookAndFeel(&buttonLnf);
        controlBtn.setLookAndFeel(&buttonLnf);
        triggerBtn.setLookAndFeel(&buttonLnf);

        // stateLabel
        addAndMakeVisible(stateLabel);
        stateLabel.setJustificationType(juce::Justification::centred);

        // ставим крупнее в 2× — 48pt, и жирный
        stateLabel.setFont(juce::Font(100.0f, juce::Font::bold));

        // текст чёрный
        stateLabel.setColour(juce::Label::textColourId, juce::Colours::black);

        // rotary levelSlider
        addAndMakeVisible(levelSlider);
        levelSlider.setLookAndFeel(&buttonLnf);
        levelSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        levelSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
        levelSlider.setRange(0.0, 1.0, 0.01);
        levelSlider.setValue(1.0);
        levelSlider.addListener(this);

        // подпись над слайдером
        addAndMakeVisible(levelTextLabel);
        levelTextLabel.setText("Level", juce::dontSendNotification);
        levelTextLabel.setFont({ 16.0f, juce::Font::bold });
        levelTextLabel.setJustificationType(juce::Justification::centred);

        // resetBtn
        addAndMakeVisible(resetBtn);
        resetBtn.setButtonText("RESET");
        resetBtn.addListener(this);
        resetBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::lightblue);

        // controlBtn
        addAndMakeVisible(controlBtn);
        controlBtn.setButtonText("CTRL");
        controlBtn.addListener(this);

        // triggerBtn
        addAndMakeVisible(triggerBtn);
        triggerBtn.setButtonText("TRG");
        triggerBtn.setClickingTogglesState(true);
        triggerBtn.addListener(this);

        // прогресс-бар
        addAndMakeVisible(progressSlider);
        progressSlider.setLookAndFeel(&buttonLnf);
        progressSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        progressSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        progressSlider.setRange(0.0, 1.0, 0.0);
        progressSlider.setEnabled(false);

        // таймкоды
        addAndMakeVisible(currentTimeLabel);
        currentTimeLabel.setJustificationType(juce::Justification::centredLeft);
        currentTimeLabel.setFont({ 32.0f, juce::Font::bold });
        currentTimeLabel.setText("00:00", juce::dontSendNotification);

        addAndMakeVisible(totalTimeLabel);
        totalTimeLabel.setJustificationType(juce::Justification::centredRight);
        totalTimeLabel.setFont({ 32.0f, juce::Font::bold });
        totalTimeLabel.setText("00:00", juce::dontSendNotification);

        startTimerHz(30);
        updateStateLabel();
    }

    ~LooperComponent() override
    {
        // отвязываем LookAndFeel
        resetBtn.setLookAndFeel(nullptr);
        controlBtn.setLookAndFeel(nullptr);
        triggerBtn.setLookAndFeel(nullptr);
        levelSlider.setLookAndFeel(nullptr);
        progressSlider.setLookAndFeel(nullptr);
    }

    void resized() override
    {
        auto W = getWidth();
        auto H = getHeight();
        auto sw = W / 5;
        auto sh = H / 5;
        auto mw = sw / 10;
        auto mh = sh / 10;

        // row 0: stateLabel (cols 1..3)
        stateLabel.setBounds(sw * 1 + mw, sh * 0 + mh,
            sw * 3 - 2 * mw, sh - 2 * mh);

        // row 1: sectors 7–9 (cols 1..3)
        {
            int x = sw * 1 + mw;
            int y = sh * 1 + mh;
            int w = sw * 3 - 2 * mw;
            int h = sh - 2 * mh;
            juce::Rectangle<int> area(x, y, w, h);

            currentTimeLabel.setBounds(area.removeFromLeft(sw - 2 * mw));
            progressSlider.setBounds(area.removeFromLeft(sw - 2 * mw));
            totalTimeLabel.setBounds(area);
        }

        // row 2: levelSlider + levelTextLabel (col 2)
        {
            // col=5 → индекс 4 (0..4)
            auto sliderBounds = juce::Rectangle<int>(
                sw * 4 + mw,  // смещаемся на 4 сектора вправо
                sh * 0 + mh,
                sw - 2 * mw,
                sh - 2 * mh);

            levelSlider.setBounds(sliderBounds);

            // подпись поверх слайдера
            auto lblH = 20;
            levelTextLabel.setBounds(
                sliderBounds.getX(),
                sliderBounds.getY() + (sliderBounds.getHeight() - lblH) / 2,
                sliderBounds.getWidth(),
                lblH);
        }

        // row 4: buttons (cols 1..3)
        resetBtn.setBounds(sw * 1 + mw, sh * 4 + mh, sw - 2 * mw, sh - 2 * mh);
        controlBtn.setBounds(sw * 2 + mw, sh * 4 + mh, sw - 2 * mw, sh - 2 * mh);
        triggerBtn.setBounds(sw * 3 + mw, sh * 4 + mh, sw - 2 * mw, sh - 2 * mh);
    }

private:
    void updateStateLabel()
    {
        using S = LooperEngine::State;
        static const char* names[] = { "CLEAN", "REC", "STOP", "PLAY" };
        auto st = engine.getState();
        stateLabel.setText(names[int(st)], juce::dontSendNotification);

        juce::Colour c;
        switch (st)
        {
        case S::Clean:     c = juce::Colours::lightblue; break;
        case S::Recording: c = juce::Colours::red;       break;
        case S::Stopped:   c = juce::Colours::yellow;    break;
        case S::Playing:   c = juce::Colours::green;     break;
        }
        stateLabel.setColour(juce::Label::backgroundColourId, c);
        controlBtn.setColour(juce::TextButton::buttonOnColourId, c);
        controlBtn.setToggleState(st == S::Recording || st == S::Playing,
            juce::dontSendNotification);
    }

    void buttonClicked(juce::Button* b) override
    {
        if (b == &resetBtn)
        {
            engine.reset();
            updateStateLabel();
            progressSlider.setValue(0.0, juce::dontSendNotification);
            currentTimeLabel.setText("00:00", juce::dontSendNotification);
            totalTimeLabel.setText("00:00", juce::dontSendNotification);
        }
        else if (b == &controlBtn)
        {
            engine.controlButtonPressed();
            updateStateLabel();
        }
        else if (b == &triggerBtn)
        {
            engine.setTriggerEnabled(triggerBtn.getToggleState());
            triggerBtn.setColour(
                juce::TextButton::buttonOnColourId,
                triggerBtn.getToggleState() ? juce::Colours::yellow
                : juce::Colours::darkgrey);
        }
    }

    void sliderValueChanged(juce::Slider* s) override
    {
        if (s == &levelSlider)
            engine.setLevel((float)levelSlider.getValue());
    }

    void timerCallback() override
    {
        using S = LooperEngine::State;
        auto st = engine.getState();

        double cur = 0.0, tot = 0.0;
        switch (st)
        {
        case S::Recording:
            cur = engine.getRecordedLengthSeconds();
            tot = LooperEngine::getMaxRecordSeconds();
            break;
        case S::Playing:
            cur = engine.getPlayPositionSeconds();
            tot = engine.getLoopLengthSeconds();
            break;
        default:
            cur = engine.getRecordedLengthSeconds();
            tot = engine.getLoopLengthSeconds();
            break;
        }

        double pos = (tot > 0.0) ? (cur / tot) : 0.0;
        progressSlider.setValue(pos, juce::dontSendNotification);

        currentTimeLabel.setText(formatTime(cur), juce::dontSendNotification);
        totalTimeLabel.setText(formatTime(tot), juce::dontSendNotification);
    }

    static juce::String formatTime(double seconds)
    {
        int t = int(std::floor(seconds));
        return juce::String::formatted("%02d:%02d", t / 60, t % 60);
    }

    LooperEngine& engine;
    juce::Label         stateLabel;

    juce::Slider        levelSlider;
    juce::Label         levelTextLabel;    // ← подпись над слайдером

    juce::TextButton    resetBtn, controlBtn, triggerBtn;
    CustomLookAndFeel   buttonLnf;

    juce::Slider        progressSlider;
    juce::Label         currentTimeLabel, totalTimeLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LooperComponent)
};
