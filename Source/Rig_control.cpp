#include "Rig_control.h"

//==============================================================================
Rig_control::Rig_control()
{
    // 1. Создаём контейнер для элементов интерфейса
    mainTab = std::make_unique<juce::Component>();
    addAndMakeVisible(mainTab.get());

    // Устанавливаем начальный размер, если компонент используется автономно
    setSize(800, 600);

    // 2. Создаём 3 кнопки-пресета (например, для групп A, B, C)
    for (int i = 0; i < 3; ++i)
    {
        auto* preset = new juce::TextButton("Preset " + juce::String(i + 1));
        preset->setClickingTogglesState(true);
        preset->setRadioGroupId(100, juce::dontSendNotification);
        preset->setToggleState(false, juce::dontSendNotification);
        preset->setColour(juce::TextButton::buttonOnColourId, juce::Colours::blue);
        preset->addListener(this);
        presetButtons.add(preset);
        mainTab->addAndMakeVisible(preset);
    }

    // 3. Добавляем метку BANK NAME
    bankNameLabel.setText("BANK NAME", juce::dontSendNotification);
    bankNameLabel.setJustificationType(juce::Justification::centred);
    mainTab->addAndMakeVisible(bankNameLabel);

    // 4. Создаём кнопки SHIFT, TEMPO, UP и DOWN
    shiftButton = std::make_unique<juce::TextButton>("SHIFT");
    shiftButton->setClickingTogglesState(true);
    shiftButton->setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange);
    shiftButton->addListener(this);
    mainTab->addAndMakeVisible(shiftButton.get());

    tempoButton = std::make_unique<juce::TextButton>("TEMPO");
    tempoButton->setClickingTogglesState(false);
    tempoButton->setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange);
    tempoButton->addListener(this);
    mainTab->addAndMakeVisible(tempoButton.get());

    upButton = std::make_unique<juce::TextButton>("UP");
    upButton->setClickingTogglesState(true);
    upButton->setColour(juce::TextButton::buttonOnColourId, juce::Colours::green);
    upButton->addListener(this);
    mainTab->addAndMakeVisible(upButton.get());

    downButton = std::make_unique<juce::TextButton>("DOWN");
    downButton->setClickingTogglesState(true);
    downButton->setColour(juce::TextButton::buttonOnColourId, juce::Colours::yellow);
    downButton->addListener(this);
    mainTab->addAndMakeVisible(downButton.get());

    // 5. Создаём Rotary‑слайдер для Gain и его метку
    gainSlider = std::make_unique<juce::Slider>("Gain Slider");
    gainSlider->setSliderStyle(juce::Slider::Rotary);
    gainSlider->setRange(0, 127, 1);
    gainSlider->setValue(64);
    gainSlider->addListener(this);
    gainSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    mainTab->addAndMakeVisible(gainSlider.get());

    gainLabel.setText("Gain", juce::dontSendNotification);
    gainLabel.setJustificationType(juce::Justification::centred);
    mainTab->addAndMakeVisible(gainLabel);

    // 6. Создаём Rotary‑слайдер для Volume и его метку
    volumeSlider = std::make_unique<juce::Slider>("Volume Slider");
    volumeSlider->setSliderStyle(juce::Slider::Rotary);
    volumeSlider->setRange(0, 127, 1);
    volumeSlider->setValue(64);
    volumeSlider->addListener(this);
    volumeSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    mainTab->addAndMakeVisible(volumeSlider.get());

    volumeLabel.setText("Volume", juce::dontSendNotification);
    volumeLabel.setJustificationType(juce::Justification::centred);
    mainTab->addAndMakeVisible(volumeLabel);
}

Rig_control::~Rig_control()
{
    // Удаляем слушателей у всех компонентов
    for (auto* btn : presetButtons)
        btn->removeListener(this);
    if (shiftButton) shiftButton->removeListener(this);
    if (tempoButton) tempoButton->removeListener(this);
    if (upButton) upButton->removeListener(this);
    if (downButton) downButton->removeListener(this);
}

