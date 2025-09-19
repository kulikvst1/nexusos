#include "OutControlComponent.h"
#include <algorithm>  
#include <limits>
static void setSliderDimmed(juce::Slider& s, bool dim)
{
    s.setAlpha(dim ? 0.4f : 1.0f);
    s.setInterceptsMouseClicks(!dim, !dim);
}

static void setComboDimmed(juce::ComboBox& c, bool dim)
{
    c.setAlpha(dim ? 0.4f : 1.0f);
    c.setEnabled(!dim);
}
void OutControlComponent::timerCallback()
{
    if (shuttingDown.load(std::memory_order_acquire))
        return;

    // 1) Прочитать атомики уровней
    const float l = atomicMeterL.load(std::memory_order_relaxed);
    const float r = atomicMeterR.load(std::memory_order_relaxed);

    if (!meterWasUpdated) // если за прошлый тик не пришло новых данных — сбросить
    {
        masterMeterL.setLevel(0.0f);
        masterMeterR.setLevel(0.0f);
        repaint();
    }
    else
    {
        masterMeterL.setLevel(l);
        masterMeterR.setLevel(r);
    }

    meterWasUpdated = false;

    // 2) Обновление клип-LED
    const bool clipL = masterMeterL.isClipping();
    const bool clipR = masterMeterR.isClipping();

    if (clipL != clipLedL.isVisible()) clipLedL.setVisible(clipL);
    if (clipR != clipLedR.isVisible()) clipLedR.setVisible(clipR);
}


void OutControlComponent::prepare(double sampleRate, int blockSize) noexcept
{
    const auto numCh = 2;

    // 1) Подготовка EQ-модуля
    eqDsp.prepare(sampleRate, blockSize, numCh);

    // 2) Загрузка UI-значений в atomic для EQ и сброс флага обновления
    for (int i = 0; i < 5; ++i)
        atomicEqVals[i].store((float)eqSliders[i].getValue());
    eqParamsDirty.store(true);
   
    // 5) Сброс уровней мастермeтров
    masterMeterL.setLevel(0.f);
    masterMeterR.setLevel(0.f);
}

