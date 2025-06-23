#include "Rig_control.h"
#include "bank_editor.h"
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
    
// Если вы создаёте 3 кнопки-пресета, то добавляем метки для них:
    presetLabel1_4.setText("preset1.4", juce::dontSendNotification);
    presetLabel1_4.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(presetLabel1_4);

    presetLabel2_5.setText("preset2.5", juce::dontSendNotification);
    presetLabel2_5.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(presetLabel2_5);

    presetLabel3_6.setText("preset3.6", juce::dontSendNotification);
    presetLabel3_6.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(presetLabel3_6);


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

    // Лямбда для вычисления прямоугольника одного сектора (номера начинаются с 1)
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

    // Раскладка остальных кнопок с кастомным LookAndFeel
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
    juce::Rectangle<int> preset1Bounds, preset2Bounds, preset3Bounds;
    if (presetButtons.size() > 0)
    {
        preset1Bounds = getUnionRect(28, 30).reduced(4);
        presetButtons[0]->setBounds(preset1Bounds);
    }
    if (presetButtons.size() > 1)
    {
        preset2Bounds = getUnionRect(31, 33).reduced(4);
        presetButtons[1]->setBounds(preset2Bounds);
    }
    if (presetButtons.size() > 2)
    {
        preset3Bounds = getUnionRect(34, 36).reduced(4);
        presetButtons[2]->setBounds(preset3Bounds);
    }

    // Раскладка метки BANK NAME (объединяем сектора 2–8)
    auto bankRect = getUnionRect(2, 8).reduced(4);
    bankNameLabel.setBounds(bankRect);
    bankNameLabel.setFont(juce::Font(bankRect.getHeight() * 0.7f, juce::Font::bold));

    // --- Раскладка новых меток для пресетов ---
    if (presetButtons.size() > 0)
    {
        int labelW = preset1Bounds.getWidth() / 1.5;
        int labelH = preset1Bounds.getHeight() / 4;
        juce::Rectangle<int> labelRect1(
            preset1Bounds.getRight() - labelW,   // сдвиг от правого края
            preset1Bounds.getBottom() - labelH,    // сдвиг от нижнего края
            labelW,
            labelH
        );
        presetLabel1_4.setBounds(labelRect1);
        presetLabel1_4.setColour(juce::Label::textColourId, juce::Colours::grey);
        presetLabel1_4.setColour(juce::Label::backgroundColourId, juce::Colours::lightgrey);
        presetLabel1_4.setOpaque(true);
        presetLabel1_4.setJustificationType(juce::Justification::centred);
    }
    if (presetButtons.size() > 1)
    {
        int labelW = preset2Bounds.getWidth() / 1.5;
        int labelH = preset2Bounds.getHeight() / 4;
        juce::Rectangle<int> labelRect2(
            preset2Bounds.getRight() - labelW,
            preset2Bounds.getBottom() - labelH,
            labelW,
            labelH
        );
        presetLabel2_5.setBounds(labelRect2);
        presetLabel2_5.setColour(juce::Label::textColourId, juce::Colours::grey);
        presetLabel2_5.setColour(juce::Label::backgroundColourId, juce::Colours::lightgrey);
        presetLabel2_5.setOpaque(true);
        presetLabel2_5.setJustificationType(juce::Justification::centred);
    }
    if (presetButtons.size() > 2)
    {
        int labelW = preset3Bounds.getWidth() / 1.5;
        int labelH = preset3Bounds.getHeight() / 4;
        juce::Rectangle<int> labelRect3(
            preset3Bounds.getRight() - labelW,
            preset3Bounds.getBottom() - labelH,
            labelW,
            labelH
        );
        presetLabel3_6.setBounds(labelRect3);
        presetLabel3_6.setColour(juce::Label::textColourId, juce::Colours::grey);
        presetLabel3_6.setColour(juce::Label::backgroundColourId, juce::Colours::lightgrey);
        presetLabel3_6.setOpaque(true);
        presetLabel3_6.setJustificationType(juce::Justification::centred);
    }

}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Новый метод для установки BankEditor и подписки на его изменения:
void Rig_control::setBankEditor(BankEditor* editor)
{
    bankEditor = editor;

    if (bankEditor != nullptr)
    {
        // Подписываемся на обратный вызов, когда изменяются данные в BankEditor,
        // чтобы обновлять отображение пресетов в Rig_control.
        bankEditor->onBankEditorChanged = [this]()
            {
                updatePresetDisplays();
            };

        // Подписываемся на обратный вызов при смене активного пресета.
        // Если выбран пресет из второй группы (индекс >= 3), включаем режим SHIFT.
        bankEditor->onActivePresetChanged = [this](int newPresetIndex)
            {
                bool requiredShift = (newPresetIndex >= 3);
                if (shiftButton->getToggleState() != requiredShift)
                {
                    shiftButton->setToggleState(requiredShift, juce::dontSendNotification);
                }
                updatePresetDisplays();
            };

        // Сразу обновляем отображение
        updatePresetDisplays();
    }
}

