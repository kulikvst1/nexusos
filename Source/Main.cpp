#define JUCE_DISABLE_LEAK_DETECTOR 1
#include <JuceHeader.h>
#include "MainWindow.h"
#include "BootConfig.h" 
#include "AppStateManager.h"

// --- Основное приложение ---
class MyApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "NEXUS OS"; }
    const juce::String getApplicationVersion() override { return "1.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    AppStateManager appState; // поле класса

    void initialise(const juce::String&) override
    {
        int prev = appState.readState();

        if (prev == 1)
        {
            juce::AlertWindow::showOkCancelBox(
                juce::AlertWindow::WarningIcon,
                juce::String::fromUTF8("⚠️ Startup Warning"),
                "The application did not close correctly last time.\nDo you want to reset to default configuration?",
                juce::String::fromUTF8("✔️ Yes"),
                juce::String::fromUTF8("❌ No"),
                nullptr,
                juce::ModalCallbackFunction::create([this](int result)
                    {
                        if (result == 1)
                        {
                            auto bootFile = getBootConfigFile();
                            if (bootFile.existsAsFile())
                                bootFile.deleteFile();
                            ensureBootFile();
                        }
                        else
                        {
                            ensureBootFile();
                        }

                        appState.markRunning();
                        mainWindow = std::make_unique<MainWindow>(getApplicationName());
                    })
            );
        }
        else
        {
            ensureBootFile();
            appState.markRunning();
            mainWindow = std::make_unique<MainWindow>(getApplicationName());
        }
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

        AppStateManager appState;
        appState.markCleanExit(); // ставим "2"

        mainWindow = nullptr;
    }


    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted(const juce::String&) override {}

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(MyApplication)
