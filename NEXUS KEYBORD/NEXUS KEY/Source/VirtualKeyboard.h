#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>
#include "KeySender.h"

class VirtualKeyboard : public juce::Component
{
public:
    VirtualKeyboard(IKeySender& sender,
        std::function<void()> onAnyKey = {},
        std::function<void()> onClose = {})
        : keySender(sender), onAnyKey(std::move(onAnyKey)), onClose(std::move(onClose))
    {
        setWantsKeyboardFocus(false);
        buildLayout();
        for (auto& row : rows)
            for (auto* b : row)
                addAndMakeVisible(b);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::darkgrey);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);
        const int gap = 6;
        int y = area.getY();

        for (auto& row : rows)
        {
            int totalWidth = 0;
            for (auto* b : row)
                totalWidth += (int)b->getProperties().getWithDefault("keyW", 52) + gap;
            totalWidth -= gap;

            int x = area.getCentreX() - totalWidth / 2;

            for (auto* b : row)
            {
                int w = (int)b->getProperties().getWithDefault("keyW", 52);
                b->setBounds(x, y, w, 52);
                x += w + gap;
            }
            y += 52 + gap;
        }
    }

private:
    IKeySender& keySender;
    std::function<void()> onAnyKey;
    std::function<void()> onClose;

    juce::OwnedArray<juce::TextButton> allKeys;
    std::vector<std::vector<juce::TextButton*>> rows;

    bool shiftActive = false;
    bool capsActive = false;

    juce::TextButton* shiftLeft = nullptr;
    juce::TextButton* shiftRight = nullptr;
    juce::TextButton* capsButton = nullptr;

    // ===== Логика =====
    void toggleShift()
    {
        shiftActive = !shiftActive;
        auto colour = shiftActive ? juce::Colours::yellow : juce::Colours::lightgrey;
        if (shiftLeft)  shiftLeft->setColour(juce::TextButton::buttonColourId, colour);
        if (shiftRight) shiftRight->setColour(juce::TextButton::buttonColourId, colour);
    }

    void addKey(std::vector<juce::TextButton*>& row,
        const juce::String& label,
        std::function<void()> action,
        int w = 52)
    {
        auto* b = new juce::TextButton(label);
        b->setWantsKeyboardFocus(false);
        b->setColour(juce::TextButton::buttonColourId, juce::Colours::lightgrey);
        b->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        b->onClick = [this, action]() { action(); if (onAnyKey) onAnyKey(); };
        b->getProperties().set("keyW", w);
        allKeys.add(b);
        row.push_back(b);
    }

    void buildLayout()
    {
        const int keyW = 52;
        const int wideW = 100;
        const int enterW = 120;
        const int shiftW = 120;
        const int spaceW = 400;

        // Row 1: цифры + Backspace
        {
            std::vector<juce::TextButton*> row;
            for (auto c : juce::String("1234567890"))
                addKey(row, juce::String::charToString(c),
                    [this, c] { keySender.sendChar(c); }, keyW);
            addKey(row, "Backspace", [this] { keySender.backspace(); }, wideW);
            rows.push_back(row);
        }

        // Row 2: QWERTYUIOP + Del
        {
            std::vector<juce::TextButton*> row;
            for (auto c : juce::String("QWERTYUIOP"))
                addKey(row, juce::String::charToString(c),
                    [this, c] { handleCharKey(c); }, keyW);

#ifdef _WIN32
            addKey(row, "Del", [this] { keySender.sendVk(VK_DELETE); }, keyW);
#else
            addKey(row, "Del", [this] { keySender.sendVk(KEY_DELETE); }, keyW);
#endif
            rows.push_back(row);
        }

        // Row 3: Caps + ASDFGHJKL + Enter
        {
            std::vector<juce::TextButton*> row;
            capsButton = new juce::TextButton("Caps");
            capsButton->setWantsKeyboardFocus(false);
            capsButton->setColour(juce::TextButton::buttonColourId, juce::Colours::lightgrey);
            capsButton->setColour(juce::TextButton::textColourOffId, juce::Colours::black); // текст всегда чёрный
            capsButton->onClick = [this] {
                capsActive = !capsActive;
                capsButton->setColour(juce::TextButton::buttonColourId,
                    capsActive ? juce::Colours::yellow : juce::Colours::lightgrey);
                };

            capsButton->getProperties().set("keyW", wideW);
            addAndMakeVisible(capsButton);
            allKeys.add(capsButton);
            row.push_back(capsButton);

            for (auto c : juce::String("ASDFGHJKL"))
                addKey(row, juce::String::charToString(c),
                    [this, c] { handleCharKey(c); }, keyW);

            addKey(row, "Enter", [this] { keySender.enter(); if (onClose) onClose(); }, enterW);
            rows.push_back(row);
        }

        // Row 4: Shift + ZXCVBNM + Shift
        {
            std::vector<juce::TextButton*> row;

            // Левый Shift
            shiftLeft = new juce::TextButton("Shift");
            shiftLeft->setWantsKeyboardFocus(false);
            shiftLeft->setColour(juce::TextButton::buttonColourId, juce::Colours::lightgrey);
            shiftLeft->setColour(juce::TextButton::textColourOffId, juce::Colours::black); // текст чёрный
            shiftLeft->onClick = [this] { toggleShift(); };
            shiftLeft->getProperties().set("keyW", shiftW);
            addAndMakeVisible(shiftLeft);
            allKeys.add(shiftLeft);
            row.push_back(shiftLeft);

            // Буквы Z..M
            for (auto c : juce::String("ZXCVBNM"))
                addKey(row, juce::String::charToString(c),
                    [this, c] { handleCharKey(c); }, keyW);

            // Правый Shift
            shiftRight = new juce::TextButton("Shift");
            shiftRight->setWantsKeyboardFocus(false);
            shiftRight->setColour(juce::TextButton::buttonColourId, juce::Colours::lightgrey);
            shiftRight->setColour(juce::TextButton::textColourOffId, juce::Colours::black); // текст чёрный
            shiftRight->onClick = [this] { toggleShift(); };
            shiftRight->getProperties().set("keyW", shiftW);
            addAndMakeVisible(shiftRight);
            allKeys.add(shiftRight);
            row.push_back(shiftRight);

            rows.push_back(row);
        }

        // Row 5: ← ↑ [Space] ↓ →
        {
            std::vector<juce::TextButton*> row;

            // Две слева от пробела
            addKey(row, juce::String::charToString((juce::juce_wchar)0x2190), // ←
                [this] {
#ifdef _WIN32
                    keySender.sendVk(VK_LEFT);
#else
                    keySender.sendVk(KEY_LEFT);
#endif
                }, keyW);

            addKey(row, juce::String::charToString((juce::juce_wchar)0x2191), // ↑
                [this] {
#ifdef _WIN32
                    keySender.sendVk(VK_UP);
#else
                    keySender.sendVk(KEY_UP);
#endif
                }, keyW);

            // Пробел
            addKey(row, "Space", [this] { keySender.sendSpace(); }, spaceW);

            // Две справа от пробела
            addKey(row, juce::String::charToString((juce::juce_wchar)0x2193), // ↓
                [this] {
#ifdef _WIN32
                    keySender.sendVk(VK_DOWN);
#else
                    keySender.sendVk(KEY_DOWN);
#endif
        }, keyW);

            addKey(row, juce::String::charToString((juce::juce_wchar)0x2192), // →
                [this] {
#ifdef _WIN32
                    keySender.sendVk(VK_RIGHT);
#else
                    keySender.sendVk(KEY_RIGHT);
#endif
    }, keyW);

            rows.push_back(row);
}

    }

    // ===== Логика =====
    void handleCharKey(juce::juce_wchar c)
    {
        juce::juce_wchar outChar = c;

        if (capsActive ^ shiftActive) // XOR: либо Caps, либо Shift
            outChar = juce::CharacterFunctions::toUpperCase(c);
        else
            outChar = juce::CharacterFunctions::toLowerCase(c);

        keySender.sendChar(outChar);

        if (shiftActive) {
            shiftActive = false;
            if (shiftLeft)  shiftLeft->setColour(juce::TextButton::buttonColourId, juce::Colours::lightgrey);
            if (shiftRight) shiftRight->setColour(juce::TextButton::buttonColourId, juce::Colours::lightgrey);
        }
    }

    void activateShift()
    {
        shiftActive = true;
        if (shiftLeft)  shiftLeft->setColour(juce::TextButton::buttonColourId, juce::Colours::yellow);
        if (shiftRight) shiftRight->setColour(juce::TextButton::buttonColourId, juce::Colours::yellow);
    }
};
