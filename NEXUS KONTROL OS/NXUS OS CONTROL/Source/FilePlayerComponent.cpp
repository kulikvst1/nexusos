#include "FilePlayerComponent.h"
#include <JuceHeader.h>

FilePlayerComponent::FilePlayerComponent(FilePlayerEngine& eng)
    : engine(eng)
{
    for (auto* btn : { &resetBtn, &controlBtn, &triggerBtn, &fileSelectBtn })
        btn->setLookAndFeel(&buttonLnf);

    // STATE
    addAndMakeVisible(stateLabel);
    stateLabel.setJustificationType(juce::Justification::centred);
    stateLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    stateLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    stateLabel.setFont({ 100.0f, juce::Font::bold });

    // LEVEL
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

    // MIX (Trigger threshold)
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

  

    // Buttons
    addAndMakeVisible(resetBtn);
    resetBtn.setButtonText("RESET");
    resetBtn.addListener(this);

    addAndMakeVisible(controlBtn);
    controlBtn.addListener(this);

    addAndMakeVisible(triggerBtn);
    triggerBtn.setButtonText("TRG");
    triggerBtn.setClickingTogglesState(true);
    triggerBtn.addListener(this);

    addAndMakeVisible(fileSelectBtn);
    fileSelectBtn.setButtonText("📁");
    fileSelectBtn.addListener(this);

    // Progress
    addAndMakeVisible(progressSlider);
    progressSlider.setSliderStyle(juce::Slider::LinearBar);
    progressSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    progressSlider.setRange(0.0, 1.0, 0.0);
    progressSlider.setInterceptsMouseClicks(false, false);

    // Time labels
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

    startTimerHz(30);
    updateStateLabel();
}

FilePlayerComponent::~FilePlayerComponent()
{
    for (auto* btn : { &resetBtn, &controlBtn, &triggerBtn, &fileSelectBtn })
        btn->setLookAndFeel(nullptr);
    for (auto* sl : { &levelSlider, &mixSlider, &progressSlider })
        sl->setLookAndFeel(nullptr);
}

void FilePlayerComponent::setScale(float newScale) noexcept
{
    scale = newScale;
    resized();
}

void FilePlayerComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey);
}

void FilePlayerComponent::resized()
{
    // 5 cols × 3 rows, как в лупере
    Grid grid(getLocalBounds(), 5, 3);

    auto cell0 = grid.getSector(0);
    int mx = cell0.getWidth() / 10;
    int my = cell0.getHeight() / 10;

    auto sector = [&](int r, int c) { return grid.getSector(r * 5 + c).reduced(mx, my); };
    auto span = [&](int r, int c0, int c1) { return grid.getUnion(r * 5 + c0, r * 5 + c1).reduced(mx, my); };

    // ROW 0
    {
        auto m = sector(0, 0);
        mixSlider.setBounds(m);
       
        auto b = sector(0, 4);
        levelSlider.setBounds(b);

        stateLabel.setBounds(span(0, 1, 3));
    }

    // ROW 1
    {
        auto sMix = sector(1, 0);
        mixTextLabel.setBounds(sMix.withHeight(sMix.getHeight() / 4));
        mixTextLabel.setColour(juce::Label::backgroundColourId, juce::Colours::white);
        mixTextLabel.setColour(juce::Label::textColourId, juce::Colours::black);

        auto sLevel = sector(1, 4);
        levelTextLabel.setBounds(sLevel.withHeight(sLevel.getHeight() / 4));
        levelTextLabel.setColour(juce::Label::backgroundColourId, juce::Colours::white);
        levelTextLabel.setColour(juce::Label::textColourId, juce::Colours::black);

        auto sProgress = span(1, 1, 3);
        int h = sProgress.getHeight() / 4;
        int y = sProgress.getY() + h * 0;
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

    // ROW 2
    {
        resetBtn.setBounds(sector(2, 1));
        controlBtn.setBounds(sector(2, 2));
        triggerBtn.setBounds(sector(2, 3));
        fileSelectBtn.setBounds(sector(2, 4));
    }
}

// === Audio hooks ===
void FilePlayerComponent::prepareToPlay(int spb, double sr)
{
    engine.prepareToPlay(spb, sr);
}

void FilePlayerComponent::releaseResources()
{
    engine.releaseResources();
}

void FilePlayerComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    engine.getNextAudioBlock(info);
}

void FilePlayerComponent::onExternalSignal()
{
    engine.onSignalDetected();
}
void FilePlayerComponent::buttonClicked(juce::Button* b)
{
    if (b == &resetBtn)
    {
        engine.reset();
        progressSlider.setValue(0.0, juce::dontSendNotification);
        currentTimeLabel.setText("00:00", juce::dontSendNotification);
        totalTimeLabel.setText("00:00", juce::dontSendNotification);
        updateStateLabel();
    }
    else if (b == &controlBtn)
    {
        if (engine.isPlaying() || engine.isWaitingForTrigger())
        {
            engine.stop(); // ⬅️ сбрасывает и воспроизведение, и ожидание
        }
        else
        {
            if (engine.isTriggerArmed())
            {
                engine.armTriggerAndWait(); // ⬅️ ждём сигнал
            }
            else
            {
                engine.startFromTop(); // ⬅️ ручной запуск
            }
        }

        updateStateLabel();
    }

    else if (b == &triggerBtn)
    {
        engine.setTriggerArmed(triggerBtn.getToggleState());
    }
    else if (b == &fileSelectBtn)
    {
        fileChooser.reset(new juce::FileChooser("select", {}, "*.wav,*.mp3,*.flac"));

        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file.existsAsFile())
                {
                    if (engine.loadFile(file))
                        updateStateLabel();
                }
            });
    }
}

