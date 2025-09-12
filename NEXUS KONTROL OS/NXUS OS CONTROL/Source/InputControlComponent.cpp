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

    // Заголовок
    titleLabel.setText("INPUT CONTROL", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    titleLabel.setColour(juce::Label::backgroundColourId, juce::Colours::white);
    titleLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(titleLabel);
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

    // Метки INPUT / GATE IN / THRESHOLD / ATTACK / RELEASE
    input1Label.setText("INPUT 1", juce::dontSendNotification);
    input2Label.setText("INPUT 2", juce::dontSendNotification);
    gateIn1Label.setText("GATE IN1", juce::dontSendNotification);
    gateIn2Label.setText("GATE IN2", juce::dontSendNotification);
    threshold1Label.setText("THRESHOLD 1", juce::dontSendNotification);
    threshold2Label.setText("THRESHOLD 2", juce::dontSendNotification);
    attack1Label.setText("ATTACK 1", juce::dontSendNotification);
    attack2Label.setText("ATTACK 2", juce::dontSendNotification);
    release1Label.setText("RELEASE 1", juce::dontSendNotification);
    release2Label.setText("RELEASE 2", juce::dontSendNotification);
    for (auto* l : { &input1Label, &input2Label,
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
    impedanceLabel.setText("Impedance (Channel 2 only)", juce::dontSendNotification);
    impedanceLabel.setJustificationType(juce::Justification::centred);
    impedanceLabel.setColour(juce::Label::backgroundColourId, juce::Colours::white);
    impedanceLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    addAndMakeVisible(impedanceLabel);
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

    loadSettings();
}
//==============================================================================
//  Деструктор — снимаем LookAndFeel
//==============================================================================
InputControlComponent::~InputControlComponent()
{
    saveSettings();
    setLookAndFeel(nullptr);
}

//==============================================================================
//  Размещение компонентов по 6×4 сетке
//==============================================================================
void InputControlComponent::resized()
{
    auto area = getLocalBounds();
    const int w = area.getWidth(), h = area.getHeight(), m = 4;

    auto cell = [&](int idx, int sx = 1, int sy = 1)
        {
            return getCellBounds(idx, w, h, sx, sy).reduced(m);
        };

    // центрирование по умолчанию
    auto ctrBox = [&](juce::Rectangle<int> r, int bw, int bh)
        {
            return juce::Rectangle<int>(bw, bh).withCentre(r.getCentre());
        };

    // новые лямбды для симметрии
    const int pad = 6;

    auto leftBox = [&](juce::Rectangle<int> r, int bw, int bh)
        {
            return juce::Rectangle<int>(bw, bh)
                .withCentre({ r.getCentreX(), r.getCentreY() }) // центр по Y
                .withX(r.getX() + pad);                         // прижать влево
        };

    auto rightBox = [&](juce::Rectangle<int> r, int bw, int bh)
        {
            return juce::Rectangle<int>(bw, bh)
                .withCentre({ r.getCentreX(), r.getCentreY() }) // центр по Y
                .withX(r.getRight() - bw - pad);                // X = правый край - ширина - отступ
        };



    // ==== Настройки позиционирования ====
    const float knobScaleAll = 1.1f;
    const int   knobYOffset = -50;
    const int   knobSpacing = 30;
    const int   labelYOffset = -30;
    const int   labelSpacing = 30;

    auto placeKnob = [&](juce::Slider& knob, juce::Rectangle<int> c)
        {
            int lh = c.getHeight() / 4;
            auto u = c.withTrimmedBottom(lh);
            int d = juce::roundToInt(knobScaleAll * float(juce::jmin(u.getWidth(), u.getHeight())));
            knob.setBounds(juce::Rectangle<int>(d, d).withCentre(u.getCentre()));
        };

    auto placeTriple = [&](juce::Slider& s, juce::Label& lbl,
        juce::Rectangle<int> c,
        bool isThreshold,
        int offsetIndex)
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

    // ==== Размещение ====
    titleLabel.setBounds(cell(0, 6, 1).removeFromTop(cell(0, 6, 1).getHeight() / 4));

    // VU
    {
        auto top = cell(6), bot = cell(18);
        juce::Rectangle<int> col(top.getX(), top.getCentreY(),
            top.getWidth(), juce::jmax(1, bot.getCentreY() - top.getCentreY()));

        auto meterWidth = col.getWidth() / 4;

        juce::Rectangle<int> meterArea = juce::Rectangle<int>(meterWidth, col.getHeight())
            .withCentre(col.getCentre());

        vuLeft.setBounds(meterArea);
        vuScaleL.setBounds(meterArea);
    }
    {
        auto top = cell(11), bot = cell(23);
        juce::Rectangle<int> col(top.getX(), top.getCentreY(),
            top.getWidth(), juce::jmax(1, bot.getCentreY() - top.getCentreY()));

        auto meterWidth = col.getWidth() / 4;

        juce::Rectangle<int> meterArea = juce::Rectangle<int>(meterWidth, col.getHeight())
            .withCentre(col.getCentre());

        vuRight.setBounds(meterArea);
        vuScaleR.setBounds(meterArea);
    }

    // ==== Левая колонка ====
    placeTriple(attackSliderL, attack1Label, cell(7), false, -1);
    placeTriple(releaseSliderL, release1Label, cell(13), false, 0);
    placeTriple(gateKnob1, threshold1Label, cell(19), true, 1);

    // ==== Правая колонка ====
    placeTriple(attackSliderR, attack2Label, cell(10), false, -1);
    placeTriple(releaseSliderR, release2Label, cell(16), false, 0);
    placeTriple(gateKnob2, threshold2Label, cell(22), true, 1);

    // Gate bypass — симметрия
    gateBypass1.setBounds(leftBox(cell(1), cell(1).getWidth() / 4, cell(1).getHeight() / 4));
    inputBypass1.setBounds(leftBox(cell(0), cell(0).getWidth() / 4, cell(0).getHeight() / 4));

    gateBypass2.setBounds(rightBox(cell(4), cell(4).getWidth() / 4, cell(4).getHeight() / 4));
    inputBypass2.setBounds(rightBox(cell(5), cell(5).getWidth() / 4, cell(5).getHeight() / 4));

    // Верхние метки входа
    gateIn1Label.setBounds(cell(1).removeFromTop(cell(1).getHeight() / 4));
    gateIn2Label.setBounds(cell(4).removeFromTop(cell(4).getHeight() / 4));

    // Нижние метки входа
    input1Label.setBounds(cell(18).removeFromBottom(cell(18).getHeight() / 4));
    input2Label.setBounds(cell(23).removeFromBottom(cell(23).getHeight() / 4));

    // ==== PEAK метки над метрами ====
    {
        const int clipHeight = 20;
        const int gap = 4;

        auto vuLBounds = vuLeft.getBounds();
        clipLabelL.setBounds(
            vuLBounds.getX(),
            vuLBounds.getY() - clipHeight - gap,
            vuLBounds.getWidth(),
            clipHeight
        );

        auto vuRBounds = vuRight.getBounds();
        clipLabelR.setBounds(
            vuRBounds.getX(),
            vuRBounds.getY() - clipHeight - gap,
            vuRBounds.getWidth(),
            clipHeight
        );
    }
    {
        // Размеры панели и колонок
        int totalCols = 6;
        int colWidth = getWidth() / totalCols;
        int rowHeight = getHeight() / 4; // если 4 ряда

        // Ширина блока = 2 колонки
        int blockWidth = colWidth * 2;
        int blockHeight = rowHeight;

        // Центрируем блок по всей панели
        int blockX = (getWidth() - blockWidth) / 2;
        int blockY = rowHeight * 1; // второй ряд (row = 1, если считать от 0)

        // ===== РЕГУЛИРОВКА ПО ВЕРТИКАЛИ =====
        // Меняй это значение, чтобы поднимать/опускать блок
        int verticalOffset = blockHeight /1; // половина высоты блока вниз
        blockY += verticalOffset;
        // ====================================

        juce::Rectangle<int> combined(blockX, blockY, blockWidth, blockHeight);

        // --- Метка ---
        auto labelArea = combined.removeFromTop(combined.getHeight() / 4);
        impedanceLabel.setBounds(labelArea);

        // --- Кнопки ---
        auto btnArea = combined;
        int bw = btnArea.getWidth() / 8;
        int bh = btnArea.getHeight() / 2;
        int spacing = (btnArea.getWidth() - (bw * 4)) / 5;

        int x = btnArea.getX() + spacing;
        int y = btnArea.getCentreY() - (bh / 2);

        impBtn33k.setBounds(x, y, bw, bh);
        x += bw + spacing;
        impBtn1M.setBounds(x, y, bw, bh);
        x += bw + spacing;
        impBtn3M.setBounds(x, y, bw, bh);
        x += bw + spacing;
        impBtn10M.setBounds(x, y, bw, bh);
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
    float preEnvL = 0.0f, preEnvR = 0.0f; // уровень ДО гейта
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

            // CLIP логика
            if (peak > 1.0f) // > 0 dB
            {
                clipL.store(true, std::memory_order_relaxed);
                clipTsL = nowMs;
            }
            else if (clipL.load() && (nowMs - clipTsL) > clipHoldMs)
            {
                clipL.store(false, std::memory_order_relaxed);
            }

            gateL.setThresholdDb(thresholdL.load());
            gateL.setTimesMs(attackL.load(), releaseL.load());
            gateL.process(leftCh, numSamples, bypassL.load());
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

            // CLIP логика
            if (peak > 1.0f)
            {
                clipR.store(true, std::memory_order_relaxed);
                clipTsR = nowMs;
            }
            else if (clipR.load() && (nowMs - clipTsR) > clipHoldMs)
            {
                clipR.store(false, std::memory_order_relaxed);
            }

            gateR.setThresholdDb(thresholdR.load());
            gateR.setTimesMs(attackR.load(), releaseR.load());
            gateR.process(rightCh, numSamples, bypassR.load());
        }
    }
    else
    {
        gateR.reset();
    }

    // Обновляем VU и CLIP-метки
    juce::MessageManager::callAsync([this, preEnvL, preEnvR]()
        {
            vuLeft.setLevel(preEnvL);
            vuRight.setLevel(preEnvR);

            clipLabelL.setVisible(clipL.load());
            clipLabelR.setVisible(clipR.load());
        });
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
    auto restoreButton = [&](juce::TextButton& btn, int ccNumber)
        {
            bool isActive = (ccNumber == activeImpedanceCC);
            btn.setToggleState(isActive, juce::dontSendNotification);

            if (isActive && rigControl) // rigControl должен быть установлен через setRigControl()
            {
                rigControl->sendSettingsMenuState(true); // CC55=127
                rigControl->sendImpedanceCC(ccNumber, true); // отправляем ON
            }
        };

    restoreButton(impBtn33k, 52);
    restoreButton(impBtn1M, 51);
    restoreButton(impBtn3M, 53);
    restoreButton(impBtn10M, 50);

}








