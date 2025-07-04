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
    enum MenuIds
    {
        miExit = 1,
        miAudioSettings = 2,
        miPluginManager = 3,
        miFullScreen = 4,
        miToggleLooper = 5   // новый пункт меню
    };

    MainWindow(const String& title)
        : DocumentWindow(title,
            Desktop::getInstance()
            .getDefaultLookAndFeel()
            .findColour(ResizableWindow::backgroundColourId),
            DocumentWindow::allButtons),
        isLooperShown(true)  // значение по умолчанию
    {
        loadAudioSettings();
        loadWindowState();   // загрузим isFullScreenMode и isLooperShown

        setUsingNativeTitleBar(false);
        setTitleBarHeight(0);
        setTitleBarButtonsRequired(0, false);

        // создаём главный UI-компонент
        auto* content = new MainContentComponent();
        setContentOwned(content, true);

        // применяем сохранённое состояние видимости вкладки LOOPER
        content->setLooperTabVisible(isLooperShown);

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
        m.addItem(miToggleLooper,
            "Show LOOPER TAB",
            true,
            isLooperShown);   // чекбокс по флагу
        m.addSeparator();
        m.addItem(miExit, "Exit");
        return m;
    }

    void menuItemSelected(int itemID, int) override
    {
        switch (itemID)
        {
        case miToggleLooper:
        {
            isLooperShown = !isLooperShown;
            if (auto* mcc = dynamic_cast<MainContentComponent*>(getContentComponent()))
                mcc->setLooperTabVisible(isLooperShown);
            break;
        }

        case miFullScreen:
        {
            isFullScreenMode = !isFullScreenMode;
            updateFullScreenMode();
            break;
        }

        case miPluginManager:
            showPluginManager();
            break;

        case miAudioSettings:
            showAudioMidiSettings();
            break;

        case miExit:
            JUCEApplication::getInstance()->systemRequestedQuit();
            break;
        }
    }

    void closeButtonPressed() override {}
    void resized() override { DocumentWindow::resized(); }

private:
    bool isFullScreenMode{ false };
    bool isLooperShown{ true };

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
            AlertWindow::showMessageBoxAsync(
                AlertWindow::InfoIcon,
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
        auto* mcc = dynamic_cast<MainContentComponent*>(getContentComponent());
        jassert(mcc != nullptr);

        PluginManager& pm = mcc->getVstHostComponent().getPluginManager();

        DialogWindow::LaunchOptions o;
        o.content.setOwned(new PluginManagerBox(pm));
        o.dialogTitle = "NEXUS Plugin Manager";
        o.dialogBackgroundColour = Colour::fromRGBA(50, 62, 68, 255);
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = false;
        o.resizable = false;
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
            setSize(900, 800);
        }
    }

    //==========================================================================
    void saveWindowState()
    {
        XmlElement xml("WindowState");
        xml.setAttribute("fullScreen", isFullScreenMode);
        xml.setAttribute("showLooperTab", isLooperShown);

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
                isLooperShown = xml->getBoolAttribute("showLooperTab", true);

                if (!isFullScreenMode)
                {
                    auto r = Rectangle<int>::fromString(
                        xml->getStringAttribute("bounds", ""));
                    if (!r.isEmpty())
                        setBounds(r);
                }
            }
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};
