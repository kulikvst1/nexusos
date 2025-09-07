#include "Rig_control.h"
#include "bank_editor.h"
#include "OutControlComponent.h"
//==============================================================================
void Rig_control::setOutControlComponent(OutControlComponent* oc) noexcept
{
    outControl = oc;

    if (outControl != nullptr)
    {
        outControl->onMasterGainChanged = [this](float newAvgDb)
            {
                // Если юзер сейчас держит и тащит мастер-слайдер — не лезем
                if (volumeSlider->isMouseButtonDown())
                    return;

                // иначе — привычный silent-апдейт
                int raw = (int)juce::jmap<float>(newAvgDb,
                    -60.0f, 12.0f,
                    0.0f, 127.0f);

                volumeSlider->setValue(raw, juce::dontSendNotification);
                prevVolDb = newAvgDb;
            };

        // начальная синхронизация
        float startAvgDb = 0.5f * (outControl->getGainDbL() + outControl->getGainDbR());
        int   startRaw = (int)juce::jmap<float>(startAvgDb,
            -60.0f, 12.0f,
            0.0f, 127.0f);

        volumeSlider->setValue(startRaw, juce::dontSendNotification);
        prevVolDb = startAvgDb;
    }
    else
    {
        outControl->onMasterGainChanged = nullptr;
    }
}

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
        preset->setColour(juce::TextButton::buttonColourId,juce::Colour::fromRGB(200, 230, 255)); // очень светлый голубой
        preset->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        preset->setColour(juce::TextButton::buttonOnColourId, juce::Colours::blue);
        preset->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
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
    gainSlider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
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
    volumeSlider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    volumeSlider->setRange(0, 127, 1);
    volumeSlider->setValue(64);
    volumeSlider->addListener(this);
    prevVolDb = juce::jmap<float>((float)volumeSlider->getValue(),
        0.0f, 127.0f,
        -60.0f, 12.0f);

    volumeSlider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    volumeSlider->setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::red);
    mainTab->addAndMakeVisible(volumeSlider.get());

    volumeLabel.setText("Volume", juce::dontSendNotification);
    volumeLabel.setJustificationType(juce::Justification::centred);
    mainTab->addAndMakeVisible(volumeLabel);
    //
    // 1) Добавляем кнопку Looper
    addAndMakeVisible(looperBtn);
    looperBtn.setClickingTogglesState(true);
    looperBtn.addListener(this);

    // Tuner button
    tunerBtn.setClickingTogglesState(true);
    tunerBtn.addListener(this);
    addAndMakeVisible(tunerBtn);
    // Stomp button
    addAndMakeVisible(stompBtn);
    stompBtn.setClickingTogglesState(true);
    stompBtn.addListener(this);
    stompBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colours::darkred);
    stompBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);

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
    // ─── БЛОК LOOPER + TUNER + STOMP В СЕКТОРЕ 26 ──────────────────
    constexpr int m2 = 4;
    auto cell26 = getSectorRect(26);
    int btnH = cell26.getHeight() / 2;

    auto btnRow = juce::Rectangle<int>(
        cell26.getX(),
        cell26.getBottom() - btnH,
        cell26.getWidth(),
        btnH
    ).reduced(m2);

    // делим на 3 равные части
    int thirdW = btnRow.getWidth() / 3;
    auto looperBtnArea = btnRow.removeFromLeft(thirdW);
    auto tunerBtnArea = btnRow.removeFromLeft(thirdW);
    auto stompBtnArea = btnRow;

    looperBtn.setBounds(looperBtnArea.reduced(m2));
    tunerBtn.setBounds(tunerBtnArea.reduced(m2));
    stompBtn.setBounds(stompBtnArea.reduced(m2));

    looperBtn.toFront(false);
    tunerBtn.toFront(false);
    stompBtn.toFront(false);


    // если движок не сконфигурирован — прячем лупер и выходим
    if (enginePtr == nullptr)
    {
        if (looperComponent) looperComponent->setBounds(0, 0, 0, 0);
        return;
    }

    // лениво создаём лупер, тюнер уже есть во внешней переменной
    if (!looperComponent)
    {
        looperComponent = std::make_unique<LooperComponent>(*enginePtr);
        addAndMakeVisible(*looperComponent);
        looperComponent->setVisible(false);
    }

    // область для обоих компонентов
    auto topRow = getUnionRect(11, 17).reduced(m2);
    auto bottomRow = getUnionRect(20, 25).reduced(m2);
    juce::Rectangle<int> sharedArea{
        topRow.getX(), topRow.getY(),
        topRow.getWidth(),
        bottomRow.getBottom() - topRow.getY()
    };

    // показываем/прячем
    looperComponent->setBounds(
        looperComponent->isVisible() ? sharedArea
        : juce::Rectangle<int>());

    if (externalTuner)
        externalTuner->setBounds(
            externalTuner->isVisible() ? sharedArea
            : juce::Rectangle<int>());


}

