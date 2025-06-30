#include "LooperComponent.h"

LooperComponent::LooperComponent(LooperEngine& eng)
    : engine(eng)
{
    // — LookAndFeel на кнопки/слайдеры
    for (auto* btn : { &resetBtn, &controlBtn, &triggerBtn })
        btn->setLookAndFeel(&buttonLnf);
    for (auto* sl : { &levelSlider, &mixSlider, &progressSlider })
        sl->setLookAndFeel(&buttonLnf);

    // — STATE label
    addAndMakeVisible(stateLabel);
    stateLabel.setJustificationType(juce::Justification::centred);
    stateLabel.setFont({ 100.0f, juce::Font::bold });
    stateLabel.setColour(juce::Label::textColourId, juce::Colours::black);

    // — LEVEL
    addAndMakeVisible(levelSlider);
    levelSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    levelSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    levelSlider.setRange(0.0, 1.0, 0.01);
    levelSlider.setValue(1.0);
    levelSlider.addListener(this);

    addAndMakeVisible(levelTextLabel);
    levelTextLabel.setText("Level", juce::dontSendNotification);
    levelTextLabel.setJustificationType(juce::Justification::centred);

    // — MIX
    addAndMakeVisible(mixSlider);
    mixSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    mixSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    mixSlider.setRange(0.0, 1.0, 0.01);
    mixSlider.setValue(0.5);
    mixSlider.addListener(this);

    addAndMakeVisible(mixTextLabel);
    mixTextLabel.setText("MIX", juce::dontSendNotification);
    mixTextLabel.setJustificationType(juce::Justification::centred);

    // — MIX value & min/max labels
    addAndMakeVisible(mixValueLabel);
    mixValueLabel.setJustificationType(juce::Justification::centred);
    mixValueLabel.setFont({ 14.0f, juce::Font::plain });

    addAndMakeVisible(mixMinLabel);
    mixMinLabel.setText("Clean", juce::dontSendNotification);
    mixMinLabel.setJustificationType(juce::Justification::centredLeft);

    addAndMakeVisible(mixMaxLabel);
    mixMaxLabel.setText("Loop", juce::dontSendNotification);
    mixMaxLabel.setJustificationType(juce::Justification::centredRight);

    // — RESET button
    addAndMakeVisible(resetBtn);
    resetBtn.setButtonText("RESET");
    resetBtn.addListener(this);

    // — CONTROL button
    addAndMakeVisible(controlBtn);
    controlBtn.addListener(this);

    // — TRIGGER button
    addAndMakeVisible(triggerBtn);
    triggerBtn.setButtonText("TRG");
    triggerBtn.setClickingTogglesState(true);
    triggerBtn.addListener(this);

    // — PROGRESS slider
    addAndMakeVisible(progressSlider);
    progressSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    progressSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    progressSlider.setEnabled(false);

    // — TIME labels
    addAndMakeVisible(currentTimeLabel);
    currentTimeLabel.setJustificationType(juce::Justification::centredLeft);
    currentTimeLabel.setFont({ 32.0f, juce::Font::bold });

    addAndMakeVisible(totalTimeLabel);
    totalTimeLabel.setJustificationType(juce::Justification::centredRight);
    totalTimeLabel.setFont({ 32.0f, juce::Font::bold });

    startTimerHz(30);
    updateStateLabel();
}

LooperComponent::~LooperComponent()
{
    for (auto* btn : { &resetBtn, &controlBtn, &triggerBtn })
        btn->setLookAndFeel(nullptr);
    for (auto* sl : { &levelSlider, &mixSlider, &progressSlider })
        sl->setLookAndFeel(nullptr);
}

void LooperComponent::setScale(float newScale) noexcept
{
    scale = newScale;
    resized();
}

void LooperComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey);
}

