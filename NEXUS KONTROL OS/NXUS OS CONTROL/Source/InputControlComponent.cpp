#include "InputControlComponent.h"
//==============================================================================
//  Вспомогательный расчёт сетки ячеек
//==============================================================================
juce::Rectangle<int> getCellBounds(int cellIndex,
    int totalWidth,
    int totalHeight,
    int spanX,
    int spanY)
{
    constexpr int cols = 6, rows = 4;
    const int cellW = totalWidth / cols;
    const int cellH = totalHeight / rows;
    const int col = cellIndex % cols;
    const int row = cellIndex / cols;
    return { col * cellW,
             row * cellH,
             cellW * spanX,
             cellH * spanY };
}
//==============================================================================
//  Обновление цвета BYPASS-кнопок
//==============================================================================
void InputControlComponent::updateBypassButtonColour(juce::TextButton& b)
{
    const bool on = b.getToggleState();
    b.setColour(juce::TextButton::buttonColourId,
        on ? juce::Colours::red
        : juce::Colours::darkred);
    b.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    b.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
}
void InputControlComponent::updateChannelBypassColour(juce::TextButton& b)
{
    const bool on = b.getToggleState();
    b.setColour(juce::TextButton::buttonColourId,
        on ? juce::Colours::red
        : juce::Colours::darkred);
    b.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    b.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
}

