#include "DigitalKeyboard.h"

// ---------------- DigitalKeyboard ----------------
DigitalKeyboard::DigitalKeyboard(juce::Label& targetLabel,
    std::function<void(double)> onValueEntered)
    : externalLabel(targetLabel), onValueEnteredCallback(onValueEntered)
{
    addAndMakeVisible(editor);
    editor.setText(externalLabel.getText());
    editor.setJustification(juce::Justification::centred);

    editor.onTextChange = [this]
        {
            externalLabel.setText(editor.getText(), juce::dontSendNotification);
        };

    // ÷ифры 0Ц9
    for (int i = 0; i <= 9; ++i)
    {
        auto* b = new juce::TextButton(juce::String(i));
        b->setWantsKeyboardFocus(false);
        b->onClick = [this, i]
            {
                editor.insertTextAtCaret(juce::String(i));
                editor.grabKeyboardFocus();
            };
        digitButtons.add(b);
        addAndMakeVisible(b);
    }

    // Backspace (жовта)
    backButton.setButtonText(juce::String::charToString((wchar_t)0x232B));
    backButton.setColour(juce::TextButton::buttonColourId, juce::Colours::yellow);
    backButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    backButton.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    backButton.onClick = [this]
        {
            auto text = editor.getText();
            int pos = editor.getCaretPosition();
            if (pos > 0)
            {
                juce::String before = text.substring(0, pos - 1);
                juce::String after = text.substring(pos);
                editor.setText(before + after, juce::dontSendNotification);
                editor.setCaretPosition(pos - 1);
            }
            editor.grabKeyboardFocus();
        };
    addAndMakeVisible(backButton);

    // Clear (червона)
    clearButton.setButtonText("C");
    clearButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red);
    clearButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    clearButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    clearButton.onClick = [this]
        {
            editor.clear();
            editor.grabKeyboardFocus();
        };
    addAndMakeVisible(clearButton);

    // Point
    pointButton.setButtonText(".");
    pointButton.onClick = [this]
        {
            editor.insertTextAtCaret(".");
            editor.grabKeyboardFocus();
        };
    addAndMakeVisible(pointButton);

    // OK (зелена)
    okButton.setButtonText("OK");
    okButton.setColour(juce::TextButton::buttonColourId, juce::Colours::green);
    okButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    okButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    okButton.onClick = [this]
        {
            double val = editor.getText().getDoubleValue();
            if (onValueEnteredCallback)
                onValueEnteredCallback(val);

            if (auto* p = findParentComponentOfClass<juce::CallOutBox>())
                p->dismiss();
        };
    addAndMakeVisible(okButton);

    setSize(240, 320);
}

void DigitalKeyboard::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkslategrey);
    g.setColour(juce::Colours::lightgrey);
    g.drawRect(getLocalBounds(), 2);
}

void DigitalKeyboard::resized()
{
    auto area = getLocalBounds().reduced(8);
    const int cols = 4, rows = 5;
    const int w = area.getWidth() / cols;
    const int h = area.getHeight() / rows;

    editor.setBounds(area.removeFromTop(h));

    digitButtons[7]->setBounds(area.getX(), area.getY(), w, h);
    digitButtons[8]->setBounds(area.getX() + w, area.getY(), w, h);
    digitButtons[9]->setBounds(area.getX() + 2 * w, area.getY(), w, h);
    backButton.setBounds(area.getX() + 3 * w, area.getY(), w, h);
    area.removeFromTop(h);

    digitButtons[4]->setBounds(area.getX(), area.getY(), w, h);
    digitButtons[5]->setBounds(area.getX() + w, area.getY(), w, h);
    digitButtons[6]->setBounds(area.getX() + 2 * w, area.getY(), w, h);
    clearButton.setBounds(area.getX() + 3 * w, area.getY(), w, h);
    area.removeFromTop(h);

    digitButtons[1]->setBounds(area.getX(), area.getY(), w, h);
    digitButtons[2]->setBounds(area.getX() + w, area.getY(), w, h);
    digitButtons[3]->setBounds(area.getX() + 2 * w, area.getY(), w, h);
    area.removeFromTop(h);

    digitButtons[0]->setBounds(area.getX(), area.getY(), w, h);
    pointButton.setBounds(area.getX() + w, area.getY(), w, h);
    okButton.setBounds(area.getX() + 2 * w, area.getY(), 2 * w, h);
}

// --- LookAndFeel дл€ Slider ---
juce::Label* SliderTextBoxLNF::createSliderTextBox(juce::Slider& slider)
{
    auto* l = new ClickableLabel();
    l->setJustificationType(juce::Justification::centred);
    l->setEditable(false, false, false);

    l->onClick = [&slider, l]
        {
            auto keyboard = std::make_unique<DigitalKeyboard>(
                *l,
                [&slider](double val) { slider.setValue(val); }
            );

            juce::CallOutBox::launchAsynchronously(
                std::move(keyboard),
                l->getScreenBounds(),
                l->getTopLevelComponent()
            );
        };

    return l;
}
