#pragma once

#include <JuceHeader.h>
#include "AudioMidiSettingsDialog.h"
#include "MainContentComponent.h"
#include "vst_host.h"

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
        // --- Загрузка прошлых audio-настроек (если есть) ---
        loadAudioSettings();

        // --- Остальной UI ---
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
        saveWindowState();
    }


    //==============================================================================
    // MenuBarModel:
    StringArray getMenuBarNames() override
    {
        return { "NEXUS OS" };
    }

    PopupMenu getMenuForIndex(int, const String& menuName) override
    {
        PopupMenu m;
        if (menuName == "NEXUS OS")
        {
            if (menuName == "NEXUS OS")
            {
                m.addItem(2, "Audio/MIDI Settings...");
                m.addSeparator();
                m.addItem(4, "FullScreen", true, isFullScreenMode);
                m.addSeparator();
                m.addItem(1, "Exit");
            }
        }
        return m;
    }

    void menuItemSelected(int itemID, int) override
    {
        switch (itemID)
        {
            {
        case 1:
            JUCEApplication::getInstance()->systemRequestedQuit();
            break;

        case 2:
            showAudioMidiSettings();
            break;

        case 4:
            isFullScreenMode = !isFullScreenMode;
            updateFullScreenMode();
            break;
            }
        }
    }

    void closeButtonPressed() override {}
    void resized() override { DocumentWindow::resized(); }


private:
    bool isFullScreenMode{ false };


    //==============================================================================
    // 1) Загрузка аудио-настроек из XML (вызывается в конструкторе)
    void loadAudioSettings()
    {
        auto file = File::getSpecialLocation(File::userDocumentsDirectory)
            .getChildFile("MyPluginAudioSettings.xml");
        auto& adm = VSTHostComponent::getDefaultAudioDeviceManager();

        if (file.existsAsFile())
        {
            std::unique_ptr<XmlElement> xml(XmlDocument::parse(file));
            if (xml != nullptr)
            {
                // инициализируем из XML
                adm.initialise(0, 2, xml.get(), true);
                return;
            }
        }

        // если файла нет или он битый — ставим дефолты
        adm.initialiseWithDefaultDevices(0, 2);
    }


    //==============================================================================
    // 2) Сохранение аудио-настроек
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


    //==============================================================================
    // 3) Показываем стандартный Audio/MIDI диалог
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


    //==============================================================================
    // Window-state, FullScreen…
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
            // можно убрать или поставить свой дефолт
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
