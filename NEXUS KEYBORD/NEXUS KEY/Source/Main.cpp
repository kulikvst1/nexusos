#include <juce_gui_basics/juce_gui_basics.h>
#include "KeyboardWindow.h"

class NexusKeyApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "NEXUS KEY"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }

    void initialise(const juce::String&) override
    {
        // ñîçäà¸ì overlay?îêíî êëàâèàòóðû
        overlay.reset(new KeyboardWindow());
        overlay->showDockedBottom(); // ïåðâè÷íûé ïîêàç
    }

    void shutdown() override
    {
        overlay = nullptr;
    }

private:
    std::unique_ptr<KeyboardWindow> overlay;
};

START_JUCE_APPLICATION(NexusKeyApplication)
