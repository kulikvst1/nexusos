// LibraryMode.cpp
#include "LibraryMode.h"
#include "Rig_control.h"
#include "bank_editor.h"

LibraryMode::LibraryMode(Rig_control& r) : rig(r) {}

void LibraryMode::toggleLibraryMode()
{
    libraryMode = !libraryMode;
    sendLibraryModeState();

    if (libraryMode)
    {
        // --- Âõîä â ðåæèì áèáëèîòåêè ---
        if (currentLibraryFileIndex < 0)
        {
            int loadedIndex = findLoadedFileIndex();
            if (loadedIndex >= 0)
            {
                currentLibraryFileIndex = loadedIndex;
                currentLibraryOffset = loadedIndex; // ? çàãðóæåííûé ôàéë áóäåò íà ïåðâîé êíîïêå
            }
            else
            {
                currentLibraryFileIndex = 0;
                currentLibraryOffset = 0;
            }
        }
       // updateBankSelector();
        updateLibraryDisplays();
    }
    else
    {
        // --- Âûõîä èç ðåæèìà áèáëèîòåêè ---
        if (rig.bankEditor)
            rig.bankEditor->setActivePresetIndex(0);
        rig.updatePresetDisplays();
    }
}

void LibraryMode::sendLibraryModeState()
{
    int value = libraryMode ? 127 : 0;
    if (rig.midiOut)
        rig.midiOut->sendMessageNow(juce::MidiMessage::controllerEvent(1, 107, value));
}

void LibraryMode::prevLibraryBlock()
{
    if (!libraryMode) return; // ?? åñëè íå áèáëèîòåêà — íè÷åãî íå äåëàåì
    if (!rig.bankEditor) return;

    auto files = rig.bankEditor->scanNumericBankFiles();
    if (files.isEmpty()) return;

    currentLibraryOffset = (currentLibraryOffset - 3 + files.size()) % files.size();
    updateLibraryDisplays();

    if (rig.midiOut)
        rig.midiOut->sendMessageNow(juce::MidiMessage::controllerEvent(4, 101, 127));
}

void LibraryMode::nextLibraryBlock()
{
    if (!libraryMode) return;
    if (!rig.bankEditor) return;

    auto files = rig.bankEditor->scanNumericBankFiles();
    if (files.isEmpty()) return;

    currentLibraryOffset = (currentLibraryOffset + 3) % files.size();
    updateLibraryDisplays();

    if (rig.midiOut)
        rig.midiOut->sendMessageNow(juce::MidiMessage::controllerEvent(4, 106, 127));
}

void LibraryMode::selectLibraryFile(int idx)
{
    if (!libraryMode) return;
    if (!rig.bankEditor) return;

    auto files = rig.bankEditor->scanNumericBankFiles();
    if (files.isEmpty()) return;

    int fileIndex = (currentLibraryOffset + idx) % files.size();

    if (fileIndex != currentLibraryFileIndex)
    {
        rig.triggeredFromRig = true; // ?? ïîìå÷àåì èñòî÷íèê
        rig.bankEditor->loadBankFile(files[fileIndex]); // òîëüêî îäèí àðãóìåíò!
        rig.triggeredFromRig = false;

        currentLibraryFileIndex = fileIndex;

        // ?? ñèíõðîíèçàöèÿ áåç ñáðîñà offset
        syncLibraryToLoadedFile(false);
    }

    if (rig.midiOut)
        rig.midiOut->sendMessageNow(juce::MidiMessage::controllerEvent(4, idx + 1, 127));
}

void LibraryMode::syncLibraryToLoadedFile()
{
    syncLibraryToLoadedFile(false);
}

void LibraryMode::syncLibraryToLoadedFile(bool resetOffset)
{
    if (!rig.bankEditor) return;

    int loadedIndex = findLoadedFileIndex();
    if (loadedIndex < 0) return;

    currentLibraryFileIndex = loadedIndex;

    if (resetOffset)
        currentLibraryOffset = loadedIndex; // ñáðîñ ? ôàéë íà ïåðâîé êíîïêå

    updateLibraryDisplays();
   // updateBankSelector();
}

