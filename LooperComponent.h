#pragma once

#include <JuceHeader.h>
#include "LooperEngine.h"
#include "fount_label.h"   // ваш глобальный LookAndFeel
#include "Grid.h"

class LooperComponent  : public juce::Component,
                         private juce::Button::Listener,
                         private juce::Slider::Listener,
                         private juce::Timer
{
public:
    LooperComponent(LooperEngine& eng)
        : engine(eng)
    {
        // Look&Feel
        resetBtn   .setLookAndFeel(&buttonLnf);
        controlBtn .setLookAndFeel(&buttonLnf);
        triggerBtn .setLookAndFeel(&buttonLnf);
        levelSlider.setLookAndFeel(&buttonLnf);
        mixSlider  .setLookAndFeel(&buttonLnf);
        progressSlider.setLookAndFeel(&buttonLnf);

        // stateLabel
        addAndMakeVisible(stateLabel);
        stateLabel.setJustificationType(juce::Justification::centred);
        stateLabel.setFont({100.0f, juce::Font::bold});
        stateLabel.setColour(juce::Label::textColourId, juce::Colours::black);

        // LEVEL-слайдер
        addAndMakeVisible(levelSlider);
        levelSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        levelSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
        levelSlider.setRange(0.0, 1.0, 0.01);
        levelSlider.setValue(1.0);
        levelSlider.addListener(this);

        addAndMakeVisible(levelTextLabel);
        levelTextLabel.setText("Level", juce::dontSendNotification);
        levelTextLabel.setFont({16.0f, juce::Font::bold});
        levelTextLabel.setJustificationType(juce::Justification::centred);

        // MIX-слайдер
        addAndMakeVisible(mixSlider);
        mixSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        mixSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
        mixSlider.setRange(0.0, 1.0, 0.01);
        mixSlider.setValue(0.5);
        mixSlider.addListener(this);

        addAndMakeVisible(mixTextLabel);
        mixTextLabel.setText("MIX", juce::dontSendNotification);
        mixTextLabel.setFont({16.0f, juce::Font::bold});
        mixTextLabel.setJustificationType(juce::Justification::centred);

        // Label для числового баланса –1…+1
        addAndMakeVisible(mixValueLabel);
        mixValueLabel.setFont({14.0f, juce::Font::plain});
        mixValueLabel.setJustificationType(juce::Justification::centred);
        mixValueLabel.setText("0.00", juce::dontSendNotification);

        // Метки крайних положений: Clean / Loop
        addAndMakeVisible(mixMinLabel);
        mixMinLabel.setText("Clean", juce::dontSendNotification);
        mixMinLabel.setFont({ 14.0f, juce::Font::bold });
        mixMinLabel.setJustificationType(juce::Justification::centredLeft);

        addAndMakeVisible(mixMaxLabel);
        mixMaxLabel.setText("Loop", juce::dontSendNotification);
        mixMaxLabel.setFont({ 14.0f, juce::Font::bold });
        mixMaxLabel.setJustificationType(juce::Justification::centredRight);

        // кнопки RESET / CTRL / TRG
        addAndMakeVisible(resetBtn);
        resetBtn.setButtonText("RESET");
        resetBtn.addListener(this);
        resetBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::lightblue);

        addAndMakeVisible(controlBtn);
        controlBtn.setButtonText("CTRL");
        controlBtn.addListener(this);

        addAndMakeVisible(triggerBtn);
        triggerBtn.setButtonText("TRG");
        triggerBtn.setClickingTogglesState(true);
        triggerBtn.addListener(this);

        // прогресс-бар
        addAndMakeVisible(progressSlider);
        progressSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        progressSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        progressSlider.setRange(0.0, 1.0, 0.0);
        progressSlider.setEnabled(false);

        // таймеры
        addAndMakeVisible(currentTimeLabel);
        currentTimeLabel.setJustificationType(juce::Justification::centredLeft);
        currentTimeLabel.setFont({32.0f, juce::Font::bold});
        currentTimeLabel.setText("00:00", juce::dontSendNotification);

        addAndMakeVisible(totalTimeLabel);
        totalTimeLabel.setJustificationType(juce::Justification::centredRight);
        totalTimeLabel.setFont({32.0f, juce::Font::bold});
        totalTimeLabel.setText("00:00", juce::dontSendNotification);

        startTimerHz(30);
        updateStateLabel();
    }
    
    ~LooperComponent() override
    {
        resetBtn   .setLookAndFeel(nullptr);
        controlBtn .setLookAndFeel(nullptr);
        triggerBtn .setLookAndFeel(nullptr);
        levelSlider.setLookAndFeel(nullptr);
        mixSlider  .setLookAndFeel(nullptr);
        progressSlider.setLookAndFeel(nullptr);
    }
    // ─── Добавляем метод для масштабирования ───
    void setScale(float newScale) noexcept
    {
        scale = newScale;
        resized();  // сразу перестраиваем GUI
    }
    void LooperComponent::resized()
    {
        // 1) режем весь компонент на 5×5
        Grid grid(getLocalBounds(), 5, 5);

        // 2) отступы = 1/10 от ширины/высоты ячейки
        auto cell0 = grid.getSector(0);
        int mx = cell0.getWidth() / 10;
        int my = cell0.getHeight() / 10;

        // ───── row 0 ─────
        // stateLabel в cols 1..3 (индексы 1,2,3)
        stateLabel.setBounds(grid.getUnion(1, 3).reduced(mx, my));

        // LEVEL в col 0 (индекс 0)
        {
            auto b = grid.getSector(0).reduced(mx, my);
            levelSlider.setBounds(b);
            levelTextLabel.setBounds(
                b.getX(),
                b.getCentreY() - 10,
                b.getWidth(), 20);
        }

        // MIX в col 4 (индекс 4)
        {
            auto b = grid.getSector(4).reduced(mx, my);
            mixSlider.setBounds(b);
            mixTextLabel.setBounds(
                b.getX(),
                b.getCentreY() - 10,
                b.getWidth(), 20);
            mixValueLabel.setBounds(
                b.getX(),
                b.getBottom() + 2,
                b.getWidth(), 16);

            // Clean / Loop под ним
            const int labelH = 20, yOff = 20;
            mixMinLabel.setBounds(
                b.getX(),
                b.getBottom() + yOff,
                b.getWidth() / 2, labelH);
            mixMaxLabel.setBounds(
                b.getX() + b.getWidth() / 2,
                b.getBottom() + yOff,
                b.getWidth() / 2, labelH);
        }

        // ───── row 1 ─────
        // cols 1..3 → индексы 6,7,8
        currentTimeLabel.setBounds(grid.getSector(6).reduced(mx, my));
        progressSlider.setBounds(grid.getSector(7).reduced(mx, my));
        totalTimeLabel.setBounds(grid.getSector(8).reduced(mx, my));

        // ───── row 4 ─────
        // cols 1,2,3 → индексы 21,22,23
        resetBtn.setBounds(grid.getSector(11).reduced(mx, my));
        controlBtn.setBounds(grid.getSector(12).reduced(mx, my));
        triggerBtn.setBounds(grid.getSector(13).reduced(mx, my));
    }