// Новый метод для обновления отображения кнопок и меток
// Предполагается, что BankEditor возвращает список имён пресетов для активного банка (6 элементов)
void Rig_control::updatePresetDisplays()
{
    if (bankEditor == nullptr)
        return;

    juce::StringArray names = bankEditor->getPresetNames(bankEditor->getActiveBankIndex());
    if (names.size() < 6)
        return;

    bool shiftActive = shiftButton->getToggleState();
    int activePresetIndex = bankEditor->getActivePresetIndex();

    if (!shiftActive)
    {
        presetButtons[0]->setButtonText(names[0]);
        presetButtons[1]->setButtonText(names[1]);
        presetButtons[2]->setButtonText(names[2]);

        // Обновляем toggle-состояния: если активный пресет принадлежит группе A (индексы 0-2)
        for (int i = 0; i < 3; ++i)
        {
            presetButtons[i]->setToggleState(activePresetIndex == i, juce::dontSendNotification);
        }

        presetLabel1_4.setText(names[3], juce::dontSendNotification);
        presetLabel2_5.setText(names[4], juce::dontSendNotification);
        presetLabel3_6.setText(names[5], juce::dontSendNotification);
    }
    else
    {
        presetButtons[0]->setButtonText(names[3]);
        presetButtons[1]->setButtonText(names[4]);
        presetButtons[2]->setButtonText(names[5]);

        for (int i = 0; i < 3; ++i)
        {
            // Если shift активирован, активный пресет считается из группы B (индексы 3-5)
            presetButtons[i]->setToggleState(activePresetIndex == (i + 3), juce::dontSendNotification);
        }

        presetLabel1_4.setText(names[0], juce::dontSendNotification);
        presetLabel2_5.setText(names[1], juce::dontSendNotification);
        presetLabel3_6.setText(names[2], juce::dontSendNotification);
    }

    repaint();
}


void Rig_control::buttonClicked(juce::Button* button)
{
    // Обработка кнопки SHIFT: при её нажатии сразу обновляем отображение
    if (button == shiftButton.get())
    {
        // При переключении SHIFT обновляем имена пресетов сразу
        updatePresetDisplays();
        return;
    }

    // Обработка preset‑кнопок (RadioGroup ID == 100)
    if (button->getRadioGroupId() == 100)
    {
        // Определяем индекс нажатой кнопки
        int clickedIndex = presetButtons.indexOf(static_cast<juce::TextButton*>(button));
        if (clickedIndex != -1 && button->getToggleState())
        {
            // Определяем, активен ли SHIFT (например, shiftButton->getToggleState() или ваш флаг isShiftActive)
            bool shiftActive = shiftButton->getToggleState();
            // Если SHIFT выключен, используем индексы 0..2; если включён, смещение +3 для группы пресетов 3..5
            int presetIndex = shiftActive ? (clickedIndex + 3) : clickedIndex;
            if (bankEditor != nullptr)
                bankEditor->setActivePreset(presetIndex); // Меняем активный пресет в BankEditor
        }
        return;  // событие обработано
    }
    ///////////////////////////////////////////////
    if (button == tempoButton.get())
    {
        // Проверка: если плагин не загружен, то не обрабатываем нажатие
        if ((hostComponent == nullptr) || (hostComponent->getPluginInstance() == nullptr))
        {
            // Можно ещё вывести сообщение или установить disabled-состояние для кнопки,
            // если это необходимо.
            return;
        }

        // Если плагин загружен, выполняем tap tempo
        tapTempo.tap();
        double newBpm = tapTempo.getBpm();

        // Передаём новое значение BPM в VSTHostComponent, который обновляет дисплей
        hostComponent->updateBPM(newBpm);
        return;
    }
    

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
