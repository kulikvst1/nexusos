#include "LooperComponent.h"

LooperComponent::LooperComponent(LooperEngine& eng)
    : engine(eng)
{
    // === LookAndFeel ===
    for (auto* btn : { &resetBtn, &controlBtn, &triggerBtn })
        btn->setLookAndFeel(&buttonLnf);

    // === STATE label ===
    addAndMakeVisible(stateLabel);
    stateLabel.setJustificationType(juce::Justification::centred);
    stateLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    stateLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    stateLabel.setFont({ 100.0f, juce::Font::bold });

    // === LEVEL ===
    addAndMakeVisible(levelSlider);
    levelSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    levelSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    levelSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::red);
    levelSlider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::black);
    levelSlider.setRange(0.0, 1.0, 0.01);
    levelSlider.setValue(1.0);
    levelSlider.addListener(this);

    addAndMakeVisible(levelTextLabel);
    levelTextLabel.setText("Level", juce::dontSendNotification);
    levelTextLabel.setJustificationType(juce::Justification::centred);
    levelTextLabel.setFont(juce::Font(20.0f, juce::Font::bold));

    // === MIX ===
    addAndMakeVisible(mixSlider);
    mixSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    mixSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    mixSlider.setColour(juce::Slider::trackColourId, juce::Colours::red);
    mixSlider.setColour(juce::Slider::backgroundColourId, juce::Colours::red);
    mixSlider.setRange(0.0, 1.0, 0.01);
    mixSlider.setValue(0.02);
    mixSlider.addListener(this);

    addAndMakeVisible(mixTextLabel);
    mixTextLabel.setText("TRgate", juce::dontSendNotification);
    mixTextLabel.setJustificationType(juce::Justification::centred);
    mixTextLabel.setFont(juce::Font(20.0f, juce::Font::bold));

    // === Buttons ===
    addAndMakeVisible(resetBtn);
    resetBtn.setButtonText("RESET");
    resetBtn.addListener(this);

    addAndMakeVisible(controlBtn);
    controlBtn.addListener(this);

    addAndMakeVisible(triggerBtn);
    triggerBtn.setButtonText("TRG");
    triggerBtn.setClickingTogglesState(true);
    triggerBtn.addListener(this);

    // === Progress ===
    addAndMakeVisible(progressSlider);
    progressSlider.setSliderStyle(juce::Slider::LinearBar);
    progressSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    progressSlider.setRange(0.0, 1.0, 0.0);
    progressSlider.setInterceptsMouseClicks(false, false); // не кликабельный, но яркий


    // === Time Labels ===
    addAndMakeVisible(currentTimeLabel);
    currentTimeLabel.setJustificationType(juce::Justification::centredLeft);
    currentTimeLabel.setFont({ 32.0f, juce::Font::bold });
    currentTimeLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    currentTimeLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);

    addAndMakeVisible(totalTimeLabel);
    totalTimeLabel.setJustificationType(juce::Justification::centredRight);
    totalTimeLabel.setFont({ 32.0f, juce::Font::bold });
    totalTimeLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    totalTimeLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);


    // === Sync & Timer ===
    startTimerHz(30);
    engine.addListener(this);
    updateFromEngine();
    updateStateLabel();
}