OutControlComponent::OutControlComponent()
{
     setLookAndFeel(&customLF);
    //==================================================================================
    // EQ 
    // =================================================================================
    // EQ BUTTON
    addAndMakeVisible(bypassButton);
    bypassButton.setClickingTogglesState(true);
    bypassButton.setToggleState(false, juce::dontSendNotification);
    bypassButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
    bypassButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::red);
    bypassButton.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    bypassButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    bypassButton.onClick = [this]()
        {
            bool eqActive = bypassButton.getToggleState();           // true = EQ включен
            eqBypassed.store(eqActive, std::memory_order_relaxed);   // теперь без инверсии
            applyEqBypassUI();                                       // обновляем затенение
        };



    // 1) rotary-EQ + их подписи
    static const char* names[5] = { "Low-Cut", "Low", "Mid", "High", "High-Cut" };
    for (int i = 0; i < 5; ++i)
    {
        addAndMakeVisible(eqSliders[i]);
        eqSliders[i].setTextBoxStyle(juce::Slider::TextBoxAbove, false, 120, 80);//or TextBoxBelow
        eqSliders[i].setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);//цвет рамки

        for (int i = 0; i < 5; ++i)
        {
            auto& s = eqSliders[i];
            auto& l = eqLabels[i];
            // слайдеры уже добавлены и видимы
            s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 60, 20);

            if (i == 0) // Low-Cut
            {
                s.setRange(20.0, 500.0, 1.0);   // допустим, от 20 до 1кГц
                s.setValue(50.0);                // стандартный HP @30 Hz
                s.setTextValueSuffix(" Hz");
                eqSliders[i].setDoubleClickReturnValue(true, 50.0);
                eqSliders[i].setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::darkblue);
                eqSliders[i].setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::black);
                s.setColour(juce::Slider::thumbColourId, juce::Colours::blue);

            }
            else if (i == 4) // High-Cut
            {
                s.setRange(4000.0, 20000.0, 100.0); // 4…20 кГц
                s.setValue(16000.0);                // LP @16 кГц
                s.setTextValueSuffix(" Hz");
                eqSliders[i].setDoubleClickReturnValue(true, 16000.0);
                eqSliders[i].setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::black);
                eqSliders[i].setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::darkred);
                s.setColour(juce::Slider::thumbColourId, juce::Colours::red);
            }
            else
            {
                // 1) Общие dB-настройки
                s.setRange(-20.0, 20.0, 0.1);
                s.setValue(0.0);
                s.setTextValueSuffix(" dB");
                s.setDoubleClickReturnValue(true, 0.0);

                // 2) Цветовые схемы в статических массивах (индексы 1,2,3)
                static const juce::Colour thumbCols[] = { {}, juce::Colours::blue,    juce::Colours::yellow,    juce::Colours::red };
                static const juce::Colour fillCols[] = { {}, juce::Colours::darkblue,juce::Colours::yellowgreen,juce::Colours::darkred };
                static const juce::Colour outlineCols[] = { {}, juce::Colours::darkblue,juce::Colours::yellowgreen,juce::Colours::darkred };

                // 3) Применяем только для центральных трёх слайдеров
                if (i >= 1 && i <= 3)
                {
                    s.setColour(juce::Slider::thumbColourId, thumbCols[i]);
                    s.setColour(juce::Slider::rotarySliderFillColourId, fillCols[i]);
                    s.setColour(juce::Slider::rotarySliderOutlineColourId, outlineCols[i]);
                }

            }
            eqSliders[i].onValueChange = [this, i]
             {
                    // сохраняем новый параметр и помечаем dirty
                    atomicEqVals[i].store((float)eqSliders[i].getValue());
                    eqParamsDirty.store(true);
             };
            // подпись EQ под слайдером остаётся прежней
            l.setJustificationType(juce::Justification::centred);
            l.setLookAndFeel(&customLF);
            l.setColour(juce::Label::backgroundColourId, juce::Colours::white);
            l.setColour(juce::Label::textColourId, juce::Colours::black);
            l.setOpaque(true);
        }
        addAndMakeVisible(eqLabels[i]);
        eqLabels[i].setText(names[i], juce::dontSendNotification);
        eqLabels[i].setJustificationType(juce::Justification::centred);
        eqLabels[i].setLookAndFeel(&customLF);
        eqLabels[i].setColour(juce::Label::backgroundColourId, juce::Colours::white);
        eqLabels[i].setColour(juce::Label::textColourId, juce::Colours::black);
        eqLabels[i].setOpaque(true);
    }
    // 2) MASTER EQ над cols 1–3 label
    addAndMakeVisible(masterEqLabel);
    masterEqLabel.setText("MASTER EQ", juce::dontSendNotification);
    masterEqLabel.setJustificationType(juce::Justification::centred);
    masterEqLabel.setLookAndFeel(&customLF);
 
    // =======================================================
    // 3) Gain L + подписи (настройка в dB, без skew)
    // =======================================================
   
    gainSliderL.setSliderStyle(juce::Slider::LinearVertical);
    gainSliderL.setTextBoxStyle(juce::Slider::TextBoxAbove, true, 80, 50);
    gainSliderL.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    gainSliderL.setColour(juce::Slider::trackColourId, juce::Colours::darkred);
    gainSliderL.setColour(juce::Slider::backgroundColourId, juce::Colours::black);

    // Привязка кастомного LookAndFeel
    gainSliderL.setLookAndFeel(&customLaf);

    // Лиснер, всплывающие подсказки и значение
    gainSliderL.addListener(this);
    gainSliderL.setPopupDisplayEnabled(true, true, this);

    // Диапазон, шаг и единицы
    gainSliderL.setRange(-60.0, 12.0, 0.1);
    gainSliderL.setValue(0.0, juce::dontSendNotification);
    gainSliderL.setNumDecimalPlacesToDisplay(1);
    gainSliderL.setTextValueSuffix(" dB");
    gainSliderL.setDoubleClickReturnValue(true, 0.0);
    gainSliderL.setName("left");
    // Добавляем в окно
    addAndMakeVisible(gainSliderL);

    {
        float db = (float)gainSliderL.getValue();
        float lin = juce::Decibels::decibelsToGain(db);
        gainValL.store(lin, std::memory_order_relaxed);

        // если кнопка Link включена — клонируем в правый
        if (atomicLink.load(std::memory_order_relaxed) && !isLinking)
        {
            isLinking = true;
            gainSliderR.setValue(db, juce::dontSendNotification);
            gainValR.store(lin, std::memory_order_relaxed);
            isLinking = false;
        }
    };
    // начальная инициализация атомика для L
    gainValL.store(juce::Decibels::decibelsToGain((float)gainSliderL.getValue()),
        std::memory_order_relaxed);

    addAndMakeVisible(gainLabelL);
    gainLabelL.setText("Gain L", juce::dontSendNotification);
    gainLabelL.setJustificationType(juce::Justification::centred);
    gainLabelL.setLookAndFeel(&customLF);
    gainLabelL.setColour(juce::Label::backgroundColourId, juce::Colours::white);
    gainLabelL.setColour(juce::Label::textColourId, juce::Colours::black);
    gainLabelL.setOpaque(true);
    // =======================================================
    // 4) Gain R + подписи
    // =======================================================
    //  Правый слайдер
    gainSliderR.setSliderStyle(juce::Slider::LinearVertical);
    gainSliderR.setTextBoxStyle(juce::Slider::TextBoxAbove, true, 80, 50);
    gainSliderR.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    gainSliderR.setColour(juce::Slider::trackColourId, juce::Colours::darkred);
    gainSliderR.setColour(juce::Slider::backgroundColourId, juce::Colours::black);

    // Привязка кастомного LookAndFeel
    gainSliderR.setLookAndFeel(&customLaf);

    // Лиснер, всплывающие подсказки и значение
    gainSliderR.addListener(this);
    gainSliderR.setPopupDisplayEnabled(true, true, this);

    // Диапазон, шаг и единицы
    gainSliderR.setRange(-60.0, 12.0, 0.1);
    gainSliderR.setValue(0.0, juce::dontSendNotification);
    gainSliderR.setNumDecimalPlacesToDisplay(1);
    gainSliderR.setTextValueSuffix(" dB");
    gainSliderR.setDoubleClickReturnValue(true, 0.0);
    gainSliderR.setName("right");
    // Добавляем в окно

    addAndMakeVisible(gainSliderR);

    {
        float db = (float)gainSliderR.getValue();
        float lin = juce::Decibels::decibelsToGain(db);
        gainValR.store(lin, std::memory_order_relaxed);

        if (atomicLink.load(std::memory_order_relaxed) && !isLinking)
        {
            isLinking = true;
            gainSliderL.setValue(db, juce::dontSendNotification);
            gainValL.store(lin, std::memory_order_relaxed);
            isLinking = false;
        }
    };

    // начальная инициализация атомика для R
    gainValR.store(juce::Decibels::decibelsToGain((float)gainSliderR.getValue()),
        std::memory_order_relaxed);

    addAndMakeVisible(gainLabelR);
    gainLabelR.setText("Gain R", juce::dontSendNotification);
    gainLabelR.setJustificationType(juce::Justification::centred);
    gainLabelR.setLookAndFeel(&customLF);
    gainLabelR.setColour(juce::Label::backgroundColourId, juce::Colours::white);
    gainLabelR.setColour(juce::Label::textColourId, juce::Colours::black);
    gainLabelR.setOpaque(true);

    // 4) MASTER OUT (row 3, col 2)
    addAndMakeVisible(masterOutLabel);
    masterOutLabel.setText("MASTER OUT", juce::dontSendNotification);
    masterOutLabel.setJustificationType(juce::Justification::centred);
    masterOutLabel.setLookAndFeel(&customLF);
    masterOutLabel.setColour(juce::Label::backgroundColourId, juce::Colours::white);
    masterOutLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    masterOutLabel.setOpaque(true);

    // ––– Мастер-метры и дБ-шкала –––
    const float minDb = -60.0f, maxDb = 6.0f;
    const std::vector<float> marks{ -60.f, -30.f, -20.f, -12.f, -6.f, -3.f, 0.f };

    // единый диапазон
    masterScale.setDbRange(minDb, maxDb);
    masterMeterL.setDbRange(minDb, maxDb);
    masterMeterR.setDbRange(minDb, maxDb);

    // единый набор меток
    masterScale.setScaleMarks(marks);
    masterMeterL.setScaleMarks(marks);
    masterMeterR.setScaleMarks(marks);

    // делаем все три компонента видимыми
    addAndMakeVisible(masterScale);
    addAndMakeVisible(masterMeterL);
    addAndMakeVisible(masterMeterR);


    // ========== Mute/Solo для левого канала ==========
    addAndMakeVisible(muteButtonL);
    muteButtonL.setButtonText("M");
    muteButtonL.setColour(juce::TextButton::textColourOffId, juce::Colours::red);
    muteButtonL.setClickingTogglesState(true);
    muteButtonL.onClick = [this]()
        {
            atomicMuteL.store(muteButtonL.getToggleState(),
                std::memory_order_relaxed);
        };

    addAndMakeVisible(soloButtonL);
    soloButtonL.setButtonText("S");
    soloButtonL.setColour(juce::TextButton::textColourOffId, juce::Colours::yellow);
    soloButtonL.setClickingTogglesState(true);
    soloButtonL.onClick = [this]()
        {
            atomicSoloL.store(soloButtonL.getToggleState(),
                std::memory_order_relaxed);
        };

    // ========== Mute/Solo для правого канала ==========
    addAndMakeVisible(muteButtonR);
    muteButtonR.setButtonText("M");
    muteButtonR.setColour(juce::TextButton::textColourOffId, juce::Colours::red);
    muteButtonR.setClickingTogglesState(true);
    muteButtonR.onClick = [this]()
        {
            atomicMuteR.store(muteButtonR.getToggleState(),
                std::memory_order_relaxed);
        };

    addAndMakeVisible(soloButtonR);
    soloButtonR.setButtonText("S");
    soloButtonR.setColour(juce::TextButton::textColourOffId, juce::Colours::yellow);
    soloButtonR.setClickingTogglesState(true);
    soloButtonR.onClick = [this]()
        {
            atomicSoloR.store(soloButtonR.getToggleState(),
                std::memory_order_relaxed);
        };

    // 1) Link-кнопка
    addAndMakeVisible(linkButton);
    linkButton.setButtonText("L+R");
    linkButton.setClickingTogglesState(true);

    // включаем линк по умолчанию и синхронизируем атомик
    linkButton.setToggleState(true, juce::dontSendNotification);
    atomicLink.store(true, std::memory_order_relaxed);

    // обновляем atomicLink при клике на кнопку
    linkButton.onClick = [this]()
        {
            atomicLink.store(linkButton.getToggleState(),
                std::memory_order_relaxed);
        };

    // 2) Лямбда для левого слайдера (L→R)
    gainSliderL.onValueChange = [this]()
        {
            float db = (float)gainSliderL.getValue();
            float lin = juce::Decibels::decibelsToGain(db);
            gainValL.store(lin, std::memory_order_relaxed);

            if (atomicLink.load(std::memory_order_relaxed) && !isLinking)
            {
                isLinking = true;
                gainSliderR.setValue(db, juce::dontSendNotification);
                gainValR.store(lin, std::memory_order_relaxed);
                isLinking = false;
            }
        };

    // 3) Лямбда для правого слайдера (R→L)
    gainSliderR.onValueChange = [this]()
        {
            float db = (float)gainSliderR.getValue();
            float lin = juce::Decibels::decibelsToGain(db);
            gainValR.store(lin, std::memory_order_relaxed);

            if (atomicLink.load(std::memory_order_relaxed) && !isLinking)
            {
                isLinking = true;
                gainSliderL.setValue(db, juce::dontSendNotification);
                gainValL.store(lin, std::memory_order_relaxed);
                isLinking = false;
            }
        };
    auto setupToggleButton = [](juce::TextButton& b,
        juce::Colour offBg, juce::Colour onBg,
        juce::Colour textCol)
        {
            b.setClickingTogglesState(true);
            b.setColour(juce::TextButton::buttonColourId, offBg);  // фон выкл.
            b.setColour(juce::TextButton::buttonOnColourId, onBg);   // фон вкл.
            b.setColour(juce::TextButton::textColourOffId, textCol);
            b.setColour(juce::TextButton::textColourOnId, textCol);

        };
    // ========== Meter Mode Button ==========
    addAndMakeVisible(meterModeButton);
    meterModeButton.setButtonText("Post VU");
    muteButtonR.setColour(juce::TextButton::textColourOffId, juce::Colours::green);
    meterModeButton.setClickingTogglesState(true);

    // сразу синхронизируем атомик с состоянием кнопки
    atomicMeterMode.store(meterModeButton.getToggleState(), std::memory_order_relaxed);

    meterModeButton.onClick = [this]()
        {
            bool isPost = meterModeButton.getToggleState();
            meterModeButton.setButtonText(isPost ? "Post VU" : "Pre VU");
            atomicMeterMode.store(isPost, std::memory_order_relaxed);
        };

    setupToggleButton(muteButtonL, juce::Colour(0xFF660000), juce::Colours::red, juce::Colours::black);
    setupToggleButton(soloButtonL, juce::Colour(0x66666600), juce::Colours::yellow, juce::Colours::black);
    setupToggleButton(muteButtonR, juce::Colour(0xFF660000), juce::Colours::red, juce::Colours::black);
    setupToggleButton(soloButtonR, juce::Colour(0x66666600), juce::Colours::yellow, juce::Colours::black);
    setupToggleButton(linkButton, juce::Colours::darkblue, juce::Colours::blue, juce::Colours::black);
    setupToggleButton(meterModeButton, juce::Colours::darkgreen, juce::Colours::limegreen, juce::Colours::black);

    // 11) Cliping 
    clipLedL.setText(" CLIP", juce::dontSendNotification);
    clipLedR.setText(" CLIP", juce::dontSendNotification);
    clipLedL.setColour(juce::Label::backgroundColourId, juce::Colours::red);
    clipLedR.setColour(juce::Label::backgroundColourId, juce::Colours::red);
    clipLedL.setVisible(false);
    clipLedR.setVisible(false);
    addAndMakeVisible(clipLedL);
    addAndMakeVisible(clipLedR);
   
    // Лямбда для общей настройки любого ComboBox + Label
    auto setupCombo = [this](juce::ComboBox& box,
        const std::vector<float>& presets,
        std::atomic<float>& storage)
        {
            // 1) Показываем только сам ComboBox
            addAndMakeVisible(box);

            // 2) Заполняем пункты меню
            box.clear();
            for (int i = 0; i < (int)presets.size(); ++i)
                box.addItem(juce::String(presets[i], 0) + " Hz", i + 1);
            // 3) Подбираем в списке сохранённое значение из storage
            float curValue = storage.load(std::memory_order_relaxed);
            auto  it = std::find(presets.begin(), presets.end(), curValue);
            int index;
            if (it != presets.end())
                index = int(it - presets.begin());     // нашли — берём позицию
            else
                index = 0;                              // не нашли — дефолтный первый
            box.setSelectedItemIndex(index, juce::dontSendNotification);

            // 4) Обработчик смены пункта
            box.onChange = [this, &box, &presets, &storage]()
                {
                    int idx = box.getSelectedId() - 1;
                    storage.store(presets[idx], std::memory_order_relaxed);
                    eqParamsDirty.store(true, std::memory_order_relaxed);
                };
        };

    loadSettings();

    setupCombo(lowShelfFreqBox, lowShelfPresets, lowShelfFreqVal);
    setupCombo(peakFreqBox, peakPresets, peakFreqVal);
    setupCombo(highShelfFreqBox, highShelfPresets, highShelfFreqVal);

    syncButtonStatesToAtomics();
    startTimerHz(30);
    meterWasUpdated = false;
}
OutControlComponent::~OutControlComponent()
{
    // Сигнал всем потокам и таймерам: выходим
    shuttingDown.store(true, std::memory_order_release);

    // Остановить таймер, чтобы timerCallback не пришёл после начала деструкции
    stopTimer();

    // 1) Сохраняем UI-/EQ-параметры
    saveSettings();

    // 2) Сброс L&F у детей
    for (auto* child : getChildren())
        child->setLookAndFeel(nullptr);

    // 3) Сброс L&F у самого компонента
    setLookAndFeel(nullptr);

    // 4) Снимаем L&F и слушателей со слайдеров
    gainSliderL.setLookAndFeel(nullptr);
    gainSliderR.setLookAndFeel(nullptr);
    gainSliderL.removeListener(this);
    gainSliderR.removeListener(this);
}

