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
        miAudioSettings,
        miPluginManager,
        miFullScreen,
        miToggleLooper,
        miToggleTuner,
        miToggleInputMaster,
        miToggleOutMaster,
        miToggleTunerStyle
    };

    MainWindow(const String& title)
        : DocumentWindow(title,
            Desktop::getInstance()
            .getDefaultLookAndFeel()
            .findColour(ResizableWindow::backgroundColourId),
            allButtons),
        isLooperShown(true),
        isTunerShown(true),
        isInputMasterShown(true),
        isOutMasterShown(true),
        boundsRestored(false)
    {
        setMenuBar(this, 25);
        setUsingNativeTitleBar(false);
        setTitleBarHeight(0);
        setTitleBarButtonsRequired(0, false);

        auto* content = new MainContentComponent();
        setContentOwned(content, true);
       
        content->setLooperTabVisible(isLooperShown);
        content->setTunerTabVisible(isTunerShown);
        content->setInMasterTabVisible(isInputMasterShown);
        content->setOutMasterTabVisible(isOutMasterShown);

        // Применяем стиль тюнера к содержимому
        // Требуется метод в MainContentComponent:
        // void setTunerStyleClassic(bool isClassic);
        content->setTunerStyleClassic(tunerVisualStyle == TunerVisualStyle::Classic);

        updateFullScreenMode();
        setVisible(true);
    }

    ~MainWindow() override
    {
        // Сначала убираем меню
        setMenuBar(nullptr);

        // Сохраняем настройки
        saveAudioSettings();
        saveWindowState();

       

    }

    // MenuBarModel
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
        m.addItem(miToggleLooper, "Show LOOPER TAB", true, isLooperShown);
        m.addItem(miToggleTuner, "Show TUNER TAB", true, isTunerShown);
        m.addItem(miToggleInputMaster, "Show INPUT MASTER TAB", true, isInputMasterShown);
        m.addItem(miToggleOutMaster, "Show OUT MASTER TAB", true, isOutMasterShown);
        m.addSeparator();
        m.addItem(miToggleTunerStyle,
            "Tuner Style: " + String(tunerVisualStyle == TunerVisualStyle::Classic ? "Classic" : "Triangles"));
        m.addSeparator();
        m.addItem(miExit, "Exit");
        return m;
    }

    void menuItemSelected(int itemID, int) override
    {
        if (auto* mcc = dynamic_cast<MainContentComponent*>(getContentComponent()))
        {
            switch (itemID)
            {
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

            case miToggleLooper:
                isLooperShown = !isLooperShown;
                mcc->setLooperTabVisible(isLooperShown);
                break;

            case miToggleTuner:
                isTunerShown = !isTunerShown;
                mcc->setTunerTabVisible(isTunerShown);
                break;

            case miToggleInputMaster:
                isInputMasterShown = !isInputMasterShown;
                mcc->setInMasterTabVisible(isInputMasterShown);
                break;

            case miToggleOutMaster:
                isOutMasterShown = !isOutMasterShown;
                mcc->setOutMasterTabVisible(isOutMasterShown);
                break;

            case miToggleTunerStyle:
            {
                tunerVisualStyle = (tunerVisualStyle == TunerVisualStyle::Classic)
                    ? TunerVisualStyle::Triangles
                    : TunerVisualStyle::Classic;

                // Применяем стиль
                mcc->setTunerStyleClassic(tunerVisualStyle == TunerVisualStyle::Classic);
                break;
            }

            case miExit:
                JUCEApplication::getInstance()->systemRequestedQuit();
                break;
            }
        }
    }

    void closeButtonPressed() override
    {
        JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    enum class TunerVisualStyle { Classic, Triangles };
   
    bool isFullScreenMode{ false },
        isLooperShown{ true },
        isTunerShown{ true },
        isOutMasterShown{ true },
        isInputMasterShown{ true },
        boundsRestored{ false };

    TunerVisualStyle tunerVisualStyle{ TunerVisualStyle::Triangles };

    // ==========================================================================
    void loadAudioSettings(MainContentComponent& content)
    {
        auto dir = File::getSpecialLocation(File::userApplicationDataDirectory);
        auto file = dir.getChildFile("MyPluginAudioSettings.xml");
        if (!file.existsAsFile())
        {
            content.getAudioDeviceManager().initialiseWithDefaultDevices(0, 2);
            return;
        }

        auto xmlText = file.loadFileAsString().trimStart();
        std::unique_ptr<XmlElement> xml(XmlDocument(xmlText).getDocumentElement());
        if (xml == nullptr)
        {
            content.getAudioDeviceManager().initialiseWithDefaultDevices(0, 2);
            return;
        }

        content.getAudioDeviceManager().initialise(0, 2, xml.get(), true);
    }

    void saveAudioSettings()
    {
        if (auto* mcc = dynamic_cast<MainContentComponent*>(getContentComponent()))
        {
            if (auto xml = mcc->getAudioDeviceManager().createStateXml())
            {
                auto dir = File::getSpecialLocation(File::userApplicationDataDirectory);
                dir.createDirectory();
                auto file = dir.getChildFile("MyPluginAudioSettings.xml");
                String xmlString = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" + xml->toString();
                file.replaceWithText(xmlString);
            }
        }
    }

    // ==========================================================================
    void loadWindowState()
    {
        auto dir = File::getSpecialLocation(File::userApplicationDataDirectory);
        auto file = dir.getChildFile("NEXUS_OS_WindowState.xml");
        if (auto xml = XmlDocument::parse(file))
        {
            isFullScreenMode = xml->getBoolAttribute("fullScreen", false);
            isLooperShown = xml->getBoolAttribute("showLooperTab", true);
            isTunerShown = xml->getBoolAttribute("showTunerTab", true);
            isInputMasterShown = xml->getBoolAttribute("showInputMasterTab", true);
            isOutMasterShown = xml->getBoolAttribute("showOutMasterTab", true);

            const auto styleStr = xml->getStringAttribute("tunerStyle", "Triangles");
            tunerVisualStyle = (styleStr == "Classic") ? TunerVisualStyle::Classic
                : TunerVisualStyle::Triangles;

            auto bs = xml->getStringAttribute("bounds", "");
            if (!isFullScreenMode && bs.isNotEmpty())
            {
                auto r = juce::Rectangle<int>::fromString(bs);
                if (!r.isEmpty())
                {
                    setBounds(r);
                    boundsRestored = true;
                }
            }
        }
    }

    void saveWindowState()
    {
        XmlElement xml("WindowState");
        xml.setAttribute("fullScreen", isFullScreenMode);
        xml.setAttribute("showLooperTab", isLooperShown);
        xml.setAttribute("showTunerTab", isTunerShown);
        xml.setAttribute("showInputMasterTab", isInputMasterShown); // фикс
        xml.setAttribute("showOutMasterTab", isOutMasterShown);

        xml.setAttribute("tunerStyle", tunerVisualStyle == TunerVisualStyle::Classic ? "Classic" : "Triangles");

        if (!isFullScreenMode)
            xml.setAttribute("bounds", getBounds().toString());

        auto dir = File::getSpecialLocation(File::userApplicationDataDirectory);
        dir.createDirectory();
        auto file = dir.getChildFile("NEXUS_OS_WindowState.xml");

        String out = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" + xml.toString();
        file.replaceWithText(out);
    }

    // ==========================================================================
    void showAudioMidiSettings()
    {
        if (auto* mcc = dynamic_cast<MainContentComponent*>(getContentComponent()))
        {
            DialogWindow::LaunchOptions opts;
            opts.content.setOwned(new AudioMidiSettingsDialog(mcc->getAudioDeviceManager()));
            opts.dialogTitle = "Audio/MIDI Settings";
            opts.dialogBackgroundColour = findColour(ResizableWindow::backgroundColourId);
            opts.escapeKeyTriggersCloseButton = true;
            opts.useNativeTitleBar = false;
            opts.resizable = true;
            opts.useBottomRightCornerResizer = true;
            opts.launchAsync();
        }
    }

    void showPluginManager()
    {
        if (auto* mcc = dynamic_cast<MainContentComponent*>(getContentComponent()))
        {
            auto& pm = mcc->getVstHostComponent().getPluginManager();
            DialogWindow::LaunchOptions opts;
            opts.content.setOwned(new PluginManagerBox(pm));
            opts.dialogTitle = "NEXUS Plugin Manager";
            opts.dialogBackgroundColour = Colour::fromRGBA(50, 62, 68, 255);
            opts.escapeKeyTriggersCloseButton = true;
            opts.useNativeTitleBar = false;
            opts.resizable = false;
            opts.launchAsync();
        }
    }

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
            if (!boundsRestored)
                setSize(900, 800);
        }
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};