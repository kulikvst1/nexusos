#include "OutControlComponent.h"
#include <algorithm>  // для std::max, std::abs

void OutControlComponent::prepare(double, int) noexcept {}

OutControlComponent::OutControlComponent()
{
    setLookAndFeel(&customLF);

    // 1) rotary-EQ + их подписи
    static const char* names[5] = { "Low-Cut", "Low", "Mid", "High", "High-Cut" };
    for (int i = 0; i < 5; ++i)
    {
        addAndMakeVisible(eqSliders[i]);
        eqSliders[i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        eqSliders[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);

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
    linkButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
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
    loadSettings();
}
OutControlComponent::~OutControlComponent()
{
    // 1) Отключаем наш кастомный LookAndFeel у всех дочерних компонентов,
    //    чтобы они сняли слабые ссылки на него.
    for (auto* child : getChildren())
        child->setLookAndFeel(nullptr);

    // 2) Отключаем его у самого контейнера
    setLookAndFeel(nullptr);
    saveSettings();

}
void OutControlComponent::resized()
{

    Grid grid(getLocalBounds(), 5, 4);

    // базовые размеры
    int   cellH = grid.getSector(0, 0).getHeight();  // высота одной ячейки
    int   lblH = cellH / 4;                          // 1/4 под подписи снизу
    float fontPct = 0.6f;                               // масштаб шрифта
    int   offsetY = lblH;                               // смещение EQ вниз
    float shrink = 0.10f;                              // 10% сжатие EQ
    int   marginX = 8;                                  // padding для EQ-лейблов
    int   pad = 4;                                  // внутренний padding в мастере

    // 1) Rotary-EQ (row=0)
    for (int col = 0; col < 5; ++col)
    {
        auto c = grid.getSector(0, col)
            .reduced(int(cellH * shrink),
                int(cellH * shrink))
            .translated(0, offsetY);

        eqSliders[col].setBounds(c);
    }

    // 2) EQ-лейблы (row=1)
    for (int col = 0; col < 5; ++col)
    {
        auto c = grid.getSector(1, col).translated(0, offsetY);
        auto lb = c.withHeight(lblH)
            .withY(c.getY())
            .reduced(marginX, 0);

        eqLabels[col].setBounds(lb);
        eqLabels[col].setFont({ lblH * fontPct, juce::Font::bold });
    }

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

    // 8) Link-кнопка — повышенный gap над «MASTER OUT»
    {
        auto sec = grid.getSector(3, 2);
        int  btnW = sec.getWidth() / 4;
        int  btnH = sec.getHeight() / 4;
        int  linkGapY = 12;                             // ↑ gap между меткой и Link
        int  x = sec.getCentreX() - btnW / 2;
        int  labelTop = sec.getBottom() - lblH;
        int  y = labelTop - linkGapY - btnH;

        linkButton.setBounds(x, y, btnW, btnH);
    }

}
void OutControlComponent::processAudioBlock(juce::AudioBuffer<float>& buffer) noexcept
{
    DBG("[OutControl] entered processAudioBlock, numSamples=" << buffer.getNumSamples());
    buffer.applyGain(0.5f);
    DBG("[OutControl] applied gain, firstSample=" << buffer.getSample(0, 0));

    const int numCh = buffer.getNumChannels();
    const int numSamps = buffer.getNumSamples();

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
static const char* settingsFileName = "OutControlSettings.xml";

void OutControlComponent::loadSettings()
{
    auto file = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile(settingsFileName);

    std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(file));
    if (xml && xml->hasTagName("OutControl"))
    {
        // читаем атрибуты, указывая defaults
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
    }
}

void OutControlComponent::saveSettings() const
{
    auto xml = std::make_unique<juce::XmlElement>("OutControl");

    // сохраняем актуальные значения
    xml->setAttribute("gainL", gainSliderL.getValue());
    xml->setAttribute("gainR", gainSliderR.getValue());

    xml->setAttribute("muteL", muteButtonL.getToggleState());
    xml->setAttribute("muteR", muteButtonR.getToggleState());

    xml->setAttribute("soloL", soloButtonL.getToggleState());
    xml->setAttribute("soloR", soloButtonR.getToggleState());

    xml->setAttribute("link", linkButton.getToggleState());

    // записываем в файл
    auto file = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile(settingsFileName);

    xml->writeTo(file);
}






