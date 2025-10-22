#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>
#include <map>
#include "KeySender.h"

// ====== кастомный стиль для кнопок ======
class BigFontLookAndFeel : public juce::LookAndFeel_V4
{
public:
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override
    {
        // делаем шрифт крупнее, например 22pt
        return juce::Font(22.0f, juce::Font::bold);
    }
};

class VirtualKeyboard : public juce::Component
{
public:
    VirtualKeyboard(IKeySender& sender,
        std::function<void()> onAnyKey = {},
        std::function<void()> onClose = {})
        : keySender(sender), onAnyKey(std::move(onAnyKey)), onClose(std::move(onClose))
    {
        setWantsKeyboardFocus(false);
        setLookAndFeel(&bigFontLNF); // применяем стиль ко всем кнопкам
        buildLayout();
        for (auto& row : rows)
            for (auto* b : row)
                addAndMakeVisible(b);
    }

    ~VirtualKeyboard() override
    {
        setLookAndFeel(nullptr); // сбросить стиль при уничтожении
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

    juce::TextButton* digitButtons[10] = { nullptr };
    std::vector<juce::TextButton*> letterButtons;

    BigFontLookAndFeel bigFontLNF; // стиль с крупным шрифтом

    // ===== Логика =====
    void toggleShift()
    {
        shiftActive = !shiftActive;
        auto colour = shiftActive ? juce::Colours::yellow : juce::Colours::lightgrey;
        if (shiftLeft)  shiftLeft->setColour(juce::TextButton::buttonColourId, colour);
        if (shiftRight) shiftRight->setColour(juce::TextButton::buttonColourId, colour);

        updateDigitLabels();
        updateLetterLabels();
    }

    void updateDigitLabels()
    {
        static const char* normal[10] = { "1","2","3","4","5","6","7","8","9","0" };
        static const char* shifted[10] = { "!","@","#","$","%","^","&","*","(",")" };

        for (int i = 0; i < 10; ++i)
            if (digitButtons[i])
                digitButtons[i]->setButtonText(shiftActive ? shifted[i] : normal[i]);
    }

    void updateLetterLabels()
    {
        for (auto* b : letterButtons)
        {
            if (!b) continue;
            juce::String stored = b->getProperties()[juce::Identifier("baseChar")].toString();
            juce::juce_wchar base = stored.isNotEmpty() ? stored[0] : L'?';

            juce::juce_wchar newChar = (capsActive ^ shiftActive)
                ? juce::CharacterFunctions::toUpperCase(base)
                : juce::CharacterFunctions::toLowerCase(base);

            b->setButtonText(juce::String::charToString(newChar));
        }
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
        const int keyW = 100;
        const int wideW = 100;
        const int enterW = 100;
        const int shiftW = 100;
        const int spaceW = 310;

        // Row 1: цифры + Backspace
        {
            std::vector<juce::TextButton*> row;
            juce::String digits("1234567890");
            int idx = 0;
            for (auto c : digits)
            {
                auto* b = new juce::TextButton(juce::String::charToString(c));
                b->setColour(juce::TextButton::buttonColourId, juce::Colours::lightgrey);
                b->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
                b->getProperties().set("keyW", keyW);
                b->getProperties().set(juce::Identifier("baseDigit"), juce::String::charToString(c));
                b->onClick = [this, b] { handleDigitKey(b); if (onAnyKey) onAnyKey(); };
                allKeys.add(b);
                row.push_back(b);
                if (idx < 10) digitButtons[idx++] = b;
            }
            addKey(row, juce::String::charToString((juce::juce_wchar)0x232B), // ⌫
                [this] { keySender.backspace(); }, wideW);

            rows.push_back(row);
        }

        // Row 2: qwertyuiop + Del
        {
            std::vector<juce::TextButton*> row;
            for (auto c : juce::String("qwertyuiop"))
            {
                auto* b = new juce::TextButton(juce::String::charToString(c));
                b->setColour(juce::TextButton::buttonColourId, juce::Colours::lightgrey);
                b->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
                b->getProperties().set("keyW", keyW);
                b->getProperties().set(juce::Identifier("baseChar"), juce::String::charToString(c));
                b->onClick = [this, c] { handleCharKey(c); if (onAnyKey) onAnyKey(); };
                allKeys.add(b);
                row.push_back(b);
                letterButtons.push_back(b);
            }
            addKey(row, "Del", [this] { keySender.sendVk(VK_DELETE); }, keyW);
            rows.push_back(row);
        }
        // Row 3: Caps + asdfghjkl + Enter (⏎)
        {
            std::vector<juce::TextButton*> row;

            // Caps Lock
            capsButton = new juce::TextButton("Caps");
            capsButton->setColour(juce::TextButton::buttonColourId, juce::Colours::lightgrey);
            capsButton->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
            capsButton->onClick = [this] {
                capsActive = !capsActive;
                capsButton->setColour(juce::TextButton::buttonColourId,
                    capsActive ? juce::Colours::yellow : juce::Colours::lightgrey);
                updateLetterLabels();
                };
            capsButton->getProperties().set("keyW", wideW);
            addAndMakeVisible(capsButton);
            allKeys.add(capsButton);
            row.push_back(capsButton);

            // Буквы
            for (auto c : juce::String("asdfghjkl"))
            {
                auto* b = new juce::TextButton(juce::String::charToString(c));
                b->setColour(juce::TextButton::buttonColourId, juce::Colours::lightgrey);
                b->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
                b->getProperties().set("keyW", keyW);
                b->getProperties().set(juce::Identifier("baseChar"), juce::String::charToString(c));
                b->onClick = [this, c] { handleCharKey(c); if (onAnyKey) onAnyKey(); };
                allKeys.add(b);
                row.push_back(b);
                letterButtons.push_back(b);
            }

            // Enter (⏎) с увеличенным шрифтом
            auto* enterButton = new juce::TextButton(juce::String::charToString((juce::juce_wchar)0x21B5)); // ⏎
            enterButton->setColour(juce::TextButton::buttonColourId, juce::Colours::lightgrey);
            enterButton->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
            enterButton->getProperties().set("keyW", enterW);
            enterButton->onClick = [this] { keySender.enter(); if (onClose) onClose(); };

            // назначаем увеличенный шрифт только для Enter
            class BiggerFontLookAndFeel : public juce::LookAndFeel_V4 {
            public:
                juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override {
                    return juce::Font(40.0f, juce::Font::bold); // крупнее чем у остальных
                }
            };
            static BiggerFontLookAndFeel biggerFontLNF;
            enterButton->setLookAndFeel(&biggerFontLNF);

            addAndMakeVisible(enterButton);
            allKeys.add(enterButton);
            row.push_back(enterButton);

            rows.push_back(row);
        }

        // Row 4: ⇑ + zxcvbnm + ⇑
        {
            std::vector<juce::TextButton*> row;

            shiftLeft = new juce::TextButton(juce::String::charToString((juce::juce_wchar)0x2B06)); // ⬆
            shiftLeft->setColour(juce::TextButton::buttonColourId, juce::Colours::lightgrey);
            shiftLeft->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
            shiftLeft->onClick = [this] { toggleShift(); updateLetterLabels(); };
            shiftLeft->getProperties().set("keyW", shiftW);
            addAndMakeVisible(shiftLeft);
            allKeys.add(shiftLeft);
            row.push_back(shiftLeft);

            for (auto c : juce::String("zxcvbnm"))
            {
                auto* b = new juce::TextButton(juce::String::charToString(c));
                b->setColour(juce::TextButton::buttonColourId, juce::Colours::lightgrey);
                b->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
                b->getProperties().set("keyW", keyW);
                b->getProperties().set(juce::Identifier("baseChar"), juce::String::charToString(c));
                b->onClick = [this, c] { handleCharKey(c); if (onAnyKey) onAnyKey(); };
                allKeys.add(b);
                row.push_back(b);
                letterButtons.push_back(b);
            }

            shiftRight = new juce::TextButton(juce::String::charToString((juce::juce_wchar)0x2B06)); // ⬆
            shiftRight->setColour(juce::TextButton::buttonColourId, juce::Colours::lightgrey);
            shiftRight->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
            shiftRight->onClick = [this] { toggleShift(); updateLetterLabels(); };
            shiftRight->getProperties().set("keyW", shiftW);
            addAndMakeVisible(shiftRight);
            allKeys.add(shiftRight);
            row.push_back(shiftRight);

            rows.push_back(row);
        }

        // Row 5: стрелки + Space
        {
            std::vector<juce::TextButton*> row;

            addKey(row, juce::String::charToString((juce::juce_wchar)0x25C0), // ◀
                [this] { keySender.sendVk(VK_LEFT); }, keyW);

            addKey(row, juce::String::charToString((juce::juce_wchar)0x25B2), // ▲
                [this] { keySender.sendVk(VK_UP); }, keyW);

            addKey(row, "Space", [this] { keySender.sendSpace(); }, spaceW);

            addKey(row, juce::String::charToString((juce::juce_wchar)0x25BC), // ▼
                [this] { keySender.sendVk(VK_DOWN); }, keyW);

            addKey(row, juce::String::charToString((juce::juce_wchar)0x25B6), // ▶
                [this] { keySender.sendVk(VK_RIGHT); }, keyW);

            rows.push_back(row);
        }
    }


    // ===== Обработка =====
    void handleDigitKey(juce::TextButton* button)
    {
        juce::String stored = button->getProperties()[juce::Identifier("baseDigit")].toString();
        juce::juce_wchar base = stored.isNotEmpty() ? stored[0] : L'?';

        static const std::map<juce::juce_wchar, juce::juce_wchar> shiftMap = {
            { '1', '!' }, { '2', '@' }, { '3', '#' }, { '4', '$' },
            { '5', '%' }, { '6', '^' }, { '7', '&' }, { '8', '*' },
            { '9', '(' }, { '0', ')' }
        };

        if (shiftActive)
        {
            switch (base)
            {
            case '1': keySender.sendVkCombo(VK_SHIFT, '1'); break;
            case '8': keySender.sendVkCombo(VK_SHIFT, '8'); break;
            case '9': keySender.sendVkCombo(VK_SHIFT, '9'); break;
            case '0': keySender.sendVkCombo(VK_SHIFT, '0'); break;
            default:
            {
                auto it = shiftMap.find(base);
                if (it != shiftMap.end())
                    keySender.sendChar(it->second);
                else
                    keySender.sendChar(base);
                break;
            }
            }

            shiftActive = false;
            if (shiftLeft)  shiftLeft->setColour(juce::TextButton::buttonColourId, juce::Colours::lightgrey);
            if (shiftRight) shiftRight->setColour(juce::TextButton::buttonColourId, juce::Colours::lightgrey);
            updateDigitLabels();
            updateLetterLabels();
        }
        else
        {
            keySender.sendChar(base);
        }
    }

    void handleCharKey(juce::juce_wchar c)
    {
        juce::juce_wchar outChar = (capsActive ^ shiftActive)
            ? juce::CharacterFunctions::toUpperCase(c)
            : juce::CharacterFunctions::toLowerCase(c);

        keySender.sendChar(outChar);

        if (shiftActive) {
            shiftActive = false;
            if (shiftLeft)  shiftLeft->setColour(juce::TextButton::buttonColourId, juce::Colours::lightgrey);
            if (shiftRight) shiftRight->setColour(juce::TextButton::buttonColourId, juce::Colours::lightgrey);
            updateDigitLabels();
            updateLetterLabels();
        }
    }
};