void OutControlComponent::applyEqBypassUI()
{
    bool eqActive = bypassButton.getToggleState();
    bool dim = !eqActive; // если EQ выключен — затеняем

    for (auto& s : eqSliders)
    {
        s.setAlpha(dim ? 0.4f : 1.0f);
        s.setInterceptsMouseClicks(!dim, !dim);
    }

    auto setComboDimmed = [&](juce::ComboBox& c, bool dim)
        {
            c.setAlpha(dim ? 0.4f : 1.0f);
            c.setEnabled(!dim);
        };

    setComboDimmed(lowShelfFreqBox, dim);
    setComboDimmed(peakFreqBox, dim);
    setComboDimmed(highShelfFreqBox, dim);
}


void OutControlComponent::resized()
{
    // ==== БАЗОВЫЕ РАЗМЕРЫ МАКЕТА ====
    const float baseW = 1280.0f; // ← ширина, под которую верстался UI
    const float baseH = 720.0f;  // ← высота, под которую верстался UI

    // ==== КОЭФФИЦИЕНТЫ МАСШТАБА ====
    const float scaleX = getWidth() / baseW;
    const float scaleY = getHeight() / baseH;

    auto SX = [&](int v) { return juce::roundToInt(v * scaleX); }; // горизонтальные размеры
    auto SY = [&](int v) { return juce::roundToInt(v * scaleY); }; // вертикальные размеры

    Grid grid(getLocalBounds(), 5, 4);

    // 1) Базовые константы (масштабируемые)
    int   cellH = grid.getSector(0, 0).getHeight();
    int   lblH = cellH / 4;
    float fontPct = 0.6f;
    int   offsetY = lblH;
    float shrink = 0.10f;
    int   marginX = SX(8);
    int   pad = SX(4);

    // 2) Размещение 5 Rotary-слайдеров
    for (int col = 0; col < 5; ++col)
    {
        auto area = grid.getSector(0, col)
            .reduced(int(cellH * shrink), int(cellH * shrink))
            .translated(0, offsetY);
        eqSliders[col].setBounds(area);
    }

    // 3) EQ-лейблы (row = 1)
    for (int col = 0; col < 5; ++col)
    {
        auto sector = grid.getSector(1, col).translated(0, offsetY);
        auto lbArea = sector.withHeight(lblH)
            .withY(sector.getY() + SY(30))
            .reduced(marginX, 0);
        eqLabels[col].setBounds(lbArea);
        eqLabels[col].setFont({ lblH * fontPct, juce::Font::bold });
    }

    // 4) Лямбда для ComboBox над лейблами
    auto placeComboOnLabel = [&](juce::ComboBox& cb, int col,
        float widthPct = 0.5f,
        float heightPct = 0.6f,
        int   liftPx = 10)
        {
            auto sector = grid.getSector(1, col).translated(0, offsetY);
            auto lbArea = sector.withHeight(lblH)
                .withY(sector.getY() - SY(30))
                .reduced(marginX, 0);

            int comboW = int(lbArea.getWidth() * widthPct);
            int comboH = int(lblH * heightPct);

            auto comboArea = lbArea.withSizeKeepingCentre(comboW, comboH)
                .translated(0, SY(liftPx));

            cb.setBounds(comboArea);
            cb.setJustificationType(juce::Justification::centred);
        };

    placeComboOnLabel(lowShelfFreqBox, 1, 0.5f, 0.6f, 8);
    placeComboOnLabel(peakFreqBox, 2, 0.5f, 0.6f, 8);
    placeComboOnLabel(highShelfFreqBox, 3, 0.5f, 0.6f, 8);

    // 5) «MASTER EQ» header
    {
        auto hdr = grid.getUnion(0, 1, 3)
            .reduced(SX(4))
            .withHeight(lblH);
        masterEqLabel.setBounds(hdr);
        masterEqLabel.setFont({ lblH * fontPct, juce::Font::bold });
    }

    // 6) Гейны L/R
    const int gainLift = SY(8); // поднимаем слайдеры

    juce::Rectangle<int> gainBoundsL, gainBoundsR;

    // 6.1) Gain L
    {
        auto parent = grid.getSector(2, 1)
            .getUnion(grid.getSector(3, 1))
            .reduced(SX(6));

        int labelTopY = parent.getBottom() - lblH;
        int origH = parent.getHeight() - lblH;
        int newH = int(origH * 1.1f);
        int newY = labelTopY - newH - gainLift;

        gainBoundsL = { parent.getX(), newY, parent.getWidth(), newH };
        gainSliderL.setBounds(gainBoundsL);

        auto lb = parent.withHeight(lblH)
            .withY(parent.getBottom() - lblH);
        gainLabelL.setBounds(lb);
        gainLabelL.setFont({ lblH * fontPct, juce::Font::bold });
    }

    // 6.2) Gain R
    {
        auto parent = grid.getSector(2, 3)
            .getUnion(grid.getSector(3, 3))
            .reduced(SX(6));

        int labelTopY = parent.getBottom() - lblH;
        int origH = parent.getHeight() - lblH;
        int newH = int(origH * 1.1f);
        int newY = labelTopY - newH - gainLift;

        gainBoundsR = { parent.getX(), newY, parent.getWidth(), newH };
        gainSliderR.setBounds(gainBoundsR);

        auto lb = parent.withHeight(lblH)
            .withY(parent.getBottom() - lblH);
        gainLabelR.setBounds(lb);
        gainLabelR.setFont({ lblH * fontPct, juce::Font::bold });
    }

    // 7) Master-метры + шкала + подпись MASTER OUT
    {
        auto cell = grid.getSector(3, 2).reduced(SX(4));
        int pad2 = SX(4);
        int lblH2 = grid.getSector(0, 0).getHeight() / 4;
        int meterH = gainBoundsL.getHeight();
        int availW = cell.getWidth() - pad2 * 2;
        int meterW = availW / 4;
        int scaleW = availW - meterW * 2;
        int startX = cell.getX() + pad2;
        int startY = cell.getBottom() - lblH2 - meterH;

        masterMeterL.setBounds(startX, startY, meterW, meterH);
        masterScale.setBounds(startX + meterW, startY, scaleW, meterH);
        masterMeterR.setBounds(startX + meterW + scaleW, startY, meterW, meterH);

        auto outArea = cell.withHeight(lblH2)
            .withY(cell.getBottom() - lblH2);
        masterOutLabel.setBounds(outArea);
        masterOutLabel.setFont({ lblH2 * 0.6f, juce::Font::bold });
    }

    // 8) Mute/Solo L/R
    {
        auto secL = grid.getSector(3, 1);
        auto secR = grid.getSector(3, 3);
        int  btnW = secL.getWidth() / 4;
        int  btnH = secL.getHeight() / 4;
        int  offsetX = btnW;
        int  padY = SY(15);
        int  msGap = SY(8);

        // L-канал
        {
            int centreX = (gainBoundsL.getRight() + masterMeterL.getX()) / 2;
            int xL = centreX - btnW / 3 - offsetX;
            int labelY = secL.getBottom() - lblH;

            int ySolo = labelY - padY - btnH;
            int yMute = ySolo - btnH - msGap;

            muteButtonL.setBounds(xL, yMute, btnW, btnH);
            soloButtonL.setBounds(xL, ySolo, btnW, btnH);
        }
        // R-канал
        {
            int centreX = (masterMeterR.getRight() + gainBoundsR.getX()) / 2;
            int xR = centreX - btnW / 1.5 + offsetX;
            int labelY = secR.getBottom() - lblH;

            int ySolo = labelY - padY - btnH;
            int yMute = ySolo - btnH - msGap;

            muteButtonR.setBounds(xR, yMute, btnW, btnH);
            soloButtonR.setBounds(xR, ySolo, btnW, btnH);
        }
    }

    // 9) Link‑кнопка + MeterMode
    {
        auto sec = grid.getSector(3, 2);

        int btnW = sec.getWidth() / 4;
        int btnH = sec.getHeight() / 4;

        int gapToLink = SY(12); // отступ от подписи до linkButton
        int gapBetween = SY(8);  // отступ между linkButton и meterModeButton

        int x = sec.getCentreX() - btnW / 2;
        int labelTopY = sec.getBottom() - lblH;

        // Y для linkButton
        int yLink = labelTopY - gapToLink - btnH;
        linkButton.setBounds(x, yLink, btnW, btnH);

        // Y для meterModeButton
        int yMeterMode = yLink - gapBetween - btnH;
        meterModeButton.setBounds(x, yMeterMode, btnW, btnH);
    }

    // 10) Bypass EQ (правый верхний угол)
    {
        auto refSec = grid.getSector(3, 2);
        int btnW = refSec.getWidth() / 4;
        int btnH = refSec.getHeight() / 4;
        int margin = SX(8);

        int x = getWidth() - margin - btnW;
        int y = margin;

        bypassButton.setBounds(x, y, btnW, btnH);
    }

    // 12) CLIP‑LED и PEAK‑LED над кнопками Mute
    {
        auto placeLed = [&](juce::Label& led,
            const juce::TextButton& btn,
            int extraX,
            int extraY)
            {
                auto b = btn.getBounds();
                int ledW = b.getWidth();
                int ledH = b.getHeight() / 2; // высота LED — половина кнопки
                int ledX = b.getX() + extraX;
                int ledY = b.getY() - ledH - extraY;

                led.setBounds(ledX, ledY, ledW, ledH);
            };

        int peakOffsetY = SY(12); // вертикальный отступ LED от кнопки Mute

        placeLed(clipLedL, muteButtonL, 0, peakOffsetY);
        placeLed(clipLedR, muteButtonR, 0, peakOffsetY);
    }
}

