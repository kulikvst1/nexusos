#include <JuceHeader.h>
#include <vector>
#include <string>
#include "bank manager.h"
//==============================================================================
// Общие настройки и константы
static const juce::String settingsFileName = "MIDI_Preset_Settings.ini";
static const juce::String midiOutputKey = "midiOutput";
static const juce::String midiInputKey = "midiInput";

static const int kPresetHeight = 50;  // Высота кнопок пресета

// Функция для отключения всех MIDI-входов.
void enableAllMidiInputs(juce::AudioDeviceManager& deviceManager)
{
    auto midiDevices = juce::MidiInput::getAvailableDevices();
    for (auto& device : midiDevices)
        deviceManager.setMidiInputDeviceEnabled(device.identifier, false);
}

//==============================================================================
/**
    MultiMidiSenderComponent – компонент для отправки и приёма MIDI-сообщений,
    работы с кнопками (CC, пресеты, SHIFT, TEMPO, UP, DOWN) и загрузки/сохранения настроек.
    Дополнен двумя rotary-слайдерами (Gain и Volume), которые отправляют и принимают MIDI CC,
    а также имеют соответствующие геттеры/сеттеры для настроек номеров CC.
*/
class MultiMidiSenderComponent : public juce::Component,
    public juce::Button::Listener,
    public juce::Slider::Listener,
    public juce::MidiInputCallback
{
public:
    MultiMidiSenderComponent()
    {
        // Создаем таббар с вкладками, расположенными горизонтально (TabsAtTop)
        tabbedComponent = std::make_unique<juce::TabbedComponent>(juce::TabbedButtonBar::TabsAtTop);

        // Создаем 3 вкладки: первая - пустая, вторая - основное окно с вашим UI, третья - пустая (на будущее)
        bankTab = std::make_unique<BankManager>();
        mainTab = std::make_unique<juce::Component>();
        emptyTab = std::make_unique<juce::Component>();

        
        tabbedComponent->addTab("RIG KONTROL",juce::LookAndFeel::getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId),mainTab.get(), true);
        tabbedComponent->addTab("BANK MANAGER", juce::LookAndFeel::getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId), bankTab.get(), false);
        // После создания вкладки BankManager (bankTab) и добавления её в TabbedComponent:
if (auto* bm = dynamic_cast<BankManager*>(bankTab.get()))
{
    bm->onBankManagerChanged = [this, bm]()
    {
        // 1. Обновляем имя банка
        auto activeBank = bm->getBank(bm->getActiveBankIndex());
        bankNameLabel.setText(activeBank.bankName, juce::dontSendNotification);

        // 2. Обновляем подписи кнопок-пресетов в зависимости от состояния SHIFT
        if (shiftButton->getToggleState())
        {
            for (int i = 0; i < presetButtons.size(); ++i)
            {
                if (i < 3)
                    presetButtons[i]->setButtonText(activeBank.presetNames[i + 3]);
            }
        }
        else
        {
            for (int i = 0; i < presetButtons.size(); ++i)
            {
                if (i < 3)
                    presetButtons[i]->setButtonText(activeBank.presetNames[i]);
            }
        }
        // Это гарантирует, что при переключении банка состояние SHIFT всегда будет false.
        if (shiftButton->getToggleState())
            shiftButton->setToggleState(false, juce::sendNotification);
        // 3. При смене банка автоматически выбираем первый пресет:
        currentPresetIndex = 0;
        for (int i = 0; i < presetButtons.size(); ++i)
            presetButtons[i]->setToggleState(i == 0, juce::dontSendNotification);
        // Можно вызвать обработчик нажатия для первой кнопки, чтобы обновить CC состояния и т.д.
        buttonClicked(presetButtons[0]);

        // 4. Обновляем состояния CC-кнопок для текущего пресета
        int presetIndex = currentPresetIndex;
        if (shiftButton->getToggleState())
            presetIndex += 3;
        const auto& ccStates = bm->getBanks()[bm->getActiveBankIndex()].ccPresetStates[presetIndex];
        for (int i = 0; i < ccButtons.size(); ++i)
        {
            bool state = (i < ccStates.size()) ? ccStates[i] : false;
            ccButtons[i]->setToggleState(state, juce::dontSendNotification);
            int ccValue = state ? 64 : 0;
            int ccNumber = i + 1;
            if (midiOut != nullptr)
            {
                auto msg = juce::MidiMessage::controllerEvent(1, ccNumber, (juce::uint8)ccValue);
                handleOutgoingMidiMessage(msg);
            }
        }

        // 5. *** Обновляем volumeSlider из данных активного банка ***
        volumeSlider->setValue(activeBank.presetVolume[currentPresetIndex], juce::dontSendNotification);
    };
}



        tabbedComponent->addTab("VST HOST", juce::Colours::lightgrey, emptyTab.get(), false);

        // Добавляем таббар как основной элемент компонента
        addAndMakeVisible(tabbedComponent.get());

        


        // *** Ниже идет остальной ваш первоначальный код - все элементы добавляются во вкладку "Main" ***

        // 1) Создаем 10 кнопок для MIDI CC (стандартные)
        for (int i = 0; i < 10; ++i)
        {
            auto* btn = new juce::TextButton("CC " + juce::String(i + 1));
            btn->setClickingTogglesState(true);
            btn->setToggleState(false, juce::dontSendNotification);
            btn->setColour(juce::TextButton::buttonOnColourId, juce::Colours::red);
            btn->addListener(this);
            mainTab->addAndMakeVisible(btn);
            ccButtons.add(btn);
        }

        // 2) Создаем 3 кнопки-пресета (например, для групп A, B, C)
        for (int i = 0; i < 3; ++i)
        {
            auto* preset = new juce::TextButton();
            preset->setClickingTogglesState(true);
            preset->setRadioGroupId(100, juce::dontSendNotification);
            preset->setToggleState(false, juce::dontSendNotification);
            preset->setColour(juce::TextButton::buttonOnColourId, juce::Colours::blue);
            preset->setButtonText(getPresetLabel(i));
            preset->addListener(this);
            mainTab->addAndMakeVisible(preset);
            presetButtons.add(preset);
        }

        // 3) Добавляем метку BANK NAME
        bankNameLabel.setText("BANK NAME", juce::dontSendNotification);
        bankNameLabel.setJustificationType(juce::Justification::centred);
        mainTab->addAndMakeVisible(bankNameLabel);

        // 4) Кнопка SHIFT
        shiftButton = std::make_unique<juce::TextButton>("SHIFT");
        shiftButton->setClickingTogglesState(true);
        shiftButton->setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange);
        shiftButton->addListener(this);
        mainTab->addAndMakeVisible(shiftButton.get());

        // 5) Кнопки TEMPO, UP и DOWN
        tempoButton = std::make_unique<juce::TextButton>("TEMPO");
        tempoButton->setClickingTogglesState(true);
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

        // 6) Rotary‑слайдер для Gain + его метка
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

        // 7) Rotary‑слайдер для Volume + его метка
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

        // --- Инициализация номеров MIDI CC (оставлено без изменений) ---

        shiftCCNumber = 90;
        upCCNumber = 72;
        downCCNumber = 73;
        tempoCCNumber = 74;

        presetCCNumbers.resize(6);
        for (int i = 0; i < 6; ++i)
            presetCCNumbers[i] = 80 + i;

        // Инициализация массива состояний для пресетов (каждый пресет хранит состояния 10 кнопок CC)

        presetStates.resize(6);
        for (auto& stateGroup : presetStates)
            stateGroup.resize(ccButtons.size(), false);

        // --- Новые элементы: rotary-слайдеры для Gain и Volume ---
        // Номера MIDI CC по умолчанию: Gain = 20, Volume = 21.

        gainCCNumber = 20;
        volumeCCNumber = 21;

        // Сохраняем положение слайдера Volume для каждого из 6 пресетов (по умолчанию 64)

        presetVolume.resize(6, 64);
        currentPresetIndex = -1; // Ни один пресет не выбран
        
        // Загрузка настроек, как и в оригинале
        // Вставляем настройку callback-а для BankManager:
        
        
        loadSettings();


    }

    ~MultiMidiSenderComponent() override
    {
        saveSettings();
        for (auto* btn : ccButtons)
            btn->removeListener(this);
        for (auto* btn : presetButtons)
            btn->removeListener(this);
        if (shiftButton)   shiftButton->removeListener(this);
        if (tempoButton)   tempoButton->removeListener(this);
        if (upButton)      upButton->removeListener(this);
        if (downButton)    downButton->removeListener(this);
        if (gainSlider)    gainSlider->removeListener(this);
        if (volumeSlider)  volumeSlider->removeListener(this);
    }

    void resized() override
    {
        // Таббар занимает всю область компонента
        tabbedComponent->setBounds(getLocalBounds());

        // Вкладка "Main" получает оригинальную разметку, как у вас было:
        if (mainTab != nullptr)
        {
            const int margin = 10;
            auto content = mainTab->getLocalBounds().reduced(margin);

            // Применяем разметку по секторам (9 столбцов x 4 строки = 36 секторов)
            int numCols = 9;
            int numRows = 4;
            int usableWidth = content.getWidth();
            int sectorWidth = usableWidth / numCols;
            int extra = usableWidth - (sectorWidth * numCols);
            int sectorHeight = content.getHeight() / numRows;

            // Лямбда для получения прямоугольника одного сектора (номер начинается с 1)
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

            // Лямбда для объединения секторов по горизонтали (от startSector до endSector)
            auto getUnionRect = [=](int startSector, int endSector) -> juce::Rectangle<int>
                {
                    auto r1 = getSectorRect(startSector);
                    auto r2 = getSectorRect(endSector);
                    int x = r1.getX();
                    int y = r1.getY();
                    int width = r2.getRight() - x;
                    int height = r1.getHeight();
                    return juce::Rectangle<int>(x, y, width, height);
                };

            // Пример размещения (сектора подобраны для ваших элементов):

            // Сектор 1: Gain slider и его метка
            auto gainSector = getSectorRect(1).reduced(4);
            if (gainSlider)
            {
                gainSlider->setBounds(gainSector);
                int labelW = gainSector.getWidth() / 2;
                int labelH = 20;
                juce::Rectangle<int> labelRect(labelW, labelH);
                labelRect.setCentre(gainSector.getCentre());
                gainLabel.setBounds(labelRect);
            }

            // Сектор 9: Volume slider и его метка
            auto volumeSector = getSectorRect(9).reduced(4);
            if (volumeSlider)
            {
                volumeSlider->setBounds(volumeSector);
                int labelW = volumeSector.getWidth() * 2;
                int labelH = 30;
                juce::Rectangle<int> labelRect(labelW, labelH);
                labelRect.setCentre(volumeSector.getCentre());
                volumeLabel.setBounds(labelRect);
            }

            // Сектор 10: UP-кнопка
            if (upButton)
                upButton->setBounds(getSectorRect(10).reduced(4));

            // Сектор 18: TEMPO-кнопка
            if (tempoButton)
                tempoButton->setBounds(getSectorRect(18).reduced(4));

            // Сектор 19: DOWN-кнопка
            if (downButton)
                downButton->setBounds(getSectorRect(19).reduced(4));

            // Сектор 27: SHIFT-кнопка
            if (shiftButton)
                shiftButton->setBounds(getSectorRect(27).reduced(4));

            // Секторы 28-30: кнопка-пресет A (первая из presetButtons)
            if (presetButtons.size() > 0)
                presetButtons[0]->setBounds(getUnionRect(28, 30).reduced(4));

            // Секторы 31-33: кнопка-пресет B (вторая)
            if (presetButtons.size() > 1)
                presetButtons[1]->setBounds(getUnionRect(31, 33).reduced(4));

            // Секторы 34-36: кнопка-пресет C (третья)
            if (presetButtons.size() > 2)
                presetButtons[2]->setBounds(getUnionRect(34, 36).reduced(4));

            // Секторы 11-17: группа стандартных CC-кнопок (ширина делится поровну, высота вдвое)
            {
                auto ccRect = getUnionRect(11, 17).reduced(4);
                ccRect = ccRect.withHeight(ccRect.getHeight() / 2);
                int numButtons = ccButtons.size();
                int buttonWidth = ccRect.getWidth() / numButtons;
                for (int i = 0; i < numButtons; ++i)
                    ccButtons[i]->setBounds(ccRect.removeFromLeft(buttonWidth).reduced(2));
            }

            // Секторы 2-8: метка BANK NAME
            {
                auto bankRect = getUnionRect(2, 8).reduced(4);
                bankNameLabel.setBounds(bankRect);
            }
        }
    }

    // -------------- Обработка событий ----------------

    void buttonClicked(juce::Button* button) override
    {
        // Пример обработки кнопок TEMPO, UP, DOWN
        if (button == tempoButton.get())
        {
            bool state = tempoButton->getToggleState();
            int ccValue = state ? 127 : 0;
            if (midiOut != nullptr)
            {
                auto msg = juce::MidiMessage::controllerEvent(1, tempoCCNumber, (juce::uint8)ccValue);
                handleOutgoingMidiMessage(msg);
            }
            return;
        }
        // Для кнопок UP и DOWN мы хотим переключить активный банк в BankManager
        if (button == upButton.get() || button == downButton.get())
        {
            // Предположим, что BankManager хранится во вкладке bankTab
            if (auto* bm = dynamic_cast<BankManager*>(bankTab.get()))
            {
                int currentIndex = bm->getActiveBankIndex();
                int totalBanks = 15; // либо, если у вас есть метод getBankCount(), вызовите его

                if (button == upButton.get())
                {
                    int newIndex = (currentIndex > 0) ? currentIndex - 1 : totalBanks - 1;
                    bm->setActiveBankIndex(newIndex);
                }
                else if (button == downButton.get())
                {
                    int newIndex = (currentIndex < totalBanks - 1) ? currentIndex + 1 : 0;
                    bm->setActiveBankIndex(newIndex);
                }
            }
            return;
        }


        // Обработка стандартных CC-кнопок
         // Обработка стандартных CC‑кнопок
        for (int i = 0; i < ccButtons.size(); ++i)
        {
            if (ccButtons[i] == button)
            {
                bool state = button->getToggleState();
                int ccValue = state ? 64 : 0;
                int ccNumber = i + 1;
                if (midiOut != nullptr)
                {
                    auto msg = juce::MidiMessage::controllerEvent(1, ccNumber, (juce::uint8)ccValue);
                    handleOutgoingMidiMessage(msg);
                }
                // Определяем группу и выбранный пресет
                int groupOffset = shiftButton->getToggleState() ? 3 : 0;
                int selectedPreset = -1;
                for (int j = 0; j < presetButtons.size(); ++j)
                {
                    if (presetButtons[j]->getToggleState())
                    {
                        selectedPreset = j;
                        break;
                    }
                }
                if (selectedPreset != -1)
                {
                    int presetIndex = groupOffset + selectedPreset;
                    if (auto* bm = dynamic_cast<BankManager*>(bankTab.get()))
                    {
                        int activeIndex = bm->getActiveBankIndex();
                        if (activeIndex >= 0 && activeIndex < bm->getBanks().size())
                        {
                            // Обновляем состояние CC-кнопки в выбранном банке
                            bm->getBanks()[activeIndex].ccPresetStates[presetIndex][i] = state;
                            // Вставляем здесь вызов сохранения настроек, чтобы изменения сразу записывались в файл:
                            bm->saveSettings();
                            // Альтернативно можно использовать: bm->restartAutoSaveTimer();
                        }
                    }
                }
                return;
            }
        }





        // Обработка кнопки SHIFT
        if (button == shiftButton.get())
        {
            updatePresetButtonLabels();
            bool shiftState = shiftButton->getToggleState();
            if (midiOut != nullptr)
            {
                auto msg = juce::MidiMessage::controllerEvent(1, shiftCCNumber, (juce::uint8)(shiftState ? 127 : 0));
                handleOutgoingMidiMessage(msg);
            }
            return;
        }
        // Обработка кнопок-пресетов (RadioGroup ID = 100)
        if (button->getRadioGroupId() == 100)
        {
            int clickedIndex = -1;
            for (int i = 0; i < presetButtons.size(); ++i)
            {
                if (presetButtons[i] == button)
                {
                    clickedIndex = i;
                    break;
                }
            }
            if (clickedIndex != -1 && button->getToggleState())
            {
                if (auto* bm = dynamic_cast<BankManager*>(bankTab.get()))
                {
                    int activeIndex = bm->getActiveBankIndex();
                    if (activeIndex >= 0 && activeIndex < bm->getBanks().size())
                    {
                        int presetIndex = shiftButton->getToggleState() ? (3 + clickedIndex) : clickedIndex;
                        // Получаем массив состояний CC для выбранного пресета активного банка:
                        const auto& ccStates = bm->getBanks()[activeIndex].ccPresetStates[presetIndex];
                        for (int j = 0; j < ccButtons.size(); ++j)
                        {
                            bool presetState = (j < ccStates.size()) ? ccStates[j] : false;
                            ccButtons[j]->setToggleState(presetState, juce::dontSendNotification);
                            int ccValue = presetState ? 64 : 0;
                            int ccNumber = j + 1;
                            if (midiOut != nullptr)
                            {
                                auto msg = juce::MidiMessage::controllerEvent(1, ccNumber, (juce::uint8)ccValue);
                                handleOutgoingMidiMessage(msg);
                            }
                        }
                        currentPresetIndex = presetIndex;
                        // Отправляем MIDI-сообщения для кнопок-пресетов (если требуется)
                        for (int i = 0; i < presetButtons.size(); ++i)
                        {
                            int tempPresetIndex = shiftButton->getToggleState() ? (3 + i) : i;
                            int messageValue = presetButtons[i]->getToggleState() ? 127 : 0;
                            if (midiOut != nullptr)
                            {
                                auto msg = juce::MidiMessage::controllerEvent(1, presetCCNumbers[tempPresetIndex], (juce::uint8)messageValue);
                                handleOutgoingMidiMessage(msg);
                            }
                        }
                        if (auto* bm = dynamic_cast<BankManager*>(bankTab.get()))
                        {
                            int activeIndex = bm->getActiveBankIndex();
                            if (activeIndex >= 0 && activeIndex < bm->getBanks().size())
                            {
                                // Определяем, какой пресет выбран, например:
                                int presetIndex = shiftButton->getToggleState() ? (3 + clickedIndex) : clickedIndex;
                                // Запоминаем новый текущий пресет (примерно так же, как для CC состояния)
                                currentPresetIndex = presetIndex;
                                // Устанавливаем слайдер в соответствии с сохранённым значением для этого пресета
                                volumeSlider->setValue(bm->getBanks()[activeIndex].presetVolume[presetIndex], juce::dontSendNotification);
                            }
                        }
                    }
                }
            }
        }
    }

    // --- Обработка событий слайдеров (Slider::Listener) ---
    void sliderValueChanged(juce::Slider* slider) override
    {
        if (slider == volumeSlider.get())
        {
            int newVolume = static_cast<int>(volumeSlider->getValue());
            if (auto* bm = dynamic_cast<BankManager*>(bankTab.get()))
            {
                int activeIndex = bm->getActiveBankIndex();
                if (activeIndex >= 0 && activeIndex < bm->getBanks().size() && currentPresetIndex >= 0)
                {
                    // Обновляем значение Volume для текущего пресета в активном банке
                    bm->getBanks()[activeIndex].presetVolume[currentPresetIndex] = newVolume;
                    // Сохраняем настройки, если нужно:
                    bm->saveSettings(); // или restartAutoSaveTimer();
                }
            }
            // При необходимости можно сразу отправлять MIDI-сообщение для Volume CC:
            if (midiOut != nullptr)
            {
                auto msg = juce::MidiMessage::controllerEvent(1, volumeCCNumber, (juce::uint8)newVolume);
                handleOutgoingMidiMessage(msg);
            }
        }
        // Если изменяется слайдер gain – отправляем MIDI CC с номером 10
        int newVolume = static_cast<int>(gainSlider->getValue());
        if (slider == gainSlider.get())
        {
            auto msg = juce::MidiMessage::controllerEvent(1, gainCCNumber, (juce::uint8)newVolume);
            handleOutgoingMidiMessage(msg);
        }
    }

    ////////////////////////// Обработка входящих MIDI-сообщений.
    void handleIncomingMidiMessage(juce::MidiInput* /*source*/, const juce::MidiMessage& message) override
    {
        juce::MessageManager::callAsync([this, message]()
            {
                handleMidiMessage(message);
            });
    }

    void handleMidiMessage(const juce::MidiMessage& message)
    {
        if (!message.isController())
            return;
        int controller = message.getControllerNumber();

        //обработка сообщений для TEMPO
        if (controller == tempoCCNumber)
        {
            bool newState = (message.getControllerValue() > 0);
            tempoButton->setToggleState(newState, juce::sendNotificationSync);
            updatePresetButtonLabels();
            if (midiOut != nullptr)
            {
                auto msg = juce::MidiMessage::controllerEvent(1, tempoCCNumber,
                    (juce::uint8)(newState ? 127 : 0));
                handleOutgoingMidiMessage(msg);
            }
        }
        //обработка сообщений для DOWN
        if (controller == downCCNumber)
        {
            bool newState = (message.getControllerValue() > 0);
            downButton->setToggleState(newState, juce::sendNotificationSync);
            updatePresetButtonLabels();
            if (midiOut != nullptr)
            {
                auto msg = juce::MidiMessage::controllerEvent(1, downCCNumber,
                    (juce::uint8)(newState ? 127 : 0));
                handleOutgoingMidiMessage(msg);
            }
        }
        //обработка сообщений для UP
        if (controller == upCCNumber)
        {
            bool newState = (message.getControllerValue() > 0);
            upButton->setToggleState(newState, juce::sendNotificationSync);
            updatePresetButtonLabels();
            if (midiOut != nullptr)
            {
                auto msg = juce::MidiMessage::controllerEvent(1, upCCNumber,
                    (juce::uint8)(newState ? 127 : 0));
                handleOutgoingMidiMessage(msg);
            }
        }
        // Обработка сообщения для SHIFT.
        if (controller == shiftCCNumber)
        {
            bool newState = (message.getControllerValue() > 0);
            shiftButton->setToggleState(newState, juce::sendNotificationSync);
            updatePresetButtonLabels();
            if (midiOut != nullptr)
            {
                auto msg = juce::MidiMessage::controllerEvent(1, shiftCCNumber, (juce::uint8)(newState ? 127 : 0));
                handleOutgoingMidiMessage(msg);
            }
        }
        else if (controller == gainCCNumber)
        {
            // Обновляем значение слайдера Gain при входящем MIDI CC
            gainSlider->setValue(message.getControllerValue(), juce::dontSendNotification);
        }
        else if (controller == volumeCCNumber)
        {
            // Обновляем значение слайдера Volume при входящем MIDI CC
            volumeSlider->setValue(message.getControllerValue(), juce::dontSendNotification);
        }
        else
        {
            for (int i = 0; i < (int)presetCCNumbers.size(); ++i)
            {
                if (controller == presetCCNumbers[i])
                {
                    int index = i % 3;
                    for (int j = 0; j < presetButtons.size(); ++j)
                        presetButtons[j]->setToggleState(j == index, juce::dontSendNotification);
                    buttonClicked(presetButtons[index]);
                    return;
                }
            }
        }
    }
    // Отправка MIDI-сообщения (как в вашем исходном коде с задержкой 1 мс)
    void handleOutgoingMidiMessage(const juce::MidiMessage& message)
    {
        // По умолчанию сохраняем тот же механизм задержки 1 мс
        juce::Timer::callAfterDelay(1, [this, message]()
            {
                if (midiOut != nullptr)
                    midiOut->sendMessageNow(message);
                else
                    DBG("MIDI Output not available: " << message.getDescription());
            });
    }

    // Генерация метки для кнопки-пресета с учётом состояния SHIFT
    juce::String getPresetLabel(int index)
    {
        int offset = (shiftButton && shiftButton->getToggleState()) ? 3 : 0;
        char letter = static_cast<char>('A' + index + offset);
        char buf[2] = { letter, '\0' };
        return juce::String(buf);
    }

    void updatePresetButtonLabels()
    {
        // Предположим, что у вас есть указатель на менеджер банков, например:
        // auto* bm = dynamic_cast<BankManager*>(bankTab.get());
        // и если он не равен nullptr, получаем активный банк:
        auto* bm = dynamic_cast<BankManager*>(bankTab.get());
        if (bm != nullptr)
        {
            auto activeBank = bm->getBank(bm->getActiveBankIndex());
            // Проверяем состояние кнопки SHIFT:
            if (shiftButton->getToggleState())
            {
                // Если SHIFT активна, отображаем имена пресетов 4, 5, 6 (индексы 3, 4, 5)
                for (int i = 0; i < presetButtons.size(); ++i)
                {
                    // Предполагается, что presetButtons содержит 3 кнопки для пресетов.
                    if (i < 3)
                        presetButtons[i]->setButtonText(activeBank.presetNames[i + 3]);
                }
            }
            else
            {
                // Если SHIFT не активна, отображаем имена пресетов 1, 2, 3 (индексы 0, 1, 2)
                for (int i = 0; i < presetButtons.size(); ++i)
                {
                    if (i < 3)
                        presetButtons[i]->setButtonText(activeBank.presetNames[i]);
                }
            }
        }
    }


    // --- Функции загрузки и сохранения настроек (копия вашей реализации) ---
    void loadSettings()
    {
        juce::File configFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile(settingsFileName);
        if (!configFile.existsAsFile())
            return;

        juce::PropertiesFile::Options options;
        options.applicationName = "MIDI Preset Scenes";
        juce::PropertiesFile propertiesFile(configFile, options);

        bool shiftState = propertiesFile.getBoolValue("shiftButton", false);
        shiftButton->setToggleState(shiftState, juce::sendNotificationSync);
        shiftCCNumber = propertiesFile.getIntValue("shiftCCNumber", 90);
        upCCNumber = propertiesFile.getIntValue("upCCNumber", 72);
        downCCNumber = propertiesFile.getIntValue("downCCNumber", 73);
        tempoCCNumber = propertiesFile.getIntValue("tempoCCNumber", 74);
        for (int i = 0; i < ccButtons.size(); ++i)
        {
            bool ccState = propertiesFile.getBoolValue("ccButton" + juce::String(i), false);
            ccButtons[i]->setToggleState(ccState, juce::sendNotificationSync);
        }
        for (int preset = 0; preset < (int)presetStates.size(); ++preset)
        {
            juce::String stateStr = propertiesFile.getValue("presetStates_" + juce::String(preset), "");
            if (stateStr.isNotEmpty())
            {
                juce::StringArray tokens;
                tokens.addTokens(stateStr, ",", "");
                for (int i = 0; i < juce::jmin(tokens.size(), (int)presetStates[preset].size()); ++i)
                    presetStates[preset][i] = (tokens[i].getIntValue() != 0);
            }
        }
        for (int i = 0; i < (int)presetCCNumbers.size(); ++i)
            presetCCNumbers[i] = propertiesFile.getIntValue("presetCCNumber" + juce::String(i), 80 + i);

        gainCCNumber = propertiesFile.getIntValue("gainCCNumber", 20);
        volumeCCNumber = propertiesFile.getIntValue("volumeCCNumber", 21);

        // Загружаем значение слайдера Gain
        double savedGain = propertiesFile.getDoubleValue("gainSliderValue", gainSlider->getValue());
        gainSlider->setValue(savedGain, juce::dontSendNotification);

        // Загружаем значения Volume для пресетов
        for (int i = 0; i < (int)presetVolume.size(); i++)
            presetVolume[i] = propertiesFile.getIntValue("presetVolume" + juce::String(i), 64);
        currentPresetIndex = propertiesFile.getIntValue("currentPresetIndex", -1);
        if (currentPresetIndex >= 0 && currentPresetIndex < (int)presetVolume.size())
            volumeSlider->setValue(presetVolume[currentPresetIndex], juce::dontSendNotification);

        // Если нужно автоматически активировать кнопку Preset A (предполагается, что она первая)
        int presetIndex = presetButtons[0]->getToggleState();
        presetButtons[0]->setToggleState(true, juce::sendNotificationSync);
        buttonClicked(presetButtons[0]); // если ваш обработчик отправляет MIDI, либо вызовите метод отправки
        auto msg = juce::MidiMessage::controllerEvent(1, presetCCNumbers[presetIndex], 127);
        handleOutgoingMidiMessage(msg);

        // Пример явного обновления после загрузки настроек:
        bankManager.loadSettings();
        const auto& activeBank = bankManager.getBank(bankManager.getActiveBankIndex());
        bankNameLabel.setText(activeBank.bankName, juce::dontSendNotification);


        updatePresetButtonLabels();
    }

    void saveSettings()
    {
        juce::File configFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile(settingsFileName);
        juce::PropertiesFile::Options options;
        options.applicationName = "MIDI Preset Scenes";
        juce::PropertiesFile propertiesFile(configFile, options);

        propertiesFile.setValue("shiftButton", shiftButton->getToggleState());
        propertiesFile.setValue("shiftCCNumber", shiftCCNumber);
        propertiesFile.setValue("upCCNumber", upCCNumber);
        propertiesFile.setValue("downCCNumber", downCCNumber);
        propertiesFile.setValue("tempoCCNumber", tempoCCNumber);
        for (int i = 0; i < ccButtons.size(); ++i)
            propertiesFile.setValue("ccButton" + juce::String(i), ccButtons[i]->getToggleState());
        for (int preset = 0; preset < (int)presetStates.size(); ++preset)
        {
            juce::String stateStr;
            for (int i = 0; i < presetStates[preset].size(); ++i)
            {
                stateStr += presetStates[preset][i] ? "1" : "0";
                if (i < presetStates[preset].size() - 1)
                    stateStr += ",";
            }
            propertiesFile.setValue("presetStates_" + juce::String(preset), stateStr);
        }
        for (int i = 0; i < (int)presetCCNumbers.size(); ++i)
            propertiesFile.setValue("presetCCNumber" + juce::String(i), presetCCNumbers[i]);

        propertiesFile.setValue("gainCCNumber", gainCCNumber);
        propertiesFile.setValue("volumeCCNumber", volumeCCNumber);
        propertiesFile.setValue("gainSliderValue", gainSlider->getValue());
        for (int i = 0; i < presetVolume.size(); i++)
            propertiesFile.setValue("presetVolume" + juce::String(i), presetVolume[i]);
        propertiesFile.setValue("currentPresetIndex", currentPresetIndex);
        propertiesFile.saveIfNeeded();
    }

    // --- Геттеры и сеттеры для номеров MIDI CC ---
    int getShiftCCNumber() const { return shiftCCNumber; }
    void setShiftCCNumber(int newCC) { shiftCCNumber = newCC; }

    int getUpCCNumber() const { return upCCNumber; }
    void setUpCCNumber(int newCC) { upCCNumber = newCC; }

    int getDownCCNumber() const { return downCCNumber; }
    void setDownCCNumber(int newCC) { downCCNumber = newCC; }

    int getTempoCCNumber() const { return tempoCCNumber; }
    void setTempoCCNumber(int newCC) { tempoCCNumber = newCC; }

    int getGainCCNumber() const { return gainCCNumber; }
    void setGainCCNumber(int newCC) { gainCCNumber = newCC; }

    int getVolumeCCNumber() const { return volumeCCNumber; }
    void setVolumeCCNumber(int newCC) { volumeCCNumber = newCC; }

    int getPresetCCNumber(int index) const { return presetCCNumbers[index]; }
    void setPresetCCNumber(int index, int newCC)
    {
        if (index >= 0 && index < (int)presetCCNumbers.size())
            presetCCNumbers[index] = newCC;
    }



    juce::String getCurrentMidiOutputID() const { return currentMidiOutputID; }

    // Открытие MIDI Output (как в вашем коде)
    void openMidiOut(const juce::MidiDeviceInfo& device)
    {
        DBG("Attempting to open MIDI Output: " << device.name << ", id: " << device.identifier);
        if (midiOut != nullptr)
            midiOut.reset();
        midiOut = juce::MidiOutput::openDevice(device.identifier);
        if (midiOut != nullptr)
        {
            currentMidiOutputID = device.identifier;
            DBG("Opened MIDI Output: " << device.name << " (ID = " << currentMidiOutputID << ")");
        }
        else
        {
            DBG("Failed to open MIDI Output: " << device.name);
        }
    }

    void updateMidiOutputDevice(const juce::MidiDeviceInfo& device)
    {
        openMidiOut(device);
    }

