#include "LooperComponent.h"
#include "Rig_control.h"
#include <windows.h>
LooperComponent::LooperComponent(LooperEngine& eng)
    : engine(eng)
{
    for (auto* btn : { &resetBtn, &controlBtn, &triggerBtn, &modeSwitchBtn, &fileSelectBtn })
        btn->setLookAndFeel(&buttonLnf);

    addAndMakeVisible(modeLabel);
    modeLabel.setJustificationType(juce::Justification::centred);
    modeLabel.setFont(juce::Font(50.0f, juce::Font::bold)); // ⬅️ крупный шрифт
    modeLabel.setColour(juce::Label::textColourId, juce::Colours::white); // ⬅️ белый текст
    modeLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack); // ⬅️ прозрачный фон
    modeLabel.setOpaque(false); // ⬅️ отключить заливку


    addAndMakeVisible(fileNameLabel);
    fileNameLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    fileNameLabel.setFont(juce::Font(32.0f, juce::Font::bold));
    fileNameLabel.setJustificationType(juce::Justification::centred);
    fileNameLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);

    addAndMakeVisible(stateLabel);
    stateLabel.setJustificationType(juce::Justification::centred);
    stateLabel.setFont({ 100.0f, juce::Font::bold });

    addAndMakeVisible(levelSlider);
    levelSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    levelSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    levelSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::black);
    levelSlider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::white);
    levelSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::red);
    levelSlider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::black);
    levelSlider.setRange(0.0, 1.0, 0.01);
    levelSlider.setValue(1.0);
    levelSlider.addListener(this);

    addAndMakeVisible(levelTextLabel);
    levelTextLabel.setText("Level", juce::dontSendNotification);
    levelTextLabel.setJustificationType(juce::Justification::centred);
    levelTextLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    levelTextLabel.setColour(juce::Label::backgroundColourId, juce::Colours::white);
    levelTextLabel.setFont(juce::Font(20.0f, juce::Font::bold));
    levelTextLabel.setOpaque(true);

    addAndMakeVisible(mixSlider);
    mixSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    mixSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    mixSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::black);
    mixSlider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::white);
    mixSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::white);
    mixSlider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::black);
    mixSlider.setRange(0.0, 1.0, 0.01);
    mixSlider.setValue(0.02);
    mixSlider.addListener(this);

    addAndMakeVisible(mixTextLabel);
    mixTextLabel.setText("TRgate", juce::dontSendNotification);
    mixTextLabel.setJustificationType(juce::Justification::centred);
    mixTextLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    mixTextLabel.setColour(juce::Label::backgroundColourId, juce::Colours::white);
    mixTextLabel.setFont(juce::Font(20.0f, juce::Font::bold));
    mixTextLabel.setOpaque(true);

    addAndMakeVisible(progressSlider);
    progressSlider.setSliderStyle(juce::Slider::LinearBar);
    progressSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    progressSlider.setRange(0.0, 1.0, 0.0);
    progressSlider.setInterceptsMouseClicks(false, false);

    addAndMakeVisible(currentTimeLabel);
    currentTimeLabel.setJustificationType(juce::Justification::centredLeft);
    currentTimeLabel.setFont({ 32.0f, juce::Font::bold });
    currentTimeLabel.setColour(juce::Label::textColourId, juce::Colours::black);
   
    addAndMakeVisible(totalTimeLabel);
    totalTimeLabel.setJustificationType(juce::Justification::centredRight);
    totalTimeLabel.setFont({ 32.0f, juce::Font::bold });
    totalTimeLabel.setColour(juce::Label::textColourId, juce::Colours::black);

   addAndMakeVisible(resetBtn);
    resetBtn.setButtonText("CLEAN");
    resetBtn.addListener(this);

  addAndMakeVisible(controlBtn);
    controlBtn.addListener(this);

    addAndMakeVisible(triggerBtn);
    triggerBtn.setButtonText("TRG");
    triggerBtn.setClickingTogglesState(true);
    triggerBtn.addListener(this);

    addAndMakeVisible(fileSelectBtn);
    fileSelectBtn.setButtonText("FILE");
    fileSelectBtn.addListener(this);

    addAndMakeVisible(modeSwitchBtn);
    modeSwitchBtn.setButtonText("MODE");
    modeSwitchBtn.addListener(this);
    
    startTimerHz(30);
    engine.addListener(this);
    updateFromEngine();
    updateStateLabel();
}

