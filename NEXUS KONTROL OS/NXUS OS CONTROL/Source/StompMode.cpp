// StompMode.cpp
#include "StompMode.h"
#include "Rig_control.h"
#include "bank_editor.h"

StompMode::StompMode(Rig_control& r) : rig(r) {}

// Важно: не инвертируем сами, читаем состояние из UI,
// либо вообще убираем toggleStompMode и используем setStompState(bool).
void StompMode::toggleStompMode()
{
    const bool on = rig.stompBtn.getToggleState();
    setStompState(on);
}

void StompMode::sendStompState()
{
    if (!rig.midiOut) return;

    static bool lastSentStomp = false;
    if (stompMode != lastSentStomp)
    {
        lastSentStomp = stompMode;
        int value = stompMode ? 127 : 0;

        rig.midiOut->sendMessageNow(
            juce::MidiMessage::controllerEvent(1, 100, value) // ch1 CC100
        );
    }
}

// Единственная точка изменения состояния стомпа.
void StompMode::setStompState(bool on)
{
    // Не дёргаем UI с уведомлениями, чтобы не получить повторный buttonClicked.
    rig.stompBtn.setToggleState(on, juce::dontSendNotification);

    stompMode = on;

    sendStompState();

    if (stompMode)
    {
        // Вход в стомп ? рисуем стомп-отображение
        updateStompDisplays();
    }
    else
    {
        // Выход из стомпа ? возвращаем пресеты
        rig.updatePresetDisplays();
        rig.updateAllSButtons();
        rig.repaint();
    }
}

// Удалить или оставить как alias на sendStompState — но лучше удалить.
// void StompMode::sendStompModeState() { sendStompState(); }

void StompMode::updateStompDisplays()
{
    // Состояние шифта только читаем, не трогаем кнопки (UI остаётся в Rig_control).
    if (!rig.bankEditor) return;

    const int bankIdx = rig.bankEditor->getActiveBankIndex();
    const int presetIdx = rig.bankEditor->getActivePresetIndex();
    const bool shiftOn = rig.shiftButton->getToggleState();

    std::array<juce::Button*, 3> btns = {
        rig.presetButtons[0], rig.presetButtons[1], rig.presetButtons[2]
    };
    std::array<juce::Label*, 3> labs = {
        &rig.presetLabel1_4, &rig.presetLabel2_5, &rig.presetLabel3_6
    };

    for (int i = 0; i < 3; ++i)
    {
        int btnCC = shiftOn ? (i + 3) : i;
        int lblCC = shiftOn ? i : (i + 3);

        juce::String btnName = rig.bankEditor->getCCName(btnCC);
        if (btnName.isEmpty()) btnName = "CC" + juce::String(btnCC + 1);
        btns[i]->setButtonText(btnName);

        bool btnActive = rig.bankEditor->getBank(bankIdx).ccPresetStates[presetIdx][btnCC];
        btns[i]->setColour(juce::TextButton::buttonColourId, btnActive ? juce::Colours::red : juce::Colours::black);
        btns[i]->setColour(juce::TextButton::buttonOnColourId, btnActive ? juce::Colours::red : juce::Colours::black);
        btns[i]->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        btns[i]->setColour(juce::TextButton::textColourOnId, juce::Colours::white);

        // Это отображение состояния CC, а не выбор пресета — оставляем.
        btns[i]->setToggleState(btnActive, juce::dontSendNotification);

        juce::String lblName = rig.bankEditor->getCCName(lblCC);
        if (lblName.isEmpty()) lblName = "CC" + juce::String(lblCC + 1);
        labs[i]->setText(lblName, juce::dontSendNotification);

        bool lblActive = rig.bankEditor->getBank(bankIdx).ccPresetStates[presetIdx][lblCC];
        labs[i]->setOpaque(true);
        labs[i]->setColour(juce::Label::backgroundColourId, lblActive ? juce::Colours::red : juce::Colours::lightgrey);
        labs[i]->setColour(juce::Label::textColourId, lblActive ? juce::Colours::white : juce::Colours::darkgrey);

        if (rig.midiOut)
        {
            int globalCC = (btnCC % 3) + 1;
            int value = btnActive ? 127 : 0;
            rig.midiOut->sendMessageNow(
                juce::MidiMessage::controllerEvent(3, globalCC, value)
            );
        }
    }

    rig.updateAllSButtons();
    rig.repaint();
}
