#include "OutControlComponent.h"
#include <algorithm>  // для std::max, std::abs

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
                s.setColour(juce::Slider::thumbColourId, juce::Colours::blue);
                
            }
            else if (i == 4) // High-Cut
            {
                s.setRange(4000.0, 20000.0, 100.0); // 4…20 кГц
                s.setValue(16000.0);                // LP @16 кГц
                s.setTextValueSuffix(" Hz");
                eqSliders[i].setDoubleClickReturnValue(true, 16000.0);
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

    // 3) Gain L/R + подписи
    addAndMakeVisible(gainSliderL);
    gainSliderL.setSliderStyle(juce::Slider::LinearVertical);
    gainSliderL.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    gainSliderL.setDoubleClickReturnValue(true, 1.0);
    addAndMakeVisible(gainLabelL);
    gainLabelL.setText("Gain L", juce::dontSendNotification);
    gainLabelL.setJustificationType(juce::Justification::centred);
    gainLabelL.setLookAndFeel(&customLF);
    gainLabelL.setColour(juce::Label::backgroundColourId, juce::Colours::white);
    gainLabelL.setColour(juce::Label::textColourId, juce::Colours::black);
    gainLabelL.setOpaque(true);

    addAndMakeVisible(gainSliderR);
    gainSliderR.setSliderStyle(juce::Slider::LinearVertical);
    gainSliderR.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    gainSliderR.setDoubleClickReturnValue(true, 1.0);
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

    //––– Мастер-метры –––
    addAndMakeVisible(masterMeterL);
    addAndMakeVisible(masterMeterR);
    masterMeterL.setScaleMarks({ -60, -20, -12, -6, -3, 0 });
    masterMeterR.setScaleMarks({ -60, -20, -12, -6, -3, 0 });
    addAndMakeVisible(masterScale);
    masterScale.setScaleMarks({ -20, -12, -6, -3, 0 });

    /// Mute/Solo для левого канала
    addAndMakeVisible(muteButtonL);
    muteButtonL.setButtonText("M");
    muteButtonL.setColour(juce::TextButton::textColourOffId, juce::Colours::red);

    addAndMakeVisible(soloButtonL);
    soloButtonL.setButtonText("S");
    soloButtonL.setColour(juce::TextButton::textColourOffId, juce::Colours::yellow);

    // Mute/Solo для правого канала
    addAndMakeVisible(muteButtonR);
    muteButtonR.setButtonText("M");
    muteButtonR.setColour(juce::TextButton::textColourOffId, juce::Colours::red);

    addAndMakeVisible(soloButtonR);
    soloButtonR.setButtonText("S");
    soloButtonR.setColour(juce::TextButton::textColourOffId, juce::Colours::yellow);

    // Link L/R обычная кнопка
    addAndMakeVisible(linkButton);
    linkButton.setButtonText("L/R");
    linkButton.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    // включаем фиксацию состояния
    linkButton.setClickingTogglesState(true);

    // реакция на движение левого слайдера
    gainSliderL.onValueChange = [this]
        {
            // если линк включён и мы не в процессе зеркалирования
            if (linkButton.getToggleState() && !isLinking)
            {
                isLinking = true;
                // дублируем значение, но без уведомлений, чтобы не зайти в бесконечный цикл
                gainSliderR.setValue(gainSliderL.getValue(), juce::dontSendNotification);
                isLinking = false;
            }
        };

    // аналогично для правого
    gainSliderR.onValueChange = [this]
        {
            if (linkButton.getToggleState() && !isLinking)
            {
                isLinking = true;
                gainSliderL.setValue(gainSliderR.getValue(), juce::dontSendNotification);
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

    setupToggleButton(muteButtonL, juce::Colour(0xFF660000), juce::Colours::red, juce::Colours::black);
    setupToggleButton(soloButtonL, juce::Colour(0x66666600), juce::Colours::yellow, juce::Colours::black);
    setupToggleButton(muteButtonR, juce::Colour(0xFF660000), juce::Colours::red, juce::Colours::black);
    setupToggleButton(soloButtonR, juce::Colour(0x66666600), juce::Colours::yellow, juce::Colours::black);
    setupToggleButton(linkButton, juce::Colour(0xFF333333), juce::Colour(0xFF888888), juce::Colours::black);

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
}
void OutControlComponent::resized()
{
    Grid grid(getLocalBounds(), 5, 4);
    // 1) Базовые константы
    int   cellH = grid.getSector(0, 0).getHeight();   // высота ячейки
    int   lblH = cellH / 4;                           // высота области под подпись
    float fontPct = 0.6f;                                // масштаб шрифта для ComboBox
    int   offsetY = lblH;                                // слайдеры сдвинуть вниз, чтобы подписи не наслаивались
    float shrink = 0.10f;                               // сжать слайдеры на 10% по краям
    int   marginX = 8;                                   // горизонтальный padding внутри слайдера для ComboBox
    int   pad = 4;                                   // внутренний padding (можно будет использовать для Label)
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
            .withY(sector.getY()+30)
            .reduced(marginX, 0);

        eqLabels[col].setBounds(lbArea);
        eqLabels[col].setFont({ lblH * fontPct, juce::Font::bold });
    }
    // 4) Лямбда для размещения ComboBox над лейблами row = 1
    auto placeComboOnLabel = [&](juce::ComboBox& cb, int col,
        float widthPct = 0.5f,
        float heightPct = 0.6f,
        int   liftPx = 10)   // liftPx = пикселей вверх
        {
            // 1) Та же область, что и для лейбла:
            auto sector = grid.getSector(1, col).translated(0, offsetY);
            auto lbArea = sector
                .withHeight(lblH)
                .withY(sector.getY()-30)
                .reduced(marginX, 0);

            // 2) Размеры ComboBox
            int comboW = int(lbArea.getWidth() * widthPct);
            int comboH = int(lblH * heightPct);

            // 3) Центрируем по X и Y над меткой
            auto comboArea = lbArea.withSizeKeepingCentre(comboW, comboH);

            // 4) Поднимаем вверх на liftPx пикселей
            comboArea = comboArea.translated(0, liftPx);

            cb.setBounds(comboArea);
            cb.setJustificationType(juce::Justification::centred);
        };

    // 5) Вызываем для ваших трёх списков
    //    Подроскачивайте liftPx (последний аргумент), пока не получите нужное расположение
    placeComboOnLabel(lowShelfFreqBox, 1, 0.5f, 0.6f, 8);
    placeComboOnLabel(peakFreqBox, 2, 0.5f, 0.6f, 8);
    placeComboOnLabel(highShelfFreqBox, 3, 0.5f, 0.6f, 8);

    // 3) «MASTER EQ» header (rows=0→1, cols=1…3)
    {
        auto hdr = grid.getUnion(0, 1, 3)
            .reduced(4)
            .withHeight(lblH);

        masterEqLabel.setBounds(hdr);
        masterEqLabel.setFont({ lblH * fontPct, juce::Font::bold });
    }

    // заготовки для гейнов
    juce::Rectangle<int> gainBoundsL, gainBoundsR;

    // 4) Gain L (cols=1, rows=2→3) — +10% высоты, над меткой
    {
        auto parent = grid.getSector(2, 1)
            .getUnion(grid.getSector(3, 1))
            .reduced(6);

        // Y верхней границы метки
        int labelTopY = parent.getBottom() - lblH;

        // оригинальная высота без метки
        int origH = parent.getHeight() - lblH;
        // новая высота +10%
        int newH = int(origH * 1.1f);
        // привязка низа к labelTopY
        int newY = labelTopY - newH;

        gainBoundsL = { parent.getX(), newY,
                          parent.getWidth(), newH };

        gainSliderL.setBounds(gainBoundsL);

        // метка снизу
        auto lb = parent.withHeight(lblH)
            .withY(parent.getBottom() - lblH);

        gainLabelL.setBounds(lb);
        gainLabelL.setFont({ lblH * fontPct, juce::Font::bold });
    }

    // 5) Gain R (cols=3, rows=2→3) — аналогично
    {
        auto parent = grid.getSector(2, 3)
            .getUnion(grid.getSector(3, 3))
            .reduced(6);

        int labelTopY = parent.getBottom() - lblH;
        int origH = parent.getHeight() - lblH;
        int newH = int(origH * 1.1f);
        int newY = labelTopY - newH;

        gainBoundsR = { parent.getX(), newY,
                          parent.getWidth(), newH };

        gainSliderR.setBounds(gainBoundsR);

        auto lb = parent.withHeight(lblH)
            .withY(parent.getBottom() - lblH);

        gainLabelR.setBounds(lb);
        gainLabelR.setFont({ lblH * fontPct, juce::Font::bold });
    }

    // 6) Master-метры + шкала + подпись MASTER OUT (row=3, col=2)
    {
        auto cell = grid.getSector(3, 2).reduced(4);
        int  pad = 4;
        int  lblH = grid.getSector(0, 0).getHeight() / 4;

        // Высота как у Gain L
        int  meterH = gainBoundsL.getHeight();

        // Доступная ширина внутри cell
        int  availW = cell.getWidth() - pad * 2;

        // по 1/4 ширины на каждый метр, 1/2 — на шкалу
        int  meterW = availW / 4;
        int  scaleW = availW - meterW * 2;

        // стартовые координаты
        int  startX = cell.getX() + pad;
        int  startY = cell.getBottom() - lblH - meterH;  // над меткой

        masterMeterL.setBounds(startX, startY, meterW, meterH);
        masterScale.setBounds(startX + meterW, startY, scaleW, meterH);
        masterMeterR.setBounds(startX + meterW + scaleW,
            startY, meterW, meterH);

        // подпись MASTER OUT
        auto outArea = cell.withHeight(lblH)
            .withY(cell.getBottom() - lblH);
        masterOutLabel.setBounds(outArea);
        masterOutLabel.setFont({ lblH * 0.6f, juce::Font::bold });
    }
    // 7) Mute/Solo — просто gap между меткой и кнопками
    {
        auto secL = grid.getSector(3, 1);
        auto secR = grid.getSector(3, 3);
        int  btnW = secL.getWidth() / 4;
        int  btnH = secL.getHeight() / 4;
        int  offsetX = btnW;         // горизонтальное смещение от центра к фейдеру/метру
        int  padY = 12;           // ↑ gap между меткой и Solo
        int  msGap = 8;            // ↑ дополнительный gap между Mute и Solo

        // L-канал
        {
            int centreX = (gainBoundsL.getRight() + masterMeterL.getX()) / 2;
            int xL = centreX - btnW / 2 - offsetX;
            int labelTopY = secL.getBottom() - lblH;

            // Solo на padY над меткой, Mute — на msGap над Solo
            int ySolo = labelTopY - padY - btnH;
            int yMute = ySolo - btnH - msGap;

            muteButtonL.setBounds(xL, yMute, btnW, btnH);
            soloButtonL.setBounds(xL, ySolo, btnW, btnH);
        }

        // R-канал
        {
            int centreX = (masterMeterR.getRight() + gainBoundsR.getX()) / 2;
            int xR = centreX - btnW / 2 + offsetX;
            int labelTopY = secR.getBottom() - lblH;

            int ySolo = labelTopY - padY - btnH;
            int yMute = ySolo - btnH - msGap;

            muteButtonR.setBounds(xR, yMute, btnW, btnH);
            soloButtonR.setBounds(xR, ySolo, btnW, btnH);
        }
    }
    // 8) Link-кнопка и новая Bypass EQ-кнопка
    {
        auto sec = grid.getSector(3, 2);

        // размеры кнопок
        int btnW = sec.getWidth() / 4;
        int btnH = sec.getHeight() / 4;

        // отступ от подписи до linkButton
        int gapToLink = 12;
        // отступ между linkButton и bypassButton
        int gapBetween = 8;   // <-- играйте этим значением

        int x = sec.getCentreX() - btnW / 2;
        int labelTopY = sec.getBottom() - lblH;

        // рассчитываем Y для linkButton
        int yLink = labelTopY - gapToLink - btnH;
        linkButton.setBounds(x, yLink, btnW, btnH);

        // рассчитываем Y для bypassButton
        int yByp = yLink - gapBetween - btnH;
        bypassButton.setBounds(x, yByp, btnW, btnH);

   }


}
void OutControlComponent::processAudioBlock(juce::AudioBuffer<float>& buffer) noexcept
{   
    const int numCh = buffer.getNumChannels();
    const int numSamps = buffer.getNumSamples();
    // 1) Считываем атомарные EQ-параметры
    const float lowCutFreq = atomicEqVals[0].load();
    const float lowGain = atomicEqVals[1].load();
    const float midGain = atomicEqVals[2].load();
    const float highGain = atomicEqVals[3].load();
    const float highCutFreq = atomicEqVals[4].load();

    // 2) Обновляем коэффициенты только если параметры изменились
    if (eqParamsDirty.exchange(false))
    {
        // формируем структуру настроек
        EqSettings settings
        {
             lowCutFreq,
             lowShelfFreqVal.load(),
             lowGain,
             peakFreqVal.load(),
             midGain,
            /* peakQ*/ 1.0f,
             highShelfFreqVal.load(),
             highGain,
             highCutFreq
                     
        };
        // передаём их в DSP
        eqDsp.updateSettings(settings);
    }
    // 3) Прогоним через EQ
    if (!eqBypassed.load(std::memory_order_relaxed))
    {
        eqDsp.process(buffer);
    }
    // 1) Считываем состояния Solo/Mute/Link
    bool soloL = soloButtonL.getToggleState();
    bool soloR = soloButtonR.getToggleState();
    bool anySolo = soloL || soloR;
    bool linkOn = linkButton.getToggleState();

    // 2) Считываем gain’ы
    float gainL = (float)gainSliderL.getValue();
    float gainR = linkOn ? gainL : (float)gainSliderR.getValue();

    // 3) Применяем gain + Solo/Mute
    for (int ch = 0; ch < numCh; ++ch)
    {
        // базовый гейн для канала
        float g = (ch == 0 ? gainL : gainR);

        if (anySolo)
        {
            // если хоть одно Solo включено, глушим не-solo каналы
            bool thisSolo = (ch == 0 ? soloL : soloR);
            g = thisSolo ? g : 0.0f;
        }
        else
        {
            // иначе глушим только помеченные Mute
            bool muted = (ch == 0 ? muteButtonL.getToggleState()
                : muteButtonR.getToggleState());
            if (muted)
                g = 0.0f;
        }

        buffer.applyGain(ch, 0, numSamps, g);
    }

    // 4) Считаем пиковый уровень и обновляем метрчики
    if (numCh > 0)
    {
        auto* data = buffer.getReadPointer(0);
        float peak = 0.0f;
        for (int i = 0; i < numSamps; ++i)
            peak = std::max(peak, std::abs(data[i]));
        masterMeterL.setLevel(peak);
    }

    if (numCh > 1)
    {
        auto* data = buffer.getReadPointer(1);
        float peak = 0.0f;
        for (int i = 0; i < numSamps; ++i)
            peak = std::max(peak, std::abs(data[i]));
        masterMeterR.setLevel(peak);
    }
}
//==============================================================================
// OutControlComponent.cpp
//==============================================================================

static const char* settingsFileName = "OutControlSettings.xml";

void OutControlComponent::saveSettings() const
{
    // создать XML-дерево
    auto xml = std::make_unique<juce::XmlElement>("OutControl");

    // ————————————————————————————————
    // 1) сохраняем параметры эквалайзера
    xml->setAttribute("lowCutFreq", atomicEqVals[0].load());          // слайдер LowCut
    xml->setAttribute("lowShelfFreq", lowShelfFreqVal.load());          // ComboBox LowShelf Freq
    xml->setAttribute("lowShelfGain", atomicEqVals[1].load());          // слайдер LowShelf Gain

    xml->setAttribute("peakFreq", peakFreqVal.load());              // ComboBox Peak Freq
    xml->setAttribute("peakGain", atomicEqVals[2].load());          // слайдер Peak Gain

    xml->setAttribute("highShelfFreq", highShelfFreqVal.load());         // ComboBox HighShelf Freq
    xml->setAttribute("highShelfGain", atomicEqVals[3].load());          // слайдер HighShelf Gain
    xml->setAttribute("highCutFreq", atomicEqVals[4].load());          // слайдер HighCut

    // ————————————————————————————————
    // 2) сохраняем прочий UI-стейт
    xml->setAttribute("gainL", gainSliderL.getValue());
    xml->setAttribute("gainR", gainSliderR.getValue());
    xml->setAttribute("muteL", muteButtonL.getToggleState());
    xml->setAttribute("muteR", muteButtonR.getToggleState());
    xml->setAttribute("soloL", soloButtonL.getToggleState());
    xml->setAttribute("soloR", soloButtonR.getToggleState());
    xml->setAttribute("link", linkButton.getToggleState());
    xml->setAttribute("eqBypassed", bypassButton.getToggleState());

    // ————————————————————————————————
    // 3) записать в файл
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

    // ————————————————————————————————
    // 1) загрузка параметров эквалайзера
    atomicEqVals[0].store((float)xml->getDoubleAttribute("lowCutFreq", atomicEqVals[0].load()), std::memory_order_relaxed);
    lowShelfFreqVal.store((float)xml->getDoubleAttribute("lowShelfFreq", lowShelfFreqVal.load()), std::memory_order_relaxed);
    atomicEqVals[1].store((float)xml->getDoubleAttribute("lowShelfGain", atomicEqVals[1].load()), std::memory_order_relaxed);

    peakFreqVal.store((float)xml->getDoubleAttribute("peakFreq", peakFreqVal.load()), std::memory_order_relaxed);
    atomicEqVals[2].store((float)xml->getDoubleAttribute("peakGain", atomicEqVals[2].load()), std::memory_order_relaxed);

    highShelfFreqVal.store((float)xml->getDoubleAttribute("highShelfFreq", highShelfFreqVal.load()), std::memory_order_relaxed);
    atomicEqVals[3].store((float)xml->getDoubleAttribute("highShelfGain", atomicEqVals[3].load()), std::memory_order_relaxed);
    atomicEqVals[4].store((float)xml->getDoubleAttribute("highCutFreq", atomicEqVals[4].load()), std::memory_order_relaxed);

    // Обновляем слайдеры и ComboBox так, чтобы UI отобразил загруженные значения:
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

    // ————————————————————————————————
    // 2) загрузка остального UI-стейта
    gainSliderL.setValue(xml->getDoubleAttribute("gainL", gainSliderL.getValue()),
        juce::dontSendNotification);
    gainSliderR.setValue(xml->getDoubleAttribute("gainR", gainSliderR.getValue()),
        juce::dontSendNotification);

    muteButtonL.setToggleState(xml->getBoolAttribute("muteL", muteButtonL.getToggleState()),
        juce::dontSendNotification);
    muteButtonR.setToggleState(xml->getBoolAttribute("muteR", muteButtonR.getToggleState()),
        juce::dontSendNotification);

    soloButtonL.setToggleState(xml->getBoolAttribute("soloL", soloButtonL.getToggleState()),
        juce::dontSendNotification);
    soloButtonR.setToggleState(xml->getBoolAttribute("soloR", soloButtonR.getToggleState()),
        juce::dontSendNotification);

    linkButton.setToggleState(xml->getBoolAttribute("link", linkButton.getToggleState()),
        juce::dontSendNotification);
    bypassButton.setToggleState(xml->getBoolAttribute("eqBypassed", bypassButton.getToggleState()),
        juce::dontSendNotification);
}