void Rig_control::resized()
{
    // Устанавливаем mainTab на всю область компонента
    mainTab->setBounds(getLocalBounds());

    const int margin = 10;
    auto content = mainTab->getLocalBounds().reduced(margin);
    const int numCols = 9, numRows = 4;
    int usableWidth = content.getWidth();
    int sectorWidth = usableWidth / numCols;
    int extra = usableWidth - (sectorWidth * numCols);
    int sectorHeight = content.getHeight() / numRows;

    // Лямбда для вычисления прямоугольника одного сектора (с номерами, начинающимися с 1)
    auto getSectorRect = [=](int sectorNumber) -> juce::Rectangle<int>
        {
            int idx = sectorNumber - 1;
            int row = idx / numCols;
            int col = idx % numCols;
            int x = content.getX();
            for (int c = 0; c < col; ++c)
                x += sectorWidth + (c < extra ? 1 : 0);
            int w = sectorWidth + (col < extra ? 1 : 0);
            int y = content.getY() + row * sectorHeight;
            return juce::Rectangle<int>(x, y, w, sectorHeight);
        };

    // Лямбда для объединения горизонтальных секторов (от startSector до endSector)
    auto getUnionRect = [=](int startSector, int endSector) -> juce::Rectangle<int>
        {
            auto r1 = getSectorRect(startSector);
            auto r2 = getSectorRect(endSector);
            int x = r1.getX(), y = r1.getY(), width = r2.getRight() - x;
            return juce::Rectangle<int>(x, y, width, r1.getHeight());
        };

    // Раскладка слайдера Gain и его метки в секторе 1
    auto gainSector = getSectorRect(1).reduced(4);
    if (gainSlider)
        gainSlider->setBounds(gainSector);
    int gainLabelWidth = gainSector.getWidth() / 2, gainLabelHeight = 40;
    juce::Rectangle<int> gainLabelBounds(gainLabelWidth, gainLabelHeight);
    gainLabelBounds.setCentre(gainSector.getCentre());
    gainLabel.setBounds(gainLabelBounds);
    gainLabel.setFont(juce::Font(gainSector.getHeight() * 0.20f, juce::Font::bold));

    // Раскладка слайдера Volume и его метки в секторе 9
    auto volumeSector = getSectorRect(9).reduced(4);
    if (volumeSlider)
        volumeSlider->setBounds(volumeSector);
    int volumeLabelW = volumeSector.getWidth() * 2, volumeLabelH = 40;
    juce::Rectangle<int> volumeLabelRect(volumeLabelW, volumeLabelH);
    volumeLabelRect.setCentre(volumeSector.getCentre());
    volumeLabel.setBounds(volumeLabelRect);
    volumeLabel.setFont(juce::Font(volumeSector.getHeight() * 0.17f, juce::Font::bold));

    // Вспомогательная лямбда для установки размеров кнопок и назначения им общего кастомного LookAndFeel
    auto layoutButton = [this, &getSectorRect](std::unique_ptr<juce::TextButton>& btn, int sector)
        {
            if (btn)
            {
                btn->setBounds(getSectorRect(sector).reduced(4));
                btn->setLookAndFeel(&customLF);
            }
        };

    layoutButton(upButton, 10);
    layoutButton(tempoButton, 18);
    layoutButton(downButton, 19);
    layoutButton(shiftButton, 27);

    // Раскладка кнопок-пресетов (объединяем сектора)
    if (presetButtons.size() > 0)
        presetButtons[0]->setBounds(getUnionRect(28, 30).reduced(4));
    if (presetButtons.size() > 1)
        presetButtons[1]->setBounds(getUnionRect(31, 33).reduced(4));
    if (presetButtons.size() > 2)
        presetButtons[2]->setBounds(getUnionRect(34, 36).reduced(4));

    // Раскладка метки BANK NAME (объединяем сектора 2–8)
    auto bankRect = getUnionRect(2, 8).reduced(4);
    bankNameLabel.setBounds(bankRect);
    bankNameLabel.setFont(juce::Font(bankRect.getHeight() * 0.7f, juce::Font::bold));
}

void Rig_control::buttonClicked(juce::Button* button)
{
    // ЗДЕСЬ разместите логику обработки нажатий кнопок.
}

void Rig_control::sliderValueChanged(juce::Slider* slider)
{
    // ЗДЕСЬ разместите логику обработки изменений слайдеров.
}

void Rig_control::handleIncomingMidiMessage(juce::MidiInput* /*source*/, const juce::MidiMessage& message)
{
    // Обработка входящих MIDI-сообщений (при необходимости можно использовать callAsync).
}

void Rig_control::timerCallback()
{
    // Реализация периодических обновлений (если нужно).
}