int LibraryMode::findLoadedFileIndex() const
{
    if (!rig.bankEditor) return -1;

    auto files = rig.bankEditor->scanNumericBankFiles();
    if (files.isEmpty()) return -1;

    const juce::String loaded = rig.bankEditor->loadedFileName;
    if (loaded.isEmpty()) return -1;

    for (int i = 0; i < files.size(); ++i)
    {
        if (files[i].getFullPathName() == loaded ||
            files[i].getFileName() == loaded ||
            files[i].getFileNameWithoutExtension() == loaded)
        {
            return i;
        }
    }
    return -1;
}
void LibraryMode::updateLibraryDisplays()
{
    if (!rig.bankEditor) return;

    auto files = rig.bankEditor->scanNumericBankFiles();
    if (files.isEmpty()) return;

    if (currentLibraryFileIndex < 0)
        currentLibraryFileIndex = findLoadedFileIndex();

    // êíîïêè – ôàéëû 1–3
    for (int i = 0; i < 3; ++i)
    {
        rig.presetButtons[i]->setButtonText(
            files[(currentLibraryOffset + i) % files.size()].getFileNameWithoutExtension()
        );
    }

    // ìåòêè – ôàéëû 4–6
    rig.presetLabel1_4.setText(files[(currentLibraryOffset + 3) % files.size()].getFileNameWithoutExtension(),
        juce::dontSendNotification);
    rig.presetLabel2_5.setText(files[(currentLibraryOffset + 4) % files.size()].getFileNameWithoutExtension(),
        juce::dontSendNotification);
    rig.presetLabel3_6.setText(files[(currentLibraryOffset + 5) % files.size()].getFileNameWithoutExtension(),
        juce::dontSendNotification);

    bool anyActive = false;

    // ïîäñâåòêà + MIDI ñèíõðîíèçàöèÿ
    for (int i = 0; i < rig.presetButtons.size(); ++i)
    {
        auto* btn = rig.presetButtons[i];
        int fileIdx = (currentLibraryOffset + i) % files.size();
        bool isActive = (fileIdx == currentLibraryFileIndex);

        if (isActive)
        {
            anyActive = true;
            lastActivePresetIndex = i;

            btn->setColour(juce::TextButton::buttonColourId, juce::Colours::green);
            btn->setColour(juce::TextButton::buttonOnColourId, juce::Colours::green);
            btn->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            btn->setColour(juce::TextButton::textColourOnId, juce::Colours::white);

            if (libraryMode)
                rig.sendLibraryState(i, true);
        }
        else
        {
            btn->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(200, 230, 255));
            btn->setColour(juce::TextButton::buttonOnColourId, juce::Colour::fromRGB(200, 230, 255));
            btn->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
            btn->setColour(juce::TextButton::textColourOnId, juce::Colours::black);
        }
    }

    if (!anyActive && lastActivePresetIndex >= 0 && libraryMode)
    {
        rig.sendLibraryState(lastActivePresetIndex, false);
    }

    // ãëàâíàÿ ìåòêà = íàçâàíèå àêòèâíîé áèáëèîòåêè
    juce::String libName = rig.bankEditor->loadedFileName;
    if (!libName.isEmpty())
    {
        // åñëè ýòî ïóòü – áåð¸ì òîëüêî èìÿ áåç ðàñøèðåíèÿ
        juce::File f = juce::File::getCurrentWorkingDirectory().getChildFile(libName);
        libName = f.getFileNameWithoutExtension();
    }
    else
    {
        libName = "<NO BANK>";
    }

    rig.bankNameLabel.setText(libName, juce::dontSendNotification);
    rig.activeLibraryLabel.setText("BANK: " + libName, juce::dontSendNotification);

    rig.repaint();
}
