#pragma once
#include <JuceHeader.h>

class OnScreenKeyboard : public juce::Component
{
public:
    std::function<void(const juce::String&)> onKeyPressed;

    OnScreenKeyboard()
    {
        setWantsKeyboardFocus(false);
        setMouseClickGrabsKeyboardFocus(false);
        buildKeyboard();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(30, 30, 30));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(4);
        const int rowH = area.getHeight() / (int)rows.size();

        for (auto& row : rows)
        {
            auto rowArea = area.removeFromTop(rowH);
            layoutRow(row, rowArea);
        }
    }

private:
    struct KeyDef
    {
        juce::String label;
        juce::String code;
        int widthUnits = 1;
        juce::TextButton* button = nullptr;
    };

    std::vector<std::vector<KeyDef>> rows;
    bool shiftOn = false;

    void buildKeyboard()
    {
        rows.push_back(makeRow("1234567890"));
        rows.push_back(makeRow("QWERTYUIOP"));
        rows.push_back(makeRow("ASDFGHJKL", {
            { juce::String(juce::CharPointer_UTF8(u8"⌫")), "{BACKSPACE}", 2 },
            { "DEL", "{DEL}", 2 }
            }));
        rows.push_back(makeRow("ZXCVBNM", {
            { "Enter", "{ENTER}", 2 },
            { "Shift", "{SHIFT}", 2 },
            { "BREAK", "{BREAK}", 2 }
            }));
        rows.push_back({
            { ",", ",", 1 }, { ".", ".", 1 }, { "-", "-", 1 }, { "_", "_", 1 }, { "Space", "{SPACE}", 6 }
            });

        for (auto& row : rows)
        {
            for (auto& key : row)
            {
                auto* btn = new juce::TextButton(key.label);
                btn->setWantsKeyboardFocus(false);
                btn->setMouseClickGrabsKeyboardFocus(false);

                btn->setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
                btn->setColour(juce::TextButton::textColourOffId, juce::Colours::white);

                btn->onClick = [this, &key]()
                    {
                        if (!onKeyPressed) return;

                        if (key.code == "{BACKSPACE}") { onKeyPressed("{BACKSPACE}"); return; }
                        if (key.code == "{DEL}") { onKeyPressed("{DEL}"); return; }
                        if (key.code == "{ENTER}") { onKeyPressed("{ENTER}"); return; }
                        if (key.code == "{SPACE}") { onKeyPressed(" "); return; }
                        if (key.code == "{SHIFT}") { shiftOn = !shiftOn; updateKeyCaps(); return; }
                        if (key.code == "{BREAK}") { onKeyPressed("{BREAK}"); return; }

                        auto out = key.label;
                        if (out.length() == 1)
                            out = shiftOn ? out.toUpperCase() : out.toLowerCase();

                        onKeyPressed(out);
                    };

                key.button = btn;
                addAndMakeVisible(btn);
            }
        }
    }

    std::vector<KeyDef> makeRow(const juce::String& letters,
        std::initializer_list<KeyDef> specials = {})
    {
        std::vector<KeyDef> row;
        for (auto ch : letters)
            row.push_back({ juce::String::charToString(ch), juce::String::charToString(ch), 1, nullptr });
        for (auto& sp : specials)
            row.push_back(sp);
        return row;
    }

    void layoutRow(std::vector<KeyDef>& row, juce::Rectangle<int> bounds)
    {
        int totalUnits = 0;
        for (auto& k : row) totalUnits += k.widthUnits;
        if (totalUnits <= 0) return;

        const int gap = 4;
        const int unitW = (bounds.getWidth() - gap * ((int)row.size() - 1)) / totalUnits;
        int x = bounds.getX();

        for (auto& k : row)
        {
            int w = unitW * k.widthUnits;
            if (k.button)
                k.button->setBounds(x, bounds.getY(), w, bounds.getHeight());
            x += w + gap;
        }
    }

    void updateKeyCaps()
    {
        for (auto& row : rows)
        {
            for (auto& k : row)
            {
                if (k.label.length() == 1 && k.code == k.label)
                    k.button->setButtonText(shiftOn ? k.label.toUpperCase() : k.label.toLowerCase());
            }
        }
    }
};