LooperComponent::~LooperComponent()
{
    for (auto* btn : { &resetBtn, &controlBtn, &triggerBtn })
        btn->setLookAndFeel(nullptr);
    for (auto* sl : { &levelSlider, &mixSlider, &progressSlider })
        sl->setLookAndFeel(nullptr);
    engine.removeListener(this);    // отписка
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
    int mx = cell0.getWidth() / 10;
    int my = cell0.getHeight() / 10;

    auto sector = [&](int r, int c) {
        return grid.getSector(r * 5 + c).reduced(mx, my);
        };
    auto span = [&](int r, int c0, int c1) {
        return grid.getUnion(r * 5 + c0, r * 5 + c1).reduced(mx, my);
        };

    // === ROW 0: слайдеры MIX и LEVEL + stateLabel ===
    {
        auto m = sector(0, 0);
        mixSlider.setBounds(m);
        mixValueLabel.setBounds(m.getX(), m.getBottom() + 2, m.getWidth(), 16);
        mixMinLabel.setBounds(m.getX(), m.getBottom() + 20, m.getWidth() / 2, 20);
        mixMaxLabel.setBounds(m.getX() + m.getWidth() / 2, m.getBottom() + 20, m.getWidth() / 2, 20);

        auto b = sector(0, 4);
        levelSlider.setBounds(b);

        stateLabel.setBounds(span(0, 1, 3));
    }

    // === ROW 1: метки и прогресс — всё поднято вверх ===
    {
        auto sMix = sector(1, 0);
        mixTextLabel.setBounds(sMix.withHeight(sMix.getHeight() / 4));
        mixTextLabel.setColour(juce::Label::backgroundColourId, juce::Colours::white);
        mixTextLabel.setColour(juce::Label::textColourId, juce::Colours::black);

        auto sLevel = sector(1, 4);
        levelTextLabel.setBounds(sLevel.withHeight(sLevel.getHeight() / 4));
        levelTextLabel.setColour(juce::Label::backgroundColourId, juce::Colours::white);
        levelTextLabel.setColour(juce::Label::textColourId, juce::Colours::black);

        auto sProgress = span(1, 1, 3); // объединение колонок 2 и 3
        int h = sProgress.getHeight() / 4;
        int y = sProgress.getY() + h * 0; // опускаем в третью четверть сектора
        progressSlider.setBounds(sProgress.getX(), y, sProgress.getWidth(), h);

        auto sCurrent = sector(0, 1);
        int hCur = sCurrent.getHeight() / 4;
        int yCur = sCurrent.getBottom() - hCur;
        currentTimeLabel.setBounds(sCurrent.getX(), yCur, sCurrent.getWidth(), hCur);

        auto sTotal = sector(0, 3);
        int hTot = sTotal.getHeight() / 4;
        int yTot = sTotal.getBottom() - hTot;
        totalTimeLabel.setBounds(sTotal.getX(), yTot, sTotal.getWidth(), hTot);

    }

    // === ROW 2: кнопки ===
    {
        resetBtn.setBounds(sector(2, 1));
        controlBtn.setBounds(sector(2, 2));
        triggerBtn.setBounds(sector(2, 3));
    }
}

void LooperComponent::buttonClicked(juce::Button* b)
{
    if (!engine.isPreparedSuccessfully())
        return;

    if (b == &resetBtn)
    {
        engine.reset();
        progressSlider.setValue(0.0, juce::dontSendNotification);
        currentTimeLabel.setText("00:00", juce::dontSendNotification);
        totalTimeLabel.setText("00:00", juce::dontSendNotification);
        // updateStateLabel() будет вызван через updateFromEngine()
    }
    else if (b == &controlBtn)
    {
        engine.controlButtonPressed();
        // updateStateLabel() будет вызван через updateFromEngine()
    }
    else if (b == &triggerBtn)
    {
        engine.setTriggerEnabled(triggerBtn.getToggleState());
        // Цвет и toggle обновятся через updateFromEngine()
    }
}
void LooperComponent::sliderValueChanged(juce::Slider* s)
{
    if (!engine.isPreparedSuccessfully())
        return;

    if (s == &levelSlider)
    {
        engine.setLevel((float)levelSlider.getValue());
        // Слайдер обновится через updateFromEngine()
    }
    else if (s == &mixSlider)
    {
        engine.setTriggerThreshold((float)mixSlider.getValue());
        // mixValueLabel обновится через updateFromEngine()
    }
}

void LooperComponent::timerCallback()
{
    // если движок ещё не подготовлен — честно выходим
    if (!engine.isPreparedSuccessfully())
    {
        auto st = engine.getState();

        switch (st)
        {
        case LooperEngine::Recording:
            progressSlider.setColour(juce::Slider::trackColourId, juce::Colours::red);
            break;
        case LooperEngine::Playing:
            progressSlider.setColour(juce::Slider::trackColourId, juce::Colours::green);
            break;
        default:
            progressSlider.setColour(juce::Slider::trackColourId, juce::Colours::black);
            break;
        }

        progressSlider.setValue(0.0, juce::dontSendNotification);
        currentTimeLabel.setText("00:00", juce::dontSendNotification);
        totalTimeLabel.setText("00:00", juce::dontSendNotification);
        return;
    }

    // 1) расчёт текущего и общего времени
    using S = LooperEngine::State;
    auto st = engine.getState();

    // — обновление цвета заливки по состоянию
    switch (st)
    {
    case S::Recording:
        progressSlider.setColour(juce::Slider::trackColourId, juce::Colours::red);
        break;
    case S::Playing:
        progressSlider.setColour(juce::Slider::trackColourId, juce::Colours::green);
        break;
    default:
        progressSlider.setColour(juce::Slider::trackColourId, juce::Colours::black);
        break;
    }

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

    // 2) обновление слайдера и меток времени
    if (st != lastState || !juce::approximatelyEqual(totSec, lastTotal))
    {
        progressSlider.setRange(0.0, totSec, 0.0);
        totalTimeLabel.setText(formatTime(totSec), juce::dontSendNotification);
        lastTotal = totSec;
    }

    progressSlider.setValue(curSec, juce::dontSendNotification);
    currentTimeLabel.setText(formatTime(curSec), juce::dontSendNotification);

    // 3) мигание stateLabel в режиме Armed → Rec
    ++blinkCounter;

    if (engine.isTriggerArmed())
    {
        int half = blinkPeriodTicks / 2;
        blinkOn = ((blinkCounter / half) & 1) != 0;

        stateLabel.setText("RECORD ", juce::dontSendNotification);
        stateLabel.setColour(juce::Label::backgroundColourId,
            blinkOn ? juce::Colours::red
            : juce::Colours::transparentBlack);
    }
    else if (engine.isRecordingLive())
    {
        blinkCounter = 0;
        stateLabel.setText("RECORD", juce::dontSendNotification);
        stateLabel.setColour(juce::Label::backgroundColourId, juce::Colours::red);
    }
    else if (st != lastState)
    {
        updateStateLabel();
        blinkCounter = 0;
    }

    lastState = st;
}

