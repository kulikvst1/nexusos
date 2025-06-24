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
    // mainTab занимает всю область компонента
    mainTab->setBounds(getLocalBounds());

    const int margin = 10;
    auto content = mainTab->getLocalBounds().reduced(margin);

    // Параметры сетки: фиксированное число столбцов и строк
    constexpr int numCols = 9;
    constexpr int numRows = 4;
    int usableWidth = content.getWidth();
    int sectorWidth = usableWidth / numCols;
    int extra = usableWidth - (sectorWidth * numCols); // остаток, распределяем по первым столбцам
    int sectorHeight = content.getHeight() / numRows;

    // Предвычисляем прямоугольники всех секторов и сохраняем их в вектор (нумерация идет по строкам)
    std::vector<juce::Rectangle<int>> sectors;
    sectors.reserve(numCols * numRows);
    for (int row = 0; row < numRows; ++row)
    {
        int y = content.getY() + row * sectorHeight;
        int x = content.getX();
        for (int col = 0; col < numCols; ++col)
        {
            int extraWidth = (col < extra ? 1 : 0);
            int w = sectorWidth + extraWidth;
            sectors.push_back(juce::Rectangle<int>(x, y, w, sectorHeight));
            x += w;
        }
    }

    // Лямбда для получения сектора (1-индексация)
    auto getSectorRect = [&sectors](int sectorNumber) -> juce::Rectangle<int>
        {
            return sectors[sectorNumber - 1];
        };

    // Лямбда для объединения горизонтальных секторов (от startSector до endSector, оба включительно)
    auto getUnionRect = [&sectors](int startSector, int endSector) -> juce::Rectangle<int>
        {
            const auto& r1 = sectors[startSector - 1];
            const auto& r2 = sectors[endSector - 1];
            int x = r1.getX();
            int y = r1.getY();
            int width = r2.getRight() - x;
            return juce::Rectangle<int>(x, y, width, r1.getHeight());
        };

    // Раскладка слайдера Gain и его метки (сектор 1)
    auto gainSector = getSectorRect(1).reduced(4);
    if (gainSlider)
        gainSlider->setBounds(gainSector);
    int gainLabelWidth = gainSector.getWidth() / 2;
    int gainLabelHeight = 40;
    juce::Rectangle<int> gainLabelBounds(gainLabelWidth, gainLabelHeight);
    gainLabelBounds.setCentre(gainSector.getCentre());
    gainLabel.setBounds(gainLabelBounds);
    gainLabel.setFont(juce::Font(gainSector.getHeight() * 0.20f, juce::Font::bold));

    // Раскладка слайдера Volume и его метки (сектор 9)
    auto volumeSector = getSectorRect(9).reduced(4);
    if (volumeSlider)
        volumeSlider->setBounds(volumeSector);
    int volumeLabelW = volumeSector.getWidth() * 2;
    int volumeLabelH = 40;
    juce::Rectangle<int> volumeLabelRect(volumeLabelW, volumeLabelH);
    volumeLabelRect.setCentre(volumeSector.getCentre());
    volumeLabel.setBounds(volumeLabelRect);
    volumeLabel.setFont(juce::Font(volumeSector.getHeight() * 0.17f, juce::Font::bold));

    // Универсальная лямбда для размещения кнопки в указанном секторе
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

    // Раскладка кнопок-пресетов с объединением секторов
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

    // Лямбда для универсального размещения метки для пресета
    auto layoutPresetLabel = [](juce::Label& label, const juce::Rectangle<int>& bounds)
        {
            int labelW = static_cast<int>(bounds.getWidth() / 1.5);
            int labelH = bounds.getHeight() / 4;
            juce::Rectangle<int> labelRect(labelW, labelH);
            // Размещаем метку так, чтобы её правый нижний угол был выровнен с правым нижним углом bounds
            labelRect.setPosition(bounds.getRight() - labelW, bounds.getBottom() - labelH);
            label.setBounds(labelRect);
            label.setColour(juce::Label::textColourId, juce::Colours::grey);
            label.setColour(juce::Label::backgroundColourId, juce::Colours::lightgrey);
            label.setOpaque(true);
            label.setJustificationType(juce::Justification::centred);
        };

    if (presetButtons.size() > 0)
        layoutPresetLabel(presetLabel1_4, preset1Bounds);
    if (presetButtons.size() > 1)
        layoutPresetLabel(presetLabel2_5, preset2Bounds);
    if (presetButtons.size() > 2)
        layoutPresetLabel(presetLabel3_6, preset3Bounds);
}

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

    // Если shift выключен, используем группу A (индексы 0-2 для кнопок, 3-5 для меток)
    // Если shift включён, наоборот – кнопки берут значения из группы B (индексы 3-5), а метки – из группы A (индексы 0-2)
    int buttonBaseIndex = shiftActive ? 3 : 0;
    int labelBaseIndex = shiftActive ? 0 : 3;

    // Обновляем кнопки пресетов
    for (int i = 0; i < 3; ++i)
    {
        presetButtons[i]->setButtonText(names[buttonBaseIndex + i]);
        presetButtons[i]->setToggleState(activePresetIndex == (buttonBaseIndex + i), juce::dontSendNotification);
    }

    // Обновляем сопутствующие метки для альтернативной группы
    presetLabel1_4.setText(names[labelBaseIndex + 0], juce::dontSendNotification);
    presetLabel2_5.setText(names[labelBaseIndex + 1], juce::dontSendNotification);
    presetLabel3_6.setText(names[labelBaseIndex + 2], juce::dontSendNotification);

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
    // Обработка tempo 
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
