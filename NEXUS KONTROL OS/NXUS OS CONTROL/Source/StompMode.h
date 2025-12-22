// StompMode.h
#pragma once
#include <JuceHeader.h>

class Rig_control;

class StompMode {
public:
    explicit StompMode(Rig_control& rig);

    // Entry/exit
    void toggleStompMode();
  //  void sendStompModeState();
    void setStompState(bool on);
    void sendStompState();

    // UI sync
    void updateStompDisplays();

    // State
    bool isActive() const noexcept { return stompMode; }

private:
    Rig_control& rig;
    bool stompMode = false;
};