//==============================================================================
//  Конструктор — инициализация UI и атомарных колбэков
//==============================================================================
InputControlComponent::InputControlComponent()
{
    setLookAndFeel(&inLnF);
    // VU-метры
    const float minDb = -60.0f, maxDb = 6.0f;
    const std::vector<float> marks{ -60.f, -30.f, -20.f, -12.f, -6.f, -3.f, 0.f };

    vuLeft.setDbRange(minDb, maxDb);
    vuRight.setDbRange(minDb, maxDb);
    vuLeft.setScaleMarks(marks);
    vuRight.setScaleMarks(marks);

    vuScaleL.setDbRange(minDb, maxDb);
    vuScaleR.setDbRange(minDb, maxDb);
    vuScaleL.setScaleMarks(marks);
    vuScaleR.setScaleMarks(marks);

    addAndMakeVisible(vuLeft);
    addAndMakeVisible(vuRight);
    addAndMakeVisible(vuScaleL);
    addAndMakeVisible(vuScaleR);
    vuScale.setShowDbSuffix(false);
    vuScaleL.setShowDbSuffix(false);
    vuScaleR.setShowDbSuffix(false);
    //PEAK
    for (auto* l : { &clipLabelL, &clipLabelR })
    {
        l->setText("CLIP", juce::dontSendNotification);
        l->setJustificationType(juce::Justification::centred);
        l->setColour(juce::Label::textColourId, juce::Colours::white);
        l->setColour(juce::Label::backgroundColourId, juce::Colours::red);
        l->setInterceptsMouseClicks(false, false);
        l->setVisible(false);
        addAndMakeVisible(*l);
    }

    // -------- Threshold (GateKnob) --------
    for (auto* s : { &gateKnob1, &gateKnob2 })
    {
        s->setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
        s->setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::red);
        s->setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::black);
        s->setColour(juce::Slider::textBoxTextColourId, juce::Colours::black);
        s->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::white);
        s->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::grey);
        s->setTextValueSuffix(" dB");
        s->setRange(-80.0, 0.0, 0.1);
        s->setSkewFactorFromMidPoint(-24.0);
        s->setDoubleClickReturnValue(true, -24.0); // ← сброс к -24 dB
        addAndMakeVisible(*s);
    }

    gateKnob1.onValueChange = [this]()
        {
            auto v = (float)gateKnob1.getValue();
            thresholdL.store(v, std::memory_order_relaxed);
        };
    gateKnob2.onValueChange = [this]()
        {
            auto v = (float)gateKnob2.getValue();
            thresholdR.store(v, std::memory_order_relaxed);
        };

    // -------- Attack --------
    for (auto* s : { &attackSliderL, &attackSliderR })
    {
        s->setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
        s->setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::gold);
        s->setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::black);
        s->setColour(juce::Slider::textBoxTextColourId, juce::Colours::black);
        s->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::white);
        s->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::grey);
        s->setTextValueSuffix(" ms");
        s->setRange(0.1, 200.0, 0.1);
        s->setDoubleClickReturnValue(true, 20.0); // ← сброс к 20 ms
        addAndMakeVisible(*s);
    }

    attackSliderL.onValueChange = [this]()
        {
            auto v = (float)attackSliderL.getValue();
            attackL.store(v, std::memory_order_relaxed);
        };
    attackSliderR.onValueChange = [this]()
        {
            auto v = (float)attackSliderR.getValue();
            attackR.store(v, std::memory_order_relaxed);
        };
    // -------- Release --------
    for (auto* s : { &releaseSliderL, &releaseSliderR })
    {
        s->setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
        s->setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::white);
        s->setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::black);
        s->setColour(juce::Slider::textBoxTextColourId, juce::Colours::black);
        s->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::white);
        s->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::grey);
        s->setColour(juce::Slider::textBoxTextColourId, juce::Colours::black);
        s->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::white);
        s->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::grey);
        s->setTextValueSuffix(" ms");
        s->setRange(1.0, 1000.0, 1.0);
        s->setDoubleClickReturnValue(true, 200.0); // ← сброс к 200 ms
        addAndMakeVisible(*s);
    }

    releaseSliderL.onValueChange = [this]()
        {
            auto v = (float)releaseSliderL.getValue();
            releaseL.store(v, std::memory_order_relaxed);
        };
    releaseSliderR.onValueChange = [this]()
        {
            auto v = (float)releaseSliderR.getValue();
            releaseR.store(v, std::memory_order_relaxed);

        };
    //==============================================================================
    //  BYPASS-кнопки: Gate и Input
    //==============================================================================

    // // Gate BYPASS
    auto updateGateUI = [this]()
        {
            auto setDim = [](juce::Slider& s, bool dim)
                {
                    s.setAlpha(dim ? 0.4f : 1.0f);
                    s.setInterceptsMouseClicks(!dim, !dim);
                };

            bool leftBypass = gateBypass1.getToggleState();
            bool rightBypass = gateBypass2.getToggleState();

            setDim(gateKnob1, leftBypass);
            setDim(attackSliderL, leftBypass);
            setDim(releaseSliderL, leftBypass);

            setDim(gateKnob2, rightBypass);
            setDim(attackSliderR, rightBypass);
            setDim(releaseSliderR, rightBypass);
        };

    for (auto* b : { &gateBypass1, &gateBypass2 })
    {
        addAndMakeVisible(*b);
        b->setClickingTogglesState(true);

        b->onStateChange = [this, b, updateGateUI]()
            {
                if (b == &gateBypass1)
                    bypassL.store(b->getToggleState(), std::memory_order_relaxed);
                else
                    bypassR.store(b->getToggleState(), std::memory_order_relaxed);

                updateGateUI();
                updateBypassButtonColour(*b);
            };

        updateBypassButtonColour(*b);
    }

    // Input BYPASS
    for (auto* b : { &inputBypass1, &inputBypass2 })
    {
        addAndMakeVisible(*b);
        b->setClickingTogglesState(true);

        b->onStateChange = [this, b]
            {
                if (b == &inputBypass1)
                    inputBypass1State.store(b->getToggleState(), std::memory_order_relaxed);
                else
                    inputBypass2State.store(b->getToggleState(), std::memory_order_relaxed);

                updateChannelBypassColour(*b);
            };

        updateChannelBypassColour(*b); // начальная синхронизация
    }

    // Метки INPUT / GATE IN / THRESHOLD / ATTACK / VU
    VU1Label.setText("VU1", juce::dontSendNotification);
    VU2Label.setText("VU2", juce::dontSendNotification);
    input1Label.setText("INPUT / NOISE GATE 1", juce::dontSendNotification);
    input2Label.setText("INPUT / NOISE GATE 2", juce::dontSendNotification);
    gateIn1Label.setText("GATE IN1", juce::dontSendNotification);
    gateIn2Label.setText("GATE IN2", juce::dontSendNotification);
    threshold1Label.setText("THRESHOLD 1", juce::dontSendNotification);
    threshold2Label.setText("THRESHOLD 2", juce::dontSendNotification);
    attack1Label.setText("ATTACK 1", juce::dontSendNotification);
    attack2Label.setText("ATTACK 2", juce::dontSendNotification);
    release1Label.setText("RELEASE 1", juce::dontSendNotification);
    release2Label.setText("RELEASE 2", juce::dontSendNotification);
    for (auto* l : { &VU1Label, &VU2Label, &input1Label, &input2Label,
                     &gateIn1Label, &gateIn2Label,
                     &threshold1Label, &threshold2Label,
                     &attack1Label, &attack2Label,
                     &release1Label, &release2Label })
    {
        l->setJustificationType(juce::Justification::centred);
        l->setColour(juce::Label::textColourId, juce::Colours::black);
        l->setColour(juce::Label::backgroundColourId, juce::Colours::white);
        l->setInterceptsMouseClicks(false, false);
        addAndMakeVisible(*l);
    }
    //импеданс
    auto setupImpButton = [this](juce::TextButton& b,
        const juce::String& text,
        int groupId,
        int ccNumber)
        {
            b.setButtonText(text);
            b.setClickingTogglesState(true);
            b.setRadioGroupId(groupId);
            b.setColour(juce::TextButton::buttonOnColourId, juce::Colours::yellow);
            b.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgoldenrod);
            b.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
            b.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
            addAndMakeVisible(b);

            // Запоминаем начальное состояние
            bool lastState = b.getToggleState();

            b.onStateChange = [this, &b, ccNumber, lastState]() mutable
                {
                    bool newState = b.getToggleState();

                    // Если состояние не изменилось — выходим
                    if (newState == lastState)
                        return;

                    lastState = newState;

                    // OFF не отправляем — только ON
                    if (!newState)
                        return;

                    if (rigControl)
                    {
                        // Отправляем только CC импеданса
                        rigControl->sendImpedanceCC(ccNumber, true);
                    }
                    // Сохраняем активную кнопку
                    saveSettings();
                };
        };
    setupImpButton(impBtn33k, "33k", 1001, 52); // CC 52
    setupImpButton(impBtn1M, "1M", 1001, 51); // CC 51
    setupImpButton(impBtn3M, "3M", 1001, 53); // CC 53
    setupImpButton(impBtn10M, "10M", 1001, 50); // CC 50
    //
    // ==== PEDAL CONTROL ====
    pedalLabel.setText("PEDAL SETTINGS", juce::dontSendNotification);
    pedalLabel.setJustificationType(juce::Justification::centred);
    pedalLabel.setColour(juce::Label::backgroundColourId, juce::Colours::white);
    pedalLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    addAndMakeVisible(pedalLabel);

    // STATE Label
    stateLabel.setText("_", juce::dontSendNotification);
    stateLabel.setJustificationType(juce::Justification::centred);
    stateLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    stateLabel.setColour(juce::Label::backgroundColourId, juce::Colours::white);
    addAndMakeVisible(stateLabel);

    //  sliders
    stateLook = std::make_unique<InLookSliderBar>();
    stateLook->trackThickness = 50; // если хочешь шире
    pedalStateSlider.setLookAndFeel(stateLook.get());

    minLook = std::make_unique<InLookSliderArrow>(InLookSliderArrow::Direction::ToStateFromLeft);
    maxLook = std::make_unique<InLookSliderArrow>(InLookSliderArrow::Direction::ToStateFromRight);
    minLook->arrowColour = juce::Colours::blue;   // стрелка MIN синяя
    maxLook->arrowColour = juce::Colours::orange; // стрелка MAX оранжевая
    // Настройка толщины и стрелки (можешь подогнать под вкус)
    minLook->trackThickness = 24;
    minLook->arrowLength = 44.0f;
    minLook->arrowBaseWidth = 22.0f;


    // Лямбда для инициализации слайдера
    auto initPedalSlider = [&](juce::Slider& s, double initValue,
        juce::Colour fill, juce::Colour outline)
        {
            s.setRange(0, 127, 1);
            s.setValue(initValue, juce::dontSendNotification);
            s.setSliderStyle(juce::Slider::LinearVertical);
            s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 40, 18);
            s.setColour(juce::Slider::trackColourId, fill);
            s.setColour(juce::Slider::thumbColourId, outline);
            s.setColour(juce::Slider::textBoxTextColourId, juce::Colours::black);
            s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::white);
            s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::grey);
            addAndMakeVisible(s);
        };

    initPedalSlider(pedalStateSlider, 0, juce::Colours::green, juce::Colours::green);

    // --- новый Threshold Slider ---
    thresholdLook = std::make_unique<InLookSliderBar>();
    thresholdLook->trackThickness = 50;
    pedalThresholdSlider.setLookAndFeel(thresholdLook.get());
    initPedalSlider(pedalThresholdSlider, 64, juce::Colours::blue, juce::Colours::blue);
    pedalThresholdSlider.setRange(0, 30, 1);

    initPedalSlider(pedalMinSlider, 0, juce::Colours::red, juce::Colours::black);
    pedalMinSlider.setLookAndFeel(minLook.get());

    initPedalSlider(pedalMaxSlider, 127, juce::Colours::black, juce::Colours::red);
    pedalMaxSlider.setLookAndFeel(maxLook.get());


    auto initPedalLabel = [&](juce::Label& l, const juce::String& text) {
        l.setText(text, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        l.setColour(juce::Label::textColourId, juce::Colours::black);
        l.setColour(juce::Label::backgroundColourId, juce::Colours::white);
        l.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(l);
        };
    initPedalLabel(pedalStateTextLabel, "STATE");
    initPedalLabel(pedalMinTextLabel, "MIN");
    initPedalLabel(pedalMaxTextLabel, "MAX");

    // Кнопки
    autoConfigButton.setButtonText("AUTO CALIB");
    autoConfigButton.setClickingTogglesState(true);
    addAndMakeVisible(autoConfigButton);

    autoConfigButton.onStateChange = [this]() {
        updateAutoConfigButtonColour(autoConfigButton);
        // Здесь можно добавить вызов логики автокалибровки
        };

    updateAutoConfigButtonColour(autoConfigButton); // начальная синхронизация
    // SW1
    sw1Button.setButtonText("SW1");
    sw1Button.setClickingTogglesState(true); // кнопка-тумблер
    addAndMakeVisible(sw1Button);
    updateSwitchButtonColour(sw1Button, false); // начальное состояние: выключена → зелёная

    // SW2
    sw2Button.setButtonText("SW2");
    sw2Button.setClickingTogglesState(true);
    addAndMakeVisible(sw2Button);
    updateSwitchButtonColour(sw2Button, false);
    setPedalConnected(false);

    pedalStateSlider.onValueChange = [this]() {
        if (isPedalConnected && rigControl) {
            int val = (int)pedalStateSlider.getValue();
            rigControl->sendMidiCC(6, 4, val); // CC4 = State
        }
        };

    pedalMinSlider.onValueChange = [this]() {
        if (isPedalConnected && rigControl) {
            int val = (int)pedalMinSlider.getValue();
            rigControl->sendMidiCC(6, 5, val); // CC5 = Min
        }
        };

    pedalMaxSlider.onValueChange = [this]() {
        if (isPedalConnected && rigControl) {
            int val = (int)pedalMaxSlider.getValue();
            rigControl->sendMidiCC(6, 6, val); // CC6 = Max
        }
        };

    pedalThresholdSlider.onValueChange = [this]() {
        if (isPedalConnected && rigControl) {
            int val = (int)pedalThresholdSlider.getValue();
            rigControl->sendMidiCC(6, 9, val); // CC9 = Threshold
        }
        };

    autoConfigButton.onClick = [this]() {
        if (isPedalConnected && rigControl) {
            bool isOn = autoConfigButton.getToggleState();
            rigControl->sendMidiCC(6, 7, isOn ? 127 : 0); // CC7 = autoConfig
        }
        };

    // INVERT button
    addAndMakeVisible(invertButton);
    invertButton.setClickingTogglesState(true);

    // Задаём базовые цвета сразу
    invertButton.setColour(juce::TextButton::buttonColourId, juce::Colours::black);   // фон OFF
    invertButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::white); // фон ON
    invertButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);  // текст OFF
    invertButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);   // текст ON

    // Колбэк только на клик
    invertButton.setClickingTogglesState(true);

    invertButton.onClick = [this]() {
        bool on = invertButton.getToggleState();

        // Цвета
        invertButton.setColour(juce::TextButton::buttonColourId,
            on ? juce::Colours::white : juce::Colours::black);

        if (rigControl && isPedalConnected) {
            // Отправляем только новое состояние
            rigControl->sendMidiCC(6, 15, on ? 127 : 0);
        }
        };

    lookButton.setButtonText("LOOK");
    lookButton.setClickingTogglesState(true);
    lookButton.setToggleState(true, juce::dontSendNotification); // по умолчанию включена
    lookButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::yellow);
    lookButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgoldenrod);
    lookButton.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    lookButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    addAndMakeVisible(lookButton);
    lookButton.onStateChange = [this]()
        {
            if (isPedalConnected)   // реагируем только если педаль подключена
            {
                bool lookOn = lookButton.getToggleState();

                auto setSliderDimmed = [](juce::Slider& s, bool dim) {
                    s.setAlpha(dim ? 0.4f : 1.0f);
                    s.setInterceptsMouseClicks(!dim, !dim);
                    };
                auto setButtonDimmed = [](juce::Button& b, bool dim) {
                    b.setAlpha(dim ? 0.4f : 1.0f);
                    b.setInterceptsMouseClicks(!dim, !dim);
                    };

                // если Look включён → блокируем педальные органы
                setSliderDimmed(pedalStateSlider, lookOn);
                setSliderDimmed(pedalMinSlider, lookOn);
                setSliderDimmed(pedalMaxSlider, lookOn);
                setSliderDimmed(pedalThresholdSlider, lookOn);

                setButtonDimmed(sw1Button, lookOn);
                setButtonDimmed(sw2Button, lookOn);
                setButtonDimmed(invertButton, lookOn);

                // AutoConfig и Look всегда активны
                setButtonDimmed(autoConfigButton, false);
                setButtonDimmed(lookButton, false);
            }
        };

    loadSettings();
}
//==============================================================================
//  Деструктор — снимаем LookAndFeel
//==============================================================================
InputControlComponent::~InputControlComponent()
{
    shuttingDown.store(true, std::memory_order_release);
    saveSettings();
    pedalMinSlider.setLookAndFeel(nullptr);
    pedalMaxSlider.setLookAndFeel(nullptr);
    pedalStateSlider.setLookAndFeel(nullptr);
    pedalThresholdSlider.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

//==============================================================================
//  Размещение компонентов по 6×4 сетке
//==============================================================================
void InputControlComponent::resized()
{
    // ==== БАЗОВЫЕ РАЗМЕРЫ МАКЕТА ====
    const float baseW = 1280.0f; // ← ширина, под которую верстался UI
    const float baseH = 720.0f;  // ← высота, под которую верстался UI

    // ==== ТЕКУЩИЕ РАЗМЕРЫ ====
    const float w = (float)getWidth();
    const float h = (float)getHeight();

    // ==== КОЭФФИЦИЕНТЫ МАСШТАБА ====
    const float scaleX = w / baseW;
    const float scaleY = h / baseH;

    // ==== ВСПОМОГАТЕЛЬНАЯ ЛЯМБДА ДЛЯ МАСШТАБИРОВАНИЯ ====
    auto S = [&](int v, bool vertical = false)
        {
            return juce::roundToInt((vertical ? scaleY : scaleX) * v);
        };

    auto area = getLocalBounds();
    const int m = S(4); // отступы тоже масштабируем

    // ==== ЛЯМБДЫ ДЛЯ ЯЧЕЕК ====
    auto cell = [&](int idx, int sx = 1, int sy = 1)
        {
            return getCellBounds(idx, (int)w, (int)h, sx, sy).reduced(m);
        };

    auto ctrBox = [&](juce::Rectangle<int> r, int bw, int bh)
        {
            return juce::Rectangle<int>(S(bw), S(bh, true)).withCentre(r.getCentre());
        };

    auto leftBox = [&](juce::Rectangle<int> r, int bw, int bh)
        {
            return juce::Rectangle<int>(S(bw), S(bh, true))
                .withCentre({ r.getCentreX(), r.getCentreY() })
                .withX(r.getX() + S(6));
        };

    auto rightBox = [&](juce::Rectangle<int> r, int bw, int bh)
        {
            return juce::Rectangle<int>(S(bw), S(bh, true))
                .withCentre({ r.getCentreX(), r.getCentreY() })
                .withX(r.getRight() - S(bw) - S(6));
        };

    // ==== МАСШТАБИРУЕМЫЕ КОНСТАНТЫ ====
    const float knobScaleAll = 1.1f;
    const int knobYOffset = S(-50, true);
    const int knobSpacing = S(30, true);
    const int labelYOffset = S(-30, true);
    const int labelSpacing = S(30, true);

    auto placeKnob = [&](juce::Slider& knob, juce::Rectangle<int> c)
        {
            int lh = c.getHeight() / 4;
            auto u = c.withTrimmedBottom(lh);
            int d = juce::roundToInt(knobScaleAll * float(juce::jmin(u.getWidth(), u.getHeight())));
            knob.setBounds(juce::Rectangle<int>(d, d).withCentre(u.getCentre()));
        };

    auto placeTriple = [&](juce::Slider& s, juce::Label& lbl, juce::Rectangle<int> c, bool isThreshold, int offsetIndex)
        {
            auto knobRect = c.translated(0, knobYOffset + offsetIndex * knobSpacing);
            if (isThreshold)
            {
                placeKnob(s, knobRect);
                lbl.setBounds(c.removeFromBottom(c.getHeight() / 4));
            }
            else
            {
                auto labelRect = c.translated(0, labelYOffset + offsetIndex * labelSpacing)
                    .removeFromBottom(c.getHeight() / 4);
                placeKnob(s, knobRect);
                lbl.setBounds(labelRect);
            }
        };

    // ==== ДАЛЬШЕ — ТВОЙ ИСХОДНЫЙ КОД, НО С МАСШТАБИРОВАННЫМИ ЗНАЧЕНИЯМИ ====

    // VU левый
    {
        auto top = cell(8), bot = cell(20);
        juce::Rectangle<int> col(top.getX(), top.getCentreY(), top.getWidth(),
            juce::jmax(1, bot.getCentreY() - top.getCentreY()));
        auto meterWidth = col.getWidth() / 4;
        juce::Rectangle<int> meterArea = juce::Rectangle<int>(meterWidth, col.getHeight())
            .withCentre(col.getCentre());
        vuLeft.setBounds(meterArea);
        vuScaleL.setBounds(meterArea);
    }

    // VU правый
    {
        auto top = cell(10), bot = cell(22);
        juce::Rectangle<int> col(top.getX(), top.getCentreY(), top.getWidth(),
            juce::jmax(1, bot.getCentreY() - top.getCentreY()));
        auto meterWidth = col.getWidth() / 4;
        juce::Rectangle<int> meterArea = juce::Rectangle<int>(meterWidth, col.getHeight())
            .withCentre(col.getCentre());
        vuRight.setBounds(meterArea);
        vuScaleR.setBounds(meterArea);
    }

    // Левая колонка
    placeTriple(attackSliderL, attack1Label, cell(9), false, -1);
    placeTriple(releaseSliderL, release1Label, cell(15), false, 0);
    placeTriple(gateKnob1, threshold1Label, cell(21), true, 1);

    // Правая колонка
    placeTriple(attackSliderR, attack2Label, cell(11), false, -1);
    placeTriple(releaseSliderR, release2Label, cell(17), false, 0);
    placeTriple(gateKnob2, threshold2Label, cell(23), true, 1);

    // Gate bypass
    gateBypass1.setBounds(leftBox(cell(3), cell(3).getWidth() / 4, cell(3).getHeight() / 4));
    inputBypass1.setBounds(leftBox(cell(2), cell(2).getWidth() / 4, cell(2).getHeight() / 4));
    gateBypass2.setBounds(leftBox(cell(5), cell(5).getWidth() / 4, cell(5).getHeight() / 4));
    inputBypass2.setBounds(leftBox(cell(4), cell(4).getWidth() / 4, cell(4).getHeight() / 4));

    // Верхние метки входа
    auto cellRange = [&](int start, int end)
        {
            auto r1 = cell(start);
            auto r2 = cell(end);
            return r1.withRight(r2.getRight());
        };
    input1Label.setBounds(cellRange(2, 3).removeFromTop(cellRange(2, 3).getHeight() / 4));
    input2Label.setBounds(cellRange(4, 5).removeFromTop(cellRange(4, 5).getHeight() / 4));

    // VU метки
    VU1Label.setBounds(cell(20).removeFromBottom(cell(20).getHeight() / 4));
    VU2Label.setBounds(cell(22).removeFromBottom(cell(22).getHeight() / 4));

    // PEAK метки
    {
        const int clipHeight = S(20, true);
        const int gap = S(4, true);
        auto vuLBounds = vuLeft.getBounds();
        clipLabelL.setBounds(vuLBounds.getX(), vuLBounds.getY() - clipHeight - gap,
            vuLBounds.getWidth(), clipHeight);
        auto vuRBounds = vuRight.getBounds();
        clipLabelR.setBounds(vuRBounds.getX(), vuRBounds.getY() - clipHeight - gap,
            vuRBounds.getWidth(), clipHeight);
    }
    // Блок импеданса — между VU2 и Gate2, но чуть левее центра
    {
        auto vuRBounds = vuRight.getBounds();
        auto gate2Bounds = gateKnob2.getBounds()
            .getUnion(attackSliderR.getBounds())
            .getUnion(releaseSliderR.getBounds());

        const int totalHeight = juce::jmax(vuRBounds.getHeight(), gate2Bounds.getHeight());
        const int buttonSize = totalHeight / 10;
        const int spacing = buttonSize / 2;
        const int blockHeight = (buttonSize * 4) + (spacing * 3);

        int midY = (vuRBounds.getY() + gate2Bounds.getBottom()) / 2;
        int y = midY - blockHeight / 2;

        int xLeft = vuRBounds.getRight() + S(20);
        int xRight = gate2Bounds.getX() - S(20);

        int shiftLeft = S(20); // ← величина смещения влево
        int x = xLeft + (xRight - xLeft - buttonSize) / 2 - shiftLeft;

        impBtn33k.setBounds(x, y, buttonSize, buttonSize);
        y += buttonSize + spacing;
        impBtn1M.setBounds(x, y, buttonSize, buttonSize);
        y += buttonSize + spacing;
        impBtn3M.setBounds(x, y, buttonSize, buttonSize);
        y += buttonSize + spacing;
        impBtn10M.setBounds(x, y, buttonSize, buttonSize);
    }

    // ==== PEDAL BLOCK ====

// 0-я строка: только заголовок
    {
        auto r = cell(0, 2, 1); // col=0..1
        auto topH = r.getHeight() / 4;
        pedalLabel.setBounds(r.removeFromTop(topH));
    }
    // 1-я строка: метка состояния
    {
        auto r = cell(0, 2, 1); // row=1, col=0..1
        int stateOffsetY = S(50, true); // ← регулируй вертикальное положение
        auto stateH = r.getHeight() / 2;
        stateLabel.setBounds(r.getX(),
            r.getY() + stateOffsetY,
            r.getWidth(),
            stateH);
    }
    // 2-3 строки: PEDAL VALUE + MIN/MAX + STATE
    int pedalBottomOffset = S(30, true); // ← регулируй глубину посадки снизу
    int pedalTopOffset = S(80, true);  // ← регулируй, насколько короче сверху (увеличь — короче, уменьши — выше)

    {
        // --- MIN и MAX ---
        auto cMin = cell(18).translated(0, pedalBottomOffset);
        auto cMax = cell(19).translated(0, pedalBottomOffset);

        placeTriple(pedalMinSlider, pedalMinTextLabel, cMin, false, 0);
        placeTriple(pedalMaxSlider, pedalMaxTextLabel, cMax, false, 0);

        // Симметрия меток
        auto minBounds = pedalMinTextLabel.getBounds();
        auto maxBounds = pedalMaxTextLabel.getBounds();
        int totalWidth = maxBounds.getRight() - minBounds.getX();
        int newLabelWidth = totalWidth / 3;

        pedalMinTextLabel.setBounds(minBounds.getX(), minBounds.getY(), newLabelWidth, minBounds.getHeight());
        pedalMaxTextLabel.setBounds(maxBounds.getRight() - newLabelWidth, maxBounds.getY(), newLabelWidth, maxBounds.getHeight());

        // Центрируем и удлиняем вверх (с учётом pedalTopOffset)
        {
            auto s = pedalMinSlider.getBounds();
            int centerX = pedalMinTextLabel.getBounds().getCentreX();
            int newY = cell(6, 0, 1).getY() + pedalTopOffset; // верх из 1-й строки + отступ
            int newH = s.getBottom() - newY;
            pedalMinSlider.setBounds(centerX - s.getWidth() / 2, newY, s.getWidth(), newH);
        }
        {
            auto s = pedalMaxSlider.getBounds();
            int centerX = pedalMaxTextLabel.getBounds().getCentreX();
            int newY = cell(6, 1, 1).getY() + pedalTopOffset;
            int newH = s.getBottom() - newY;
            pedalMaxSlider.setBounds(centerX - s.getWidth() / 2, newY, s.getWidth(), newH);
        }

        // --- STATE ---
        const int gap = S(6, true);
        auto minLbl = pedalMinTextLabel.getBounds();
        auto maxLbl = pedalMaxTextLabel.getBounds();
        int available = maxLbl.getX() - minLbl.getRight();
        int stateLabelW = juce::jmax(0, available - 2 * gap);
        int stateLabelX = minLbl.getRight() + gap;

        pedalStateTextLabel.setBounds(stateLabelX, minLbl.getY(), stateLabelW, minLbl.getHeight());

        // --- STATE + THRESHOLD в одной колонке ---
        {
            auto s = pedalMinSlider.getBounds();
            int centerX = pedalStateTextLabel.getBounds().getCentreX();
            int newY = cell(6).getY() + pedalTopOffset;
            int totalH = s.getBottom() - newY;

            // регулируемый параметр для State
            const int stateHeight = S(260, true); // ← здесь задаёшь высоту State вручную

            // Threshold занимает всё остальное
            int thrHeight = juce::jmax(0, totalH - stateHeight);

            // Threshold сверху
            pedalThresholdSlider.setBounds(centerX - s.getWidth() / 2,
                newY,
                s.getWidth(),
                thrHeight);

            // State снизу
            pedalStateSlider.setBounds(centerX - s.getWidth() / 2,
                newY + thrHeight,
                s.getWidth(),
                stateHeight);
        }


    }
    // === Параметры регулировки кнопок ===
    int btnVerticalLift = S(60, true); // ← подними/опусти кнопки (увеличь — выше, уменьши — ниже)
    float btnWidthFactor = 0.25f;      // ← доля ширины ячейки для кнопок SW1/SW2
    float btnHeightFactor = 0.5f;      // ← доля высоты ячейки для кнопок SW1/SW2

    // Кнопки AutoConfig, Look и Invert
    {
        auto cSW1 = cell(6); // row=1, col=0
        auto cSW2 = cell(7); // row=1, col=1

        int btnW = static_cast<int>(cSW1.getWidth() * btnWidthFactor * 2.0f); // шире в 2 раза
        int btnH = static_cast<int>(cSW1.getHeight() * btnHeightFactor);

        int y = cSW1.getCentreY() - (btnH / 2) - btnVerticalLift;

        // AutoConfig слева
        autoConfigButton.setBounds(cSW1.getCentreX() - btnW / 2, y, btnW, btnH);

        // Invert справа
        invertButton.setBounds(cSW2.getCentreX() - btnW / 2, y, btnW, btnH);

        // Look — ровно посередине между AutoConfig и Invert
        int midX = (cSW1.getCentreX() + cSW2.getCentreX()) / 2;
        lookButton.setBounds(midX - btnW / 2, y, btnW, btnH);
    }
}


//==============================================================================
// prepare — готовим SimpleGate (здесь только sampleRate, blockSize не нужен)
//==============================================================================
void InputControlComponent::prepare(double sampleRate, int /*blockSize*/) noexcept
{
    gateL.prepare(sampleRate);
    gateR.prepare(sampleRate);

}

//==============================================================================
//  processBlock — вызывается из PluginProcessCallback первым звеном
//==============================================================================
void InputControlComponent::processBlock(float* const* channels, int numSamples) noexcept
{
    if (shuttingDown.load(std::memory_order_acquire))
        return;

    float preEnvL = 0.0f, preEnvR = 0.0f;
    const double nowMs = juce::Time::getMillisecondCounterHiRes();

    // === Левый канал ===
    if (auto* leftCh = channels[0])
    {
        if (inputBypass1State.load(std::memory_order_relaxed))
        {
            juce::FloatVectorOperations::clear(leftCh, numSamples);
            gateL.reset();
        }
        else
        {
            float peak = 0.0f;
            for (int n = 0; n < numSamples; ++n)
                peak = std::max(peak, std::abs(leftCh[n]));
            preEnvL = peak;

            if (peak > 1.0f)
            {
                clipL.store(true, std::memory_order_relaxed);
                clipTsL = nowMs;
            }
            else if (clipL.load(std::memory_order_relaxed) && (nowMs - clipTsL) > clipHoldMs)
            {
                clipL.store(false, std::memory_order_relaxed);
            }

            gateL.setThresholdDb(thresholdL.load(std::memory_order_relaxed));
            gateL.setTimesMs(attackL.load(std::memory_order_relaxed), releaseL.load(std::memory_order_relaxed));
            gateL.process(leftCh, numSamples, bypassL.load(std::memory_order_relaxed));
        }
    }
    else
    {
        gateL.reset();
    }

    // === Правый канал ===
    if (auto* rightCh = channels[1])
    {
        if (inputBypass2State.load(std::memory_order_relaxed))
        {
            juce::FloatVectorOperations::clear(rightCh, numSamples);
            gateR.reset();
        }
        else
        {
            float peak = 0.0f;
            for (int n = 0; n < numSamples; ++n)
                peak = std::max(peak, std::abs(rightCh[n]));
            preEnvR = peak;

            if (peak > 1.0f)
            {
                clipR.store(true, std::memory_order_relaxed);
                clipTsR = nowMs;
            }
            else if (clipR.load(std::memory_order_relaxed) && (nowMs - clipTsR) > clipHoldMs)
            {
                clipR.store(false, std::memory_order_relaxed);
            }

            gateR.setThresholdDb(thresholdR.load(std::memory_order_relaxed));
            gateR.setTimesMs(attackR.load(std::memory_order_relaxed), releaseR.load(std::memory_order_relaxed));
            gateR.process(rightCh, numSamples, bypassR.load(std::memory_order_relaxed));
        }
    }
    else
    {
        gateR.reset();
    }

    // Захватываем всё нужное по значению ДО постинга, и не держим сырой this
    const bool clipLVal = clipL.load(std::memory_order_relaxed);
    const bool clipRVal = clipR.load(std::memory_order_relaxed);
    auto self = juce::Component::SafePointer<InputControlComponent>(this);

    juce::MessageManager::callAsync([self, preEnvL, preEnvR, clipLVal, clipRVal]()
        {
            if (self == nullptr || self->isShuttingDown())
                return;

            self->vuLeft.setLevel(preEnvL);
            self->vuRight.setLevel(preEnvR);
            self->clipLabelL.setVisible(clipLVal);
            self->clipLabelR.setVisible(clipRVal);
        });
    if (onInputClipChanged)
        onInputClipChanged(clipL.load(std::memory_order_relaxed),
            clipR.load(std::memory_order_relaxed));

}

void InputControlComponent::updateAutoConfigButtonColour(juce::TextButton& b)
{
    const bool on = b.getToggleState();

    // Цвет для выключенного состояния
    b.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);

    // Цвет для включённого состояния (ярко‑красный)
    b.setColour(juce::TextButton::buttonOnColourId, juce::Colours::red);

    // Цвет текста
    b.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    b.setColour(juce::TextButton::textColourOnId, juce::Colours::white);

    b.repaint();
}