void OutControlComponent::processAudioBlock(juce::AudioBuffer<float>& buffer) noexcept
{
    if (shuttingDown.load(std::memory_order_acquire))
        return;

    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int numSamps = buffer.getNumSamples();

    // 1) Периодическое обновление параметров EQ
    if (++dspUpdateCounter >= dspUpdateInterval)
    {
        dspUpdateCounter = 0;

        if (eqParamsDirty.exchange(false, std::memory_order_acq_rel))
        {
            EqSettings s{
                atomicEqVals[0].load(),   // lowCutFreq
                lowShelfFreqVal.load(),   // lowShelfFreq
                atomicEqVals[1].load(),   // lowGain
                peakFreqVal.load(),       // peakFreq
                atomicEqVals[2].load(),   // midGain
                1.0f,                     // peakQ
                highShelfFreqVal.load(),  // highShelfFreq
                atomicEqVals[3].load(),   // highGain
                atomicEqVals[4].load()    // highCutFreq
            };
            eqDsp.updateSettings(s);
        }
    }

    // 2) Применяем EQ, если НЕ байпасс
    if (eqBypassed.load(std::memory_order_relaxed))
        eqDsp.process(buffer);

    // 3) Считываем atomics для гейна и флагов
    float gL = gainValL.load(std::memory_order_relaxed);
    float gR = gainValR.load(std::memory_order_relaxed);
    const bool link = atomicLink.load(std::memory_order_relaxed);
    const bool muteL = atomicMuteL.load(std::memory_order_relaxed);
    const bool muteR = atomicMuteR.load(std::memory_order_relaxed);
    const bool soloL = atomicSoloL.load(std::memory_order_relaxed);
    const bool soloR = atomicSoloR.load(std::memory_order_relaxed);

    if (link) gR = gL;

    const bool anySolo = (soloL || soloR);
    if (anySolo)
    {
        if (!soloL) gL = 0.0f;
        if (!soloR) gR = 0.0f;
    }
    else
    {
        if (muteL) gL = 0.0f;
        if (muteR) gR = 0.0f;
    }

    const bool postMode = atomicMeterMode.load(std::memory_order_relaxed);

    // 4) Гейн + пики (не трогаем UI из аудиопотока)
    for (int ch = 0; ch < numCh && ch < 2; ++ch)
    {
        auto* ptr = buffer.getWritePointer(ch);
        const float gain = (ch == 0 ? gL : gR);

        float preP = 0.0f;
        float postP = 0.0f;

        for (int i = 0; i < numSamps; ++i)
        {
            const float in = ptr[i];
            preP = juce::jmax(preP, std::abs(in));
            const float out = in * gain;
            ptr[i] = out;
            postP = juce::jmax(postP, std::abs(out));
        }

        if (ch == 0)
            atomicMeterL.store(postMode ? postP : preP, std::memory_order_relaxed);
        else
            atomicMeterR.store(postMode ? postP : preP, std::memory_order_relaxed);
    }

    meterWasUpdated = true; // сделай это полем std::atomic<bool>, если ещё нет
}

