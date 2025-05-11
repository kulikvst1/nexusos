#include <JuceHeader.h>
#include <vector>
#include <functional>

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
// MultiMidiSenderComponent — Компонент для отправки MIDI-сообщений,
// работы с кнопками (CC, пресеты, SHIFT, TEMPO, UP, DOWN) и сохранения/загрузки настроек.
class MultiMidiSenderComponent : public juce::Component,
    public juce::Button::Listener,
    public juce::MidiInputCallback
{
public:
    MultiMidiSenderComponent()
    {
        // Создаем 10 кнопок для MIDI CC.
        for (int i = 0; i < 10; ++i)
        {
            auto* btn = new juce::TextButton("CC " + juce::String(i + 1));
            btn->setClickingTogglesState(true);
            btn->setToggleState(false, juce::dontSendNotification);
            btn->setColour(juce::TextButton::buttonOnColourId, juce::Colours::red);
            btn->addListener(this);
            addAndMakeVisible(btn);
            ccButtons.add(btn);
        }

        // Создаем 3 кнопки-пресета (группа из 3 кнопок для пресетов A–C или D–F в зависимости от состояния SHIFT).
        for (int i = 0; i < 3; ++i)
        {
            auto* preset = new juce::TextButton();
            preset->setClickingTogglesState(true);
            preset->setRadioGroupId(100, juce::dontSendNotification);
            preset->setToggleState(false, juce::dontSendNotification);
            preset->setColour(juce::TextButton::buttonOnColourId, juce::Colours::blue);
            // Функция getPresetLabel теперь учитывает состояние SHIFT.
            preset->setButtonText(getPresetLabel(i));
            preset->addListener(this);
            addAndMakeVisible(preset);
            presetButtons.add(preset);
        }

        // Кнопка SHIFT.
        shiftButton = std::make_unique<juce::TextButton>("SHIFT");
        shiftButton->setClickingTogglesState(true);
        shiftButton->setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange);
        shiftButton->addListener(this);
        addAndMakeVisible(shiftButton.get());

        // Кнопки TEMPO, UP и DOWN.
        tempoButton = std::make_unique<juce::TextButton>("TEMPO");
        tempoButton->setClickingTogglesState(true);
        tempoButton->setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange);
        tempoButton->addListener(this);
        addAndMakeVisible(tempoButton.get());

        upButton = std::make_unique<juce::TextButton>("UP");
        upButton->setClickingTogglesState(true);
        upButton->setColour(juce::TextButton::buttonOnColourId, juce::Colours::green);
        upButton->addListener(this);
        addAndMakeVisible(upButton.get());

        downButton = std::make_unique<juce::TextButton>("DOWN");
        downButton->setClickingTogglesState(true);
        downButton->setColour(juce::TextButton::buttonOnColourId, juce::Colours::yellow);
        downButton->addListener(this);
        addAndMakeVisible(downButton.get());

        // Данные для пресетов: 6 групп (A-F) по 10 значений для каждой CC кнопки.
        presetStates.resize(6, std::vector<bool>(10, false));

        // MIDI CC номера по умолчанию.
        shiftCCNumber = 90;
        upCCNumber = 72;
        downCCNumber = 73;
        tempoCCNumber = 74;
        // Для пресетов: A=80, B=81, C=82, D=83, E=84, F=85.
        presetCCNumbers.resize(6);
        for (int i = 0; i < 6; ++i)
            presetCCNumbers[i] = 80 + i;

        loadSettings();
    }

    ~MultiMidiSenderComponent() override
    {
        saveSettings();
        for (auto* btn : ccButtons)
            btn->removeListener(this);
        for (auto* btn : presetButtons)
            btn->removeListener(this);
        if (shiftButton)
            shiftButton->removeListener(this);
        if (tempoButton)
            tempoButton->removeListener(this);
        if (upButton)
            upButton->removeListener(this);
        if (downButton)
            downButton->removeListener(this);
    }

    void handleOutgoingMidiMessage(const juce::MidiMessage& message)
    {
        juce::Timer::callAfterDelay(1, [this, message]()
            {
                if (midiOut != nullptr)
                    midiOut->sendMessageNow(message);
                else
                    DBG("MIDI Output not available: " << message.getDescription());
            });
    }

    // Геттеры/сеттеры для настроек.
    int getShiftCCNumber() const { return shiftCCNumber; }
    void setShiftCCNumber(int newCC) { shiftCCNumber = newCC; }

    int getUpCCNumber() const { return upCCNumber; }
    void setUpCCNumber(int newCC) { upCCNumber = newCC; }

    int getDownCCNumber() const { return downCCNumber; }
    void setDownCCNumber(int newCC) { downCCNumber = newCC; }

    int getTempoCCNumber() const { return tempoCCNumber; }
    void setTempoCCNumber(int newCC) { tempoCCNumber = newCC; }

    int getPresetCCNumber(int index) const { return presetCCNumbers[index]; }
    void setPresetCCNumber(int index, int newCC)
    {
        if (index >= 0 && index < (int)presetCCNumbers.size())
            presetCCNumbers[index] = newCC;
    }

    juce::String getCurrentMidiOutputID() const { return currentMidiOutputID; }

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

    // Обработка нажатия на кнопки.
    void buttonClicked(juce::Button* button) override
    {
        // Обработка кнопок TEMPO, UP, DOWN.
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
        if (button == upButton.get())
        {
            bool state = upButton->getToggleState();
            int ccValue = state ? 127 : 0;
            if (midiOut != nullptr)
            {
                auto msg = juce::MidiMessage::controllerEvent(1, upCCNumber, (juce::uint8)ccValue);
                handleOutgoingMidiMessage(msg);
            }
            return;
        }
        if (button == downButton.get())
        {
            bool state = downButton->getToggleState();
            int ccValue = state ? 127 : 0;
            if (midiOut != nullptr)
            {
                auto msg = juce::MidiMessage::controllerEvent(1, downCCNumber, (juce::uint8)ccValue);
                handleOutgoingMidiMessage(msg);
            }
            return;
        }

        // Обработка кнопок CC.
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
                    presetStates[presetIndex][i] = state;
                }
                return;
            }
        }

        // Обработка кнопки SHIFT.
        if (button == shiftButton.get())
        {
            updatePresetButtonLabels();
            bool shiftState = shiftButton->getToggleState();
            if (midiOut != nullptr)
            {
                auto msg = juce::MidiMessage::controllerEvent(1, shiftCCNumber,
                    (juce::uint8)(shiftState ? 127 : 0));
                handleOutgoingMidiMessage(msg);
            }
            return;
        }
        //////////////////////////////////////////////////////////////////////////////
        // Обработка кнопок-пресетов.
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
                // Если SHIFT выключен – пресеты A–C, иначе D–F.
                int presetIndex = shiftButton->getToggleState() ? (3 + clickedIndex) : clickedIndex;
                for (int j = 0; j < ccButtons.size(); ++j)
                {
                    bool presetState = presetStates[presetIndex][j];
                    ccButtons[j]->setToggleState(presetState, juce::dontSendNotification);
                    int ccValue = presetState ? 64 : 0;
                    int ccNumber = j + 1;
                    if (midiOut != nullptr)
                    {
                        auto msg = juce::MidiMessage::controllerEvent(1, ccNumber, (juce::uint8)ccValue);
                        handleOutgoingMidiMessage(msg);
                    }
                }
            }
            // Отправляем сообщения для пресетных кнопок с настраиваемыми CC.
            for (int i = 0; i < presetButtons.size(); ++i)
            {
                int presetIndex = shiftButton->getToggleState() ? (3 + i) : i;
                int messageValue = presetButtons[i]->getToggleState() ? 127 : 0;
                if (midiOut != nullptr)
                {
                    auto msg = juce::MidiMessage::controllerEvent(1, presetCCNumbers[presetIndex], (juce::uint8)messageValue);
                    handleOutgoingMidiMessage(msg);
                }
            }
        }
    }

    // Обработка входящих MIDI-сообщений.
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

        // Обработка сообщения для SHIFT.
        if (controller == shiftCCNumber)
        {
            bool newState = (message.getControllerValue() > 0);
            shiftButton->setToggleState(newState, juce::sendNotificationSync);
            updatePresetButtonLabels();
            if (midiOut != nullptr)
            {
                auto msg = juce::MidiMessage::controllerEvent(1, shiftCCNumber,
                    (juce::uint8)(newState ? 127 : 0));
                handleOutgoingMidiMessage(msg);
            }
        }
        else
        {
            // Ищем соответствие для пресетных настроек.
            for (int i = 0; i < (int)presetCCNumbers.size(); i++)
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

    // Формирование метки пресета с учетом состояния SHIFT.
    // Если SHIFT включен, добавляется смещение 3 (т.е. A->D, B->E, C->F).
    juce::String getPresetLabel(int index)
    {
        int offset = (shiftButton && shiftButton->getToggleState()) ? 3 : 0;
        char letter = static_cast<char>('A' + index + offset);
        char buf[2] = { letter, '\0' };
        return juce::String(buf);
    }

    void updatePresetButtonLabels()
    {
        for (int i = 0; i < presetButtons.size(); ++i)
            presetButtons[i]->setButtonText(getPresetLabel(i));
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        int totalH = bounds.getHeight();
        int topAreaHeight = int(totalH * 0.3f);
        auto ccArea = bounds.removeFromTop(topAreaHeight);
        int ccButtonWidth = ccArea.getWidth() / ccButtons.size();
        for (int i = 0; i < ccButtons.size(); ++i)
            ccButtons[i]->setBounds(ccArea.removeFromLeft(ccButtonWidth).reduced(2));

        auto bottomArea = bounds;
        int rowHeight = bottomArea.getHeight() / 3;
        int colWidth = bottomArea.getWidth() / 3;

        auto leftCol = bottomArea.removeFromLeft(colWidth).reduced(2);
        auto presetA = leftCol.removeFromBottom(rowHeight);
        if (!presetButtons.isEmpty())
            presetButtons[0]->setBounds(presetA);
        int presetAX = presetA.getX();
        {
            auto upRect = leftCol.removeFromTop(rowHeight);
            auto upBounds = upRect.withSizeKeepingCentre(rowHeight, rowHeight);
            upBounds.setX(presetAX);
            upButton->setBounds(upBounds);

            auto downRect = leftCol.removeFromTop(rowHeight);
            auto downBounds = downRect.withSizeKeepingCentre(rowHeight, rowHeight);
            downBounds.setX(presetAX);
            downButton->setBounds(downBounds);
        }

        auto centerCol = bottomArea.removeFromLeft(colWidth).reduced(2);
        auto presetB = centerCol.removeFromBottom(rowHeight);
        if (presetButtons.size() > 1)
            presetButtons[1]->setBounds(presetB);

        auto rightCol = bottomArea.removeFromRight(colWidth).reduced(2);
        auto presetC = rightCol.removeFromBottom(rowHeight);
        if (presetButtons.size() > 2)
            presetButtons[2]->setBounds(presetC);

        int presetCRight = presetC.getRight();
        {
            auto tempoRect = rightCol.removeFromTop(rowHeight);
            auto tempoBounds = tempoRect.withSizeKeepingCentre(rowHeight, rowHeight);
            tempoBounds.setX(presetCRight - rowHeight);
            tempoButton->setBounds(tempoBounds);

            auto shiftRect = rightCol.removeFromTop(rowHeight);
            auto shiftBounds = shiftRect.withSizeKeepingCentre(rowHeight, rowHeight);
            shiftBounds.setX(presetCRight - rowHeight);
            shiftButton->setBounds(shiftBounds);
        }
    }

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
        for (int i = 0; i < (int)presetCCNumbers.size(); i++)
        {
            presetCCNumbers[i] = propertiesFile.getIntValue("presetCCNumber" + juce::String(i), 80 + i);
        }
        updatePresetButtonLabels();
    }
    //////////////////////////////////////////////////////////////////////////////////
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
        for (int i = 0; i < (int)presetCCNumbers.size(); i++)
        {
            propertiesFile.setValue("presetCCNumber" + juce::String(i), presetCCNumbers[i]);
        }
        propertiesFile.saveIfNeeded();
    }