void InputControlComponent::updateSwitchButtonColour(juce::TextButton& b, bool isOn)
{
    // Цвет для выключенного состояния
    b.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgreen);

    // Цвет для включённого состояния (ярко‑красный)
    b.setColour(juce::TextButton::buttonOnColourId, juce::Colours::green);

    // Цвет текста
    b.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    b.setColour(juce::TextButton::textColourOnId, juce::Colours::white);

    // Принудительная перерисовка
    b.repaint();
}

void InputControlComponent::updateStatusLabel(const juce::String& text)
{
    stateLabel.setText(text, juce::dontSendNotification);
    stateLabel.setColour(juce::Label::backgroundColourId, juce::Colours::green);
    stateLabel.setColour(juce::Label::textColourId, juce::Colours::white);
}
void InputControlComponent::syncSwitchState(int cc, bool isOn)
{
    if (cc == 2 && isOn) {
        sw1Button.setToggleState(true, juce::dontSendNotification);
        updateSwitchButtonColour(sw1Button, true);

        sw2Button.setToggleState(false, juce::dontSendNotification);
        updateSwitchButtonColour(sw2Button, false);

        updateStatusLabel("SWITCH 1");

        // Отправляем обратно в контроллер
        if (rigControl)
        {
            rigControl->sendMidiCC(6, 2, 127); // SW1 ON
            rigControl->sendMidiCC(6, 3, 0);   // SW2 OFF
        }
    }
    else if (cc == 3 && isOn) {
        sw2Button.setToggleState(true, juce::dontSendNotification);
        updateSwitchButtonColour(sw2Button, true);

        sw1Button.setToggleState(false, juce::dontSendNotification);
        updateSwitchButtonColour(sw1Button, false);

        updateStatusLabel("SWITCH 2");

        // Отправляем обратно в контроллер
        if (rigControl)
        {
            rigControl->sendMidiCC(6, 3, 127); // SW2 ON
            rigControl->sendMidiCC(6, 2, 0);   // SW1 OFF
        }
    }
}
void InputControlComponent::syncPedalSliderByCC(int cc, int value)
{
    value = juce::jlimit(0, 127, value);

    switch (cc)
    {
    case 4: // Pedal State
        pedalStateSlider.setValue(value, juce::dontSendNotification);
        break;

    case 5: // Pedal Min
        pedalMinSlider.setValue(value, juce::dontSendNotification);
        if (rigControl)
            rigControl->sendMidiCC(6, 5, value); // отправляем наружу
        break;

    case 6: // Pedal Max
        pedalMaxSlider.setValue(value, juce::dontSendNotification);
        if (rigControl)
            rigControl->sendMidiCC(6, 6, value); // отправляем наружу
        break;

    case 8: // Pedal State (альтернативный поток)
        pedalStateSlider.setValue(value, juce::dontSendNotification);
        break;

    case 9: // Pedal Threshold
        pedalThresholdSlider.setValue(value, juce::dontSendNotification);
        if (rigControl)
            rigControl->sendMidiCC(6, 9, value); // отправляем наружу

        break;

    default:
        break;
    }
}

