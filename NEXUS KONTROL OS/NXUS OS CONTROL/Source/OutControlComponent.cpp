#include "OutControlComponent.h"
#include <algorithm>  
#include <limits>

void OutControlComponent::timerCallback()
{
    // ——— 1) Сброс VU-метров при отсутствии новых данных ———
    if (!meterWasUpdated)
    {
        masterMeterL.setLevel(0.0f);
        masterMeterR.setLevel(0.0f);
        repaint();  // чтобы сразу увидеть «опущенные» метры
    }
    meterWasUpdated = false;   // готовимся к следующему циклу

    // ——— 2) Обновление клип-LED ———
    bool clipL = masterMeterL.isClipping();
    bool clipR = masterMeterR.isClipping();

    if (clipL != clipLedL.isVisible())
        clipLedL.setVisible(clipL);

    if (clipR != clipLedR.isVisible())
        clipLedR.setVisible(clipR);
}
void OutControlComponent::prepare(double sampleRate, int blockSize) noexcept
{
    const auto numCh = 2;
    eqDsp.prepare(sampleRate, blockSize, numCh);

    // загрузить UI-значения в atomicEqVals
    for (int i = 0; i < 5; ++i)
        atomicEqVals[i].store((float)eqSliders[i].getValue());

    // заставить однократный апдейт фильтров
    eqParamsDirty.store(true);
    masterMeterL.setLevel(0.f);
    masterMeterR.setLevel(0.f);
}
OutControlComponent::OutControlComponent()
{
     setLookAndFeel(&customLF);
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

    // 2) MASTER EQ над cols 1–3
    addAndMakeVisible(masterEqLabel);
    masterEqLabel.setText("MASTER EQ", juce::dontSendNotification);
    masterEqLabel.setJustificationType(juce::Justification::centred);
    masterEqLabel.setLookAndFeel(&customLF);

  //  atomicLink.store(true, std::memory_order_relaxed);/////////////////////////////////
   // isLinking = false;

    // =======================================================
    // 3) Gain L + подписи (настройка в dB, без skew)
    // =======================================================
   //  Левый слайдер
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
   // startTimerHz(30);
    // Настройка кнопки обхода эквалайзера как toggle
    setupToggleButton(bypassButton,
        juce::Colour(0xFF660000),  // фон выкл. (darkred)
        juce::Colours::red,         // фон вкл.
        juce::Colours::black);      // текст

    bypassButton.onClick = [this]()
        {
            eqBypassed.store(bypassButton.getToggleState(), std::memory_order_relaxed);
        };
    addAndMakeVisible(bypassButton);

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
    // 1) Сохраняем UI-/EQ-параметры
    saveSettings();
    // 2) Сбрасываем кастомный LookAndFeel у всех дочерних компонентов
    for (auto* child : getChildren())
        child->setLookAndFeel(nullptr);
    // 3) Сбрасываем L&F у самого компонента на всякий случай
    setLookAndFeel(nullptr);
    // 4)
    gainSliderL.setLookAndFeel(nullptr);
    gainSliderR.setLookAndFeel(nullptr);
    gainSliderL.removeListener(this);
    gainSliderR.removeListener(this);
}
void OutControlComponent::resized()
{
    Grid grid(getLocalBounds(), 5, 4);
    // 1) Базовые константы
    int   cellH = grid.getSector(0, 0).getHeight();
    int   lblH = cellH / 4;
    float fontPct = 0.6f;
    int   offsetY = lblH;
    float shrink = 0.10f;
    int   marginX = 8;
    int   pad = 4;
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
            .withY(sector.getY() + 30)
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
                .withY(sector.getY() - 30)
                .reduced(marginX, 0);

            int comboW = int(lbArea.getWidth() * widthPct);
            int comboH = int(lblH * heightPct);

            auto comboArea = lbArea.withSizeKeepingCentre(comboW, comboH)
                .translated(0, liftPx);

            cb.setBounds(comboArea);
            cb.setJustificationType(juce::Justification::centred);
        };

    placeComboOnLabel(lowShelfFreqBox, 1, 0.5f, 0.6f, 8);
    placeComboOnLabel(peakFreqBox, 2, 0.5f, 0.6f, 8);
    placeComboOnLabel(highShelfFreqBox, 3, 0.5f, 0.6f, 8);
    // 5) «MASTER EQ» header
    {
        auto hdr = grid.getUnion(0, 1, 3)
            .reduced(4)
            .withHeight(lblH);
        masterEqLabel.setBounds(hdr);
        masterEqLabel.setFont({ lblH * fontPct, juce::Font::bold });
    }
    // 6) Гейны L/R
    constexpr int gainLift = 8;  // поднимет слайдеры на 8px

    juce::Rectangle<int> gainBoundsL, gainBoundsR;

    // 6.1) Gain L
    {
        auto parent = grid.getSector(2, 1)
            .getUnion(grid.getSector(3, 1))
            .reduced(6);

        int labelTopY = parent.getBottom() - lblH;
        int origH = parent.getHeight() - lblH;
        int newH = int(origH * 1.1f);
        int newY = labelTopY - newH
            - gainLift;          // <— подъём

        gainBoundsL = { parent.getX(), newY,
                        parent.getWidth(), newH };
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
            .reduced(6);

        int labelTopY = parent.getBottom() - lblH;
        int origH = parent.getHeight() - lblH;
        int newH = int(origH * 1.1f);
        int newY = labelTopY - newH
            - gainLift;          // <— тот же lift

        gainBoundsR = { parent.getX(), newY,
                        parent.getWidth(), newH };
        gainSliderR.setBounds(gainBoundsR);

        auto lb = parent.withHeight(lblH)
            .withY(parent.getBottom() - lblH);
        gainLabelR.setBounds(lb);
        gainLabelR.setFont({ lblH * fontPct, juce::Font::bold });
    }

    // 7) Master-метры + шкала + подпись MASTER OUT
    {
        auto cell = grid.getSector(3, 2).reduced(4);
        int pad2 = 4;
        int lblH2 = grid.getSector(0, 0).getHeight() / 4;
        int meterH = gainBoundsL.getHeight();
        int availW = cell.getWidth() - pad2 * 2;
        int meterW = availW / 4;
        int scaleW = availW - meterW * 2;
        int startX = cell.getX() + pad2;
        int startY = cell.getBottom() - lblH2 - meterH;

        masterMeterL.setBounds(startX, startY, meterW, meterH);
        masterScale.setBounds(startX + meterW, startY, scaleW, meterH);
        masterMeterR.setBounds(startX + meterW + scaleW,
            startY, meterW, meterH);

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
        int  padY = 15;
        int  msGap = 8;
        // L-канал
        {
            int centreX = (gainBoundsL.getRight() + masterMeterL.getX()) / 2;
            int xL = centreX - btnW / 3 - offsetX;//смещение по x <>
            int labelY = secL.getBottom() - lblH;

            int ySolo = labelY - padY - btnH;
            int yMute = ySolo - btnH - msGap;

            muteButtonL.setBounds(xL, yMute, btnW, btnH);
            soloButtonL.setBounds(xL, ySolo, btnW, btnH);
        }
        // R-канал
        {
            int centreX = (masterMeterR.getRight() + gainBoundsR.getX()) / 2;
            int xR = centreX - btnW / 1.5 + offsetX;//смещение по x <>
            int labelY = secR.getBottom() - lblH;

            int ySolo = labelY - padY - btnH;
            int yMute = ySolo - btnH - msGap;

            muteButtonR.setBounds(xR, yMute, btnW, btnH);
            soloButtonR.setBounds(xR, ySolo, btnW, btnH);
        }
    }
    // 9) Link-кнопка в центре (оставляем прежней)
    {
        auto sec = grid.getSector(3, 2);

        // размеры кнопок
        int btnW = sec.getWidth() / 4;
        int btnH = sec.getHeight() / 4;

        // отступ от подписи до linkButton
        int gapToLink = 12;
        // отступ между linkButton и meterModeButton
        int gapBetween = 8;   // <-- играйте этим значением

        int x = sec.getCentreX() - btnW / 2;
        int labelTopY = sec.getBottom() - lblH;

        // рассчитываем Y для linkButton
        int yLink = labelTopY - gapToLink - btnH;
        linkButton.setBounds(x, yLink, btnW, btnH);

        // рассчитываем Y для bypassButton
        int yByp = yLink - gapBetween - btnH;
        meterModeButton.setBounds(x, yByp, btnW, btnH);
    }
    // 10)  Bypass EQ 
    {
        auto refSec = grid.getSector(3, 2);
        int  btnW = refSec.getWidth() / 4;
        int  btnH = refSec.getHeight() / 4;
        const int margin = 8;  // отступ от краёв

        // вычисляем позицию в правом верхнем углу
        int x = getWidth() - margin - btnW;
        int y = margin;

        bypassButton.setBounds(x, y, btnW, btnH);
    }
    // 12) Располагаем CLIP-LED и PEAK-LED над кнопками Mute
    {
        // Общий лямбда-хелпер для LED-меток
        auto placeLed = [&](juce::Label& led,
            const juce::TextButton& btn,
            int extraX,
            int extraY)
            {
                auto b = btn.getBounds();
                int   ledW = b.getWidth();
                int   ledH = b.getHeight() / 2;   // высота LED — половина высоты кнопки
                int   ledX = b.getX() + extraX;
                int   ledY = b.getY() - ledH - extraY;

                led.setBounds(ledX, ledY, ledW, ledH);
            };

        // 12.2) Peak-LED (новый код)
        // Подбирайте extraY экспериментально, например 8–16px, чтобы LEDs не пересекались
        constexpr int peakOffsetY = 12;
        placeLed(clipLedL, muteButtonL, 0, peakOffsetY);
        placeLed(clipLedR, muteButtonR, 0, peakOffsetY);
    }

}
void OutControlComponent::processAudioBlock(juce::AudioBuffer<float>& buffer) noexcept
{
    // Защита от денормалей
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int numSamps = buffer.getNumSamples();

    // 1) Обновляем параметры EQ раз в dspUpdateInterval блоков
    if (++dspUpdateCounter >= dspUpdateInterval)
    {
        dspUpdateCounter = 0;
        if (eqParamsDirty.exchange(false, std::memory_order_acq_rel))
        {
            EqSettings s{
                atomicEqVals[0].load(),   // lowCutFreq
                lowShelfFreqVal.load(),    // lowShelfFreq
                atomicEqVals[1].load(),   // lowGain
                peakFreqVal.load(),        // peakFreq
                atomicEqVals[2].load(),   // midGain
                1.0f,                      // peakQ
                highShelfFreqVal.load(),   // highShelfFreq
                atomicEqVals[3].load(),   // highGain
                atomicEqVals[4].load()    // highCutFreq
            };
            eqDsp.updateSettings(s);
        }
    }

    // 2) Применяем EQ, если не байпасс
    if (eqBypassed.load(std::memory_order_relaxed))
        eqDsp.process(buffer);

    // 3) Считываем atomics для гейна и флагов
    float gL = gainValL.load(std::memory_order_relaxed);
    float gR = gainValR.load(std::memory_order_relaxed);
    bool  link = atomicLink.load(std::memory_order_relaxed);
    bool  muteL = atomicMuteL.load(std::memory_order_relaxed);
    bool  muteR = atomicMuteR.load(std::memory_order_relaxed);
    bool  soloL = atomicSoloL.load(std::memory_order_relaxed);
    bool  soloR = atomicSoloR.load(std::memory_order_relaxed);
    bool  anySolo = soloL || soloR;

    // 4) Логика link → solo/mute
    if (link)        gR = gL;
    if (anySolo)
    {
        if (!soloL) gL = 0.0f;
        if (!soloR) gR = 0.0f;
    }
    else
    {
        if (muteL)   gL = 0.0f;
        if (muteR)   gR = 0.0f;
    }

    // 5) Выбираем pre- или post-метр
    bool postMode = atomicMeterMode.load(std::memory_order_relaxed);

    // 6) Один проход: считаем пики и сразу применяем gain
    //    (работаем максимум по двум каналам)
    for (int ch = 0; ch < numCh && ch < 2; ++ch)
    {
        auto* ptr = buffer.getWritePointer(ch);
        float gain = (ch == 0 ? gL : gR);
        float preP = 0.0f;
        float postP = 0.0f;

        for (int i = 0; i < numSamps; ++i)
        {
            float in = ptr[i];
            preP = juce::jmax(preP, std::abs(in));
            float out = in * gain;
            ptr[i] = out;
            postP = juce::jmax(postP, std::abs(out));
        }

        if (ch == 0)
            masterMeterL.setLevel(postMode ? postP : preP);
        else
            masterMeterR.setLevel(postMode ? postP : preP);
        meterWasUpdated = true;
    }
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
    if (onMasterGainChanged)
    {
        float avgDb = 0.5f * (
            (float)gainSliderL.getValue()
            + (float)gainSliderR.getValue()
            );
        onMasterGainChanged(avgDb);
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

    if (onMasterGainChanged)
    {
        float avg = 0.5f * (newL + newR);
        onMasterGainChanged(avg);
    }
}


static const char* settingsFileName = "OutControlSettings.xml";

void OutControlComponent::saveSettings() const
{
    // создаём корневой XML-элемент
    auto xml = std::make_unique<juce::XmlElement>("OutControl");

    // 1) сохраняем EQ-параметры (атомики + combo-box’ы)
    xml->setAttribute("lowCutFreq", atomicEqVals[0].load());
    xml->setAttribute("lowShelfFreq", lowShelfFreqVal.load());
    xml->setAttribute("lowShelfGain", atomicEqVals[1].load());
    xml->setAttribute("peakFreq", peakFreqVal.load());
    xml->setAttribute("peakGain", atomicEqVals[2].load());
    xml->setAttribute("highShelfFreq", highShelfFreqVal.load());
    xml->setAttribute("highShelfGain", atomicEqVals[3].load());
    xml->setAttribute("highCutFreq", atomicEqVals[4].load());

    // 2) сохраняем UI-стейт выходных контролов
    //    slider.getValue() даёт dB-значение, а не линейный gain
    xml->setAttribute("gainL", gainSliderL.getValue());
    xml->setAttribute("gainR", gainSliderR.getValue());

    xml->setAttribute("muteL", muteButtonL.getToggleState());
    xml->setAttribute("muteR", muteButtonR.getToggleState());
    xml->setAttribute("soloL", soloButtonL.getToggleState());
    xml->setAttribute("soloR", soloButtonR.getToggleState());
    xml->setAttribute("link", linkButton.getToggleState());
    xml->setAttribute("eqBypassed", bypassButton.getToggleState());
    xml->setAttribute("meterModeButton", meterModeButton.getToggleState());

    // 3) пишем в файл
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

    // 1) Загружаем EQ-параметры и обновляем слайдеры/комбо:
    atomicEqVals[0].store((float)xml->getDoubleAttribute("lowCutFreq", atomicEqVals[0].load()), std::memory_order_relaxed);
    lowShelfFreqVal.store((float)xml->getDoubleAttribute("lowShelfFreq", lowShelfFreqVal.load()), std::memory_order_relaxed);
    atomicEqVals[1].store((float)xml->getDoubleAttribute("lowShelfGain", atomicEqVals[1].load()), std::memory_order_relaxed);
    peakFreqVal.store((float)xml->getDoubleAttribute("peakFreq", peakFreqVal.load()), std::memory_order_relaxed);
    atomicEqVals[2].store((float)xml->getDoubleAttribute("peakGain", atomicEqVals[2].load()), std::memory_order_relaxed);
    highShelfFreqVal.store((float)xml->getDoubleAttribute("highShelfFreq", highShelfFreqVal.load()), std::memory_order_relaxed);
    atomicEqVals[3].store((float)xml->getDoubleAttribute("highShelfGain", atomicEqVals[3].load()), std::memory_order_relaxed);
    atomicEqVals[4].store((float)xml->getDoubleAttribute("highCutFreq", atomicEqVals[4].load()), std::memory_order_relaxed);

    // Обновляем UI-слайдеры и Combobox'ы без триггера onChange:
    eqSliders[0].setValue(atomicEqVals[0].load(), juce::dontSendNotification);
    lowShelfFreqBox.setSelectedId(int(std::distance(lowShelfPresets.begin(),
        std::find(lowShelfPresets.begin(),
            lowShelfPresets.end(),
            lowShelfFreqVal.load()))) + 1,
        juce::dontSendNotification);

    eqSliders[1].setValue(atomicEqVals[1].load(), juce::dontSendNotification);
    peakFreqBox.setSelectedId(int(std::distance(peakPresets.begin(),
        std::find(peakPresets.begin(),
            peakPresets.end(),
            peakFreqVal.load()))) + 1,
        juce::dontSendNotification);

    eqSliders[2].setValue(atomicEqVals[2].load(), juce::dontSendNotification);
    highShelfFreqBox.setSelectedId(int(std::distance(highShelfPresets.begin(),
        std::find(highShelfPresets.begin(),
            highShelfPresets.end(),
            highShelfFreqVal.load()))) + 1,
        juce::dontSendNotification);

    eqSliders[3].setValue(atomicEqVals[3].load(), juce::dontSendNotification);
    eqSliders[4].setValue(atomicEqVals[4].load(), juce::dontSendNotification);

    // 2) Загружаем гейны (dB) — sliderValueChanged обновит линейный gainVal*
    gainSliderL.setValue(xml->getDoubleAttribute("gainL", gainSliderL.getValue()),
        juce::sendNotification);
    gainSliderR.setValue(xml->getDoubleAttribute("gainR", gainSliderR.getValue()),
        juce::sendNotification);

    // 3) Загружаем состояние кнопок без вызова onClick
    muteButtonL.setToggleState(xml->getBoolAttribute("muteL", muteButtonL.getToggleState()), juce::dontSendNotification);
    soloButtonL.setToggleState(xml->getBoolAttribute("soloL", soloButtonL.getToggleState()), juce::dontSendNotification);
    muteButtonR.setToggleState(xml->getBoolAttribute("muteR", muteButtonR.getToggleState()), juce::dontSendNotification);
    soloButtonR.setToggleState(xml->getBoolAttribute("soloR", soloButtonR.getToggleState()), juce::dontSendNotification);
    linkButton.setToggleState(xml->getBoolAttribute("link", linkButton.getToggleState()), juce::dontSendNotification);
    bypassButton.setToggleState(xml->getBoolAttribute("eqBypassed", bypassButton.getToggleState()), juce::dontSendNotification);
    meterModeButton.setToggleState(xml->getBoolAttribute("meterModeButton", bypassButton.getToggleState()), juce::dontSendNotification);

    // Синхронизируем все кнопки с атомиками
    syncButtonStatesToAtomics();

    // 4) Помечаем EQ как «грязный», чтобы updateSettings() сработал сразу
    eqParamsDirty.store(true, std::memory_order_relaxed);
    // и сбрасываем счётчик, чтобы обновить EQ уже в следующем блоке
    dspUpdateCounter = dspUpdateInterval;
}




