#include <juce_gui_basics/juce_gui_basics.h>
#include "KeyboardWindow.h"

class NexusKeyApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "NEXUS KEY"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override
    {
        overlay = std::make_unique<KeyboardWindow>();
    }

    void shutdown() override
    {
        overlay = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String&) override {}

private:
    std::unique_ptr<KeyboardWindow> overlay;
};

START_JUCE_APPLICATION(NexusKeyApplication)