void InputControlComponent::syncAutoConfigButton(bool isOn)
{
    autoConfigButton.setToggleState(isOn, juce::dontSendNotification);
    updateAutoConfigButtonColour(autoConfigButton);

    if (rigControl)
        rigControl->sendMidiCC(6, 7, isOn ? 127 : 0);
}
void InputControlComponent::showPressUp()
{
    stateLabel.setText("PRESS UP", juce::dontSendNotification);
    stateLabel.setColour(juce::Label::backgroundColourId, juce::Colours::red);
    stateLabel.setColour(juce::Label::textColourId, juce::Colours::black);
}

void InputControlComponent::showPressDown()
{
    stateLabel.setText("PRESS DOWN", juce::dontSendNotification);
    stateLabel.setColour(juce::Label::backgroundColourId, juce::Colours::red);
    stateLabel.setColour(juce::Label::textColourId, juce::Colours::black);
}
void InputControlComponent::showThresholdSetting()
{
    stateLabel.setText("THRESHOLD SET", juce::dontSendNotification);
    stateLabel.setColour(juce::Label::backgroundColourId, juce::Colours::red);
    stateLabel.setColour(juce::Label::textColourId, juce::Colours::black);
}
void InputControlComponent::resetStateLabel()
{
    stateLabel.setText("Pedal not connected", juce::dontSendNotification);
    stateLabel.setColour(juce::Label::backgroundColourId, juce::Colours::white);
    stateLabel.setColour(juce::Label::textColourId, juce::Colours::black);
}
void InputControlComponent::setPedalConnected(bool connected)
{
    isPedalConnected = connected;

    // Лямбда для "затенения" без изменения цветов
    auto setSliderDimmed = [](juce::Slider& s, bool dim)
        {
            s.setAlpha(dim ? 0.4f : 1.0f);           // прозрачность
            s.setInterceptsMouseClicks(!dim, !dim);  // блокировка мыши
        };

    // Лямбда для кнопок
    auto setButtonDimmed = [](juce::Button& b, bool dim)
        {
            b.setAlpha(dim ? 0.4f : 1.0f);
            b.setInterceptsMouseClicks(!dim, !dim);
        };

    // Применяем к педальным элементам
    setSliderDimmed(pedalStateSlider, !connected);
    setSliderDimmed(pedalMinSlider, !connected);
    setSliderDimmed(pedalMaxSlider, !connected);
    setSliderDimmed(pedalThresholdSlider, !connected); // <--- добавили Threshold

    setButtonDimmed(sw1Button, !connected);
    setButtonDimmed(sw2Button, !connected);
    setButtonDimmed(autoConfigButton, !connected);
    setButtonDimmed(invertButton, !connected);
    setButtonDimmed(lookButton, !connected);

    if (!connected)
    {
        stateLabel.setText("Pedal not connected", juce::dontSendNotification);
        stateLabel.setColour(juce::Label::backgroundColourId, juce::Colours::white);
        stateLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    }

    if (connected)
    {
        bool lookOn = lookButton.getToggleState();

        setSliderDimmed(pedalStateSlider, lookOn);
        setSliderDimmed(pedalMinSlider, lookOn);
        setSliderDimmed(pedalMaxSlider, lookOn);
        setSliderDimmed(pedalThresholdSlider, lookOn); // <--- и здесь тоже

        setButtonDimmed(sw1Button, lookOn);
        setButtonDimmed(sw2Button, lookOn);
        setButtonDimmed(invertButton, lookOn);

        // AutoConfig и Look всегда активны
        setButtonDimmed(autoConfigButton, false);
        setButtonDimmed(lookButton, false);
    }
}

