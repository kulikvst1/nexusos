#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>
#include <map>
#include "KeySender.h"

#ifndef JUCE_WCHAR_DEFINED
using juce_wchar = wchar_t;
#define JUCE_WCHAR_DEFINED 1
#endif

class BigFontLookAndFeel : public juce::LookAndFeel_V4
{
public:
    juce::Font getTextButtonFont(juce::TextButton& b, int) override
    {
        if (b.getProperties().contains("fontSize"))
            return juce::Font((float)b.getProperties()["fontSize"], juce::Font::plain);

        return juce::Font(22.0f, juce::Font::plain);
    }
};


class VirtualKeyboard : public juce::Component
{
public:
    enum class Mode { Letters, Digits, Symbols };

    VirtualKeyboard(IKeySender& sender,
        std::function<void()> onAnyKey = {},
        std::function<void()> onClose = {})
        : keySender(sender), onAnyKey(std::move(onAnyKey)), onClose(std::move(onClose))
    {
        setWantsKeyboardFocus(false);
        setLookAndFeel(&bigFontLNF);
        buildLayout();
        for (auto& row : rows) for (auto* b : row) addAndMakeVisible(b);

        indicateShiftCaps();
    }

    ~VirtualKeyboard() override { setLookAndFeel(nullptr); }

    void paint(juce::Graphics& g) override { g.fillAll(juce::Colours::darkgrey); }

