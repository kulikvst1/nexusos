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

    // Ротационные слайдеры Gate
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

    // Сохраняем в атомарки при изменении UI
    gateKnob1.onValueChange = [this] { thresholdL.store((float)gateKnob1.getValue()); };
    gateKnob2.onValueChange = [this] { thresholdR.store((float)gateKnob2.getValue()); };

    // BYPASS-кнопки
    for (auto* b : { &gateBypass1, &gateBypass2 })
    {
        addAndMakeVisible(*b);
        b->setClickingTogglesState(true);
        b->onClick = [this, b]
            {
                if (b == &gateBypass1)    bypassL.store(b->getToggleState());
                else                      bypassR.store(b->getToggleState());

                updateBypassButtonColour(*b);
            };
        updateBypassButtonColour(*b);
    }

    // Метки INPUT / GATE IN
    input1Label.setText("INPUT 1", juce::dontSendNotification);
    input2Label.setText("INPUT 2", juce::dontSendNotification);
    gateIn1Label.setText("GATE IN1", juce::dontSendNotification);
    gateIn2Label.setText("GATE IN2", juce::dontSendNotification);

    for (auto* l : { &input1Label, &input2Label, &gateIn1Label, &gateIn2Label })
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

    // Title
    {
        auto band = cell(0, 6, 1);
        int th = band.getHeight() / 4;
        titleLabel.setBounds(band.removeFromTop(th));
    }

    // VU Left
    {
        auto top = cell(6), bot = cell(18);
        int y1 = top.getCentreY(), y2 = bot.getCentreY();
        juce::Rectangle<int> col(top.getX(), y1, top.getWidth(), juce::jmax(1, y2 - y1));
        int vuW = col.getWidth() / 4;
        vuLeft.setBounds(col.withWidth(vuW).withCentre(col.getCentre()));
    }

    // VU Right
    {
        auto top = cell(10), bot = cell(22);
        int y1 = top.getCentreY(), y2 = bot.getCentreY();
        juce::Rectangle<int> col(top.getX(), y1, top.getWidth(), juce::jmax(1, y2 - y1));
        int vuW = col.getWidth() / 4;
        vuRight.setBounds(col.withWidth(vuW).withCentre(col.getCentre()));
    }

    // Bypass buttons
    gateBypass1.setBounds(ctrBox(cell(13), cell(13).getWidth() / 2, cell(13).getHeight() / 4));
    gateBypass2.setBounds(ctrBox(cell(17), cell(17).getWidth() / 2, cell(17).getHeight() / 4));

    // Input labels
    {
        auto b = cell(18);
        input1Label.setBounds(b.removeFromBottom(b.getHeight() / 4));
    }
    {
        auto b = cell(22);
        input2Label.setBounds(b.removeFromBottom(b.getHeight() / 4));
    }

    // GateIn labels
    {
        auto b = cell(19);
        gateIn1Label.setBounds(b.removeFromBottom(b.getHeight() / 4));
    }
    {
        auto b = cell(23);
        gateIn2Label.setBounds(b.removeFromBottom(b.getHeight() / 4));
    }

    // Gate knobs
    {
        auto c = cell(19);
        int lh = c.getHeight() / 4;
        auto u = c.withTrimmedBottom(lh);
        int d = juce::roundToInt(1.2f * float(juce::jmin(u.getWidth(), u.getHeight())));
        auto ctr = u.getCentre(); ctr.y -= u.getHeight() / 4;
        gateKnob1.setBounds(juce::Rectangle<int>(d, d).withCentre(ctr));
    }
    {
        auto c = cell(23);
        int lh = c.getHeight() / 4;
        auto u = c.withTrimmedBottom(lh);
        int d = juce::roundToInt(1.2f * float(juce::jmin(u.getWidth(), u.getHeight())));
        auto ctr = u.getCentre(); ctr.y -= u.getHeight() / 4;
        gateKnob2.setBounds(juce::Rectangle<int>(d, d).withCentre(ctr));
    }
}
//==============================================================================
// prepare — готовим SimpleGate (здесь только sampleRate, blockSize не нужен)
//==============================================================================
void InputControlComponent::prepare(double sampleRate, int /*blockSize*/) noexcept
{
    if (auto* g = gateProcessor.load(std::memory_order_acquire))
        g->prepare(sampleRate);
}

//==============================================================================
// processBlock — вызывается из PluginProcessCallback первым звеном
//==============================================================================
void InputControlComponent::processBlock(float* const* channels,
    int           numSamples) noexcept
{
    if (auto* g = gateProcessor.load(std::memory_order_acquire))
    {
        // Левый канал
        g->setThresholdDb(thresholdL.load());
        g->process(channels[0], numSamples, bypassL.load());
        float lvlL = g->getEnv();

        // Правый канал (если стерео)
        float* right = channels[1] != nullptr ? channels[1] : channels[0];
        g->setThresholdDb(thresholdR.load());
        g->process(right, numSamples, bypassR.load());
        float lvlR = g->getEnv();

        // Обновляем VU-метры в UI-потоке
        juce::MessageManager::callAsync([this, lvlL, lvlR] {
            vuLeft.setValue(lvlL);
            vuRight.setValue(lvlR);
            });
    }
}

