#pragma once
#include <JuceHeader.h>

class VirtualKeyboard : public juce::Component
{
public:
    VirtualKeyboard(std::function<void(juce::String)> onKeyPressCallback)
        : onKeyPress(onKeyPressCallback)
    {
        addRow({ "`","1","2","3","4","5","6","7","8","9","0","-","=","Backspace" });
        addRow({ "Q","W","E","R","T","Y","U","I","O","P","[","]","\\" });
        addRow({ "Caps","A","S","D","F","G","H","J","K","L",";","'","Enter" });
        addRow({ "Shift","Z","X","C","V","B","N","M",",",".","/","Shift" });
        addRow({ "Space" });
    }

    std::function<void()> onEnterPressed; // ?? сигнал менеджеру спрятать клаву

    void resized() override
    {
        auto area = getLocalBounds();
        int rowHeight = area.getHeight() / rows.size();

        for (auto& row : rows)
        {
            auto rowArea = area.removeFromTop(rowHeight);
            int baseWidth = row.size() > 0 ? rowArea.getWidth() / row.size() : 0;

            for (auto* b : row)
            {
                int w = baseWidth;
                if (b->getButtonText() == "Space")
                    w = rowArea.getWidth();

                b->setBounds(rowArea.removeFromLeft(w));
            }
        }
    }

private:
    juce::OwnedArray<juce::TextButton> allButtons;
    juce::Array<juce::Array<juce::TextButton*>> rows;
    std::function<void(juce::String)> onKeyPress;

    bool capsOn = false;
    bool shiftOn = false;

    void addRow(const juce::StringArray& labels)
    {
        juce::Array<juce::TextButton*> row;
        for (auto& label : labels)
        {
            auto* b = new juce::TextButton(label);

            b->setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
            b->setColour(juce::TextButton::buttonOnColourId, juce::Colours::dimgrey);
            b->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            b->setColour(juce::TextButton::textColourOnId, juce::Colours::orange);

            b->setWantsKeyboardFocus(false);

            b->onClick = [this, label, b]() { handleKey(label, b); };

            addAndMakeVisible(b);
            allButtons.add(b);
            row.add(b);
        }
        rows.add(row);
    }

    void handleKey(const juce::String& label, juce::TextButton* button)
    {
        if (!onKeyPress) return;

        if (label == "Backspace") onKeyPress("\b");
        else if (label == "Enter")
        {
            // ?? Вариант «готово»: не вставляем перенос строки
            if (onEnterPressed)
                onEnterPressed();
        }
        else if (label == "Space") onKeyPress(" ");
        else if (label == "Caps")
        {
            capsOn = !capsOn;
            button->setColour(juce::TextButton::buttonColourId,
                capsOn ? juce::Colours::orange : juce::Colours::darkgrey);
        }
        else if (label == "Shift")
        {
            shiftOn = !shiftOn;
            updateShiftButtonsColour();
        }
        else
        {
            juce::String out = (capsOn ^ shiftOn) ? label.toUpperCase() : label.toLowerCase();
            onKeyPress(out);

            if (shiftOn)
            {
                shiftOn = false;
                updateShiftButtonsColour();
            }
        }
    }

    void updateShiftButtonsColour()
    {
        for (auto* b : allButtons)
        {
            if (b->getButtonText() == "Shift")
            {
                b->setColour(juce::TextButton::buttonColourId,
                    shiftOn ? juce::Colours::orange : juce::Colours::darkgrey);
            }
        }
    }
};
