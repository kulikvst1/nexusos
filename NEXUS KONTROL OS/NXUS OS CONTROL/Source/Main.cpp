#define JUCE_DISABLE_LEAK_DETECTOR 1
#include <JuceHeader.h>
#include "MainWindow.h"
#include <windows.h> // для GetDriveTypeW

// Вспомогательная функция для создания структуры папок
void ensureNexusFolders()
{
    juce::File baseDir;

    // Проверяем диск D
    wchar_t dPath[] = L"D:\\";
    UINT type = GetDriveTypeW(dPath);

    if (type == DRIVE_FIXED) // фиксированный диск
        baseDir = juce::File("D:\\NEXUS");
    else
        baseDir = juce::File("C:\\NEXUS");

    // Подпапки
    auto irDir = baseDir.getChildFile("IR");
    auto mediaDir = baseDir.getChildFile("MEDIA");
    auto bankDir = baseDir.getChildFile("BANK");
    auto namDir = baseDir.getChildFile("NAM PROFILE");

    if (!irDir.exists())
        irDir.createDirectory();

    if (!mediaDir.exists())
        mediaDir.createDirectory();

    if (!bankDir.exists())
        bankDir.createDirectory();

    if (!namDir.exists())
        namDir.createDirectory();
}

class MyApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "NEXUS OS"; }
    const juce::String getApplicationVersion() override { return "1.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String& /*commandLine*/) override
    {
        // Создаём структуру папок при старте
        ensureNexusFolders();

        // Запускаем главное окно
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