private:
    // --- Компоненты для таббара ---
    std::unique_ptr<juce::TabbedComponent> tabbedComponent;
    std::unique_ptr<juce::Component> mainTab, emptyTab;
    std::unique_ptr<juce::Component> bankTab; // Эта вкладка будет содержать BankManager



    // --- Элементы, которые раньше добавлялись напрямую (теперь во вкладке "Main") ---
    juce::OwnedArray<juce::TextButton> ccButtons;
    juce::OwnedArray<juce::TextButton> presetButtons;
    juce::Label bankNameLabel;

    std::unique_ptr<juce::TextButton> shiftButton, tempoButton, upButton, downButton;
    std::unique_ptr<juce::Slider> gainSlider;
    juce::Label gainLabel;
    std::unique_ptr<juce::Slider> volumeSlider;
    juce::Label volumeLabel;

    std::unique_ptr<juce::MidiOutput> midiOut;
    juce::String currentMidiOutputID;
    int shiftCCNumber, upCCNumber, downCCNumber, tempoCCNumber;
    std::vector<int> presetCCNumbers;
    int gainCCNumber, volumeCCNumber;

    std::vector<std::vector<bool>> presetStates;
    std::vector<int> presetVolume;
    int currentPresetIndex = -1;

    BankManager bankManager;
    


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultiMidiSenderComponent)
};




