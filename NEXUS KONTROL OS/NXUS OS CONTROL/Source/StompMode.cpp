#include "StompMode.h"
#include "Rig_control.h"
#include "bank_editor.h"

StompMode::StompMode(BankEditor* be, Rig_control* r) noexcept
    : bankEditor(be), rig(r) {}

void StompMode::enter() noexcept {
    active = true;

    // сохраняем текущее состояние Shift
    presetShiftState = rig->shift;

    // временно выключаем Shift (как раньше setToggleState(false))
    rig->setShiftState(false);

    // сбрасываем группу радиокнопок
    for (auto* btn : rig->presetButtons)
        btn->setRadioGroupId(0, juce::dontSendNotification);

    updateDisplays();
}
void StompMode::exit() noexcept {
    active = false;
    rig->setShiftState(presetShiftState);

    for (auto* btn : rig->presetButtons)
        btn->setRadioGroupId(100, juce::dontSendNotification);

    // ?? сбросить lastSentPresetIndex, чтобы updatePresetDisplays точно отправил MIDI
    rig->lastSentPresetIndex = -1;

    rig->updatePresetDisplays();
}

void StompMode::handleButtonClick(int idx, bool shiftOn) noexcept {
    if (!active || !bankEditor) return;

    const int bankIdx = bankEditor->getActiveBankIndex();
    const int presetIdx = bankEditor->getActivePresetIndex();
    const int ccIndex = shiftOn ? (idx + 3) : idx; // 0..5

    // текущее состояние
    bool currentState = bankEditor->getBank(bankIdx).ccPresetStates[presetIdx][ccIndex];

    // обновляем модель в BankEditor
    bankEditor->updateCCParameter(ccIndex, !currentState);

    // перерисовываем UI
    updateDisplays();

    // передаём MIDI наружу через Rig_control
    rig->sendStompButtonState(ccIndex, !currentState);
}
void StompMode::updateDisplays() {
    rig->sendShiftState();
    if (!bankEditor) return;

    const int bankIdx = bankEditor->getActiveBankIndex();
    const int presetIdx = bankEditor->getActivePresetIndex();
    const bool shiftOn = rig->shift; // вместо shiftButton->getToggleState()

    std::array<juce::Button*, 3> btns = {
        rig->presetButtons[0], rig->presetButtons[1], rig->presetButtons[2]
    };
    std::array<juce::Label*, 3> labs = {
        &rig->presetLabel1_4, &rig->presetLabel2_5, &rig->presetLabel3_6
    };

    for (int i = 0; i < 3; ++i) {
        int btnCC = shiftOn ? (i + 3) : i;
        int lblCC = shiftOn ? i : (i + 3);

        // --- КНОПКА ---
        juce::String btnName = bankEditor->getCCName(btnCC);
        if (btnName.isEmpty()) btnName = "CC" + juce::String(btnCC + 1);
        btns[i]->setButtonText(btnName);

        bool btnActive = bankEditor->getBank(bankIdx).ccPresetStates[presetIdx][btnCC];
        btns[i]->setColour(juce::TextButton::buttonColourId, btnActive ? juce::Colours::red : juce::Colours::black);
        btns[i]->setColour(juce::TextButton::buttonOnColourId, btnActive ? juce::Colours::red : juce::Colours::black);
        btns[i]->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        btns[i]->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        btns[i]->setToggleState(btnActive, juce::dontSendNotification);

        // --- МЕТКА ---
        juce::String lblName = bankEditor->getCCName(lblCC);
        if (lblName.isEmpty()) lblName = "CC" + juce::String(lblCC + 1);
        labs[i]->setText(lblName, juce::dontSendNotification);

        bool lblActive = bankEditor->getBank(bankIdx).ccPresetStates[presetIdx][lblCC];
        labs[i]->setOpaque(true);
        labs[i]->setColour(juce::Label::backgroundColourId, lblActive ? juce::Colours::red : juce::Colours::lightgrey);
        labs[i]->setColour(juce::Label::textColourId, lblActive ? juce::Colours::white : juce::Colours::darkgrey);

        // --- MIDI OUT через Rig_control ---
        rig->sendStompButtonState(btnCC, btnActive);
    }
    rig->updateAllSButtons();
    rig->repaint();
}
void StompMode::setState(bool on)
{
    if (on)
        enter();
    else
        exit();
}