// Новый метод для установки BankEditor и подписки на его изменения:
void Rig_control::setBankEditor(BankEditor* editor)
{
    bankEditor = editor;
    if (!bankEditor)
        return;

    // 1) смена пресета
    bankEditor->onActivePresetChanged = [this](int)
        {
            if (stompMode)
                updateStompDisplays();
            else
                updatePresetDisplays();
        };

    // 2) смена банка
    bankEditor->onBankChanged = [this]()
        {
            shiftButton->setToggleState(false, juce::dontSendNotification);
            if (stompMode)
                updateStompDisplays();
            else
                updatePresetDisplays();
        };

    // 3) любое редактирование текста
    bankEditor->onBankEditorChanged = [this]()
        {
            if (stompMode)
                updateStompDisplays();
            else
                updatePresetDisplays();
        };

    // 4) Shift-кнопка
    shiftButton->setClickingTogglesState(true);
    shiftButton->onClick = [this]()
        {
            if (stompMode)
                updateStompDisplays();
            else
                updatePresetDisplays();
        };

    // начальная отрисовка
    if (stompMode)
        updateStompDisplays();
    else
        updatePresetDisplays();
}

// Новый метод для обновления отображения кнопок и меток
// Предполагается, что BankEditor возвращает список имён пресетов для активного банка (6 элементов)
void Rig_control::updatePresetDisplays()
{
    if (!bankEditor)
        return;

    const int bankIdx = bankEditor->getActiveBankIndex();
    const int active = bankEditor->getActivePresetIndex();
    auto names = bankEditor->getPresetNames(bankIdx);
    if (names.size() < 6)
        return;

    // 0) Решаем, включён ли сейчас Shift:
    const bool wantShift = manualShift
        ? shiftButton->getToggleState()
        : (active >= 3);

    // синхронизируем саму кнопку (без колбеков)
    shiftButton->setToggleState(wantShift, juce::dontSendNotification);
    const bool shiftOn = wantShift;

    // 1) Имя банка
    bankNameLabel.setText(bankEditor->getBank(bankIdx).bankName,
        juce::dontSendNotification);

    // 2) Сбросим и заполним тексты/состояния трёх кнопок и меток
    std::array<juce::Button*, 3> btns = {
        presetButtons[0], presetButtons[1], presetButtons[2]
    };
    std::array<juce::Label*, 3> labs = {
        &presetLabel1_4, &presetLabel2_5, &presetLabel3_6
    };

    for (int i = 0; i < 3; ++i)
    {
        int btnIdx = shiftOn ? (3 + i) : i;
        int lblIdx = shiftOn ? i : (3 + i);

        // 🔹 Сброс цветовой схемы на «пресетную»
        btns[i]->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(200, 230, 255));
        btns[i]->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        btns[i]->setColour(juce::TextButton::buttonOnColourId, juce::Colours::blue);
        btns[i]->setColour(juce::TextButton::textColourOnId, juce::Colours::white);

        btns[i]->setButtonText(names[btnIdx]);
        btns[i]->setToggleState(false, juce::dontSendNotification);

        labs[i]->setText(names[lblIdx], juce::dontSendNotification);
        labs[i]->setOpaque(true);
        labs[i]->setColour(juce::Label::backgroundColourId, juce::Colours::lightgrey);
        labs[i]->setColour(juce::Label::textColourId, juce::Colours::darkgrey);
    }

    // 3) Подсветить единственный active
    if (active < 3)
    {
        if (shiftOn)
        {
            labs[active]->setColour(juce::Label::backgroundColourId, juce::Colours::blue);
            labs[active]->setColour(juce::Label::textColourId, juce::Colours::white);
        }
        else
        {
            btns[active]->setToggleState(true, juce::dontSendNotification);
        }
    }
    else
    {
        int i = active - 3;
        if (shiftOn)
            btns[i]->setToggleState(true, juce::dontSendNotification);
        else
        {
            labs[i]->setColour(juce::Label::backgroundColourId, juce::Colours::blue);
            labs[i]->setColour(juce::Label::textColourId, juce::Colours::white);
        }
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
        // юзер руками переключил Shift
        manualShift = true;
        updatePresetDisplays();
        return;
    }
    // Переключение Stomp режима
    if (button == &stompBtn)
    {
        stompMode = stompBtn.getToggleState();

        if (stompMode)
        {
            // Запоминаем состояние Shift в пресет-режиме
            presetShiftState = shiftButton->getToggleState();

            // Выключаем Shift для стомп-режима
            shiftButton->setToggleState(false, juce::dontSendNotification);

            // В стомп-режиме пресет-кнопки не радиогруппа
            for (auto* btn : presetButtons)
                btn->setRadioGroupId(0, juce::dontSendNotification);

            updateStompDisplays();
        }
        else
        {
            // Возвращаем состояние Shift, которое было в пресет-режиме
            shiftButton->setToggleState(presetShiftState, juce::dontSendNotification);

            // В пресет-режиме кнопки снова радиогруппа
            for (auto* btn : presetButtons)
                btn->setRadioGroupId(100, juce::dontSendNotification);

            updatePresetDisplays();
        }

        return;
    }

    // Клик по кнопке пресета в стомп-режиме
    if (stompMode && presetButtons.contains(static_cast<juce::TextButton*>(button)))
    {
        int idx = presetButtons.indexOf(static_cast<juce::TextButton*>(button));
        if (idx >= 0 && bankEditor)
        {
            const int bankIdx = bankEditor->getActiveBankIndex();
            const int presetIdx = bankEditor->getActivePresetIndex();
            const bool shiftOn = shiftButton->getToggleState();
            int ccIndex = shiftOn ? (idx + 3) : idx; // 0..5

            bool currentState = bankEditor->getBank(bankIdx).ccPresetStates[presetIdx][ccIndex];
            bankEditor->updateCCParameter(ccIndex, !currentState);

            updateStompDisplays();
        }
        return;
    }

    // Обработка preset-кнопок (RadioGroup ID == 100)
    if (button->getRadioGroupId() == 100)
    {
        auto* btn = static_cast<juce::TextButton*>(button);
        int  idx = presetButtons.indexOf(btn);
        if (idx >= 0 && btn->getToggleState())
        {
            manualShift = true;  // переход на эту страницу — ручной режим

            bool shiftOn = shiftButton->getToggleState();
            int  presetIndex = shiftOn ? (idx + 3) : idx;

            if (bankEditor)
                bankEditor->setActivePreset(presetIndex);

            updatePresetDisplays();

            if (presetChangeCb)
                presetChangeCb(presetIndex);
        }
        return;
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
    //looper
    if (button == &looperBtn && enginePtr != nullptr)
    {
        looperComponent->setVisible(looperBtn.getToggleState());
        // пересчитаем layout, чтобы UI лупера сразу вписался
        resized();
        return;
    }
    // Tuner
    if (button == &tunerBtn && externalTuner != nullptr)
    {
        const bool show = !externalTuner->isVisible();
        tunerBtn.setToggleState(show, juce::dontSendNotification);

        // прячем лупер
        if (looperComponent) looperComponent->setVisible(false);

        // показываем/прячем тюнер
        externalTuner->setVisible(show);
        if (show) externalTuner->toFront(false);

        resized();

        // уведомляем MainContentComponent
        if (onTunerVisibilityChanged)
            onTunerVisibilityChanged(show);

        return;
    }

}

