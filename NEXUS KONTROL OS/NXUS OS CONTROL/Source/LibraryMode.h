// LibraryMode.h
#pragma once
#include <JuceHeader.h>

class Rig_control;

class LibraryMode {
public:
    explicit LibraryMode(Rig_control& rig);

    // Entry/exit
    void toggleLibraryMode();
    void sendLibraryModeState();

    // Navigation
    void prevLibraryBlock();
    void nextLibraryBlock();
    void selectLibraryFile(int idx);

    // UI sync
    void updateLibraryDisplays();
    void updateBankSelector();

    // File sync
    int  findLoadedFileIndex() const;
    void syncLibraryToLoadedFile();
    void syncLibraryToLoadedFile(bool resetOffset);

    // State
    bool isActive() const noexcept { return libraryMode; }
    int currentLibraryOffset = 0;
    int currentLibraryFileIndex = -1;
    int getCurrentLibraryOffset() const noexcept { return currentLibraryOffset; }
    int getCurrentLibraryFileIndex() const noexcept { return currentLibraryFileIndex; }
   
private:
    Rig_control& rig;
    int lastActivePresetIndex = -1;
    bool libraryMode = false;
    bool triggeredFromRig = false;
};