    void resized() override
    {
        auto full = getLocalBounds();

        int newWidth = (int)(full.getWidth() * scaleFactor);
        auto area = juce::Rectangle<int>(
            (full.getWidth() - newWidth) / 2,
            full.getY(),
            newWidth,
            full.getHeight()
        ).reduced(8);

        const int gap = 6;
        const int gridColumns = 12;
        int cellWidth = (area.getWidth() - (gridColumns - 1) * gap) / gridColumns;
        int y = area.getY();

        std::vector<int> rowWidths;
        rowWidths.reserve(rows.size());
        for (auto& row : rows)
        {
            int w = 0;
            for (auto* b : row)
            {
                float wFactor = (float)b->getProperties().getWithDefault("weight", 1.0f);
                w += (int)(cellWidth * wFactor + gap * (wFactor - 1));
            }
            w += (int(row.size()) - 1) * gap;
            rowWidths.push_back(w);
        }

        for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
        {
            auto& row = rows[rowIndex];
            int x = area.getX();

            if (mode == Mode::Letters || mode == Mode::Digits)
            {
                if (rowIndex == 0)
                {
                    x = area.getX();
                }
                else if (rowIndex == 1)
                {
                    int prevWidth = rowWidths[0];
                    int thisWidth = rowWidths[1];
                    x = area.getX() + (prevWidth - thisWidth) / 2;
                }
                else if (rowIndex == 2)
                {
                    int row2Left = area.getX() + (rowWidths[0] - rowWidths[1]) / 2;
                    x = row2Left;
                }
                else if (rowIndex == 3)
                {
                    int row4Width = rowWidths[3];
                    x = area.getX() + (area.getWidth() - row4Width) / 2;
                }
            }

            for (auto* b : row)
            {
                float wFactor = (float)b->getProperties().getWithDefault("weight", 1.0f);
                int w = (int)(cellWidth * wFactor + gap * (wFactor - 1));
                b->setBounds(x, y, w, 64);
                x += w + gap;
            }

            y += 70;
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
    Mode mode = Mode::Letters;

    juce::TextButton* shiftLeft = nullptr;
    std::vector<juce::TextButton*> letterButtons;

    BigFontLookAndFeel bigFontLNF;
    uint32_t lastShiftTapMs = 0;
    float scaleFactor = 0.85f;
    const juce::Colour keyColour = juce::Colours::lightgrey;
    const juce::Colour textColour = juce::Colours::black;

    static const std::map<juce_wchar, juce_wchar>& symbolShiftMap()
    {
        static const std::map<juce_wchar, juce_wchar> m = {
            { ';', ':' }, { '\'', '\"' }, { '[', '{' }, { ']', '}' },
            { '\\', '|' }, { ',', '<' }, { '.', '>' }, { '/', '?' }
        };
        return m;
    }

    void indicateShiftCaps()
    {
        juce::Colour colour =
            capsActive ? juce::Colours::orange
            : (shiftActive ? juce::Colours::yellow : juce::Colours::lightgrey);

        if (shiftLeft)
        {
            shiftLeft->setColour(juce::TextButton::buttonColourId, colour);
            shiftLeft->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        }
    }

    void updateLetterLabels()
    {
        for (auto* b : letterButtons)
        {
            if (!b) continue;
            juce::String stored = b->getProperties()[juce::Identifier("baseChar")].toString();
            juce_wchar base = stored.isNotEmpty() ? stored[0] : L'?';
            bool upper = capsActive ^ shiftActive;
            juce_wchar newChar = upper
                ? juce::CharacterFunctions::toUpperCase(base)
                : juce::CharacterFunctions::toLowerCase(base);
            b->setButtonText(juce::String::charToString(newChar));
        }
    }

    void addKey(std::vector<juce::TextButton*>& row,
        const juce::String& label,
        std::function<void()> action,
        float weight = 1.0f)
    {
        auto* b = new juce::TextButton(label);
        b->setWantsKeyboardFocus(false);
        b->setMouseClickGrabsKeyboardFocus(false); // ← добавлено
        b->setColour(juce::TextButton::buttonColourId, keyColour);
        b->setColour(juce::TextButton::textColourOffId, textColour);
        b->getProperties().set("weight", weight);

        b->onClick = [this, action]() {
            if (action)
                juce::MessageManager::callAsync([action]() { action(); });
            if (onAnyKey) onAnyKey();
            };

        allKeys.add(b);
        row.push_back(b);
    }

    void addLetterKey(std::vector<juce::TextButton*>& row, juce_wchar c, float weight = 1.0f)
    {
        auto* b = new juce::TextButton(juce::String::charToString(c));
        b->setWantsKeyboardFocus(false);
        b->setMouseClickGrabsKeyboardFocus(false); // ← добавлено
        b->setColour(juce::TextButton::buttonColourId, keyColour);
        b->setColour(juce::TextButton::textColourOffId, textColour);
        b->getProperties().set("weight", weight);
        b->getProperties().set(juce::Identifier("baseChar"), juce::String::charToString(c));

        b->onClick = [this, b]() {
            juce::String stored = b->getProperties()[juce::Identifier("baseChar")].toString();
            juce_wchar base = stored.isNotEmpty() ? stored[0] : L'?';

            bool upper = capsActive ^ shiftActive;
            juce_wchar outChar = upper
                ? juce::CharacterFunctions::toUpperCase(base)
                : juce::CharacterFunctions::toLowerCase(base);

            handleCharKey(outChar);

            if (shiftActive && !capsActive)
            {
                shiftActive = false;
                indicateShiftCaps();
                updateLetterLabels();
            }

            if (onAnyKey) onAnyKey();
            };

        allKeys.add(b);
        row.push_back(b);
        letterButtons.push_back(b);
    }

    void addCharKey(std::vector<juce::TextButton*>& row, juce_wchar c, float weight = 1.0f)
    {
        auto* b = new juce::TextButton(juce::String::charToString(c));
        b->setWantsKeyboardFocus(false);
        b->setMouseClickGrabsKeyboardFocus(false); // ← добавлено
        b->setColour(juce::TextButton::buttonColourId, keyColour);
        b->setColour(juce::TextButton::textColourOffId, textColour);
        b->getProperties().set("weight", weight);

        b->onClick = [this, c]() { handleCharKey(c); if (onAnyKey) onAnyKey(); };

        allKeys.add(b);
        row.push_back(b);
    }


    void rebuildLayout()
    {
        for (auto& row : rows)
            for (auto* b : row)
                removeChildComponent(b);

        allKeys.clear(true);
        rows.clear();
        letterButtons.clear();

        buildLayout();
        for (auto& row : rows)
            for (auto* b : row)
                addAndMakeVisible(b);

        resized();
        if (!letterButtons.empty())
            updateLetterLabels();
    }

    void buildLayout()
    {
        switch (mode)
        {
        case Mode::Letters: buildLettersLayout(); break;
        case Mode::Digits:  buildDigitsLayout();  break;
        case Mode::Symbols: buildSymbolsLayout(); break;
        }
    }
    void buildLettersLayout()
    {
        rows.clear();

        // Row 1: ?123 + QWERTYUIOP + Backspace
        {
            std::vector<juce::TextButton*> row;

            addKey(row, "?123", [this] { mode = Mode::Digits; rebuildLayout(); }, 1.0f);

            for (auto c : juce::String("qwertyuiop"))
                addLetterKey(row, c, 1.0f);

            addKey(row, juce::String::charToString((juce_wchar)0x232B), // ⌫
                [this] { keySender.backspace(); }, 1.0f);

            rows.push_back(row);
        }

        // Row 2: ASDFGHJKL + Del
        {
            std::vector<juce::TextButton*> row;
            for (auto c : juce::String("asdfghjkl"))
                addLetterKey(row, c, 1.0f);

            addKey(row, "Del", [this] { keySender.sendVk(VK_DELETE); }, 1.0f);

            rows.push_back(row);
        }

        // Row 3: Shift + ZXCVBNM + Enter
        {
            std::vector<juce::TextButton*> row;

            // ⇧ Shift
            shiftLeft = new juce::TextButton(juce::String::charToString((juce_wchar)0x21E7));
            shiftLeft->setWantsKeyboardFocus(false);
            shiftLeft->setMouseClickGrabsKeyboardFocus(false); // ← добавлено
            shiftLeft->getProperties().set("weight", 1.5f);
            shiftLeft->getProperties().set("fontSize", 50.0f); // увеличенный значок
            shiftLeft->setColour(juce::TextButton::buttonColourId, keyColour);
            shiftLeft->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
            wireShiftButton(*shiftLeft);
            allKeys.add(shiftLeft);
            row.push_back(shiftLeft);

            for (auto c : juce::String("zxcvbnm"))
                addLetterKey(row, c, 1.0f);

            // ⏎ Enter
            auto* enterBtn = new juce::TextButton(juce::String::charToString((juce_wchar)0x21B5));
            enterBtn->setWantsKeyboardFocus(false);
            enterBtn->setMouseClickGrabsKeyboardFocus(false); // ← добавлено
            enterBtn->getProperties().set("weight", 1.5f);
            enterBtn->getProperties().set("fontSize", 50.0f); // увеличенный значок
            enterBtn->setColour(juce::TextButton::buttonColourId, keyColour);
            enterBtn->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
            enterBtn->onClick = [this] { keySender.enter(); if (onClose) onClose(); };
            allKeys.add(enterBtn);
            row.push_back(enterBtn);

            rows.push_back(row);
        }


        // Row 4: , стрелки + Space + стрелки + .
        {
            std::vector<juce::TextButton*> row;

            addCharKey(row, ',', 1.0f);

            addKey(row, juce::String::charToString((juce_wchar)0x25C0), [this] { keySender.sendVk(VK_LEFT); }, 1.0f); // ◀
            addKey(row, juce::String::charToString((juce_wchar)0x25B2), [this] { keySender.sendVk(VK_UP); }, 1.0f);   // ▲

            addKey(row, "Space", [this] { keySender.sendSpace(); }, 4.0f);

            addKey(row, juce::String::charToString((juce_wchar)0x25BC), [this] { keySender.sendVk(VK_DOWN); }, 1.0f); // ▼
            addKey(row, juce::String::charToString((juce_wchar)0x25B6), [this] { keySender.sendVk(VK_RIGHT); }, 1.0f); // ▶

            addCharKey(row, '.', 1.0f);

            rows.push_back(row);
        }
    }

    void buildDigitsLayout()
    {
        rows.clear();

        // Row 1: ABC + 1234567890 + Backspace
        {
            std::vector<juce::TextButton*> row;
            addKey(row, "ABC", [this] { mode = Mode::Letters; rebuildLayout(); }, 1.0f);

            for (auto c : juce::String("1234567890"))
                addCharKey(row, c, 1.0f);

            addKey(row, juce::String::charToString((juce_wchar)0x232B),
                [this] { keySender.backspace(); }, 1.0f);

            rows.push_back(row);
        }

        // Row 2
        {
            std::vector<juce::TextButton*> row;
            for (auto c : juce::String("-/:;()$&@\""))
                addCharKey(row, c, 1.0f);
            rows.push_back(row);
        }

        // Row 3
        {
            std::vector<juce::TextButton*> row;
            for (auto c : juce::String(".,?!'[]{}"))
                addCharKey(row, c, 1.0f);

            addKey(row, juce::String::charToString((juce_wchar)0x21B5),
                [this] { keySender.enter(); if (onClose) onClose(); }, 1.0f);

            rows.push_back(row);
        }

        // Row 4
        {
            std::vector<juce::TextButton*> row;
            addKey(row, "Space", [this] { keySender.sendSpace(); }, 4.0f);
            rows.push_back(row);
        }
    }

    void buildSymbolsLayout()
    {
        // Row 1
        {
            std::vector<juce::TextButton*> row;
            for (auto c : { '!', '?', ',', '.', '\'', '\"', '[', ']', '{', '}', '#' })
                addCharKey(row, (juce_wchar)c);
            rows.push_back(row);
        }

        // Row 2
        {
            std::vector<juce::TextButton*> row;
            for (auto c : { '%','^','*','+','=','_','|','\\','/' })
                addCharKey(row, (juce_wchar)c);
            rows.push_back(row);
        }

        // Row 3
        {
            std::vector<juce::TextButton*> row;
            addKey(row, "ABC", [this] { mode = Mode::Letters; rebuildLayout(); }, 1.3f);
            addKey(row, "?123", [this] { mode = Mode::Digits;  rebuildLayout(); }, 1.3f);
            addKey(row, "Space", [this] { keySender.sendSpace(); }, 4.0f);
            addKey(row, juce::String::charToString((juce_wchar)0x21B5),
                [this] { keySender.enter(); if (onClose) onClose(); }, 1.5f);
            rows.push_back(row);
        }
    }

    void handleCharKey(juce_wchar c)
    {
        juce_wchar outChar = c;

        if (shiftActive)
        {
            auto it = symbolShiftMap().find(c);
            if (it != symbolShiftMap().end())
                outChar = it->second;
        }


        keySender.sendChar(outChar);

        if (shiftActive)
        {
            shiftActive = false;
            indicateShiftCaps();
            updateLetterLabels();
        }
    }

    void wireShiftButton(juce::TextButton& btn)
    {
        btn.onClick = [this]() {
            auto now = juce::Time::getMillisecondCounter();
            bool doubleTap = (now - lastShiftTapMs) <= 350;
            lastShiftTapMs = now;

            if (doubleTap)
            {
                // Caps Lock
                capsActive = !capsActive;
                shiftActive = false;
            }
            else
            {
                if (capsActive)
                {
                    // выключаем Caps Lock
                    capsActive = false;
                    shiftActive = false;
                }
                else
                {
                    // включаем/выключаем одноразовый Shift
                    shiftActive = !shiftActive;
                }
            }

            indicateShiftCaps();
            updateLetterLabels();
            };
    }
};