void LooperComponent::resized()
{
    // 5 столбцов × 3 строки
    Grid grid(getLocalBounds(), 5, 3);

    auto cell0 = grid.getSector(0);
    int mx = cell0.getWidth() / 10,
        my = cell0.getHeight() / 10;

    auto sector = [&](int r, int c) {
        return grid.getSector(r * 5 + c).reduced(mx, my);
        };
    auto span = [&](int r, int c0, int c1) {
        return grid.getUnion(r * 5 + c0, r * 5 + c1).reduced(mx, my);
        };

    // ROW 0
    {
        auto b = sector(0, 0);
        levelSlider.setBounds(b);
        levelTextLabel.setBounds(b.getX(), b.getCentreY() - 10, b.getWidth(), 20);

        stateLabel.setBounds(span(0, 1, 3));

        auto m = sector(0, 4);
        mixSlider.setBounds(m);
        mixTextLabel.setBounds(m.getX(), m.getCentreY() - 10, m.getWidth(), 20);
        mixValueLabel.setBounds(m.getX(), m.getBottom() + 2, m.getWidth(), 16);
        mixMinLabel.setBounds(m.getX(), m.getBottom() + 20, m.getWidth() / 2, 20);
        mixMaxLabel.setBounds(m.getX() + m.getWidth() / 2, m.getBottom() + 20, m.getWidth() / 2, 20);
    }

    // ROW 1
    currentTimeLabel.setBounds(sector(1, 1));
    progressSlider.setBounds(sector(1, 2));
    totalTimeLabel.setBounds(sector(1, 3));

    // ROW 2
    resetBtn.setBounds(sector(2, 1));
    controlBtn.setBounds(sector(2, 2));
    triggerBtn.setBounds(sector(2, 3));
}

void LooperComponent::buttonClicked(juce::Button* b)
{
    if (!engine.isPreparedSuccessfully())
        return;
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
        triggerBtn.setColour(juce::TextButton::buttonOnColourId,
            triggerBtn.getToggleState() ? juce::Colours::yellow
            : juce::Colours::darkgrey);
    }
}

void LooperComponent::sliderValueChanged(juce::Slider* s)
{
    // если движок ещё не подготовлен — просто игнорируем
    if (!engine.isPreparedSuccessfully())
        return;

    if (s == &levelSlider)
        engine.setLevel((float)levelSlider.getValue());

    if (s == &mixSlider)
    {
        auto bal = (mixSlider.getValue() - 0.5) * 2.0;
        mixValueLabel.setText(juce::String(bal, 2),
            juce::dontSendNotification);
        engine.setMix((float)mixSlider.getValue());
    }
}
void LooperComponent::timerCallback()
{
    // если движок ещё не подготовлен — честно выходим
    if (!engine.isPreparedSuccessfully())
    {
        // можно сразу обнулить прогресс, чтобы UI не вёл себя странно:
        progressSlider.setValue(0.0, juce::dontSendNotification);
        currentTimeLabel.setText("00:00", juce::dontSendNotification);
        totalTimeLabel.setText("00:00", juce::dontSendNotification);
        return;
    }

    using S = LooperEngine::State;
    auto st = engine.getState();

    double curSec = 0.0, totSec = 0.0;
    switch (st)
    {
    case S::Clean:
        curSec = 0.0;
        totSec = LooperEngine::getMaxRecordSeconds();
        break;
    case S::Recording:
        curSec = engine.getRecordedLengthSeconds();
        totSec = LooperEngine::getMaxRecordSeconds();
        break;
    case S::Stopped:
        curSec = engine.getLoopLengthSeconds();
        totSec = engine.getLoopLengthSeconds();
        break;
    case S::Playing:
        curSec = engine.getPlayPositionSeconds();
        totSec = engine.getLoopLengthSeconds();
        break;
    }

    if (st != lastState || !juce::approximatelyEqual(totSec, lastTotal))
    {
        progressSlider.setRange(0.0, totSec, 0.0);
        totalTimeLabel.setText(formatTime(totSec), juce::dontSendNotification);
        lastTotal = totSec;
    }

    progressSlider.setValue(curSec, juce::dontSendNotification);
    currentTimeLabel.setText(formatTime(curSec), juce::dontSendNotification);

    if (st != lastState)
        updateStateLabel();

    lastState = st;
}

void LooperComponent::updateStateLabel()
{
    using S = LooperEngine::State;
    static constexpr const char* stateNames[] = { "CLEAN","REC","STOP","PLAY" };
    static constexpr const char* controlNames[] = { "RECORD","STOP","PLAY","STOP" };

    auto st = engine.getState();
    stateLabel.setText(stateNames[int(st)], juce::dontSendNotification);
    controlBtn.setButtonText(controlNames[int(st)]);
    controlBtn.setToggleState(st == S::Recording || st == S::Playing,
        juce::dontSendNotification);

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

   
}

juce::String LooperComponent::formatTime(double seconds)
{
    int totalS = int(std::floor(seconds + 0.5));
    return juce::String::formatted("%02d:%02d", totalS / 60, totalS % 60);
}

