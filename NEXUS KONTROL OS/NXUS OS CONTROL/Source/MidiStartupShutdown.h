#pragma once
#include "Rig_control.h"

class MidiStartupShutdown
{
public:
    explicit MidiStartupShutdown(Rig_control& rig) : rigControl(rig) {}

    // Âûçûâàåì ïðè ñòàðòå ïðèëîæåíèÿ
    void sendStartupCommands();

    // Âûçûâàåì ïðè çàêðûòèè ïðèëîæåíèÿ
    void sendShutdownCommands();

private:
    Rig_control& rigControl;

    // Âñïîìîãàòåëüíûå ìåòîäû
    void sendImpedanceFromSettings();
    void sendOtherStartupCommands();
    void sendOtherShutdownCommands();
};