void InputControlComponent::saveSettings() const
{
    auto xml = std::make_unique<juce::XmlElement>("InputControl");

    // 1) Сохраняем атомарные значения
    xml->setAttribute("thresholdL", thresholdL.load());
    xml->setAttribute("thresholdR", thresholdR.load());
    xml->setAttribute("attackL", attackL.load());
    xml->setAttribute("attackR", attackR.load());
    xml->setAttribute("releaseL", releaseL.load());
    xml->setAttribute("releaseR", releaseR.load());

    xml->setAttribute("inputBypass1", inputBypass1.getToggleState());
    xml->setAttribute("inputBypass2", inputBypass2.getToggleState());
    xml->setAttribute("gateBypass1", gateBypass1.getToggleState());
    xml->setAttribute("gateBypass2", gateBypass2.getToggleState());
    // Сохраняем активную кнопку импеданса
    int activeImpedanceCC = -1;
    if (impBtn33k.getToggleState())      activeImpedanceCC = 52;
    else if (impBtn1M.getToggleState())  activeImpedanceCC = 51;
    else if (impBtn3M.getToggleState())  activeImpedanceCC = 53;
    else if (impBtn10M.getToggleState()) activeImpedanceCC = 50;

    xml->setAttribute("activeImpedanceCC", activeImpedanceCC);
    xml->setAttribute("pedalMinValue", pedalMinSlider.getValue());
    xml->setAttribute("pedalMaxValue", pedalMaxSlider.getValue());
    xml->setAttribute("pedalThresholdValue", pedalThresholdSlider.getValue()); // NEW


    xml->setAttribute("invertState", invertButton.getToggleState());

    // 2) Пишем в файл
    auto file = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("InputControlSettings.xml");

    file.getParentDirectory().createDirectory();
    xml->writeTo(file, {});
}
void InputControlComponent::loadSettings()
{
    auto file = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("InputControlSettings.xml");

    if (!file.existsAsFile())
        return;

    std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(file));
    if (!xml || !xml->hasTagName("InputControl"))
        return;

    // 1) Загружаем атомарные значения
    thresholdL.store((float)xml->getDoubleAttribute("thresholdL", thresholdL.load()), std::memory_order_relaxed);
    thresholdR.store((float)xml->getDoubleAttribute("thresholdR", thresholdR.load()), std::memory_order_relaxed);
    attackL.store((float)xml->getDoubleAttribute("attackL", attackL.load()), std::memory_order_relaxed);
    attackR.store((float)xml->getDoubleAttribute("attackR", attackR.load()), std::memory_order_relaxed);
    releaseL.store((float)xml->getDoubleAttribute("releaseL", releaseL.load()), std::memory_order_relaxed);
    releaseR.store((float)xml->getDoubleAttribute("releaseR", releaseR.load()), std::memory_order_relaxed);

    // 2) Обновляем UI без триггера onChange
    gateKnob1.setValue(thresholdL.load(), juce::dontSendNotification);
    gateKnob2.setValue(thresholdR.load(), juce::dontSendNotification);
    attackSliderL.setValue(attackL.load(), juce::dontSendNotification);
    attackSliderR.setValue(attackR.load(), juce::dontSendNotification);
    releaseSliderL.setValue(releaseL.load(), juce::dontSendNotification);
    releaseSliderR.setValue(releaseR.load(), juce::dontSendNotification);

    // 3) Загружаем состояния кнопок
    inputBypass1.setToggleState(xml->getBoolAttribute("inputBypass1", inputBypass1.getToggleState()), juce::dontSendNotification);
    inputBypass2.setToggleState(xml->getBoolAttribute("inputBypass2", inputBypass2.getToggleState()), juce::dontSendNotification);
    gateBypass1.setToggleState(xml->getBoolAttribute("gateBypass1", gateBypass1.getToggleState()), juce::dontSendNotification);
    gateBypass2.setToggleState(xml->getBoolAttribute("gateBypass2", gateBypass2.getToggleState()), juce::dontSendNotification);

    // 4) Синхронизируем атомики
    inputBypass1State.store(inputBypass1.getToggleState(), std::memory_order_relaxed);
    inputBypass2State.store(inputBypass2.getToggleState(), std::memory_order_relaxed);
    bypassL.store(gateBypass1.getToggleState(), std::memory_order_relaxed);
    bypassR.store(gateBypass2.getToggleState(), std::memory_order_relaxed);

    // 5) Обновляем цвет кнопок
    updateChannelBypassColour(inputBypass1);
    updateChannelBypassColour(inputBypass2);
    updateBypassButtonColour(gateBypass1);
    updateBypassButtonColour(gateBypass2);

    // 6) Применяем затенение и блокировку слайдеров
    auto setSliderDimmed = [](juce::Slider& s, bool dim)
        {
            s.setAlpha(dim ? 0.4f : 1.0f);           // прозрачность
            s.setInterceptsMouseClicks(!dim, !dim);  // блокировка мыши
        };

    bool leftBypass = gateBypass1.getToggleState();
    bool rightBypass = gateBypass2.getToggleState();

    setSliderDimmed(gateKnob1, leftBypass);
    setSliderDimmed(attackSliderL, leftBypass);
    setSliderDimmed(releaseSliderL, leftBypass);

    setSliderDimmed(gateKnob2, rightBypass);
    setSliderDimmed(attackSliderR, rightBypass);
    setSliderDimmed(releaseSliderR, rightBypass);
    // Восстанавливаем активную кнопку импеданса
    int activeImpedanceCC = xml->getIntAttribute("activeImpedanceCC", -1);
    pendingImpedanceCC = activeImpedanceCC; // запомним для отложенной отправки

    auto restoreButton = [&](juce::TextButton& btn, int ccNumber)
        {
            bool isActive = (ccNumber == activeImpedanceCC);
            btn.setToggleState(isActive, juce::dontSendNotification);

            if (isActive && rigControl) // rigControl уже есть
            {
                rigControl->sendSettingsMenuState(true);
                rigControl->sendImpedanceCC(ccNumber, true);
                pendingImpedanceCC = -1; // уже отправили
            }
        };

    restoreButton(impBtn33k, 52);
    restoreButton(impBtn1M, 51);
    restoreButton(impBtn3M, 53);
    restoreButton(impBtn10M, 50);

    double minVal = xml->getDoubleAttribute("pedalMinValue", pedalMinSlider.getValue());
    double maxVal = xml->getDoubleAttribute("pedalMaxValue", pedalMaxSlider.getValue());
    double thrVal = xml->getDoubleAttribute("pedalThresholdValue", pedalThresholdSlider.getValue()); // NEW

    pedalMinSlider.setValue(minVal, juce::dontSendNotification);
    pedalMaxSlider.setValue(maxVal, juce::dontSendNotification);
    pedalThresholdSlider.setValue(thrVal, juce::dontSendNotification); // NEW


    bool inv = xml->getBoolAttribute("invertState", false);
    invertButton.setToggleState(inv, juce::dontSendNotification);

    // обновляем цвет сразу после загрузки
    invertButton.setColour(juce::TextButton::buttonColourId,
        inv ? juce::Colours::white : juce::Colours::black);
}