/**
    MidiManagerContent – компонент для управления MIDI-настройками.

    Позволяет:
      • Выбрать MIDI Input и MIDI Output (комбобоксы заполняются списком доступных устройств).
      • Настроить номера CC для базовых функций (SHIFT, TEMPO, UP, DOWN).
      • Настроить номера CC для новых контроллеров: Gain и Volume.
      • Настроить номера CC для пресетов (A, B, C, D, E, F) через отдельные слайдеры.
      • Сохранять и загружать настройки, включая выбранные порты MIDI.
*/
class MidiManagerContent : public juce::Component,
    public juce::ComboBox::Listener,
    public juce::Slider::Listener,
    public juce::Button::Listener
{
public:
    MidiManagerContent(juce::AudioDeviceManager& adm, MultiMidiSenderComponent* mcomp)
        : deviceManager(adm), midiComp(mcomp)
    {
        setSize(600, 600);

        // --- Инициализация комбобоксов для MIDI Input/Output ---
        midiInputLabel.setText("MIDI Input:", juce::dontSendNotification);
        addAndMakeVisible(midiInputLabel);
        midiInputCombo.addListener(this);
        addAndMakeVisible(midiInputCombo);
        auto inputs = juce::MidiInput::getAvailableDevices();
        int id = 1;
        for (auto& input : inputs)
            midiInputCombo.addItem(input.name, id++);

        midiOutputLabel.setText("MIDI Output:", juce::dontSendNotification);
        addAndMakeVisible(midiOutputLabel);
        midiOutputCombo.addListener(this);
        addAndMakeVisible(midiOutputCombo);
        id = 1;
        auto outputs = juce::MidiOutput::getAvailableDevices();
        for (auto& output : outputs)
            midiOutputCombo.addItem(output.name, id++);

        // --- Кнопка сохранения настроек ---
        saveButton.setButtonText("Save MIDI Settings");
        saveButton.addListener(this);
        addAndMakeVisible(saveButton);

        // --- Слайдеры для базовых MIDI CC: SHIFT, TEMPO, UP, DOWN ---
        // SHIFT CC
        shiftCCSlider.setRange(0, 127, 1);
        if (midiComp != nullptr)
            shiftCCSlider.setValue(midiComp->getShiftCCNumber(), juce::dontSendNotification);
        shiftCCSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        shiftCCSlider.addListener(this);
        addAndMakeVisible(shiftCCSlider);
        shiftCCLabel.setText("SHIFT CC:", juce::dontSendNotification);
        addAndMakeVisible(shiftCCLabel);

        // TEMPO CC
        tempoCCSlider.setRange(0, 127, 1);
        if (midiComp != nullptr)
            tempoCCSlider.setValue(midiComp->getTempoCCNumber(), juce::dontSendNotification);
        tempoCCSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        tempoCCSlider.addListener(this);
        addAndMakeVisible(tempoCCSlider);
        tempoCCLabel.setText("TEMPO CC:", juce::dontSendNotification);
        addAndMakeVisible(tempoCCLabel);

        // UP CC
        upCCSlider.setRange(0, 127, 1);
        if (midiComp != nullptr)
            upCCSlider.setValue(midiComp->getUpCCNumber(), juce::dontSendNotification);
        upCCSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        upCCSlider.addListener(this);
        addAndMakeVisible(upCCSlider);
        upLabel.setText("UP CC:", juce::dontSendNotification);
        addAndMakeVisible(upLabel);

        // DOWN CC
        downCCSlider.setRange(0, 127, 1);
        if (midiComp != nullptr)
            downCCSlider.setValue(midiComp->getDownCCNumber(), juce::dontSendNotification);
        downCCSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        downCCSlider.addListener(this);
        addAndMakeVisible(downCCSlider);
        downLabel.setText("DOWN CC:", juce::dontSendNotification);
        addAndMakeVisible(downLabel);

        // --- Слайдеры для новых контроллеров: Gain и Volume ---
        gainCCSlider.setRange(0, 127, 1);
        if (midiComp != nullptr)
            gainCCSlider.setValue(midiComp->getGainCCNumber(), juce::dontSendNotification);
        gainCCSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        gainCCSlider.addListener(this);
        addAndMakeVisible(gainCCSlider);
        gainCCLabel.setText("Gain CC:", juce::dontSendNotification);
        addAndMakeVisible(gainCCLabel);

        volumeCCSlider.setRange(0, 127, 1);
        if (midiComp != nullptr)
            volumeCCSlider.setValue(midiComp->getVolumeCCNumber(), juce::dontSendNotification);
        volumeCCSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        volumeCCSlider.addListener(this);
        addAndMakeVisible(volumeCCSlider);
        volumeCCLabel.setText("Volume CC:", juce::dontSendNotification);
        addAndMakeVisible(volumeCCLabel);

        // --- Слайдеры для пресетов (A, B, C, D, E, F) ---
        int numPresets = 6;
        for (int i = 0; i < numPresets; ++i)
        {
            auto* slider = new juce::Slider();
            slider->setRange(0, 127, 1);
            // Значения по умолчанию: Preset A = 80, B = 81, и т.д.
            slider->setValue(80 + i, juce::dontSendNotification);
            slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
            slider->addListener(this);
            presetCCSliders.add(slider);
            addAndMakeVisible(slider);

            auto* label = new juce::Label();
            char presetLetter = 'A' + i;
            juce::String labelText;
            labelText << presetLetter << " Preset CC:";
            label->setText(labelText, juce::dontSendNotification);
            presetCCLabels.add(label);
            addAndMakeVisible(label);
        }

        // --- Загрузка сохранённых настроек, включая порты MIDI ---
        juce::File configFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile(settingsFileName);
        if (configFile.existsAsFile())
        {
            juce::PropertiesFile::Options options;
            options.applicationName = "MIDI Preset Scenes";
            juce::PropertiesFile propertiesFile(configFile, options);

            // Передаём сохранённые значения в основной компонент
            if (midiComp != nullptr)
            {
                midiComp->setShiftCCNumber(propertiesFile.getIntValue("shiftCCNumber", midiComp->getShiftCCNumber()));
                midiComp->setTempoCCNumber(propertiesFile.getIntValue("tempoCCNumber", midiComp->getTempoCCNumber()));
                midiComp->setUpCCNumber(propertiesFile.getIntValue("upCCNumber", midiComp->getUpCCNumber()));
                midiComp->setDownCCNumber(propertiesFile.getIntValue("downCCNumber", midiComp->getDownCCNumber()));
                midiComp->setGainCCNumber(propertiesFile.getIntValue("gainCCNumber", midiComp->getGainCCNumber()));
                midiComp->setVolumeCCNumber(propertiesFile.getIntValue("volumeCCNumber", midiComp->getVolumeCCNumber()));
            }
            // Загружаем значения для слайдеров пресетов
            for (int i = 0; i < presetCCSliders.size(); ++i)
            {
                int value = propertiesFile.getIntValue("presetCCNumber" + juce::String(i), 80 + i);
                presetCCSliders[i]->setValue(value, juce::dontSendNotification);
            }
            // Загружаем сохранённые ID выбранных портов MIDI
            savedMidiOutputID = propertiesFile.getValue("midiOutputID", "");
            savedMidiInputID = propertiesFile.getValue("midiInputID", "");
        }

        // После загрузки настроек обновляем выбор в комбобоксах
        updateMidiDeviceSelections();
    }

    ~MidiManagerContent() override
    {
        saveSettings();
    }

    //==============================================================================
    // ComboBox::Listener
    void comboBoxChanged(juce::ComboBox* comboThatChanged) override
    {
        if (comboThatChanged == &midiOutputCombo)
        {
            // Выбор MIDI Output: ищем устройство по имени
            juce::String selectedName = midiOutputCombo.getText();
            auto outputs = juce::MidiOutput::getAvailableDevices();
            for (auto& dev : outputs)
            {
                if (dev.name == selectedName)
                {
                    if (midiComp != nullptr)
                        midiComp->openMidiOut(dev);
                    // Сохраняем идентификатор выбранного устройства
                    savedMidiOutputID = dev.identifier;
                    break;
                }
            }
        }
        else if (comboThatChanged == &midiInputCombo)
        {
            // Обрабатываем выбор MIDI Input: сначала отключаем все
            auto inputs = juce::MidiInput::getAvailableDevices();
            int index = midiInputCombo.getSelectedItemIndex();
            for (auto& dev : inputs)
                deviceManager.setMidiInputDeviceEnabled(dev.identifier, false);
            if (index >= 0 && index < inputs.size())
            {
                deviceManager.setMidiInputDeviceEnabled(inputs[index].identifier, true);
                savedMidiInputID = inputs[index].identifier;
                DBG("Selected MIDI Input: " << inputs[index].name);
            }
        }
    }

    //==============================================================================
    // Slider::Listener
    void sliderValueChanged(juce::Slider* slider) override
    {
        if (slider == &shiftCCSlider)
        {
            if (midiComp != nullptr)
                midiComp->setShiftCCNumber(static_cast<int> (shiftCCSlider.getValue()));
        }
        else if (slider == &tempoCCSlider)
        {
            if (midiComp != nullptr)
                midiComp->setTempoCCNumber(static_cast<int> (tempoCCSlider.getValue()));
        }
        else if (slider == &upCCSlider)
        {
            if (midiComp != nullptr)
                midiComp->setUpCCNumber(static_cast<int> (upCCSlider.getValue()));
        }
        else if (slider == &downCCSlider)
        {
            if (midiComp != nullptr)
                midiComp->setDownCCNumber(static_cast<int> (downCCSlider.getValue()));
        }
        else if (slider == &gainCCSlider)
        {
            if (midiComp != nullptr)
                midiComp->setGainCCNumber(static_cast<int> (gainCCSlider.getValue()));
        }
        else if (slider == &volumeCCSlider)
        {
            if (midiComp != nullptr)
                midiComp->setVolumeCCNumber(static_cast<int> (volumeCCSlider.getValue()));
        }
        else
        {
            // Обработка слайдеров для пресетов (A–F)
            for (int i = 0; i < presetCCSliders.size(); ++i)
            {
                if (slider == presetCCSliders[i])
                {
                    int value = static_cast<int> (presetCCSliders[i]->getValue());
                    if (midiComp != nullptr)
                        midiComp->setPresetCCNumber(i, value);
                    // Отправка сообщения по желанию (здесь можно вставить логику отправки)
                    break;
                }
            }
        }
    }

    //==============================================================================
    // Button::Listener
    void buttonClicked(juce::Button* button) override
    {
        if (button == &saveButton)
        {
            saveSettings();
        }
    }

    //==============================================================================
    // Раскладка компонентов
    void resized() override
    {
        auto area = getLocalBounds().reduced(10);

        // Верхняя секция – MIDI Input/Output
        auto topArea = area.removeFromTop(100);
        midiInputLabel.setBounds(topArea.removeFromTop(20));
        midiInputCombo.setBounds(topArea.removeFromTop(30));
        midiOutputLabel.setBounds(topArea.removeFromTop(20));
        midiOutputCombo.setBounds(topArea.removeFromTop(30));

        // Средняя секция – слайдеры для базовых CC (SHIFT, TEMPO, UP, DOWN)
        int sliderRowHeight = 40;
        auto sliderArea = area.removeFromTop(sliderRowHeight * 4);
        auto row = sliderArea.removeFromTop(sliderRowHeight);
        shiftCCLabel.setBounds(row.removeFromLeft(100));
        shiftCCSlider.setBounds(row);

        row = sliderArea.removeFromTop(sliderRowHeight);
        tempoCCLabel.setBounds(row.removeFromLeft(100));
        tempoCCSlider.setBounds(row);

        row = sliderArea.removeFromTop(sliderRowHeight);
        upLabel.setBounds(row.removeFromLeft(100));
        upCCSlider.setBounds(row);

        row = sliderArea.removeFromTop(sliderRowHeight);
        downLabel.setBounds(row.removeFromLeft(100));
        downCCSlider.setBounds(row);

        // Следующая секция – слайдеры для Gain и Volume CC
        int extraRowHeight = 40;
        row = area.removeFromTop(extraRowHeight);
        gainCCLabel.setBounds(row.removeFromLeft(100));
        gainCCSlider.setBounds(row);

        row = area.removeFromTop(extraRowHeight);
        volumeCCLabel.setBounds(row.removeFromLeft(100));
        volumeCCSlider.setBounds(row);

        // Нижняя секция – слайдеры для пресетов (A–F)
        int presetSliderHeight = 40;
        for (int i = 0; i < presetCCSliders.size(); ++i)
        {
            auto row = area.removeFromTop(presetSliderHeight);
            presetCCLabels[i]->setBounds(row.removeFromLeft(100));
            presetCCSliders[i]->setBounds(row.reduced(10, 5));
        }

        // В самом низу – кнопка сохранения настроек
        saveButton.setBounds(area.removeFromBottom(40));
    }

    //==============================================================================
    // Сохранение настроек в файл
    void saveSettings()
    {
        juce::File configFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile(settingsFileName);
        juce::PropertiesFile::Options options;
        options.applicationName = "MIDI Preset Scenes";
        juce::PropertiesFile propertiesFile(configFile, options);

        propertiesFile.setValue("shiftCCNumber", static_cast<int> (shiftCCSlider.getValue()));
        propertiesFile.setValue("tempoCCNumber", static_cast<int> (tempoCCSlider.getValue()));
        propertiesFile.setValue("upCCNumber", static_cast<int> (upCCSlider.getValue()));
        propertiesFile.setValue("downCCNumber", static_cast<int> (downCCSlider.getValue()));
        propertiesFile.setValue("gainCCNumber", static_cast<int> (gainCCSlider.getValue()));
        propertiesFile.setValue("volumeCCNumber", static_cast<int> (volumeCCSlider.getValue()));

        for (int i = 0; i < presetCCSliders.size(); ++i)
        {
            propertiesFile.setValue("presetCCNumber" + juce::String(i),
                static_cast<int> (presetCCSliders[i]->getValue()));
        }

        // Сохраняем выбранные MIDI порты (записываем уникальные идентификаторы)
        propertiesFile.setValue("midiOutputID", savedMidiOutputID);
        propertiesFile.setValue("midiInputID", savedMidiInputID);

        propertiesFile.saveIfNeeded();
    }

private:
    juce::AudioDeviceManager& deviceManager;
    MultiMidiSenderComponent* midiComp = nullptr;

    juce::ComboBox midiInputCombo, midiOutputCombo;
    juce::Label midiInputLabel{ "midiInputLabel", "MIDI Input:" };
    juce::Label midiOutputLabel{ "midiOutputLabel", "MIDI Output:" };

    juce::TextButton saveButton{ "Save MIDI Settings" };

    juce::Slider shiftCCSlider, tempoCCSlider, upCCSlider, downCCSlider;
    juce::Label  shiftCCLabel{ "shiftCCLabel", "SHIFT CC:" };
    juce::Label  tempoCCLabel{ "tempoCCLabel", "TEMPO CC:" };
    juce::Label  upLabel{ "upLabel",       "UP CC:" };
    juce::Label  downLabel{ "downLabel",     "DOWN CC:" };

    juce::Slider gainCCSlider, volumeCCSlider;
    juce::Label  gainCCLabel{ "gainCCLabel",   "Gain CC:" };
    juce::Label  volumeCCLabel{ "volumeCCLabel", "Volume CC:" };

    // Слайдеры и метки для пресетов (A-F)
    juce::OwnedArray<juce::Slider> presetCCSliders;
    juce::OwnedArray<juce::Label>  presetCCLabels;

    // Сохраняемые ID выбранных MIDI портов
    juce::String savedMidiOutputID, savedMidiInputID;

    // Метод для обновления выбранных портов в комбобоксах после загрузки настроек
    void updateMidiDeviceSelections()
    {
        // Обновляем для MIDI OUTPUT
        if (savedMidiOutputID.isNotEmpty())
        {
            auto outputs = juce::MidiOutput::getAvailableDevices();
            for (auto& dev : outputs)
            {
                if (dev.identifier == savedMidiOutputID)
                {
                    midiOutputCombo.setText(dev.name, juce::dontSendNotification);
                    break;
                }
            }
        }
        // Обновляем для MIDI INPUT
        if (savedMidiInputID.isNotEmpty())
        {
            auto inputs = juce::MidiInput::getAvailableDevices();
            for (auto& dev : inputs)
            {
                if (dev.identifier == savedMidiInputID)
                {
                    midiInputCombo.setText(dev.name, juce::dontSendNotification);
                    break;
                }
            }
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiManagerContent)
};
//==============================================================================
// MidiManagerWindow — Окно настроек MIDI.
class MidiManagerWindow : public juce::DocumentWindow
{
public:
    MidiManagerWindow(juce::AudioDeviceManager& adm, MultiMidiSenderComponent* mcomp)
        : DocumentWindow("MIDI Settings",
            juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId),
            DocumentWindow::closeButton),
        deviceManager(adm)
    {
        setUsingNativeTitleBar(true);
        // Увеличиваем окно настроек MIDI:
        setResizeLimits(800, 800, 1200, 800);
        midiManagerContent.reset(new MidiManagerContent(deviceManager, mcomp));
        setContentOwned(midiManagerContent.release(), true);
        centreWithSize(800, 600);
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        setVisible(false);
        delete this;
    }

private:
    juce::AudioDeviceManager& deviceManager;
    std::unique_ptr<MidiManagerContent> midiManagerContent;
};

