#pragma once
#include "Rig_control.h"

class MidiStartupShutdown
{
public:
    explicit MidiStartupShutdown(Rig_control& rig) : rigControl(rig) {}

    // �������� ��� ������ ����������
    void sendStartupCommands();

    // �������� ��� �������� ����������
    void sendShutdownCommands();

private:
    Rig_control& rigControl;

    // ��������������� ������
    void sendImpedanceFromSettings();
    void sendOtherStartupCommands();
    void sendOtherShutdownCommands();
};

