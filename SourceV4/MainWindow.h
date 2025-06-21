#pragma once

#include <JuceHeader.h>
#include "AudioMidiSettingsDialog.h"
#include "MainContentComponent.h"
#include "Audio_core.h"

using namespace juce;

class MainWindow : public DocumentWindow,
    public MenuBarModel
{
public:
    MainWindow(const String& title)
        : DocumentWindow(title,
            Desktop::getInstance().getDefaultLookAndFeel()
            .findColour(ResizableWindow::backgroundColourId),
            DocumentWindow::allButtons)
    {
        // Вместо собственных настроек аудио, создаем Audio_core,
        // который инициализирует AudioDeviceManager.
        audio_core = std::make_unique<Audio_core>();

        loadWindowState();

        // Отключаем нативный заголовок и убираем кнопки
        setUsingNativeTitleBar(false);
        setTitleBarHeight(0);
        setTitleBarButtonsRequired(0, false);
        setName("");

        // Определяем содержимое окна через MainContentComponent
        setContentOwned(new MainContentComponent(), true);

        // Устанавливаем главное меню с высотой 25 пикселей
        setMenuBar(this, 25);

        updateFullScreenMode();
        setVisible(true);
    }

    ~MainWindow() override
    {
        setMenuBar(nullptr, 0);
        saveWindowState();
    }

    // Реализация MenuBarModel:

    StringArray getMenuBarNames() override
    {
        StringArray names;
        names.add("NEXUS OS");
        return names;
    }

    PopupMenu getMenuForIndex(int /*menuIndex*/, const String& menuName) override
    {
        PopupMenu menu;
        if (menuName == "NEXUS OS")
        {
            menu.addItem(2, "Audio/MIDI Settings...", true);
            menu.addItem(3, "FullScreen", true, isFullScreenMode);
            menu.addSeparator();
            menu.addItem(1, "Exit", true);
        }
        return menu;
    }

    void menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/) override
    {
        if (menuItemID == 1)
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }
        else if (menuItemID == 2)
        {
            showAudioMidiSettings();
        }
        else if (menuItemID == 3)
        {
            isFullScreenMode = !isFullScreenMode;
            updateFullScreenMode();
        }
    }

    void closeButtonPressed() override { }
    void resized() override { DocumentWindow::resized(); }

private:
    void updateFullScreenMode()
    {
        if (isFullScreenMode)
        {
            setFullScreen(true);
            setAlwaysOnTop(true);
            setResizable(false, false);
        }
        else
        {
            setFullScreen(false);
            setAlwaysOnTop(false);
            setResizable(true, true);
            setSize(800, 600);
        }
    }

    // Диалог настроек Audio/MIDI теперь получает AudioDeviceManager из Audio_core
    void showAudioMidiSettings()
    {
        DialogWindow::LaunchOptions options;
        options.content.setOwned(new AudioMidiSettingsDialog(audio_core->getDeviceManager()));
        options.dialogTitle = "Audio/MIDI Settings";
        auto bgColour = getLookAndFeel().findColour(ResizableWindow::backgroundColourId);
        options.dialogBackgroundColour = bgColour;
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = false;
        options.resizable = false;
        options.launchAsync();
    }

    void saveWindowState()
    {
        XmlElement xml("WindowState");
        xml.setAttribute("fullScreen", isFullScreenMode);
        if (!isFullScreenMode)
            xml.setAttribute("bounds", getBounds().toString());

        auto stateFile = File::getSpecialLocation(File::userApplicationDataDirectory)
            .getChildFile("NEXUS_OS_WindowState.xml");
        xml.writeToFile(stateFile, {});
    }

    void loadWindowState()
    {
        auto stateFile = File::getSpecialLocation(File::userApplicationDataDirectory)
            .getChildFile("NEXUS_OS_WindowState.xml");
        if (stateFile.existsAsFile())
        {
            std::unique_ptr<XmlElement> xml(XmlDocument::parse(stateFile));
            if (xml)
            {
                isFullScreenMode = xml->getBoolAttribute("fullScreen", false);
                if (!isFullScreenMode)
                {
                    auto boundsStr = xml->getStringAttribute("bounds", "");
                    auto rect = Rectangle<int>::fromString(boundsStr);
                    if (!rect.isEmpty())
                        setBounds(rect);
                }
            }
            else
                isFullScreenMode = false;
        }
        else
            isFullScreenMode = false;
    }

    std::unique_ptr<Audio_core> audio_core;
    bool isFullScreenMode{ false };
};
