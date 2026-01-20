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
        // --- ¬ход в режим библиотеки ---
        if (currentLibraryFileIndex < 0)
        {
            int loadedIndex = findLoadedFileIndex();
            if (loadedIndex >= 0)
            {
                currentLibraryFileIndex = loadedIndex;
                currentLibraryOffset = loadedIndex; // ? загруженный файл будет на первой кнопке
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
        // --- ¬ыход из режима библиотеки ---
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
    if (!libraryMode) return; // ?? если не библиотека Ч ничего не делаем
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
        rig.triggeredFromRig = true; // ?? помечаем источник
        rig.bankEditor->loadBankFile(files[fileIndex]); // только один аргумент!
        rig.triggeredFromRig = false;

        currentLibraryFileIndex = fileIndex;

        // ?? синхронизаци€ без сброса offset
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
        currentLibraryOffset = loadedIndex; // сброс ? файл на первой кнопке

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

    // кнопки Ц файлы 1Ц3
    for (int i = 0; i < 3; ++i)
    {
        rig.presetButtons[i]->setButtonText(
            files[(currentLibraryOffset + i) % files.size()].getFileNameWithoutExtension()
        );
    }

    // метки Ц файлы 4Ц6
    rig.presetLabel1_4.setText(files[(currentLibraryOffset + 3) % files.size()].getFileNameWithoutExtension(),
        juce::dontSendNotification);
    rig.presetLabel2_5.setText(files[(currentLibraryOffset + 4) % files.size()].getFileNameWithoutExtension(),
        juce::dontSendNotification);
    rig.presetLabel3_6.setText(files[(currentLibraryOffset + 5) % files.size()].getFileNameWithoutExtension(),
        juce::dontSendNotification);

    bool anyActive = false;

    // подсветка + MIDI синхронизаци€
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

    // главна€ метка = название активной библиотеки
    juce::String libName = rig.bankEditor->loadedFileName;
    if (!libName.isEmpty())
    {
        // если это путь Ц берЄм только им€ без расширени€
        juce::File f = juce::File::getCurrentWorkingDirectory().getChildFile(libName);
        libName = f.getFileNameWithoutExtension();
    }
    else
    {
        libName = "<no library>";
    }

    rig.bankNameLabel.setText(libName, juce::dontSendNotification);
    rig.activeLibraryLabel.setText("Library: " + libName, juce::dontSendNotification);

    rig.repaint();
}

/*
void LibraryMode::updateBankSelector()
{
    if (!rig.bankEditor || !rig.bankSelector) return;

    rig.bankSelector->clear();

    if (libraryMode)
    {
        auto files = rig.bankEditor->scanNumericBankFiles();
        for (int i = 0; i < files.size(); ++i)
            rig.bankSelector->addItem(files[i].getFileNameWithoutExtension(), i + 1);

        int loadedIndex = findLoadedFileIndex();
        if (loadedIndex >= 0)
            rig.bankSelector->setSelectedId(loadedIndex + 1, juce::dontSendNotification);

        rig.bankSelector->onChange = [this, files]
            {
                rig.bankSelector->setEnabled(true);

                int selectedId = rig.bankSelector->getSelectedId();
                int fileIndex = selectedId - 1;
                if (fileIndex >= 0 && fileIndex < files.size())
                {
                    if (fileIndex != currentLibraryFileIndex)
                    {
                        rig.bankEditor->loadBankFile(files[fileIndex]);
                        currentLibraryFileIndex = fileIndex;
                    }
                }
            };
    }
        rig.bankNameLabel.setText(rig.bankEditor->loadedFileName, juce::dontSendNotification);
}
*/