void OutControlComponent::sliderValueChanged(juce::Slider* s)
{

    if (s == &gainSliderL)
    {
        float db = (float)gainSliderL.getValue();
        float lin = juce::Decibels::decibelsToGain(db);
        gainValL.store(lin, std::memory_order_relaxed);

        if (atomicLink.load(std::memory_order_relaxed) && !isLinking)
        {
            isLinking = true;
            gainSliderR.setValue(db, juce::dontSendNotification);
            gainValR.store(lin, std::memory_order_relaxed);
            isLinking = false;
        }
    }
    else if (s == &gainSliderR)
    {
        float db = (float)gainSliderR.getValue();
        float lin = juce::Decibels::decibelsToGain(db);
        gainValR.store(lin, std::memory_order_relaxed);

        if (atomicLink.load(std::memory_order_relaxed) && !isLinking)
        {
            isLinking = true;
            gainSliderL.setValue(db, juce::dontSendNotification);
            gainValL.store(lin, std::memory_order_relaxed);
            isLinking = false;
        }
    }
    //это вставили
    if (onMasterGainChanged)
    {
        float masterDb = juce::jmax(
            (float)gainSliderL.getValue(),
            (float)gainSliderR.getValue()
        );
        onMasterGainChanged(masterDb);
    }
    
}
void OutControlComponent::offsetGainDb(float deltaDb) noexcept
{
    if (isLinking) return;
    isLinking = true;

    float newL = (float)gainSliderL.getValue() + deltaDb;
    float newR = (float)gainSliderR.getValue() + deltaDb;

    gainSliderL.setValue(newL, juce::dontSendNotification);
    gainSliderR.setValue(newR, juce::dontSendNotification);

    gainValL.store(juce::Decibels::decibelsToGain(newL),
        std::memory_order_relaxed);
    gainValR.store(juce::Decibels::decibelsToGain(newR),
        std::memory_order_relaxed);

    isLinking = false;

    //это вставили 
    if (onMasterGainChanged)
    {
        float masterDb = juce::jmax(newL, newR);
        onMasterGainChanged(masterDb);
    }
}
static const char* settingsFileName = "OutControlSettings.xml";