private:
    juce::OwnedArray<juce::TextButton> ccButtons;
    juce::OwnedArray<juce::TextButton> presetButtons;
    std::vector<std::vector<bool>> presetStates;
    std::unique_ptr<juce::MidiOutput> midiOut;
    std::unique_ptr<juce::TextButton> shiftButton, tempoButton, upButton, downButton;
    juce::String currentMidiOutputID;
    int shiftCCNumber, upCCNumber, downCCNumber, tempoCCNumber;
    std::vector<int> presetCCNumbers;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultiMidiSenderComponent)
};

//==============================================================================
// MidiManagerContent — Компонент для управления MIDI-настройками.
class MidiManagerContent : public juce::Component,
    public juce::ComboBox::Listener,
    public juce::Slider::Listener,
    public juce::Button::Listener
{
public:
    MidiManagerContent(juce::AudioDeviceManager& adm, MultiMidiSenderComponent* mcomp)
        : deviceManager(adm), midiComp(mcomp),
        tempoLabel("tempoLabel", "TEMPO CC:"), upLabel("upLabel", "UP CC:"), downLabel("downLabel", "DOWN CC:")
    {
        midiInputLabel.setText("MIDI Input:", juce::dontSendNotification);
        midiOutputLabel.setText("MIDI Output:", juce::dontSendNotification);
        addAndMakeVisible(midiInputLabel);
        addAndMakeVisible(midiOutputLabel);

        addAndMakeVisible(midiInputCombo);
        midiInputCombo.addListener(this);
        auto midiInputs = juce::MidiInput::getAvailableDevices();
        int id = 1;
        for (auto& device : midiInputs)
            midiInputCombo.addItem(device.name, id++);

        addAndMakeVisible(midiOutputCombo);
        midiOutputCombo.addListener(this);
        auto midiOutputs = juce::MidiOutput::getAvailableDevices();
        id = 1;
        for (auto& device : midiOutputs)
            midiOutputCombo.addItem(device.name, id++);

        addAndMakeVisible(saveButton);
        saveButton.setButtonText("Save MIDI Settings");
        saveButton.addListener(this);

        // Слайдер для SHIFT CC.
        shiftCCSlider.setRange(0, 127, 1);
        if (midiComp != nullptr)
            shiftCCSlider.setValue(midiComp->getShiftCCNumber(), juce::dontSendNotification);
        shiftCCSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        shiftCCSlider.addListener(this);
        addAndMakeVisible(shiftCCSlider);
        shiftCCLabel.setText("SHIFT CC:", juce::dontSendNotification);
        addAndMakeVisible(shiftCCLabel);

        // Слайдер для TEMPO CC.
        tempoCCSlider.setRange(0, 127, 1);
        if (midiComp != nullptr)
            tempoCCSlider.setValue(midiComp->getTempoCCNumber(), juce::dontSendNotification);
        tempoCCSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        tempoCCSlider.addListener(this);
        addAndMakeVisible(tempoCCSlider);
        addAndMakeVisible(tempoLabel);

        // Слайдер для UP CC.
        upCCSlider.setRange(0, 127, 1);
        if (midiComp != nullptr)
            upCCSlider.setValue(midiComp->getUpCCNumber(), juce::dontSendNotification);
        upCCSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        upCCSlider.addListener(this);
        addAndMakeVisible(upCCSlider);
        addAndMakeVisible(upLabel);

        // Слайдер для DOWN CC.
        downCCSlider.setRange(0, 127, 1);
        if (midiComp != nullptr)
            downCCSlider.setValue(midiComp->getDownCCNumber(), juce::dontSendNotification);
        downCCSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        downCCSlider.addListener(this);
        addAndMakeVisible(downCCSlider);
        addAndMakeVisible(downLabel);

        // Слайдеры и метки для пресетов A–F.
        for (int i = 0; i < 6; i++)
        {
            auto* slider = new juce::Slider();
            slider->setRange(0, 127, 1);
            if (midiComp != nullptr)
                slider->setValue(midiComp->getPresetCCNumber(i), juce::dontSendNotification);
            slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
            slider->addListener(this);
            presetCCSliders.add(slider);
            addAndMakeVisible(slider);

            auto* label = new juce::Label();
            // Формируем метку-пресета с помощью буфера (например, "A", "B", ..., "F").
            char buf[2] = { 'A' + i, '\0' };
            label->setText(juce::String(buf), juce::dontSendNotification);
            presetCCLabels.add(label);
            addAndMakeVisible(label);
        }

        // Загружаем сохраненные MIDI Input/Output.
        juce::File configFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile(settingsFileName);
        if (configFile.existsAsFile())
        {
            juce::PropertiesFile::Options options;
            options.applicationName = "MIDI Preset Scenes";
            juce::PropertiesFile propertiesFile(configFile, options);
            juce::String savedInput = propertiesFile.getValue(midiInputKey, "");
            juce::String savedOutput = propertiesFile.getValue(midiOutputKey, "");

            if (savedInput.isNotEmpty())
            {
                auto midiInputs = juce::MidiInput::getAvailableDevices();
                int desiredInputId = -1;
                for (int i = 0; i < midiInputs.size(); ++i)
                {
                    if (midiInputs[i].identifier == savedInput)
                    {
                        desiredInputId = i + 1;
                        break;
                    }
                }
                if (desiredInputId != -1)
                    midiInputCombo.setSelectedId(desiredInputId, juce::dontSendNotification);
            }
            if (savedOutput.isNotEmpty())
            {
                auto midiOutputs = juce::MidiOutput::getAvailableDevices();
                int desiredOutputId = -1;
                for (int i = 0; i < midiOutputs.size(); ++i)
                {
                    if (midiOutputs[i].identifier == savedOutput)
                    {
                        desiredOutputId = i + 1;
                        break;
                    }
                }
                if (desiredOutputId != -1)
                    midiOutputCombo.setSelectedId(desiredOutputId, juce::dontSendNotification);
            }
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);

        // Верхняя часть: MIDI Input/Output.
        midiInputLabel.setBounds(area.removeFromTop(20));
        midiInputCombo.setBounds(area.removeFromTop(40));
        midiOutputLabel.setBounds(area.removeFromTop(20));
        midiOutputCombo.setBounds(area.removeFromTop(40));

        // Область слайдеров для основных CC-функций (SHIFT, TEMPO, UP, DOWN).
        int sliderRowHeight = 40;
        auto sliderArea = area.removeFromTop(sliderRowHeight * 4);

        auto row = sliderArea.removeFromTop(sliderRowHeight);
        shiftCCLabel.setBounds(row.removeFromLeft(100));
        shiftCCSlider.setBounds(row);

        row = sliderArea.removeFromTop(sliderRowHeight);
        tempoLabel.setBounds(row.removeFromLeft(100));
        tempoCCSlider.setBounds(row);

        row = sliderArea.removeFromTop(sliderRowHeight);
        upLabel.setBounds(row.removeFromLeft(100));
        upCCSlider.setBounds(row);

        row = sliderArea.removeFromTop(sliderRowHeight);
        downLabel.setBounds(row.removeFromLeft(100));
        downCCSlider.setBounds(row);

        // Новая область для пресетов A-F: размещаем 6 рядов вертикально.
        int presetRowHeight = 40;
        for (int i = 0; i < presetCCSliders.size(); i++)
        {
            auto row = area.removeFromTop(presetRowHeight);
            presetCCLabels[i]->setBounds(row.removeFromLeft(100));
            presetCCSliders[i]->setBounds(row.reduced(10, 5));
        }

        // Кнопка сохранения внизу.
        saveButton.setBounds(area.removeFromBottom(40));
    }
    ////////////////////////////////////////////////////////////////////////////////////
    void comboBoxChanged(juce::ComboBox* comboThatChanged) override
    {
        if (comboThatChanged == &midiInputCombo)
        {
            auto midiInputs = juce::MidiInput::getAvailableDevices();
            int index = midiInputCombo.getSelectedItemIndex();
            for (auto& dev : midiInputs)
                deviceManager.setMidiInputDeviceEnabled(dev.identifier, false);
            if (index >= 0 && index < midiInputs.size())
            {
                deviceManager.setMidiInputDeviceEnabled(midiInputs[index].identifier, true);
                DBG("Selected MIDI Input: " << midiInputs[index].name);
            }
        }
        else if (comboThatChanged == &midiOutputCombo)
        {
            auto midiOutputs = juce::MidiOutput::getAvailableDevices();
            int index = midiOutputCombo.getSelectedItemIndex();
            if (midiComp != nullptr && index >= 0 && index < midiOutputs.size())
            {
                midiComp->updateMidiOutputDevice(midiOutputs[index]);
                DBG("Selected MIDI Output: " << midiOutputs[index].name);
            }
        }
    }

    void sliderValueChanged(juce::Slider* slider) override
    {
        if (slider == &shiftCCSlider)
        {
            if (midiComp != nullptr)
            {
                midiComp->setShiftCCNumber(static_cast<int>(shiftCCSlider.getValue()));
                DBG("SHIFT CC updated to: " << static_cast<int>(shiftCCSlider.getValue()));
            }
        }
        else if (slider == &tempoCCSlider)
        {
            if (midiComp != nullptr)
            {
                midiComp->setTempoCCNumber(static_cast<int>(tempoCCSlider.getValue()));
                DBG("TEMPO CC updated to: " << static_cast<int>(tempoCCSlider.getValue()));
            }
        }
        else if (slider == &upCCSlider)
        {
            if (midiComp != nullptr)
            {
                midiComp->setUpCCNumber(static_cast<int>(upCCSlider.getValue()));
                DBG("UP CC updated to: " << static_cast<int>(upCCSlider.getValue()));
            }
        }
        else if (slider == &downCCSlider)
        {
            if (midiComp != nullptr)
            {
                midiComp->setDownCCNumber(static_cast<int>(downCCSlider.getValue()));
                DBG("DOWN CC updated to: " << static_cast<int>(downCCSlider.getValue()));
            }
        }
        else
        {
            // Слайдеры для пресетов.
            for (int i = 0; i < presetCCSliders.size(); i++)
            {
                if (slider == presetCCSliders[i])
                {
                    if (midiComp != nullptr)
                    {
                        midiComp->setPresetCCNumber(i, static_cast<int>(presetCCSliders[i]->getValue()));
                        DBG("Preset " << (char)('A' + i) << " CC updated to: " << static_cast<int>(presetCCSliders[i]->getValue()));
                    }
                    break;
                }
            }
        }
    }

    void buttonClicked(juce::Button* button) override
    {
        if (button == &saveButton)
        {
            auto midiInputs = juce::MidiInput::getAvailableDevices();
            auto midiOutputs = juce::MidiOutput::getAvailableDevices();
            int inputIndex = midiInputCombo.getSelectedItemIndex();
            int outputIndex = midiOutputCombo.getSelectedItemIndex();
            juce::String inputId, outputId;
            if (inputIndex >= 0 && inputIndex < midiInputs.size())
                inputId = midiInputs[inputIndex].identifier;
            if (outputIndex >= 0 && outputIndex < midiOutputs.size())
                outputId = midiOutputs[outputIndex].identifier;
            juce::File configFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                .getChildFile(settingsFileName);
            juce::PropertiesFile::Options options;
            options.applicationName = "MIDI Preset Scenes";
            juce::PropertiesFile propertiesFile(configFile, options);
            propertiesFile.setValue(midiInputKey, inputId);
            propertiesFile.setValue(midiOutputKey, outputId);
            propertiesFile.saveIfNeeded();
            DBG("Saved MIDI Settings: Input=" << inputId << ", Output=" << outputId);
        }
    }

private:
    juce::AudioDeviceManager& deviceManager;
    MultiMidiSenderComponent* midiComp = nullptr;
    juce::ComboBox midiInputCombo, midiOutputCombo;
    juce::TextButton saveButton{ "Save MIDI Settings" };
    juce::Label midiInputLabel{ "midiInputLabel", "MIDI Input:" };
    juce::Label midiOutputLabel{ "midiOutputLabel", "MIDI Output:" };

    // Слайдер и метка для SHIFT CC.
    juce::Slider shiftCCSlider;
    juce::Label shiftCCLabel{ "shiftCCLabel", "SHIFT CC:" };

    // Слайдеры и метки для TEMPO, UP, DOWN.
    juce::Slider tempoCCSlider, upCCSlider, downCCSlider;
    juce::Label tempoLabel, upLabel, downLabel;

    // Слайдеры и метки для пресетов A–F.
    juce::OwnedArray<juce::Slider> presetCCSliders;
    juce::OwnedArray<juce::Label> presetCCLabels;
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
        setResizeLimits(600, 600, 1200, 800);
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
