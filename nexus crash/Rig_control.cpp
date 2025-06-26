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
        preset->setLookAndFeel(&presetLF);
    }

    // Если вы создаёте 3 кнопки-пресета, то добавляем метки для них:
    juce::Label* labels[] = { &presetLabel1_4, &presetLabel2_5, &presetLabel3_6 };
    const char* texts[] = { "preset1.4",        "preset2.5",        "preset3.6" };
    for (int i = 0; i < 3; ++i)
    {
        labels[i]->setText(texts[i], juce::dontSendNotification);
        labels[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(*labels[i]);
        labels[i]->setOpaque(true);
        // вот нужный светло-серый фон:
        labels[i]->setColour(juce::Label::backgroundColourId, juce::Colours::lightgrey);
        // и текст пусть будет тёмно-серым
        labels[i]->setColour(juce::Label::textColourId, juce::Colours::darkgrey);

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

    for (auto* btn : { shiftButton.get(),
                       tempoButton.get(),
                       upButton.get(),
                       downButton.get() })
    {
        if (btn)
            btn->setLookAndFeel(&custom);
    }

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
    // 1) Останавливаем таймер (если вы его запускали где-то timerCallback)
    stopTimer();
    // 2) Чистим коллбэки BankEditor, чтобы он больше не звал нас и не дергал UI после разрушения
    if (bankEditor != nullptr)
    {
        bankEditor->onBankEditorChanged = nullptr;
        bankEditor->onActivePresetChanged = nullptr;
        // если вы передавали rig_control->midiOutput в bankEditor, то:
        bankEditor->setMidiOutput(nullptr);
    }
    // 3) Чистим коллбэки VSTHostComponent
    if (hostComponent != nullptr)
    {
       // hostComponent->setParameterChangeCallback(nullptr);
       // hostComponent->setPresetCallback(nullptr);
      //  hostComponent->setLearnCallback(nullptr);
       // hostComponent->setBpmDisplayLabel(nullptr);
    }
    // 4) Если вы регистрировали себя как MIDI-коллбэк где-то в AudioDeviceManager:
    //    deviceManager.removeMidiInputCallback (deviceIndex, this);
    // 5) Удаляем слушатели UI и сбрасываем LookAndFeel
    for (auto* btn : presetButtons)
    {
        btn->removeListener(this);
        btn->setLookAndFeel(nullptr);
    }
    if (shiftButton) { shiftButton->removeListener(this);  shiftButton->setLookAndFeel(nullptr); }
    if (tempoButton) { tempoButton->removeListener(this);  tempoButton->setLookAndFeel(nullptr); }
    if (upButton) { upButton->removeListener(this);     upButton->setLookAndFeel(nullptr); }
    if (downButton) { downButton->removeListener(this);   downButton->setLookAndFeel(nullptr); }
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
    auto layoutButton = [&](juce::TextButton* btn, int sector)
        {
            if (btn)
                btn->setBounds(getSectorRect(sector).reduced(4));
        };

    layoutButton(upButton.get(), 10);
    layoutButton(tempoButton.get(), 18);
    layoutButton(downButton.get(), 19);
    layoutButton(shiftButton.get(), 27);

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
    // 1) массив указателей на ваши метки
    std::array<juce::Label*, 3> labels = { &presetLabel1_4,
                                            &presetLabel2_5,
                                            &presetLabel3_6 };
    // 2) массив соответствующих прямоугольников
    std::array<juce::Rectangle<int>, 3> bounds = { preset1Bounds,
                                                  preset2Bounds,
                                                  preset3Bounds };

    // 3) лямбда, которая и позиционирует, и красит, и задаёт шрифт
    auto layoutPresetLabel = [&](juce::Label& lbl,
        const juce::Rectangle<int>& area)
        {
            // пусть метка занимает 1/1.5 по ширине и 1/4 по высоте
            int w = int(area.getWidth() / 1.5f);
            int h = int(area.getHeight() / 4.0f);

            // выравниваем её по правому нижнему углу area
            juce::Rectangle<int> r(area.getRight() - w,
                area.getBottom() - h,
                w, h);

            lbl.setBounds(r);
            lbl.setJustificationType(juce::Justification::centred);
            lbl.setColour(juce::Label::textColourId, juce::Colours::grey);
            lbl.setColour(juce::Label::backgroundColourId, juce::Colours::lightgrey);
            lbl.setOpaque(true);

            // вот здесь и вносим шрифт: например, пол­высоты метки
            float fontSize = r.getHeight() * 0.9f;
            lbl.setFont(juce::Font(fontSize, juce::Font::bold));
        };

    // 4) единым циклом раскладываем все три:
    for (int i = 0; i < 3; ++i)
    {
        if (i < presetButtons.size())  // убедиться, что кнопка есть
            layoutPresetLabel(*labels[i], bounds[i]);
    }
}

// Новый метод для установки BankEditor и подписки на его изменения:
void Rig_control::setBankEditor(BankEditor* editor)
{
    bankEditor = editor;

    if (bankEditor != nullptr)
    {
        // Сохраняем существующий коллбэк (если он был установлен раньше)
        auto prevCB = bankEditor->onActivePresetChanged;

        // Назначаем новый, но сперва вызываем старый, а потом — своё
        bankEditor->onActivePresetChanged = [this, prevCB](int presetIndex)
            {
                // 1) максимально не трогаем старую логику
                if (prevCB)
                    prevCB(presetIndex);

                // 2) теперь наша доп-логика: прокидываем в хост
                if (hostComponent)
                    hostComponent->setExternalPresetIndex(presetIndex);

                // 3) обновляем Rig_control UI
                bool shiftOn = (presetIndex >= 3);
                shiftButton->setToggleState(shiftOn, juce::dontSendNotification);
                updatePresetDisplays();
            };

        // Подписка на глобальные изменения модели — без изменений
        bankEditor->onBankEditorChanged = [this]()
            {
                updatePresetDisplays();
            };

        updatePresetDisplays();
    }
}

// Новый метод для обновления отображения кнопок и меток
// Предполагается, что BankEditor возвращает список имён пресетов для активного банка (6 элементов)
void Rig_control::updatePresetDisplays()
{
    if (!bankEditor)
        return;

    // 0) имя банка
    int bankIdx = bankEditor->getActiveBankIndex();
    bankNameLabel.setText(bankEditor->getBank(bankIdx).bankName,
        juce::dontSendNotification);

    // 1) шесть имён пресетов
    auto names = bankEditor->getPresetNames(bankIdx);
    if (names.size() < 6)
        return;

    // 2) группы для кнопок и меток
    bool shiftOn = shiftButton->getToggleState();
    int  btnBase = shiftOn ? 3 : 0;  // кнопки 4–6 или 1–3
    int  lblBase = shiftOn ? 0 : 3;  // метки 1–3 или 4–6
    int  activePreset = bankEditor->getActivePresetIndex();

    // 3) три кнопки
    for (int i = 0; i < 3; ++i)
    {
        int g = btnBase + i;
        presetButtons[i]->setButtonText(names[g]);
        presetButtons[i]->setToggleState(activePreset == g,
            juce::dontSendNotification);
    }

    // 4) три метки + подсветка ТОЛЬКО той, чей globalIdx == activePreset
    std::array<juce::Label*, 3> labels = {
        &presetLabel1_4, &presetLabel2_5, &presetLabel3_6
    };
    for (int i = 0; i < 3; ++i)
    {
        int   g = lblBase + i;
        auto* L = labels[i];
        bool  highlight = (g == activePreset);

        L->setText(names[g], juce::dontSendNotification);
        L->setOpaque(true);  // НАВСЕГДА

        // либо синий фон, либо тот самый светло-серый
        L->setColour(juce::Label::backgroundColourId,
            highlight ? juce::Colours::blue
            : juce::Colours::lightgrey);

        // белый текст на «горящей» и тёмно-серый в обычном
        L->setColour(juce::Label::textColourId,
            highlight ? juce::Colours::white
            : juce::Colours::darkgrey);
    }

    repaint();
}
void Rig_control::buttonClicked(juce::Button* button)
{
    // --- Переключение банка UP / DOWN ---
    if (button == upButton.get() || button == downButton.get())
    {
        // 1. Сколько всего банков?
        auto totalBanks = (int)bankEditor->getBanks().size();

        // 2. Текущий индекс
        auto idx = bankEditor->getActiveBankIndex();

        // 3. Новый
        if (button == upButton.get())
            idx = (idx + 1) % totalBanks;
        else idx = (idx + totalBanks - 1) % totalBanks;

        // 4. По железу меняем банк
        bankEditor->setActiveBankIndex(idx);

        // UI подтянется через onBankEditorChanged → updatePresetDisplays()
        return;
    }

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
