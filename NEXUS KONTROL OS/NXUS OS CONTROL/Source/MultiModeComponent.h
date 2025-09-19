#pragma once

#include <JuceHeader.h>
#include "MultiModeEngine.h"
#include "Grid.h"
#include "fount_label.h"

class MultiModeComponent : public juce::Component,
    private juce::Button::Listener,
    private juce::Slider::Listener,
    private juce::Timer,
    private MultiModeEngine::Listener
{
public:
    explicit MultiModeComponent(MultiModeEngine& eng) : engine(eng)
    {
        engine.addListener(this);

        // === Инициализация элементов ===
        stateLabel.setText("State", juce::dontSendNotification);
        levelTextLabel.setText("Level", juce::dontSendNotification);
        mixTextLabel.setText("Trigger", juce::dontSendNotification);

        levelSlider.setRange(0.0, 1.0, 0.01);
        mixSlider.setRange(0.0, 1.0, 0.01);
        progressSlider.setRange(0.0, 1.0, 0.001);
        progressSlider.setEnabled(false);

        controlBtn.setButtonText("Control");
        resetBtn.setButtonText("Reset");
        triggerBtn.setButtonText("Trigger");
        fileSelectBtn.setButtonText("Select File");
        modeSwitchBtn.setButtonText("Switch Mode");

        // === Добавляем в интерфейс ===
        addAndMakeVisible(stateLabel);
        addAndMakeVisible(levelTextLabel);
        addAndMakeVisible(mixTextLabel);
        addAndMakeVisible(levelSlider);
        addAndMakeVisible(mixSlider);
        addAndMakeVisible(progressSlider);
        addAndMakeVisible(currentTimeLabel);
        addAndMakeVisible(totalTimeLabel);
        addAndMakeVisible(controlBtn);
        addAndMakeVisible(resetBtn);
        addAndMakeVisible(triggerBtn);
        addAndMakeVisible(fileSelectBtn);
        addAndMakeVisible(modeSwitchBtn);

        // === Подключаем слушатели ===
        levelSlider.addListener(this);
        mixSlider.addListener(this);
        controlBtn.addListener(this);
        resetBtn.addListener(this);
        triggerBtn.addListener(this);
        fileSelectBtn.addListener(this);
        modeSwitchBtn.addListener(this);

        startTimerHz(30);
    }

    ~MultiModeComponent() override
    {
        engine.removeListener(this);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::darkgrey);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);
        stateLabel.setBounds(area.removeFromTop(20));
        levelTextLabel.setBounds(area.removeFromTop(20));
        levelSlider.setBounds(area.removeFromTop(40));
        mixTextLabel.setBounds(area.removeFromTop(20));
        mixSlider.setBounds(area.removeFromTop(40));
        progressSlider.setBounds(area.removeFromTop(30));
        currentTimeLabel.setBounds(area.removeFromTop(20));
        totalTimeLabel.setBounds(area.removeFromTop(20));
        controlBtn.setBounds(area.removeFromTop(30));
        resetBtn.setBounds(area.removeFromTop(30));
        triggerBtn.setBounds(area.removeFromTop(30));
        fileSelectBtn.setBounds(area.removeFromTop(30));
        modeSwitchBtn.setBounds(area.removeFromTop(30));
    }

private:
    MultiModeEngine& engine;

    juce::Label stateLabel, levelTextLabel, mixTextLabel;
    juce::Slider levelSlider, mixSlider, progressSlider;
    juce::Label currentTimeLabel, totalTimeLabel;
    juce::TextButton resetBtn, controlBtn, triggerBtn, fileSelectBtn, modeSwitchBtn;

    void buttonClicked(juce::Button* b) override
    {
        if (b == &modeSwitchBtn)
        {
            auto newMode = (engine.getMode() == MultiModeEngine::Mode::Looper)
                ? MultiModeEngine::Mode::Player
                : MultiModeEngine::Mode::Looper;
            engine.setMode(newMode);
            return;
        }

        if (engine.getMode() == MultiModeEngine::Mode::Looper)
        {
            if (b == &controlBtn) engine.controlButtonPressed();
            else if (b == &resetBtn) engine.resetLooper();
        }
        else
        {
            if (b == &controlBtn) engine.togglePlayerPlayback();
            else if (b == &fileSelectBtn) engine.openFileDialog();
        }
    }

    void sliderValueChanged(juce::Slider* s) override
    {
        if (s == &levelSlider) engine.setLevel((float)s->getValue());
        else if (s == &mixSlider) engine.setTriggerThreshold((float)s->getValue());
    }

    void timerCallback() override
    {
        if (engine.getMode() == MultiModeEngine::Mode::Looper)
        {
            progressSlider.setValue(engine.getLooperProgress());
            currentTimeLabel.setText("Time: " + juce::String(engine.getPlayPositionSeconds(), 2), juce::dontSendNotification);
        }
        else
        {
            progressSlider.setValue(engine.getPlayerProgress());
            currentTimeLabel.setText("Time: " + juce::String(engine.getPlayerPositionSeconds(), 2), juce::dontSendNotification);
        }
    }

    void engineChanged() override
    {
        stateLabel.setText(engine.getMode() == MultiModeEngine::Mode::Looper ? "Looper" : "Player", juce::dontSendNotification);
        resized(); // обновить layout если нужно
    }
};