LooperComponent::~LooperComponent()
{
    for (auto* btn : { &resetBtn, &controlBtn, &triggerBtn, &modeSwitchBtn, &fileSelectBtn })
        btn->setLookAndFeel(nullptr);
    for (auto* sl : { &levelSlider, &mixSlider, &progressSlider })
        sl->setLookAndFeel(nullptr);
    engine.removeListener(this);
}

void LooperComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey);
}
void LooperComponent::resized()
{
    Grid grid(getLocalBounds(), 5, 3);
    auto cell0 = grid.getSector(0);
    int mx = cell0.getWidth() / 10;
    int my = cell0.getHeight() / 10;

    auto sector = [&](int r, int c) { return grid.getSector(r * 5 + c).reduced(mx, my); };
    auto span = [&](int r, int c0, int c1) { return grid.getUnion(r * 5 + c0, r * 5 + c1).reduced(mx, my); };

    int offsetRow0 = sector(0, 0).getHeight() / 2; // теперь половина сектора
    int offsetRow1 = sector(1, 0).getHeight() / 2;

    auto shiftDown = [](juce::Component& comp, int dy)
        {
            auto b = comp.getBounds();
            comp.setBounds(b.withY(b.getY() + dy));
        };

    // === ROW 0 === (исходная логика)
    mixSlider.setBounds(sector(0, 0));
    levelSlider.setBounds(sector(0, 4));
    stateLabel.setBounds(span(0, 1, 3));

    {
        auto sCurrent = sector(0, 1);
        int hCur = sCurrent.getHeight() / 4;
        int yCur = sCurrent.getBottom() - hCur;
        currentTimeLabel.setBounds(sCurrent.getX(), yCur, sCurrent.getWidth(), hCur);

        auto sTotal = sector(0, 3);
        int hTot = sTotal.getHeight() / 4;
        int yTot = sTotal.getBottom() - hTot;
        totalTimeLabel.setBounds(sTotal.getX(), yTot, sTotal.getWidth(), hTot);
    }

    {
        auto sFile = sector(0, 2);
        int hFile = sFile.getHeight() / 4;
        int yFile = sFile.getBottom() - hFile;
        fileNameLabel.setBounds(sFile.getX(), yFile, sFile.getWidth(), hFile);
        fileNameLabel.toFront(false);
    }

    // modeLabel — ВЕРХ объединённых секторов row=0, cols=1..3, высота = 1/4 сектора
    {
        auto sMode = span(0, 1, 3);
        int hMode = sector(0, 2).getHeight() / 2;
        modeLabel.setBounds(sMode.getX(), sMode.getY(), sMode.getWidth(), hMode);
    }

    // Сдвигаем вниз всё в Row 0, кроме modeLabel
    shiftDown(mixSlider, offsetRow0);
    shiftDown(levelSlider, offsetRow0);
    shiftDown(stateLabel, offsetRow0);
    shiftDown(currentTimeLabel, offsetRow0);
    shiftDown(totalTimeLabel, offsetRow0);
    shiftDown(fileNameLabel, offsetRow0);

    // === ROW 1 === (исходная логика)
    mixTextLabel.setBounds(sector(1, 0).withHeight(sector(1, 0).getHeight() / 4));
    levelTextLabel.setBounds(sector(1, 4).withHeight(sector(1, 4).getHeight() / 4));

    {
        auto sProgress = span(1, 1, 3);
        int hProg = sProgress.getHeight() / 4;
        int yProg = sProgress.getY();
        progressSlider.setBounds(sProgress.getX(), yProg, sProgress.getWidth(), hProg);
    }

    // Сдвигаем вниз всё в Row 1
    shiftDown(mixTextLabel, offsetRow1);
    shiftDown(levelTextLabel, offsetRow1);
    shiftDown(progressSlider, offsetRow1);

    // === ROW 2 === (кнопки без изменений)
    fileSelectBtn.setBounds(sector(2, 2));
    resetBtn.setBounds(sector(2, 0));
    controlBtn.setBounds(sector(2, 4));
    triggerBtn.setBounds(sector(2, 1));
    modeSwitchBtn.setBounds(sector(2, 3));
}
void LooperComponent::buttonClicked(juce::Button* b)
{
    if (b == &modeSwitchBtn)
    {
        auto m = engine.getMode();
        engine.setMode(m == LooperEngine::Mode::Looper ? LooperEngine::Mode::Player : LooperEngine::Mode::Looper);
        updateStateLabel();
        return;
    }
    if (b == &fileSelectBtn)
    {
        juce::File mediaDir;

        if (GetDriveTypeW(L"D:\\") == DRIVE_FIXED)
            mediaDir = juce::File("D:\\NEXUS\\MEDIA");
        else
            mediaDir = juce::File("C:\\NEXUS\\MEDIA");

        mediaDir.createDirectory();

        auto* fm = new FileManager(mediaDir, FileManager::Mode::Load);
        fm->setMinimalUI(false);
        fm->setShowRunButton(false);
        fm->setWildcardFilter("*.wav;*.mp3;*.flac");

        // Когда менеджер используется в лупере, хотим, чтобы Home вёл в NEXUS/BANK
        fm->setHomeSubfolder("MEDIA");

        // Не давать уходить выше разрешённого корня (NEXUS или подключённый USB)
        fm->setRootLocked(true);

        fm->setConfirmCallback([this](const juce::File& file)
            {
                DBG("File selected: " + file.getFullPathName());

                if (!file.existsAsFile())
                {
                    DBG("Selected file does not exist");
                    return;
                }

                bool success = engine.loadFile(file);
                if (success)
                    updateStateLabel();
            });

        juce::DialogWindow::LaunchOptions opts;
        opts.content.setOwned(fm);
        opts.dialogTitle = "Open Audio File";
        opts.dialogBackgroundColour = juce::Colours::darkgrey;
        opts.escapeKeyTriggersCloseButton = true;
        opts.useNativeTitleBar = false;
        opts.resizable = false;

        auto* dialog = opts.launchAsync();
        if (dialog != nullptr)
        {
            fm->setDialogWindow(dialog);
            auto screenBounds = juce::Desktop::getInstance().getDisplays().getMainDisplay().userArea;
            int w = 800;
            int h = 400; // высота панели сверху
            int x = screenBounds.getCentreX() - w / 2;
            int y = screenBounds.getY() + 150; // верх экрана
            dialog->setBounds(x, y, w, h);
        }

        return;
    }

    // === Остальные кнопки по режиму ===
    if (engine.getMode() == LooperEngine::Mode::Looper)
    {
        if (!engine.isPreparedSuccessfully()) return;

        if (b == &resetBtn)
        {
            engine.reset();
            progressSlider.setValue(0.0, juce::dontSendNotification);
            currentTimeLabel.setText("00:00", juce::dontSendNotification);
            totalTimeLabel.setText("00:00", juce::dontSendNotification);
        }
        else if (b == &controlBtn)
        {
            engine.controlButtonPressed();
        }
        else if (b == &triggerBtn)
        {
            engine.setTriggerEnabled(triggerBtn.getToggleState());
        }
    }
    else
    {
        if (!engine.isReady()) return;

        if (b == &resetBtn)
        {
            engine.reset();
            progressSlider.setValue(0.0, juce::dontSendNotification);
            currentTimeLabel.setText("00:00", juce::dontSendNotification);
            totalTimeLabel.setText("00:00", juce::dontSendNotification);
        }
        else if (b == &controlBtn)
        {
            if (engine.isPlaying() || engine.isPlayerWaitingForTrigger())
                engine.stop();
            else if (engine.isPlayerTriggerArmed())
                engine.armTriggerAndWait();
            else
                engine.startFromTop();
        }
        else if (b == &triggerBtn)
        {
            engine.setTriggerArmed(triggerBtn.getToggleState());
        }
    }

    updateStateLabel();
}

