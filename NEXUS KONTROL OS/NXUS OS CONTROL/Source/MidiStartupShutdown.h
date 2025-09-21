#pragma once
#include "Rig_control.h"

class MidiStartupShutdown
{
public:
    explicit MidiStartupShutdown(Rig_control& rig) : rigControl(rig) {}

    // Вызываем при старте приложения
    void sendStartupCommands();

    // Вызываем при закрытии приложения
    void sendShutdownCommands();

private:
    Rig_control& rigControl;

    // Вспомогательные методы
    void sendImpedanceFromSettings();
    void sendOtherStartupCommands();
    void sendOtherShutdownCommands();
};

