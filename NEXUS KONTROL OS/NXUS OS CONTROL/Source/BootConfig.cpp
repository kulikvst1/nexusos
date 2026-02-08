#include "BootConfig.h"
#include <windows.h> // для GetDriveTypeW

juce::File getBootConfigFile()
{
    auto sysDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("NEXUS_KONTROL_OS");
    sysDir.createDirectory();
    return sysDir.getChildFile("boot_config.xml");
}

void writeBootConfig(const juce::File& targetFile)
{
    juce::XmlElement root("BootConfig");
    root.setAttribute("FilePath", targetFile.getFullPathName());
    root.setAttribute("FileName", targetFile.getFileName());
    getBootConfigFile().replaceWithText(root.toString());
}

juce::File readBootConfig()
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

void ensureBootFile()
{
    auto bootFile = getBootConfigFile();

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

        DBG("[Startup] boot_config.xml отсутствовал ? создан: " << defFile.getFullPathName());
        return;
    }

    juce::File target = readBootConfig();
    if (!target.existsAsFile())
    {
        DBG("[Startup] Файл банка отсутствует ? пересоздаём Default.xml");

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
