#define JUCE_DISABLE_LEAK_DETECTOR 1
#include <JuceHeader.h>
#include "MainWindow.h"

class MyApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "NEXUS OS"; }
    const juce::String getApplicationVersion() override { return "1.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String& /*commandLine*/) override
    {
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted(const juce::String& /*commandLine*/) override { }

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(MyApplication)