void LooperComponent::updateStateLabel()
{
    using S = LooperEngine::State;
    static constexpr const char* stateNames[] = { "CLEAN", "RECORD", "STOP",  "PLAY" };
    static constexpr const char* controlNames[] = { "RECORD","STOP",   "PLAY",  "STOP" };

    // 1) Получим текущий и следующий режим
    auto st = engine.getState();
    S   nextMode = S::Stopped; // default

    switch (st)
    {
    case S::Clean:     nextMode = S::Recording; break;
    case S::Recording: nextMode = S::Stopped;   break;
    case S::Stopped:   nextMode = S::Playing;   break;
    case S::Playing:   nextMode = S::Stopped;   break;
    }

    // 2) Текст и toggle-флаг
    stateLabel.setText(stateNames[int(st)], juce::dontSendNotification);
    controlBtn.setButtonText(controlNames[int(st)]);
    controlBtn.setToggleState(st == S::Recording || st == S::Playing,
        juce::dontSendNotification);

    // 3) Цвета: для метки — текущий, для кнопки — следующий
    auto colourFor = [](S s)
        {
            switch (s)
            {
            case S::Clean:     return juce::Colours::lightblue;
            case S::Recording: return juce::Colours::red;
            case S::Stopped:   return juce::Colours::yellow;
            case S::Playing:   return juce::Colours::green;
            }
            return juce::Colours::grey;
        };

    juce::Colour currentColour = colourFor(st);
    juce::Colour nextColour = colourFor(nextMode);

    stateLabel.setColour(juce::Label::backgroundColourId, currentColour);

    controlBtn.setColour(juce::TextButton::buttonColourId, nextColour);
    controlBtn.setColour(juce::TextButton::buttonOnColourId, nextColour);

    // текст всегда читаемый
    auto textCol = nextColour.contrasting(0.7f);
    controlBtn.setColour(juce::TextButton::textColourOffId, textCol);
    controlBtn.setColour(juce::TextButton::textColourOnId, textCol);
}


juce::String LooperComponent::formatTime(double seconds)
{
    int totalS = int(std::floor(seconds + 0.5));
    return juce::String::formatted("%02d:%02d", totalS / 60, totalS % 60);
}

void LooperComponent::engineChanged()
{
    updateFromEngine(); // вызывается в Message Thread
}

void LooperComponent::updateFromEngine() noexcept
{
    // Слайдеры
    const float lev = engine.getLevel();
    if (!juce::approximatelyEqual((float)levelSlider.getValue(), lev))
        levelSlider.setValue(lev, juce::dontSendNotification);

    const float thr = engine.getTriggerThreshold();
    if (!juce::approximatelyEqual((float)mixSlider.getValue(), thr))
        mixSlider.setValue(thr, juce::dontSendNotification);

    // Метка под MIX
    mixValueLabel.setText(juce::String(thr, 3), juce::dontSendNotification);

    // Триггер-кнопка
    const bool trig = engine.getTriggerEnabled();
    if (triggerBtn.getToggleState() != trig)
        triggerBtn.setToggleState(trig, juce::dontSendNotification);
    triggerBtn.setColour(juce::TextButton::buttonOnColourId,
        trig ? juce::Colours::yellow : juce::Colours::darkgrey);

    // Обновление текста и цвета controlBtn + stateLabel
    updateStateLabel();
}