//==============================================================================
// AudioManagerContent — Компонент для аудио настроек.
class AudioManagerContent : public juce::Component,
    public juce::Button::Listener
{
public:
    AudioManagerContent(juce::AudioDeviceManager& adm, MultiMidiSenderComponent* mcomp)
        : deviceManager(adm), midiComp(mcomp)
    {
        selector.reset(new juce::AudioDeviceSelectorComponent(deviceManager,
            0, 2, 0, 2,
            false, false, true, false));
        addAndMakeVisible(selector.get());

        applyButton.reset(new juce::TextButton("Save Audio"));
        applyButton->setSize(150, 40);
        applyButton->addListener(this);
        addAndMakeVisible(applyButton.get());
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        auto buttonArea = bounds.removeFromBottom(40);
        applyButton->setBounds(buttonArea.withWidth(150).withCentre(buttonArea.getCentre()));
        selector->setBounds(bounds);
    }

    void buttonClicked(juce::Button* button) override
    {
        if (button == applyButton.get())
        {
            juce::AudioDeviceManager::AudioDeviceSetup setup;
            deviceManager.getAudioDeviceSetup(setup);
            juce::String error = deviceManager.setAudioDeviceSetup(setup, true);
            if (error.isEmpty())
            {
                std::unique_ptr<juce::XmlElement> xml(deviceManager.createStateXml());
                if (xml != nullptr)
                {
                    juce::File settingsFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("AudioDeviceSettings.xml");
                    settingsFile.replaceWithText(xml->toString());
                }
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                    "Audio Manager",
                    "Audio settings applied and saved.");
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                    "Audio Manager",
                    "Failed to apply audio settings: " + error);
            }
        }
    }

    void autoSaveSettings()
    {
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        deviceManager.getAudioDeviceSetup(setup);
        juce::String error = deviceManager.setAudioDeviceSetup(setup, true);
        if (error.isEmpty())
        {
            std::unique_ptr<juce::XmlElement> xml(deviceManager.createStateXml());
            if (xml != nullptr)
            {
                juce::File settingsFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                    .getChildFile("AudioDeviceSettings.xml");
                settingsFile.replaceWithText(xml->toString());
                DBG("Audio settings saved automatically.");
            }
        }
        else
        {
            DBG("Failed to save audio settings: " << error);
        }
    }