void LooperComponent::sliderValueChanged(juce::Slider* s)
{
    if (!engine.isPreparedSuccessfully()) return;

    if (s == &levelSlider)
        engine.setLevel((float)levelSlider.getValue());
    else if (s == &mixSlider)
        engine.setTriggerThreshold((float)mixSlider.getValue());
}

void LooperComponent::timerCallback()
{
    if (engine.getMode() == LooperEngine::Mode::Looper)
    {
        using S = LooperEngine::State;
        auto st = engine.getState();

        switch (st)
        {
        case S::Recording: progressSlider.setColour(juce::Slider::trackColourId, juce::Colours::red); break;
        case S::Playing:   progressSlider.setColour(juce::Slider::trackColourId, juce::Colours::green); break;
        default:           progressSlider.setColour(juce::Slider::trackColourId, juce::Colours::black); break;
        }

        double curSec = 0.0, totSec = 0.0;
        switch (st)
        {
        case S::Clean:     curSec = 0.0; totSec = LooperEngine::getMaxRecordSeconds(); break;
        case S::Recording: curSec = engine.getRecordedLengthSeconds(); totSec = LooperEngine::getMaxRecordSeconds(); break;
        case S::Stopped:   curSec = totSec = engine.getLoopLengthSeconds(); break;
        case S::Playing:   curSec = engine.getPlayPositionSeconds(); totSec = engine.getLoopLengthSeconds(); break;
        }

        if (st != lastState || !juce::approximatelyEqual(totSec, lastTotal))
        {
            progressSlider.setRange(0.0, totSec, 0.0);
            totalTimeLabel.setText(formatTime(totSec), juce::dontSendNotification);
            lastTotal = totSec;
        }

        progressSlider.setValue(curSec, juce::dontSendNotification);
        currentTimeLabel.setText(formatTime(curSec), juce::dontSendNotification);

        ++blinkCounter;

        if (engine.isTriggerArmed())
        {
            int half = blinkPeriodTicks / 2;
            blinkOn = ((blinkCounter / half) & 1) != 0;

            stateLabel.setText("RECORD", juce::dontSendNotification);
            stateLabel.setColour(juce::Label::backgroundColourId,
                blinkOn ? juce::Colours::red : juce::Colours::transparentBlack);
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
    else
    {
        if (!engine.isReady())
        {
            progressSlider.setColour(juce::Slider::trackColourId, juce::Colours::black);
            progressSlider.setValue(0.0, juce::dontSendNotification);
            currentTimeLabel.setText("00:00", juce::dontSendNotification);
            totalTimeLabel.setText("00:00", juce::dontSendNotification);

            if (triggerBtn.getToggleState() != false)
                triggerBtn.setToggleState(false, juce::dontSendNotification);
            triggerBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colours::darkgrey);

            updateStateLabel();
            return;
        }

        const double curSec = engine.getCurrentTime();
        const double totSec = engine.getTotalTime();

        progressSlider.setColour(juce::Slider::trackColourId,
            engine.isPlaying() ? juce::Colours::green : juce::Colours::black);

        if (!juce::approximatelyEqual(totSec, lastTotal))
        {
            progressSlider.setRange(0.0, totSec, 0.0);
            totalTimeLabel.setText(formatTime(totSec), juce::dontSendNotification);
            lastTotal = totSec;
        }

        progressSlider.setValue(curSec, juce::dontSendNotification);
        currentTimeLabel.setText(formatTime(curSec), juce::dontSendNotification);

        const float lev = engine.getLevel();
        if (!juce::approximatelyEqual((float)levelSlider.getValue(), lev))
            levelSlider.setValue(lev, juce::dontSendNotification);

        const float thr = engine.getTriggerThreshold();
        if (!juce::approximatelyEqual((float)mixSlider.getValue(), thr))
            mixSlider.setValue(thr, juce::dontSendNotification);

        const bool trig = engine.isPlayerTriggerArmed();
        if (triggerBtn.getToggleState() != trig)
            triggerBtn.setToggleState(trig, juce::dontSendNotification);
        triggerBtn.setColour(juce::TextButton::buttonOnColourId,
            trig ? juce::Colours::yellow : juce::Colours::darkgrey);

        ++blinkCounter;

        if (engine.isPlayerWaitingForTrigger())
        {
            int half = juce::jmax(1, blinkPeriodTicks / 2);
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
}
void LooperComponent::updateStateLabel()
{
    if (engine.getMode() == LooperEngine::Mode::Looper)
    {
        using S = LooperEngine::State;
        static constexpr const char* stateNames[] = { "CLEAN", "RECORD", "STOP",  "PLAY" };
        static constexpr const char* controlNames[] = { "RECORD","STOP",   "PLAY",  "STOP" };

        auto st = engine.getState();
        S nextMode = S::Stopped;

        switch (st)
        {
        case S::Clean:     nextMode = S::Recording; break;
        case S::Recording: nextMode = S::Stopped;   break;
        case S::Stopped:   nextMode = S::Playing;   break;
        case S::Playing:   nextMode = S::Stopped;   break;
        }

        stateLabel.setText(stateNames[int(st)], juce::dontSendNotification);
        controlBtn.setButtonText(controlNames[int(st)]);
        controlBtn.setToggleState(st == S::Recording || st == S::Playing, juce::dontSendNotification);

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
        stateLabel.setColour(juce::Label::textColourId, juce::Colours::black);
        controlBtn.setColour(juce::TextButton::buttonColourId, nextColour);
        controlBtn.setColour(juce::TextButton::buttonOnColourId, nextColour);

        auto textCol = nextColour.contrasting(0.7f);
        controlBtn.setColour(juce::TextButton::textColourOffId, textCol);
        controlBtn.setColour(juce::TextButton::textColourOnId, textCol);

        modeLabel.setText("LOOPER MODE", juce::dontSendNotification);
        if (rigControl)
            rigControl->updateAllSButtons();

    }
    else
    {
        const bool ready = engine.isReady();
        const bool playing = engine.isPlaying();

        const char* stateText = !ready ? "CLEAN" : (playing ? "PLAY" : "STOP");
        stateLabel.setText(stateText, juce::dontSendNotification);

        juce::Colour currentColour = !ready ? juce::Colours::lightblue
            : (playing ? juce::Colours::green : juce::Colours::yellow);
        stateLabel.setColour(juce::Label::backgroundColourId, currentColour);
        stateLabel.setColour(juce::Label::textColourId, juce::Colours::black);

        const bool willPlay = ready && !playing;
        juce::Colour nextColour = willPlay ? juce::Colours::green : juce::Colours::yellow;

        controlBtn.setButtonText(willPlay ? "PLAY" : "STOP");
        controlBtn.setToggleState(!willPlay, juce::dontSendNotification);
        controlBtn.setColour(juce::TextButton::buttonColourId, nextColour);
        controlBtn.setColour(juce::TextButton::buttonOnColourId, nextColour);

        auto textCol = nextColour.contrasting(0.7f);
        controlBtn.setColour(juce::TextButton::textColourOffId, textCol);
        controlBtn.setColour(juce::TextButton::textColourOnId, textCol);

        modeLabel.setText("PLAYER MODE", juce::dontSendNotification);
        if (rigControl)
            rigControl->updateAllSButtons();

    }
}
void LooperComponent::updateFromEngine() noexcept
{
    const float lev = engine.getLevel();
    if (!juce::approximatelyEqual((float)levelSlider.getValue(), lev))
        levelSlider.setValue(lev, juce::dontSendNotification);

    const float thr = engine.getTriggerThreshold();
    if (!juce::approximatelyEqual((float)mixSlider.getValue(), thr))
        mixSlider.setValue(thr, juce::dontSendNotification);

    mixValueLabel.setText(juce::String(thr, 3), juce::dontSendNotification);

    const bool trig = (engine.getMode() == LooperEngine::Mode::Looper)
        ? engine.getTriggerEnabled()
        : engine.isPlayerTriggerArmed();

    // Цвет кнопки триггера всегда обновляем
    triggerBtn.setColour(juce::TextButton::buttonOnColourId,
                         trig ? juce::Colours::yellow : juce::Colours::darkgrey);

    // В Looper синхронизируем toggleState с движком
    if (engine.getMode() == LooperEngine::Mode::Looper)
    {
        if (triggerBtn.getToggleState() != trig)
            triggerBtn.setToggleState(trig, juce::dontSendNotification);
    }
    // В Player toggleState не трогаем — даём пользователю кликнуть

    bool looperMode = (engine.getMode() == LooperEngine::Mode::Looper);

    // FILE — всегда видна, но в Looper блокируется и затеняется
    fileSelectBtn.setVisible(true);
    fileSelectBtn.setEnabled(!looperMode);
    fileSelectBtn.setAlpha(looperMode ? 0.5f : 1.0f);

    // MIX — в обоих режимах блокируется, если триггер выключен
    mixSlider.setEnabled(trig);
    mixSlider.setAlpha(trig ? 1.0f : 0.5f);

    updateStateLabel();

    if (engine.getMode() == LooperEngine::Mode::Player && engine.isReady())
        fileNameLabel.setText(engine.getLoadedFileName(), juce::dontSendNotification);
    else
        fileNameLabel.setText({}, juce::dontSendNotification);
}


void LooperComponent::engineChanged()
{
    updateFromEngine();
}
juce::String LooperComponent::formatTime(double seconds)
{
    int totalS = int(std::floor(seconds + 0.5));
    return juce::String::formatted("%02d:%02d", totalS / 60, totalS % 60);
}
void LooperComponent::setScale(float newScale) noexcept
{
    scale = newScale;
    resized(); // пересчитывает layout
}
juce::String LooperComponent::getControlButtonText() const
{
    return controlBtn.getButtonText();
}

juce::Colour LooperComponent::getControlButtonColor() const
{
    return controlBtn.findColour(juce::TextButton::buttonColourId);
}
void LooperComponent::setRigControl(Rig_control* rig)
{
    rigControl = rig;
}

