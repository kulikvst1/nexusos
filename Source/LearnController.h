#pragma once
#include <JuceHeader.h>

// ────────────────────────────────────────────────────────────────
//  Мини-машина состояний для режима LEARN
// ────────────────────────────────────────────────────────────────
class LearnController
{
public:
    struct Listener
    {
        virtual void learnStarted(int slot) = 0;
        virtual void learnFinished(int slot, int paramIdx, const juce::String& paramName) = 0;
        virtual void learnCancelled(int slot) = 0;
        virtual ~Listener() = default;
    };

    explicit LearnController(Listener& l) : listener(l) {}

    //––––– GUI-поток –––––
    void begin(int slot)       // нажали «LEARN n»
    {
        if (active) cancel();
        currentSlot = slot;
        active = true;
        listener.learnStarted(currentSlot);
    }
    void cancel()               // повторный клик, Esc, выгрузка плагина
    {
        if (!active) return;
        active = false;
        listener.learnCancelled(currentSlot);
        currentSlot = -1;
    }

    //––––– аудио-поток –––––
    void parameterTouched(int paramIdx, const juce::String& name)
    {
        if (!active) return;
        active = false;
        const int slot = currentSlot;
        currentSlot = -1;

        juce::MessageManager::callAsync([this, slot, paramIdx, name]
            {
                listener.learnFinished(slot, paramIdx, name);
            });
    }

    bool isActive()   const noexcept { return active; }
    int  slot()       const noexcept { return currentSlot; }

private:
    Listener& listener;
    std::atomic<bool>   active{ false };
    int                 currentSlot{ -1 };
};