private:
    juce::AudioDeviceManager& deviceManager;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> selector;
    std::unique_ptr<juce::TextButton> applyButton;
    MultiMidiSenderComponent* midiComp = nullptr;
};

//==============================================================================
// AudioManagerWindow — Окно для аудио настроек.
class AudioManagerWindow : public juce::DocumentWindow
{
public:
    AudioManagerWindow(juce::AudioDeviceManager& adm, MultiMidiSenderComponent* mcomp)
        : DocumentWindow("Audio Settings",
            juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId),
            DocumentWindow::closeButton),
        deviceManager(adm)
    {
        setUsingNativeTitleBar(true);
        setResizable(false, false);
        setSize(600, 400);
        audioManagerContent.reset(new AudioManagerContent(deviceManager, mcomp));
        setContentOwned(audioManagerContent.release(), true);
        centreWithSize(600, 400);
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        if (auto* amContent = dynamic_cast<AudioManagerContent*>(getContentComponent()))
            amContent->autoSaveSettings();
        setVisible(false);
        delete this;
    }

private:
    juce::AudioDeviceManager& deviceManager;
    std::unique_ptr<AudioManagerContent> audioManagerContent;
};

//==============================================================================
// MainMenu — Главное меню настроек.
class MainMenu : public juce::MenuBarModel
{
public:
    MainMenu(MultiMidiSenderComponent* mcomp, juce::AudioDeviceManager& adm,
        std::function<void()> toggleKioskCallback)
        : midiComp(mcomp), deviceManager(adm), toggleKioskCallback(toggleKioskCallback)
    { }