void Rig_control::sliderValueChanged(juce::Slider* slider)
{
    if (slider == volumeSlider.get())
    {
        // 1) абсолютное новое значение мастера в дБ
        float newVolDb = juce::jmap<float>(
            (float)volumeSlider->getValue(),  // raw 0…127
            0.0f, 127.0f,
            -60.0f, 12.0f
        );

        // 2) читаем текущие дБ каналов
        float leftDb = outControl ? outControl->getGainDbL() : prevVolDb;
        float rightDb = outControl ? outControl->getGainDbR() : prevVolDb;

        // 3) вычисляем фазу роста/падения
        float maxCh = juce::jmax(leftDb, rightDb);
        float minCh = juce::jmin(leftDb, rightDb);
        float deltaDb = 0.0f;

        if (newVolDb > maxCh)      deltaDb = newVolDb - maxCh;
        else if (newVolDb < minCh) deltaDb = newVolDb - minCh;

        // 4) сдвигаем оба канала только если есть delta
        if (deltaDb != 0.0f && outControl)
            outControl->offsetGainDb(deltaDb);

        // 5) сохраняем текущее мастера
        prevVolDb = newVolDb;
        return;
    }

}

void Rig_control::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message)
{
    if (!message.isNoteOn())
        return;

    int note = message.getNoteNumber();
    if (note >= 60 && note <= 62)
    {
        int idx = note - 60;
        if (idx < presetButtons.size())
        {
            auto* btn = presetButtons[idx];
            btn->setToggleState(true, juce::dontSendNotification);
            buttonClicked(btn); // вручную вызываем обработку
        }
    }
}
void Rig_control::timerCallback()
{
    // Реализация периодических обновлений (если нужно).
}
void Rig_control::handleExternalPresetChange(int newPresetIndex) noexcept
{
    manualShift = false;           // сброс ручного режима
    updatePresetDisplays();        // перерисовать Shift-кнопку и пресеты
}
void Rig_control::setTunerComponent(TunerComponent* t) noexcept
{
    externalTuner = t;
    if (externalTuner)
    {
        addAndMakeVisible(*externalTuner);    // вставляем в иерархию Rig_control
        externalTuner->setVisible(false);     // по умолчанию скрыт
    }
}
void Rig_control::updateStompDisplays()
{
    if (!bankEditor) return;

    const int bankIdx = bankEditor->getActiveBankIndex();
    const int presetIdx = bankEditor->getActivePresetIndex();
    const bool shiftOn = shiftButton->getToggleState();

    std::array<juce::Button*, 3> btns = {
        presetButtons[0], presetButtons[1], presetButtons[2]
    };
    std::array<juce::Label*, 3> labs = {
        &presetLabel1_4, &presetLabel2_5, &presetLabel3_6
    };

    for (int i = 0; i < 3; ++i)
    {
        // Индексы CC для кнопки и метки
        int btnCC = shiftOn ? (i + 3) : i;   // CC1..CC3 или CC4..CC6
        int lblCC = shiftOn ? i : (i + 3);   // обратная тройка

        // --- КНОПКА ---
        juce::String btnName = bankEditor->getCCName(btnCC);
        if (btnName.isEmpty())
            btnName = "CC" + juce::String(btnCC + 1);
        btns[i]->setButtonText(btnName);

        bool btnActive = bankEditor->getBank(bankIdx).ccPresetStates[presetIdx][btnCC];

        // Цвета кнопки (оба состояния)
        btns[i]->setColour(juce::TextButton::buttonColourId, btnActive ? juce::Colours::red : juce::Colours::black);
        btns[i]->setColour(juce::TextButton::buttonOnColourId, btnActive ? juce::Colours::red : juce::Colours::black);
        btns[i]->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        btns[i]->setColour(juce::TextButton::textColourOnId, juce::Colours::white);

        btns[i]->setToggleState(btnActive, juce::dontSendNotification);

        // --- МЕТКА ---
        juce::String lblName = bankEditor->getCCName(lblCC);
        if (lblName.isEmpty())
            lblName = "CC" + juce::String(lblCC + 1);
        labs[i]->setText(lblName, juce::dontSendNotification);

        bool lblActive = bankEditor->getBank(bankIdx).ccPresetStates[presetIdx][lblCC];
        labs[i]->setOpaque(true);
        labs[i]->setColour(juce::Label::backgroundColourId, lblActive ? juce::Colours::red : juce::Colours::lightgrey);
        labs[i]->setColour(juce::Label::textColourId, lblActive ? juce::Colours::white : juce::Colours::darkgrey);
    }

    repaint();
}




