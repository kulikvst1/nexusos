#pragma once
#include <JuceHeader.h>
#include "AudioMidiSettingsDialog.h"
#include "MainContentComponent.h"
#include "vst_host.h"
#include "PluginManagerBox.h"
#include "FileManager.h"
#include "BinaryData.h"
#include <tlhelp32.h>
using namespace juce;

class TouchMenuLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // увеличенный шрифт автоматически увеличивает высоту строки
    juce::Font getPopupMenuFont() override
    {
        return juce::Font(30.0f, juce::Font::bold);
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
        // используем базовую реализацию, чтобы галочки рисовались
        LookAndFeel_V4::drawPopupMenuItem(g, area,
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
    enum MenuIds {
        miExit = 1, miAudioSettings, miPluginManager, miFullScreen,
        miToggleLooper, miToggleTuner, miToggleTunerStyle,
        miFileManager, miUpdater, miScreenKey, miPowerOff
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


        // загрузка иконок
        iconFscreen = loadIcon(BinaryData::fscreen_png, BinaryData::fscreen_pngSize);
        iconconfig = loadIcon(BinaryData::config_png, BinaryData::config_pngSize);
        iconvstManager = loadIcon(BinaryData::vstManager_png, BinaryData::vstManager_pngSize);
        iconfileManeger = loadIcon(BinaryData::fileManeger_png, BinaryData::fileManeger_pngSize);
        iconlooper = loadIcon(BinaryData::looper_png, BinaryData::looper_pngSize);
        iconTuning = loadIcon(BinaryData::Tuning_png, BinaryData::Tuning_pngSize);
        iconTunMode = loadIcon(BinaryData::TunMode_png, BinaryData::TunMode_pngSize);
        iconKey = loadIcon(BinaryData::Key_png, BinaryData::Key_pngSize);
        iconKeyOff = loadIcon(BinaryData::KeyOff_png, BinaryData::KeyOff_pngSize);
        iconUpdate = loadIcon(BinaryData::Update_png, BinaryData::Update_pngSize);
        iconclose = loadIcon(BinaryData::close_png, BinaryData::close_pngSize);
        iconPowerOff = loadIcon(BinaryData::PowerOff_png, BinaryData::PowerOff_pngSize);
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
    StringArray getMenuBarNames() override { return { "NEXUS OS" }; }

    PopupMenu getMenuForIndex(int, const String&) override
    {
        PopupMenu m;
        m.setLookAndFeel(&touchMenuLAF);

        // Заголовок
        m.addItem(-1, "NEXUS OS", false, false);

        // Audio Settings
        {
            PopupMenu::Item item;
            item.itemID = miAudioSettings;
            item.text = "Audio Settings";
            item.isEnabled = true;
            if (iconconfig) item.image = std::unique_ptr<juce::Drawable>(iconconfig->createCopy());
            m.addItem(item);
        }

        m.addItem(-1, "Managers", false, false);

        // Plugin Manager
        {
            PopupMenu::Item item;
            item.itemID = miPluginManager;
            item.text = "Plugin Manager";
            item.isEnabled = true;
            if (iconvstManager) item.image = std::unique_ptr<juce::Drawable>(iconvstManager->createCopy());
            m.addItem(item);
        }

        // File Manager
        {
            PopupMenu::Item item;
            item.itemID = miFileManager;
            item.text = "File Manager";
            item.isEnabled = true;
            if (iconfileManeger) item.image = std::unique_ptr<juce::Drawable>(iconfileManeger->createCopy());
            m.addItem(item);
        }

        m.addItem(-1, "TAB Shown", false, false);

        // Looper TAB
        {
            PopupMenu::Item item;
            item.itemID = miToggleLooper;
            item.text = "Looper";
            item.isEnabled = true;
            item.isTicked = isLooperShown;
            if (iconlooper) item.image = std::unique_ptr<juce::Drawable>(iconlooper->createCopy());
            m.addItem(item);
        }

        // Tuner TAB
        {
            PopupMenu::Item item;
            item.itemID = miToggleTuner;
            item.text = "Tuner";
            item.isEnabled = true;
            item.isTicked = isTunerShown;
            if (iconTuning) item.image = std::unique_ptr<juce::Drawable>(iconTuning->createCopy());
            m.addItem(item);
        }

        // Tuner Style
        {
            PopupMenu::Item item;
            item.itemID = miToggleTunerStyle;
            item.text = "Tuner Style: " + String(tunerVisualStyle == TunerVisualStyle::Classic ? "Classic" : "Triangles");
            item.isEnabled = true;
            item.isTicked = false; // если нужно — можно переключать и визуально
            if (iconTunMode) item.image = std::unique_ptr<juce::Drawable>(iconTunMode->createCopy());
            m.addItem(item);
        }

        m.addItem(-1, "", false, false);

        // FullScreen
        {
            PopupMenu::Item item;
            item.itemID = miFullScreen;
            item.text = "FullScreen";
            item.isEnabled = true;
            item.isTicked = isFullScreenMode;
            if (iconFscreen) item.image = std::unique_ptr<juce::Drawable>(iconFscreen->createCopy());
            m.addItem(item);
        }

        // ScreenKey
        juce::File keyExe("C:\\NEXUS OS\\NEXUS KEY.exe");
        if (keyExe.existsAsFile())
        {
            PopupMenu::Item item;
            item.itemID = miScreenKey;
            item.isEnabled = true;

            if (isScreenKeyEnabled)
            {
                item.image = getIcon("Key");
                item.text = "Screen Key ON";
            }
            else
            {
                item.image = getIcon("KeyOff");
                item.text = "Screen Key OFF";
            }

            m.addItem(item);
        }


        m.addItem(-1, "", false, false);

        // Updater
        {
            PopupMenu::Item item;
            item.itemID = miUpdater;
            item.text = "NEXUS Updater";
            item.isEnabled = true;
            if (iconUpdate) item.image = std::unique_ptr<juce::Drawable>(iconUpdate->createCopy());
            m.addItem(item);
        }

        m.addItem(-1, "", false, false);

        // Exit
        {
            PopupMenu::Item item;
            item.itemID = miExit;
            item.text = "NEXUS EXIT";
            item.isEnabled = true;
            if (iconclose) item.image = std::unique_ptr<juce::Drawable>(iconclose->createCopy());
            m.addItem(item);
        }

        // PowerOff
        {
            PopupMenu::Item item;
            item.itemID = miPowerOff;
            item.text = "NEXUS OFF";
            item.isEnabled = true;
            if (iconPowerOff) item.image = std::unique_ptr<juce::Drawable>(iconPowerOff->createCopy());
            m.addItem(item);
        }

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
                auto findNexusRoot = []() -> juce::File {
                    const juce::File dRoot{ "D:\\" };
                    const juce::File cRoot{ "C:\\" };
                    juce::File cand = dRoot.getChildFile("NEXUS");
                    if (cand.exists() && cand.isDirectory()) return cand;
                    cand = cRoot.getChildFile("NEXUS");
                    if (cand.exists() && cand.isDirectory()) return cand;
                    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("NEXUS");
                    };
                juce::File nexusRoot = findNexusRoot();

                auto* fm = new FileManager(nexusRoot, FileManager::Mode::General);
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
                    int h = 400;
                    int x = screenBounds.getCentreX() - w / 2;
                    int y = screenBounds.getY() + 150;
                    dialog->setBounds(x, y, w, h);
                }
                break;
            }

            case miScreenKey:
            {
                if (isScreenKeyEnabled)
                {
                    stopScreenKey();

                    // дополнительная проверка: если процесс остался жив
                    if (isProcessRunning("NEXUS KEY.exe"))
                        system("taskkill /IM \"NEXUS KEY.exe\" /F >nul 2>&1");
                }
                else
                {
                    startScreenKey();
                }

                menuItemsChanged();

                // заставляем меню перерисоваться
                break;
            }



            case miUpdater:
            {
                juce::File updater("C:\\NEXUS OS\\NEXUS Updater.exe");
                if (updater.existsAsFile())
                {
                    updater.startAsProcess();
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
                    juce::String::fromUTF8("✔️ Yes"),
                    juce::String::fromUTF8("❌ No"),
                    nullptr,
                    juce::ModalCallbackFunction::create([this](int result)
                        {
                            if (result == 1)
                                JUCEApplication::getInstance()->systemRequestedQuit();
                        }));
                break;
            }

            case miPowerOff:
            {
                juce::AlertWindow::showOkCancelBox(
                    juce::AlertWindow::WarningIcon,
                    juce::String::fromUTF8("⚠️ Power Off"),
                    "Are you sure you want to shut down the NEXUS?",
                    juce::String::fromUTF8("✔️ Yes"),
                    juce::String::fromUTF8("❌ No"),
                    nullptr,
                    juce::ModalCallbackFunction::create([this](int result)
                        {
                            if (result == 1)
                                system("shutdown /s /t 0");
                        }));
                break;
            }
            }
        }
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

    bool isScreenKeyEnabled{ false };
    TunerVisualStyle tunerVisualStyle{ TunerVisualStyle::Triangles };

    // иконки
    std::unique_ptr<juce::Drawable> iconFscreen, iconconfig, iconvstManager,
        iconfileManeger, iconlooper, iconTuning, iconTunMode,
        iconKey, iconKeyOff, iconUpdate, iconclose, iconPowerOff;

    TouchMenuLookAndFeel touchMenuLAF;

    static std::unique_ptr<juce::Drawable> loadPNG(const void* data, size_t dataSize)
    {
        juce::MemoryInputStream stream(data, dataSize, false);
        return juce::Drawable::createFromImageDataStream(stream);
    }

    static std::unique_ptr<juce::Drawable> getIcon(const juce::String& name)
    {
        if (name == "Key")
            return loadPNG(BinaryData::Key_png, BinaryData::Key_pngSize);
        if (name == "KeyOff")
            return loadPNG(BinaryData::KeyOff_png, BinaryData::KeyOff_pngSize);
        return {};
    }

    

    void loadAudioSettings(MainContentComponent& content)
    {
        auto dir = File::getSpecialLocation(File::userApplicationDataDirectory)
            .getChildFile("NEXUS_OS_AUDIO_SET");
        auto file = dir.getChildFile("AudioSettings.xml");

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
                auto dir = File::getSpecialLocation(File::userApplicationDataDirectory)
                    .getChildFile("NEXUS_OS_AUDIO_SET");
                dir.createDirectory();
                auto file = dir.getChildFile("AudioSettings.xml");

                // сохраняем с префиксом XML, как у тебя было
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

            isScreenKeyEnabled = xml->getBoolAttribute("screenKey", false);
        }

        // 🔹 синхронизация состояния клавы
        if (isScreenKeyEnabled)
        {
            // если флаг включён, но процесса нет — запускаем
            if (!isProcessRunning("NEXUS KEY.exe"))
            {
                juce::File keyExe("C:\\NEXUS OS\\NEXUS KEY.exe");
                if (keyExe.existsAsFile())
                    keyExe.startAsProcess();
            }
        }
        else
        {
            // если флаг выключен, но процесс остался — убиваем
            if (isProcessRunning("NEXUS KEY.exe"))
                stopScreenKey();
        }
    }


    void saveWindowState()
    {
        XmlElement xml("WindowState");
        xml.setAttribute("fullScreen", isFullScreenMode);
        xml.setAttribute("showLooperTab", isLooperShown);
        xml.setAttribute("showTunerTab", isTunerShown);
        xml.setAttribute("tunerStyle", tunerVisualStyle == TunerVisualStyle::Classic ? "Classic" : "Triangles");
        xml.setAttribute("screenKey", isScreenKeyEnabled);

        if (!isFullScreenMode)
            xml.setAttribute("bounds", getBounds().toString());

        auto dir = File::getSpecialLocation(File::userApplicationDataDirectory);
        dir.createDirectory();
        auto file = dir.getChildFile("NEXUS_OS_WindowState.xml");

        // сохраняем с префиксом XML, как у тебя было
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
    // запуск клавы
    void startScreenKey()
    {
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;

        std::wstring exePath = L"C:\\NEXUS OS\\NEXUS KEY.exe";

        if (CreateProcessW(exePath.c_str(), nullptr, nullptr, nullptr,
            FALSE, 0, nullptr, nullptr, &si, &pi))
        {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            isScreenKeyEnabled = true;
            saveWindowState();
            Logger::writeToLog("ScreenKey started via WinAPI.");
        }
    }

    // остановка клавы
    void stopScreenKey()
    {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) return;

        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(hSnapshot, &pe))
        {
            do
            {
#ifdef UNICODE
                if (_wcsicmp(pe.szExeFile, L"NEXUS KEY.exe") == 0)
#else
                if (_stricmp(pe.szExeFile, "NEXUS KEY.exe") == 0)
#endif
                {
                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                    if (hProcess)
                    {
                        TerminateProcess(hProcess, 0);
                        CloseHandle(hProcess);
                        Logger::writeToLog("ScreenKey stopped via WinAPI.");
                    }
                    break;
                }
            } while (Process32Next(hSnapshot, &pe));
        }
        CloseHandle(hSnapshot);

        isScreenKeyEnabled = false;
        saveWindowState();
    }

    bool isProcessRunning(const TCHAR* processName)
    {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE)
            return false;

        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(PROCESSENTRY32);

        bool found = false;

        if (Process32First(hSnapshot, &pe))
        {
            do
            {
#ifdef UNICODE
                if (_wcsicmp(pe.szExeFile, processName) == 0)
#else
                if (_stricmp(pe.szExeFile, processName) == 0)
#endif
                {
                    found = true;
                    break;
                }
            } while (Process32Next(hSnapshot, &pe));
        }

        CloseHandle(hSnapshot);
        return found;
    }

    std::unique_ptr<juce::Drawable> loadIcon(const void* data, size_t size)
    {
        juce::MemoryInputStream stream(data, size, false);
        juce::Image image = juce::PNGImageFormat().decodeImage(stream);
        if (image.isValid())
        {
            auto drawable = std::make_unique<juce::DrawableImage>();
            drawable->setImage(image);
            return drawable;
        }
        return {};
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};
