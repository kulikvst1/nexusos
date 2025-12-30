#define JUCE_DISABLE_LEAK_DETECTOR 1
#include <JuceHeader.h>
#include "MainWindow.h"
#include <windows.h> // для GetDriveTypeW

// --- Работа с boot_config.xml прямо здесь ---
static juce::File getBootConfigFile()
{
    auto sysDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("NEXUS_KONTROL_OS");
    sysDir.createDirectory();
    return sysDir.getChildFile("boot_config.xml");
}

static void writeBootConfig(const juce::File& targetFile)
{
    juce::XmlElement root("BootConfig");
    root.setAttribute("FilePath", targetFile.getFullPathName());
    root.setAttribute("FileName", targetFile.getFileName());
    getBootConfigFile().replaceWithText(root.toString());
}

static juce::File readBootConfig()
{
    auto bootFile = getBootConfigFile();
    if (!bootFile.existsAsFile())
        return juce::File();

    std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(bootFile));
    if (!xml) return juce::File();

    juce::String path = xml->getStringAttribute("FilePath");
    if (path.isNotEmpty())
    {
        juce::File target(path);
        if (target.getFileName().equalsIgnoreCase("boot_config.xml"))
            return juce::File(); // защита от самозатирания
        return target;
    }
    return juce::File();
}

// --- Создание структуры папок ---
void ensureNexusFolders()
{
    juce::File baseDir;

    wchar_t dPath[] = L"D:\\";
    UINT type = GetDriveTypeW(dPath);

    if (type == DRIVE_FIXED)
        baseDir = juce::File("D:\\NEXUS");
    else
        baseDir = juce::File("C:\\NEXUS");

    auto irDir = baseDir.getChildFile("IR");
    auto mediaDir = baseDir.getChildFile("MEDIA");
    auto bankDir = baseDir.getChildFile("BANK");
    auto namDir = baseDir.getChildFile("NAM PROFILE");

    if (!irDir.exists())    irDir.createDirectory();
    if (!mediaDir.exists()) mediaDir.createDirectory();
    if (!bankDir.exists())  bankDir.createDirectory();
    if (!namDir.exists())   namDir.createDirectory();
}

// --- Проверка и создание boot_config.xml ---
void ensureBootFile()
{
    auto bootFile = getBootConfigFile();

    // если boot_config.xml отсутствует → создаём дефолтный
    if (!bootFile.existsAsFile())
    {
        auto bankDir = (GetDriveTypeW(L"D:\\") == DRIVE_FIXED)
            ? juce::File("D:\\NEXUS\\BANK")
            : juce::File("C:\\NEXUS\\BANK");
        bankDir.createDirectory();

        juce::File defFile = bankDir.getChildFile("Default.xml");

        juce::XmlElement root("Banks");
        root.setAttribute("Name", "Default Bank");
        defFile.replaceWithText(root.toString());

        writeBootConfig(defFile);

        DBG("[Startup] boot_config.xml отсутствовал → создан: "
            << defFile.getFullPathName());
        return;
    }

    // если boot_config.xml есть → читаем его
    juce::File target = readBootConfig();

    // 🔹 проверка: существует ли сам файл банка
    if (!target.existsAsFile())
    {
        DBG("[Startup] Файл банка отсутствует → пересоздаём Default.xml");

        auto bankDir = (GetDriveTypeW(L"D:\\") == DRIVE_FIXED)
            ? juce::File("D:\\NEXUS\\BANK")
            : juce::File("C:\\NEXUS\\BANK");
        bankDir.createDirectory();

        juce::File defFile = bankDir.getChildFile("Default.xml");

        juce::XmlElement root("Banks");
        root.setAttribute("Name", "Default Bank");
        defFile.replaceWithText(root.toString());

        writeBootConfig(defFile);
    }
}

// --- Основное приложение ---
class MyApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "NEXUS OS"; }
    const juce::String getApplicationVersion() override { return "1.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String& /*commandLine*/) override
    {
        ensureNexusFolders();
        ensureBootFile(); // 🔹 создаём boot_config.xml при старте

        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override
    {
        if (mainWindow != nullptr)
        {
            if (auto* mcc = dynamic_cast<MainContentComponent*>(mainWindow->getContentComponent()))
            {
                if (auto* ic = mcc->getInputControlComponent())
                    ic->saveSettings();
            }
        }
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted(const juce::String&) override {}

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(MyApplication)
