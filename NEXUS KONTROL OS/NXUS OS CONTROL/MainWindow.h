#pragma once
#include <JuceHeader.h>
#include "AudioMidiSettingsDialog.h"
#include "MainContentComponent.h"
#include "vst_host.h"
#include "PluginManagerBox.h"
#include "FileManager.h"

using namespace juce;
// Custom LookAndFeel for MainWindow menu
class TouchMenuLookAndFeel : public juce::LookAndFeel_V4
{
public:
    juce::Font getPopupMenuFont() override
    {
        // bigger font for touch screens
        return juce::Font(40.0f, juce::Font::bold);
    }

    void drawPopupMenuItem(juce::Graphics& g,
        const juce::Rectangle<int>& area,
        bool isSeparator,
        bool isActive,
        bool isHighlighted,
        bool isTicked,
        bool hasSubMenu,
        const juce::String& text,
        const juce::String& shortcutKeyText,
        const juce::Drawable* icon,
        const juce::Colour* textColour) override
    {
        // increase row height for touch
        juce::Rectangle<int> r(area);
        r.setHeight(40);

        LookAndFeel_V4::drawPopupMenuItem(g, r,
            isSeparator,
            isActive,
            isHighlighted,
            isTicked,
            hasSubMenu,
            text,
            shortcutKeyText,
            icon,
            textColour);
    }
};

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
        miToggleTunerStyle,
        miFileManager,
        miUpdater,
        miScreenKey,
        miPowerOff
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

        loadWindowState();

        auto* content = new MainContentComponent(pluginManager);
        setContentOwned(content, true);

        loadAudioSettings(*content);

        content->setLooperTabVisible(isLooperShown);
        content->setTunerTabVisible(isTunerShown);
        content->setTunerStyleClassic(tunerVisualStyle == TunerVisualStyle::Classic);

        updateFullScreenMode();
        setVisible(true);
        if (isScreenKeyEnabled)
        {
            juce::File keyExe("C:\\NEXUS OS\\NEXUS KEY.exe");
            if (keyExe.existsAsFile())
                keyExe.startAsProcess();
        }

    }

    ~MainWindow() override
    {
        setMenuBar(nullptr);

        auto settingsFile = File::getSpecialLocation(File::userApplicationDataDirectory)
            .getChildFile("PluginSettings.xml");
        pluginManager.saveToFile(settingsFile);

        clearContentComponent();
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
        m.setLookAndFeel(&touchMenuLAF); // ← применяем стиль

        m.addItem(miAudioSettings, juce::String::fromUTF8("⚙️ Audio/MIDI Settings"));
        m.addItem(-1, "", false, false); // добавляет пространство
        m.addItem(miPluginManager, juce::String::fromUTF8("📦 Plugin Manager"));
        m.addItem(miFileManager, juce::String::fromUTF8("🗂️ FileManager"));
        m.addItem(-1, "", false, false); // добавляет пространство
        m.addItem(miToggleLooper, "Show LOOPER TAB", true, isLooperShown);
        m.addItem(miToggleTuner, "Show TUNER TAB", true, isTunerShown);
        m.addItem(miToggleTunerStyle,
            "Tuner Style: " + String(tunerVisualStyle == TunerVisualStyle::Classic ? "Classic" : "Triangles"));
        m.addItem(-1, "", false, false); // добавляет пространство
        m.addItem(miFullScreen, juce::String::fromUTF8("🖥️ FullScreen"), true, isFullScreenMode);
        juce::File keyExe("C:\\NEXUS OS\\NEXUS KEY.exe");
        if (keyExe.existsAsFile())
        {
            m.addItem(miScreenKey, juce::String::fromUTF8("⌨️ ScreenKey"), true, isScreenKeyEnabled);
            m.addSeparator();
        }
        m.addItem(-1, "", false, false); // добавляет пространство
        m.addItem(miUpdater, juce::String::fromUTF8("🔄 NEXUS Updater"));
        m.addItem(-1, "", false, false); // добавляет пространство
        m.addItem(miExit, juce::String::fromUTF8(" ❌ NEXUS EXIT"));
        m.addItem(miPowerOff, juce::String::fromUTF8("⏻ NEXUS OFF"));
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
                saveWindowState();
                break;

            case miToggleTuner:
                isTunerShown = !isTunerShown;
                mcc->setTunerTabVisible(isTunerShown);
                saveWindowState();
                break;

            case miToggleTunerStyle:
                tunerVisualStyle = (tunerVisualStyle == TunerVisualStyle::Classic)
                    ? TunerVisualStyle::Triangles
                    : TunerVisualStyle::Classic;
                mcc->setTunerStyleClassic(tunerVisualStyle == TunerVisualStyle::Classic);
                saveWindowState();
                break;

            case miFileManager:
            {
                // Локальная лямбда для поиска корня NEXUS: сначала D:\NEXUS, затем C:\NEXUS, иначе Documents/NEXUS
                auto findNexusRoot = []() -> juce::File
                    {
                        const juce::File dRoot{ "D:\\" };
                        const juce::File cRoot{ "C:\\" };
                        juce::File cand = dRoot.getChildFile("NEXUS");
                        if (cand.exists() && cand.isDirectory()) return cand;
                        cand = cRoot.getChildFile("NEXUS");
                        if (cand.exists() && cand.isDirectory()) return cand;
                        return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("NEXUS");
                    };
                juce::File nexusRoot = findNexusRoot();
                // Создаём менеджер с найденным корнем
                auto* fm = new FileManager(nexusRoot, FileManager::Mode::General);
                // Гарантируем, что менеджер не сможет подняться выше nexusRoot
                fm->setRootLocked(true);
                fm->setConfirmCallback([this](const juce::File& file) {
                    DBG("Double-clicked: " + file.getFullPathName());
                    });
                juce::DialogWindow::LaunchOptions opts;
                opts.content.setOwned(fm);
                opts.dialogTitle = "File Manager";
                opts.dialogBackgroundColour = juce::Colours::darkgrey;
                opts.escapeKeyTriggersCloseButton = true;
                opts.useNativeTitleBar = false;
                opts.resizable = true;
                auto* dialog = opts.launchAsync();
                if (dialog != nullptr)
                {
                    auto screenBounds = juce::Desktop::getInstance().getDisplays().getMainDisplay().userArea;

                    int w = 800;
                    int h = 400; // высота панели сверху

                    // центрируем по X, но фиксируем сверху по Y
                    int x = screenBounds.getCentreX() - w / 2;
                    int y = screenBounds.getY() + 150; // смещение от верхнего края

                    dialog->setBounds(x, y, w, h);
                }

                break;
            }

            case miScreenKey:
            {
                isScreenKeyEnabled = !isScreenKeyEnabled;
                saveWindowState();

                if (isScreenKeyEnabled)
                {
                    // Запускаем клавиатуру, если файл существует
                    juce::File keyExe("C:\\NEXUS OS\\NEXUS KEY.exe");
                    if (keyExe.existsAsFile())
                        keyExe.startAsProcess();
                }
                else
                {
                    // Выгружаем процесс клавиатуры
                    system("taskkill /IM \"NEXUS KEY.exe\" /F >nul 2>&1");
                }
                break;
            }


            case miUpdater:
            {
                juce::File updater("C:\\NEXUS OS\\NEXUS Updater.exe");
                if (updater.existsAsFile())
                {
                    updater.startAsProcess();
                    // НЕ закрываем основную программу здесь!
                }
                else
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::WarningIcon,
                        "Updater not found",
                        "C:\\NEXUS OS\\NEXUS Updater.exe not found!");
                }
                break;
            }


            case miExit:
            {
                juce::AlertWindow::showOkCancelBox(
                    juce::AlertWindow::WarningIcon,
                    juce::String::fromUTF8("⚠️ Exit Application"),
                    "Are you sure you want to close NEXUS OS?",
                    juce::String::fromUTF8("✔️ Yes"), juce::String::fromUTF8("❌ No"),
                    nullptr,
                    juce::ModalCallbackFunction::create([this](int result)
                        {
                            if (result == 1) // Yes
                                JUCEApplication::getInstance()->systemRequestedQuit();
                        }));
                break;
            }

            case miPowerOff:
            {
                juce::AlertWindow::showOkCancelBox(
                    juce::AlertWindow::WarningIcon,
                    juce::String::fromUTF8("⚠️ Power Off"),
                    "Are you sure you want to shut down the PC?",
                    juce::String::fromUTF8("✔️ Yes"),
                    juce::String::fromUTF8("❌ No"),
                    nullptr,
                    juce::ModalCallbackFunction::create([this](int result)
                        {
                            if (result == 1) // Yes
                            {
                                // Команда выключения Windows
                                system("shutdown /s /t 0");
                            }
                        }));
                break;
            }
            }
                

        }
    }

    void closeButtonPressed() override
    {
        JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
  
    enum class TunerVisualStyle { Classic, Triangles };
    PluginManager pluginManager;
    bool isFullScreenMode{ false },
        isLooperShown{ true },
        isTunerShown{ true },
        isOutMasterShown{ true },
        isInputMasterShown{ true },
        boundsRestored{ false };
    bool isScreenKeyEnabled{ false }; // ← идентификатор-флаг

    TunerVisualStyle tunerVisualStyle{ TunerVisualStyle::Triangles };

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

    void loadWindowState()
    {
        auto dir = File::getSpecialLocation(File::userApplicationDataDirectory);
        auto file = dir.getChildFile("NEXUS_OS_WindowState.xml");

        std::unique_ptr<XmlElement> xml(XmlDocument::parse(file));
        if (xml != nullptr)
        {
            isFullScreenMode = xml->getBoolAttribute("fullScreen", false);
            isLooperShown = xml->getBoolAttribute("showLooperTab", true);
            isTunerShown = xml->getBoolAttribute("showTunerTab", true);

            const juce::String styleStr = xml->getStringAttribute("tunerStyle", "Triangles");
            tunerVisualStyle = (styleStr == "Classic")
                ? TunerVisualStyle::Classic
                : TunerVisualStyle::Triangles;

            const juce::String bs = xml->getStringAttribute("bounds", "");
            if (!isFullScreenMode && bs.isNotEmpty())
            {
                auto r = juce::Rectangle<int>::fromString(bs);
                if (!r.isEmpty())
                {
                    setBounds(r);
                    boundsRestored = true;
                }
            }

            // читаем флаг ScreenKey
            isScreenKeyEnabled = xml->getBoolAttribute("screenKey", false);

            // если включено и файл существует — запускаем
            if (isScreenKeyEnabled)
            {
                juce::File keyExe("C:\\NEXUS OS\\NEXUS KEY.exe");
                if (keyExe.existsAsFile())
                    keyExe.startAsProcess();
            }
        }
    }

    void saveWindowState()
    {
        XmlElement xml("WindowState");
        xml.setAttribute("fullScreen", isFullScreenMode);
        xml.setAttribute("showLooperTab", isLooperShown);
        xml.setAttribute("showTunerTab", isTunerShown);
        xml.setAttribute("tunerStyle", tunerVisualStyle == TunerVisualStyle::Classic ? "Classic" : "Triangles");
        xml.setAttribute("screenKey", isScreenKeyEnabled); // ← теперь сохраняется правильно

        if (!isFullScreenMode)
            xml.setAttribute("bounds", getBounds().toString());

        auto dir = File::getSpecialLocation(File::userApplicationDataDirectory);
        dir.createDirectory();
        auto file = dir.getChildFile("NEXUS_OS_WindowState.xml");

        String out = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" + xml.toString();
        file.replaceWithText(out);
    }


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
            setAlwaysOnTop(false);
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
   TouchMenuLookAndFeel MainWindow::touchMenuLAF;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};
