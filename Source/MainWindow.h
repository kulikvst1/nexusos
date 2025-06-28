#pragma once

#include <JuceHeader.h>
#include "AudioMidiSettingsDialog.h"
#include "MainContentComponent.h"
#include "vst_host.h"
#include "PluginManagerBox.h"

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
        loadAudioSettings();
        loadWindowState();

        setUsingNativeTitleBar(false);
        setTitleBarHeight(0);
        setTitleBarButtonsRequired(0, false);

        setContentOwned(new MainContentComponent(), true);
        setMenuBar(this, 25);
        updateFullScreenMode();
        setVisible(true);
    }

    ~MainWindow() override
    {
        removeFromDesktop();
        setMenuBar(nullptr, 0);
        saveWindowState();
    }

    //==================================================================
    enum MenuIds
    {
        miExit = 1,
        miAudioSettings = 2,
        miPluginManager = 3,
        miFullScreen = 4
    };

    StringArray getMenuBarNames() override
    {
        return { "NEXUS OS" };
    }

    PopupMenu getMenuForIndex(int, const String&) override
    {
        PopupMenu m;
        m.addItem(miAudioSettings, "Audio/MIDI Settings...");
        m.addSeparator();
        m.addItem(miPluginManager, "Plugin Manager...");
        m.addSeparator();
        m.addItem(miFullScreen, "Full Screen", true, isFullScreenMode);
        m.addSeparator();
        m.addItem(miExit, "Exit");
        return m;
    }

    void menuItemSelected(int itemID, int) override
    {
        switch (itemID)
        {
        case miExit:
            JUCEApplication::getInstance()->systemRequestedQuit();
            break;

        case miAudioSettings:
            showAudioMidiSettings();
            break;

        case miPluginManager:
            showPluginManager();
            break;

        case miFullScreen:
            isFullScreenMode = !isFullScreenMode;
            updateFullScreenMode();
            break;
        }
    }

    void closeButtonPressed() override {}
    void resized() override { DocumentWindow::resized(); }

private:
    bool isFullScreenMode{ false };

    //==========================================================================
    void loadAudioSettings()
    {
        auto file = File::getSpecialLocation(File::userDocumentsDirectory)
            .getChildFile("MyPluginAudioSettings.xml");
        auto& adm = VSTHostComponent::getDefaultAudioDeviceManager();

        if (file.existsAsFile())
        {
            if (auto xml = XmlDocument::parse(file))
            {
                adm.initialise(0, 2, xml.get(), true);
                return;
            }
        }

        adm.initialiseWithDefaultDevices(0, 2);
    }

    void saveAudioSettings()
    {
        auto& adm = VSTHostComponent::getDefaultAudioDeviceManager();
        if (auto xml = adm.createStateXml())
        {
            auto file = File::getSpecialLocation(File::userDocumentsDirectory)
                .getChildFile("MyPluginAudioSettings.xml");
            xml->writeTo(file, {});
            AlertWindow::showMessageBoxAsync(AlertWindow::InfoIcon,
                "Audio Settings",
                "Saved to:\n" + file.getFullPathName());
        }
    }

    void showAudioMidiSettings()
    {
        DialogWindow::LaunchOptions o;
        o.content.setOwned(new AudioMidiSettingsDialog(
            VSTHostComponent::getDefaultAudioDeviceManager()));
        o.dialogTitle = "Audio/MIDI Settings";
        o.dialogBackgroundColour = findColour(ResizableWindow::backgroundColourId);
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = false;
        o.resizable = false;
        o.launchAsync();
    }

    //==========================================================================
    void showPluginManager()
    {
        // 1) Достаём MainContentComponent
        auto* mcc = dynamic_cast<MainContentComponent*>(getContentComponent());
        jassert(mcc != nullptr);

        // 2) Через него — получаем PluginManager
        PluginManager& pm = mcc->getVstHostComponent().getPluginManager();

        // 3) Передаём этот менеджер в диалог
        DialogWindow::LaunchOptions o;
        o.content.setOwned(new PluginManagerBox(pm));
        o.dialogTitle = "Plugin Manager";
        o.dialogBackgroundColour = Colours::darkgrey;
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = true;
        o.resizable = true;
        o.launchAsync();
    }

    //==========================================================================
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

    void saveWindowState()
    {
        XmlElement xml("WindowState");
        xml.setAttribute("fullScreen", isFullScreenMode);
        if (!isFullScreenMode)
            xml.setAttribute("bounds", getBounds().toString());

        auto f = File::getSpecialLocation(File::userApplicationDataDirectory)
            .getChildFile("NEXUS_OS_WindowState.xml");
        xml.writeToFile(f, {});
    }

    void loadWindowState()
    {
        auto f = File::getSpecialLocation(File::userApplicationDataDirectory)
            .getChildFile("NEXUS_OS_WindowState.xml");

        if (f.existsAsFile())
        {
            if (auto xml = XmlDocument::parse(f))
            {
                isFullScreenMode = xml->getBoolAttribute("fullScreen", false);
                if (!isFullScreenMode)
                {
                    auto r = Rectangle<int>::fromString(xml->getStringAttribute("bounds", ""));
                    if (!r.isEmpty())
                        setBounds(r);
                }
            }
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};