void OutControlComponent::saveSettings() const
{
    auto xml = std::make_unique<juce::XmlElement>("OutControl");

    // 1) EQ-параметры
    xml->setAttribute("lowCutFreq", atomicEqVals[0].load());
    xml->setAttribute("lowShelfFreq", lowShelfFreqVal.load());
    xml->setAttribute("lowShelfGain", atomicEqVals[1].load());
    xml->setAttribute("peakFreq", peakFreqVal.load());
    xml->setAttribute("peakGain", atomicEqVals[2].load());
    xml->setAttribute("highShelfFreq", highShelfFreqVal.load());
    xml->setAttribute("highShelfGain", atomicEqVals[3].load());
    xml->setAttribute("highCutFreq", atomicEqVals[4].load());

    // 2) Выходные контролы
    xml->setAttribute("gainL", gainSliderL.getValue());
    xml->setAttribute("gainR", gainSliderR.getValue());

    xml->setAttribute("muteL", muteButtonL.getToggleState());
    xml->setAttribute("muteR", muteButtonR.getToggleState());
    xml->setAttribute("soloL", soloButtonL.getToggleState());
    xml->setAttribute("soloR", soloButtonR.getToggleState());
    xml->setAttribute("link", linkButton.getToggleState());

    // Сохраняем именно eqActive (состояние кнопки)
    xml->setAttribute("eqActive", bypassButton.getToggleState());

    xml->setAttribute("meterModeButton", meterModeButton.getToggleState());

    // 3) Запись в файл
    auto file = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile(settingsFileName);

    file.getParentDirectory().createDirectory();
    xml->writeTo(file, {});
}

