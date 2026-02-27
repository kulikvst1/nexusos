#pragma once
#include <JuceHeader.h>

class AppStateManager
{
public:
    AppStateManager()
    {
        dir = juce::File::getSpecialLocation(
            juce::File::userApplicationDataDirectory
        ).getChildFile("NEXUS_KONTROL_OS");

        if (!dir.exists())
            dir.createDirectory();

        stateFile = dir.getChildFile("AppState.txt");
    }

    // Чтение состояния (0 если файла нет)
    int readState()
    {
        if (!stateFile.existsAsFile())
            return 0;
        return stateFile.loadFileAsString().getIntValue();
    }

    // Пометить "идёт работа"
    void markRunning()
    {
        stateFile.replaceWithText("1");
    }

    // Пометить "корректный выход"
    void markCleanExit()
    {
        stateFile.replaceWithText("2");
    }

private:
    juce::File dir;
    juce::File stateFile;
};

