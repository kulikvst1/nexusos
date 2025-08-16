// InputControlComponent.cpp
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
    addAndMakeVisible(vuLeft);
    addAndMakeVisible(vuRight);
    // -------- Threshold (GateKnob) --------
    for (auto* s : { &gateKnob1, &gateKnob2 })
    {
        s->setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
        s->setColour(juce::Slider::textBoxTextColourId, juce::Colours::black);
        s->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::white);
        s->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::grey);
        s->setTextValueSuffix(" dB");
        s->setRange(-80.0, 0.0, 0.1);
        s->setSkewFactorFromMidPoint(-24.0);
        addAndMakeVisible(*s);
    }
    gateKnob1.onValueChange = [this]()
        {
            auto v = (float)gateKnob1.getValue();
            thresholdL.store(v, std::memory_order_relaxed);
            DBG("[UI] thresholdL → " << v);
        };
    gateKnob2.onValueChange = [this]()
        {
            auto v = (float)gateKnob2.getValue();
            thresholdR.store(v, std::memory_order_relaxed);
            DBG("[UI] thresholdR → " << v);
        };

    // -------- Attack --------
    for (auto* s : { &attackSliderL, &attackSliderR })
    {
        s->setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
        s->setColour(juce::Slider::textBoxTextColourId, juce::Colours::black);
        s->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::white);
        s->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::grey);
        s->setTextValueSuffix(" ms");
        s->setRange(0.1, 200.0, 0.1);
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
        s->setColour(juce::Slider::textBoxTextColourId, juce::Colours::black);
        s->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::white);
        s->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::grey);
        s->setTextValueSuffix(" ms");
        s->setRange(1.0, 1000.0, 1.0);
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
     // BYPASS-кнопки
    for (auto* b : { &gateBypass1, &gateBypass2 })
    {
        addAndMakeVisible(*b);
        b->setClickingTogglesState(true);
        b->onClick = [this, b]
            {
                if (b == &gateBypass1) bypassL.store(b->getToggleState(), std::memory_order_relaxed);
                else                   bypassR.store(b->getToggleState(), std::memory_order_relaxed);

                DBG("[UI] bypass" << (b == &gateBypass1 ? "L" : "R") << " → " << (int)b->getToggleState());
                updateBypassButtonColour(*b);
            };
        updateBypassButtonColour(*b);
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
}
//==============================================================================
//  Деструктор — снимаем LookAndFeel
//==============================================================================
InputControlComponent::~InputControlComponent()
{
    setLookAndFeel(nullptr);
}
void InputControlComponent::VUMeter::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour(juce::Colours::black.withAlpha(0.85f));
    g.fillRoundedRectangle(r, 3.0f);
    g.setColour(juce::Colours::white.withAlpha(0.15f));
    constexpr int ticks = 6;
    for (int i = 1; i < ticks; ++i)
    {
        float y = r.getY() + r.getHeight() * (1.0f - float(i) / ticks);
        g.drawLine(r.getX() + 3.0f, y, r.getRight() - 3.0f, y, 1.0f);
    }
    auto fill = r;
    fill.removeFromTop(r.getHeight() * (1.0f - value));
    juce::ColourGradient grad(
        juce::Colours::green, fill.getBottomLeft(),
        juce::Colours::red, fill.getTopLeft(),
        false
    );
    grad.addColour(0.6, juce::Colours::yellow);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(fill, 2.5f);
    g.setColour(juce::Colours::white.withAlpha(0.25f));
    g.drawRoundedRectangle(r, 3.0f, 1.0f);
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

    auto ctrBox = [&](juce::Rectangle<int> r, int bw, int bh)
        {
            return juce::Rectangle<int>(bw, bh).withCentre(r.getCentre());
        };

    // ==== Настройки позиционирования ====
    const float knobScaleAll = 1.1f; // (1) масштаб всех крутилок
    const int   knobYOffset = -50;  // (2) общий сдвиг по Y
    const int   knobSpacing = 30;  // (3) раздвижка крутилок внутри колонки
    const int   labelYOffset = -30;   // (4) общий сдвиг меток (кроме Threshold)
    const int   labelSpacing = 30;   // (5) раздвижка меток внутри колонки

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
        vuLeft.setBounds(col.withWidth(col.getWidth() / 4).withCentre(col.getCentre()));
    }
    {
        auto top = cell(11), bot = cell(23);
        juce::Rectangle<int> col(top.getX(), top.getCentreY(),
            top.getWidth(), juce::jmax(1, bot.getCentreY() - top.getCentreY()));
        vuRight.setBounds(col.withWidth(col.getWidth() / 4).withCentre(col.getCentre()));
    }

    // ==== Левая колонка ====
    placeTriple(attackSliderL, attack1Label, cell(7), false, -1); // верх
    placeTriple(releaseSliderL, release1Label, cell(13), false, 0); // центр
    placeTriple(gateKnob1, threshold1Label, cell(19), true, 1); // низ (Threshold)

    // ==== Правая колонка ====
    placeTriple(attackSliderR, attack2Label, cell(10), false, -1);
    placeTriple(releaseSliderR, release2Label, cell(16), false, 0);
    placeTriple(gateKnob2, threshold2Label, cell(22), true, 1);

    // Gate bypass
    gateBypass1.setBounds(ctrBox(cell(20), cell(20).getWidth() / 2, cell(20).getHeight() / 4));
    gateBypass2.setBounds(ctrBox(cell(21), cell(21).getWidth() / 2, cell(21).getHeight() / 4));

    // Верхние метки входа
    gateIn1Label.setBounds(cell(1).removeFromTop(cell(1).getHeight() / 4));
    gateIn2Label.setBounds(cell(4).removeFromTop(cell(4).getHeight() / 4));

    // Нижние метки входа
    input1Label.setBounds(cell(18).removeFromBottom(cell(18).getHeight() / 4));
    input2Label.setBounds(cell(23).removeFromBottom(cell(23).getHeight() / 4));
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
void InputControlComponent::processBlock(float* const* channels,
    int            numSamples) noexcept
{
    float preEnvL = 0.0f, preEnvR = 0.0f; // уровень ДО гейта

    // — Левый канал —
    if (auto* leftCh = channels[0])
    {
        // считаем огибающую по абсолютному значению
        float peak = 0.0f;
        for (int n = 0; n < numSamples; ++n)
            peak = std::max(peak, std::abs(leftCh[n]));
        preEnvL = peak; // или можно усреднить для RMS

        // выставляем параметры гейта
        gateL.setThresholdDb(thresholdL.load());
        gateL.setTimesMs(attackL.load(), releaseL.load());

        // обрабатываем
        gateL.process(leftCh, numSamples, bypassL.load());
    }
    else
    {
        gateL.reset();
    }

    // — Правый канал —
    if (auto* rightCh = channels[1])
    {
        float peak = 0.0f;
        for (int n = 0; n < numSamples; ++n)
            peak = std::max(peak, std::abs(rightCh[n]));
        preEnvR = peak;

        gateR.setThresholdDb(thresholdR.load());
        gateR.setTimesMs(attackR.load(), releaseR.load());

        gateR.process(rightCh, numSamples, bypassR.load());
    }
    else
    {
        gateR.reset();
    }

    // Обновляем VU уже значениями ДО гейта
    juce::MessageManager::callAsync([this, preEnvL, preEnvR]()
        {
            vuLeft.setValue(preEnvL);
            vuRight.setValue(preEnvR);
        });
}