void OutControlComponent::loadSettings()
{
    auto file = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile(settingsFileName);

    if (!file.existsAsFile())
        return;

    std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(file));
    if (!xml || !xml->hasTagName("OutControl"))
        return;

    // 1) EQ-параметры
    atomicEqVals[0].store((float)xml->getDoubleAttribute("lowCutFreq", atomicEqVals[0].load()), std::memory_order_relaxed);
    lowShelfFreqVal.store((float)xml->getDoubleAttribute("lowShelfFreq", lowShelfFreqVal.load()), std::memory_order_relaxed);
    atomicEqVals[1].store((float)xml->getDoubleAttribute("lowShelfGain", atomicEqVals[1].load()), std::memory_order_relaxed);
    peakFreqVal.store((float)xml->getDoubleAttribute("peakFreq", peakFreqVal.load()), std::memory_order_relaxed);
    atomicEqVals[2].store((float)xml->getDoubleAttribute("peakGain", atomicEqVals[2].load()), std::memory_order_relaxed);
    highShelfFreqVal.store((float)xml->getDoubleAttribute("highShelfFreq", highShelfFreqVal.load()), std::memory_order_relaxed);
    atomicEqVals[3].store((float)xml->getDoubleAttribute("highShelfGain", atomicEqVals[3].load()), std::memory_order_relaxed);
    atomicEqVals[4].store((float)xml->getDoubleAttribute("highCutFreq", atomicEqVals[4].load()), std::memory_order_relaxed);

    // 2) UI-слайдеры и ComboBox'ы
    eqSliders[0].setValue(atomicEqVals[0].load(), juce::dontSendNotification);
    lowShelfFreqBox.setSelectedId(
        int(std::distance(lowShelfPresets.begin(),
            std::find(lowShelfPresets.begin(), lowShelfPresets.end(), lowShelfFreqVal.load()))) + 1,
        juce::dontSendNotification);

    eqSliders[1].setValue(atomicEqVals[1].load(), juce::dontSendNotification);
    peakFreqBox.setSelectedId(
        int(std::distance(peakPresets.begin(),
            std::find(peakPresets.begin(), peakPresets.end(), peakFreqVal.load()))) + 1,
        juce::dontSendNotification);

    eqSliders[2].setValue(atomicEqVals[2].load(), juce::dontSendNotification);
    highShelfFreqBox.setSelectedId(
        int(std::distance(highShelfPresets.begin(),
            std::find(highShelfPresets.begin(), highShelfPresets.end(), highShelfFreqVal.load()))) + 1,
        juce::dontSendNotification);

    eqSliders[3].setValue(atomicEqVals[3].load(), juce::dontSendNotification);
    eqSliders[4].setValue(atomicEqVals[4].load(), juce::dontSendNotification);

    // 3) Гейны
    gainSliderL.setValue(xml->getDoubleAttribute("gainL", gainSliderL.getValue()), juce::sendNotification);
    gainSliderR.setValue(xml->getDoubleAttribute("gainR", gainSliderR.getValue()), juce::sendNotification);

    // 4) Кнопки
    muteButtonL.setToggleState(xml->getBoolAttribute("muteL", muteButtonL.getToggleState()), juce::dontSendNotification);
    soloButtonL.setToggleState(xml->getBoolAttribute("soloL", soloButtonL.getToggleState()), juce::dontSendNotification);
    muteButtonR.setToggleState(xml->getBoolAttribute("muteR", muteButtonR.getToggleState()), juce::dontSendNotification);
    soloButtonR.setToggleState(xml->getBoolAttribute("soloR", soloButtonR.getToggleState()), juce::dontSendNotification);
    linkButton.setToggleState(xml->getBoolAttribute("link", linkButton.getToggleState()), juce::dontSendNotification);

    bypassButton.setToggleState(xml->getBoolAttribute("eqActive", bypassButton.getToggleState()), juce::dontSendNotification);
    meterModeButton.setToggleState(xml->getBoolAttribute("meterModeButton", meterModeButton.getToggleState()), juce::dontSendNotification);

    // 5) Синхронизация с DSP
    eqBypassed.store(bypassButton.getToggleState(), std::memory_order_relaxed); // без инверсии
    syncButtonStatesToAtomics();

    // 6) Затенение
    applyEqBypassUI();

    // 7) Флаг обновления EQ
    eqParamsDirty.store(true, std::memory_order_relaxed);
    dspUpdateCounter = dspUpdateInterval;
}