    juce::StringArray getMenuBarNames() override { return { "Settings" }; }

    juce::PopupMenu getMenuForIndex(int /*menuIndex*/, const juce::String&) override
    {
        juce::PopupMenu menu;
        menu.addItem(300, "Audio Settings");
        menu.addItem(400, "MIDI Settings");
        menu.addSeparator();
        menu.addItem(1, "Save MIDI Settings");
        menu.addItem(2, "Load MIDI Settings");
        menu.addSeparator();
        menu.addItem(101, "Toggle Kiosk Mode");
        menu.addSeparator();
        menu.addItem(99, "Exit");
        return menu;
    }

    void menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/) override
    {
        if (menuItemID == 300)
        {
            new AudioManagerWindow(deviceManager, midiComp);
        }
        else if (menuItemID == 400)
        {
            new MidiManagerWindow(deviceManager, midiComp);
        }
        else if (menuItemID == 1)
        {
            if (midiComp != nullptr)
                midiComp->saveSettings();
        }
        else if (menuItemID == 2)
        {
            if (midiComp != nullptr)
                midiComp->loadSettings();
        }
        else if (menuItemID == 101)
        {
            if (toggleKioskCallback)
                toggleKioskCallback();
        }
        else if (menuItemID == 99)
        {
            if (midiComp != nullptr)
                midiComp->saveSettings();
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    }

    void applicationCommandInvoked(const juce::ApplicationCommandTarget::InvocationInfo&) override { }
    void applicationCommandListChanged() override { }

private:
    MultiMidiSenderComponent* midiComp;
    juce::AudioDeviceManager& deviceManager;
    std::function<void()> toggleKioskCallback;
};