private:
    //

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
        controlBtn.setToggleState(
            st == S::Recording || st == S::Playing,
            juce::dontSendNotification);
    }

    void buttonClicked(juce::Button* b) override
    {
        if (b == &resetBtn)
        {
            engine.reset();
            updateStateLabel();
            progressSlider .setValue(0.0, juce::dontSendNotification);
            currentTimeLabel.setText("00:00", juce::dontSendNotification);
            totalTimeLabel  .setText("00:00", juce::dontSendNotification);
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
                triggerBtn.getToggleState()
                    ? juce::Colours::yellow
                    : juce::Colours::darkgrey);
        }
    }

    void sliderValueChanged(juce::Slider* s) override
    {
        if (s == &levelSlider)
            engine.setLevel((float)levelSlider.getValue());

        if (s == &mixSlider)
        {
            // обновляем numeric balance
            auto bal = (mixSlider.getValue() - 0.5) * 2.0;
            mixValueLabel.setText(juce::String(bal, 2),
                                  juce::dontSendNotification);

            engine.setMix((float)mixSlider.getValue());
        }
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

        double pos = tot > 0.0 ? (cur / tot) : 0.0;
        progressSlider.setValue(pos, juce::dontSendNotification);

        currentTimeLabel.setText(formatTime(cur), juce::dontSendNotification);
        totalTimeLabel .setText(formatTime(tot), juce::dontSendNotification);
    }

    static juce::String formatTime(double s)
    {
        int t = int(std::floor(s));
        return juce::String::formatted("%02d:%02d", t / 60, t % 60);
    }

    LooperEngine& engine;

    juce::Label    stateLabel;

    juce::Slider   levelSlider;
    juce::Label    levelTextLabel;

    juce::Slider   mixSlider;
    juce::Label    mixTextLabel;
    juce::Label    mixValueLabel;
    juce::Label    mixMinLabel;
    juce::Label    mixMaxLabel;

    juce::TextButton resetBtn, controlBtn, triggerBtn;
    CustomLookAndFeel buttonLnf;

    juce::Slider    progressSlider;
    juce::Label     currentTimeLabel, totalTimeLabel;
  
    float scale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LooperComponent)
};
