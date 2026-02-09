#pragma once
#include <JuceHeader.h>
#include <array>

class BankEditor;
class Rig_control;

class StompMode
{
public:
    StompMode(BankEditor* be, Rig_control* rig) noexcept;

    void enter() noexcept;
    void exit() noexcept;
    void handleButtonClick(int idx, bool shiftOn) noexcept;
    void updateDisplays();
    void setState(bool on);

    bool isActive() const noexcept { return active; }

private:
    BankEditor* bankEditor = nullptr;
    Rig_control* rig = nullptr;
    bool active = false;
    bool presetShiftState = false;
};