//==============================================================================
// MainWindow — Главное окно приложения с поддержкой kiosk mode.
class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow(const juce::String& name, MultiMidiSenderComponent* mcomp, juce::AudioDeviceManager& adm)
        : DocumentWindow(name,
            juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId),
            DocumentWindow::closeButton | DocumentWindow::minimiseButton | DocumentWindow::maximiseButton),
        deviceManager(adm), mainComponent(mcomp), kioskModeEnabled(false)
    {
        setUsingNativeTitleBar(false);
        setContentOwned(mcomp, true);
        setResizable(true, true);
        centreWithSize(800, 300);
        setVisible(true);

        mainMenu.reset(new MainMenu(mcomp, deviceManager, [this]() { this->toggleKioskMode(); }));
        setMenuBar(mainMenu.get(), 25);

        // Восстанавливаем состояние kiosk mode.
        {
            juce::PropertiesFile::Options options;
            options.applicationName = "MIDI Preset Scenes";
            juce::File configFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile(settingsFileName);
            juce::PropertiesFile props(configFile, options);
            bool savedKioskMode = props.getBoolValue("kioskMode", false);
            if (savedKioskMode && !kioskModeEnabled)
                toggleKioskMode();
        }
    }

    void saveKioskModeSetting(bool kioskMode)
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "MIDI Preset Scenes";
        juce::File configFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile(settingsFileName);
        juce::PropertiesFile props(configFile, options);
        props.setValue("kioskMode", kioskMode);
        props.saveIfNeeded();
    }

    void toggleKioskMode()
    {
        kioskModeEnabled = !kioskModeEnabled;
        if (kioskModeEnabled)
        {
            setTitleBarButtonsRequired(0, false);
            setFullScreen(true);
            DBG("Kiosk mode ON: full screen enabled.");
        }
        else
        {
            setFullScreen(false);
            setTitleBarButtonsRequired(DocumentWindow::closeButton | DocumentWindow::minimiseButton | DocumentWindow::maximiseButton, true);
            DBG("Kiosk mode OFF: window controls restored.");
        }
        saveKioskModeSetting(kioskModeEnabled);
    }

    void closeButtonPressed() override
    {
        if (auto* comp = dynamic_cast<MultiMidiSenderComponent*>(getContentComponent()))
            comp->saveSettings();
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    juce::AudioDeviceManager& deviceManager;
    MultiMidiSenderComponent* mainComponent;
    std::unique_ptr<MainMenu> mainMenu;
    bool kioskModeEnabled;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};

//==============================================================================
// MidiSenderApplication — Главный класс приложения.
class MidiSenderApplication : public juce::JUCEApplication
{
public:
    MidiSenderApplication() { }

    const juce::String getApplicationName() override { return "MIDI Preset Scenes"; }
    const juce::String getApplicationVersion() override { return "1.0"; }

    juce::AudioDeviceManager& getAudioDeviceManager() { return audioDeviceManager; }

    void initialise(const juce::String& /*commandLine*/) override
    {
        // Восстанавливаем аудио настройки.
        {
            juce::File audioSettingsFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile("AudioDeviceSettings.xml");
            if (audioSettingsFile.existsAsFile())
            {
                std::unique_ptr<juce::XmlElement> audioXml(juce::XmlDocument::parse(audioSettingsFile));
                if (audioXml != nullptr)
                {
                    audioDeviceManager.initialise(0, 2, audioXml.get(), true);
                    DBG("Audio settings restored.");
                }
                else
                {
                    DBG("Failed to parse AudioDeviceSettings.xml, using default settings.");
                    audioDeviceManager.initialise(0, 2, nullptr, true);
                }
            }
            else
            {
                audioDeviceManager.initialise(0, 2, nullptr, true);
            }
        }

        // Восстанавливаем сохраненный MIDI вход.
        {
            juce::File configFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile(settingsFileName);
            juce::PropertiesFile::Options options;
            options.applicationName = "MIDI Preset Scenes";
            juce::PropertiesFile propertiesFile(configFile, options);
            juce::String midiInputId = propertiesFile.getValue(midiInputKey, "");
            if (!midiInputId.isEmpty())
            {
                auto midiInputs = juce::MidiInput::getAvailableDevices();
                for (auto& device : midiInputs)
                {
                    if (device.identifier == midiInputId)
                    {
                        audioDeviceManager.setMidiInputDeviceEnabled(device.identifier, true);
                        DBG("Restored MIDI Input: " << device.name);
                        break;
                    }
                }
            }
        }

        // Восстанавливаем сохраненный MIDI выход.
        {
            juce::File configFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile(settingsFileName);
            juce::PropertiesFile::Options options;
            options.applicationName = "MIDI Preset Scenes";
            juce::PropertiesFile propertiesFile(configFile, options);
            juce::String midiOutputId = propertiesFile.getValue(midiOutputKey, "");
            if (!midiOutputId.isEmpty())
            {
                auto midiOutputs = juce::MidiOutput::getAvailableDevices();
                for (auto& dev : midiOutputs)
                {
                    if (dev.identifier == midiOutputId)
                    {
                        if (mainComponent == nullptr)
                            mainComponent = new MultiMidiSenderComponent();
                        mainComponent->updateMidiOutputDevice(dev);
                        DBG("Restored MIDI Output: " << dev.name << " (ID=" << dev.identifier << ")");
                        break;
                    }
                }
            }
            else
            {
                auto midiOutputs = juce::MidiOutput::getAvailableDevices();
                if (midiOutputs.size() > 0)
                {
                    DBG("No saved MIDI Output found. Selecting: " << midiOutputs[0].name);
                    if (mainComponent == nullptr)
                        mainComponent = new MultiMidiSenderComponent();
                    mainComponent->updateMidiOutputDevice(midiOutputs[0]);
                }
                else
                {
                    DBG("No available MIDI Output devices.");
                }
            }
        }

        if (mainComponent == nullptr)
            mainComponent = new MultiMidiSenderComponent();

        audioDeviceManager.addMidiInputDeviceCallback("", mainComponent);
        mainWindow.reset(new MainWindow(getApplicationName(), mainComponent, audioDeviceManager));
    }

    void shutdown() override
    {
        // Сохранение MIDI входа.
        {
            auto midiDevices = juce::MidiInput::getAvailableDevices();
            juce::String enabledMidiInputId;
            for (auto& device : midiDevices)
            {
                if (audioDeviceManager.isMidiInputDeviceEnabled(device.identifier))
                {
                    enabledMidiInputId = device.identifier;
                    break;
                }
            }
            if (!enabledMidiInputId.isEmpty())
            {
                juce::File configFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                    .getChildFile(settingsFileName);
                juce::PropertiesFile::Options options;
                options.applicationName = "MIDI Preset Scenes";
                juce::PropertiesFile propertiesFile(configFile, options);
                propertiesFile.setValue(midiInputKey, enabledMidiInputId);
                propertiesFile.saveIfNeeded();
                DBG("Saved MIDI Input: " << enabledMidiInputId);
            }
        }

        // Сохранение MIDI выхода.
        {
            juce::String enabledMidiOutputId = mainComponent->getCurrentMidiOutputID();
            DBG("Before saving, MIDI Output ID = " << enabledMidiOutputId);
            if (!enabledMidiOutputId.isEmpty())
            {
                juce::File configFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                    .getChildFile(settingsFileName);
                juce::PropertiesFile::Options options;
                options.applicationName = "MIDI Preset Scenes";
                juce::PropertiesFile propertiesFile(configFile, options);
                propertiesFile.setValue(midiOutputKey, enabledMidiOutputId);
                propertiesFile.saveIfNeeded();
                DBG("Saved MIDI Output: " << enabledMidiOutputId);
            }
        }

        mainWindow = nullptr;
        mainComponent = nullptr;
    }

private:
    juce::AudioDeviceManager audioDeviceManager;
    std::unique_ptr<MainWindow> mainWindow;
    MultiMidiSenderComponent* mainComponent = nullptr;
};

START_JUCE_APPLICATION(MidiSenderApplication)