void FilePlayerComponent::sliderValueChanged(juce::Slider* s)
{
    if (s == &levelSlider)
    {
        engine.setLevel((float)levelSlider.getValue());
    }
    else if (s == &mixSlider)
    {
        engine.setTriggerThreshold((float)mixSlider.getValue());
       
    }
}

void FilePlayerComponent::timerCallback()
{
    if (!engine.isReady())
    {
        progressSlider.setColour(juce::Slider::trackColourId, juce::Colours::black);
        progressSlider.setValue(0.0, juce::dontSendNotification);
        currentTimeLabel.setText("00:00", juce::dontSendNotification);
        totalTimeLabel.setText("00:00", juce::dontSendNotification);

        // синхронизация кнопки TRG
        if (triggerBtn.getToggleState() != false)
            triggerBtn.setToggleState(false, juce::dontSendNotification);
        triggerBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colours::darkgrey);

        updateStateLabel();
        return;
    }

    const double curSec = engine.getCurrentTime();
    const double totSec = engine.getTotalTime();

    // цвет прогресса по состоянию
    progressSlider.setColour(juce::Slider::trackColourId,
        engine.isPlaying() ? juce::Colours::green : juce::Colours::black);

    // обновление диапазона и тотала
    if (!juce::approximatelyEqual(totSec, lastTotal))
    {
        progressSlider.setRange(0.0, totSec, 0.0);
        totalTimeLabel.setText(formatTime(totSec), juce::dontSendNotification);
        lastTotal = totSec;
    }

    // значения
    progressSlider.setValue(curSec, juce::dontSendNotification);
    currentTimeLabel.setText(formatTime(curSec), juce::dontSendNotification);

    // синхронизация слайдеров
    const float lev = engine.getLevel();
    if (!juce::approximatelyEqual((float)levelSlider.getValue(), lev))
        levelSlider.setValue(lev, juce::dontSendNotification);

    const float thr = engine.getTriggerThreshold();
    if (!juce::approximatelyEqual((float)mixSlider.getValue(), thr))
        mixSlider.setValue(thr, juce::dontSendNotification);
   

    // синхронизация кнопки TRG
    const bool trig = engine.isTriggerArmed();
    if (triggerBtn.getToggleState() != trig)
        triggerBtn.setToggleState(trig, juce::dontSendNotification);
    triggerBtn.setColour(juce::TextButton::buttonOnColourId,
        trig ? juce::Colours::yellow : juce::Colours::darkgrey);

    // мигание stateLabel при активном триггере
    ++blinkCounter;

    if (engine.isWaitingForTrigger())
    {
        const int half = juce::jmax(1, blinkPeriodTicks / 2);
        blinkOn = ((blinkCounter / half) & 1) != 0;

        stateLabel.setText("PLAY", juce::dontSendNotification);
        stateLabel.setColour(juce::Label::backgroundColourId,
            blinkOn ? juce::Colours::green : juce::Colours::transparentBlack);

        controlBtn.setButtonText("STOP");
    }
    else
    {
        updateStateLabel();
        blinkCounter = 0;
    }



}

// === State label and control ===
void FilePlayerComponent::updateStateLabel()
{
    // состояние: CLEAN (нет файла), STOP, PLAY
    const bool ready = engine.isReady();
    const bool playing = engine.isPlaying();

    const char* stateText = !ready ? "CLEAN" : (playing ? "PLAY" : "STOP");
    stateLabel.setText(stateText, juce::dontSendNotification);

    // цвета: как в лупере (Clean=lightblue, Stopped=yellow, Playing=green)
    juce::Colour currentColour = !ready ? juce::Colours::lightblue
        : (playing ? juce::Colours::green
            : juce::Colours::yellow);
    stateLabel.setColour(juce::Label::backgroundColourId, currentColour);

    // кнопка управления: PLAY -> next STOP, STOP/CLEAN -> next PLAY
    const bool willPlay = ready && !playing;
    juce::Colour nextColour = willPlay ? juce::Colours::green : juce::Colours::yellow;

    controlBtn.setButtonText(willPlay ? "PLAY" : "STOP");
    controlBtn.setToggleState(!willPlay, juce::dontSendNotification);
    controlBtn.setColour(juce::TextButton::buttonColourId, nextColour);
    controlBtn.setColour(juce::TextButton::buttonOnColourId, nextColour);

    auto textCol = nextColour.contrasting(0.7f);
    controlBtn.setColour(juce::TextButton::textColourOffId, textCol);
    controlBtn.setColour(juce::TextButton::textColourOnId, textCol);
}

juce::String FilePlayerComponent::formatTime(double seconds)
{
    int totalS = int(std::floor(seconds + 0.5));
    return juce::String::formatted("%02d:%02d", totalS / 60, totalS % 60);
}
