#include "Rig_control.h"

Rig_control::Rig_control()
{
    // Добавляем контейнер в главный компонент
    addAndMakeVisible(mainTab);

    // 1. Создаём 10 кнопок для CC
    for (int i = 0; i < 10; ++i)
    {
        auto* btn = new juce::TextButton("CC " + juce::String(i + 1));
        btn->setClickingTogglesState(true);
        btn->setToggleState(false, juce::dontSendNotification);
        btn->setColour(juce::TextButton::buttonOnColourId, juce::Colours::red);
        btn->addListener(this);
        mainTab.addAndMakeVisible(btn);
        ccButtons.add(btn);
    }

    // 2. Создаём 3 кнопки-пресета (например, группы A, B, C)
    for (int i = 0; i < 3; ++i)
    {
        auto* preset = new juce::TextButton("Preset " + juce::String(i + 1));
        preset->setClickingTogglesState(true);
        preset->setRadioGroupId(100, juce::dontSendNotification);
        preset->setToggleState(false, juce::dontSendNotification);
        preset->setColour(juce::TextButton::buttonOnColourId, juce::Colours::blue);
        preset->addListener(this);
        mainTab.addAndMakeVisible(preset);
        presetButtons.add(preset);
    }

    // 3. Создаём метку для названия банка
    bankNameLabel.setText("BANK NAME", juce::dontSendNotification);
    bankNameLabel.setJustificationType(juce::Justification::centred);
    mainTab.addAndMakeVisible(bankNameLabel);

    // 4. Создаём управляющие кнопки: SHIFT, TEMPO, UP, DOWN

    shiftButton.reset(new juce::TextButton("SHIFT"));
    shiftButton->setClickingTogglesState(true);
    shiftButton->setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange);
    shiftButton->addListener(this);
    mainTab.addAndMakeVisible(shiftButton.get());

    tempoButton.reset(new juce::TextButton("TEMPO"));
    tempoButton->setClickingTogglesState(false);
    tempoButton->setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange);
    tempoButton->addListener(this);
    mainTab.addAndMakeVisible(tempoButton.get());

    upButton.reset(new juce::TextButton("UP"));
    upButton->setClickingTogglesState(true);
    upButton->setColour(juce::TextButton::buttonOnColourId, juce::Colours::green);
    upButton->addListener(this);
    mainTab.addAndMakeVisible(upButton.get());

    downButton.reset(new juce::TextButton("DOWN"));
    downButton->setClickingTogglesState(true);
    downButton->setColour(juce::TextButton::buttonOnColourId, juce::Colours::yellow);
    downButton->addListener(this);
    mainTab.addAndMakeVisible(downButton.get());

    // Если нужно, можно добавить mainTab в компонент ещё раз (обычно достаточно одного раза)
   // addAndMakeVisible(mainTab);

    setSize(800, 600);
}

Rig_control::~Rig_control()
{
    // Удаляем слушателей у кнопок
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

void Rig_control::resized()
{
    auto bounds = getLocalBounds().reduced(10);
    mainTab.setBounds(bounds);

    int totalWidth = mainTab.getWidth();
    int ccButtonHeight = 50;
    int presetButtonHeight = 50;
    int gap = 10;

    // Располагаем кнопки CC (первая строка)
    int ccButtonWidth = totalWidth / 10;
    for (int i = 0; i < ccButtons.size(); ++i)
    {
        ccButtons[i]->setBounds(i * ccButtonWidth, 0, ccButtonWidth, ccButtonHeight);
    }

    // Располагаем кнопки пресетов (вторая строка)
    int presetButtonWidth = totalWidth / 3;
    for (int i = 0; i < presetButtons.size(); ++i)
    {
        presetButtons[i]->setBounds(i * presetButtonWidth, ccButtonHeight + gap,
            presetButtonWidth, presetButtonHeight);
    }

    // Метка банка ниже пресетов
    bankNameLabel.setBounds(0, ccButtonHeight + presetButtonHeight + 2 * gap, totalWidth, 30);

    // Третья строка – управляющие кнопки
    int controlButtonY = ccButtonHeight + presetButtonHeight + 2 * gap + 40;
    int controlButtonWidth = totalWidth / 4;
    int controlButtonHeight = 50;

    shiftButton->setBounds(0, controlButtonY, controlButtonWidth, controlButtonHeight);
    tempoButton->setBounds(controlButtonWidth, controlButtonY, controlButtonWidth, controlButtonHeight);
    upButton->setBounds(2 * controlButtonWidth, controlButtonY, controlButtonWidth, controlButtonHeight);
    downButton->setBounds(3 * controlButtonWidth, controlButtonY, controlButtonWidth, controlButtonHeight);
}

void Rig_control::buttonClicked(juce::Button* button)
{
    
}
