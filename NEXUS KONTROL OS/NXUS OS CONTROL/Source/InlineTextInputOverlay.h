#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class InlineTextInputOverlay : public juce::Component
{
public:
    InlineTextInputOverlay(const juce::String& initialText,
        std::function<void(const juce::String&)> onDone)
        : onDoneCallback(std::move(onDone))
    {
        addAndMakeVisible(editor);
        editor.setText(initialText);
        editor.setSelectAllWhenFocused(true);

        // Óâåëè÷åííûé øðèôò äëÿ 10" ýêðàíà
        editor.setFont(juce::Font(54.0f));
        editor.applyFontToAllText(editor.getFont()); // ñðàçó ïðèìåíÿåì

        editor.onReturnKey = [this] {
            if (onDoneCallback) onDoneCallback(editor.getText());
            if (auto* parent = findParentComponentOfClass<juce::CallOutBox>())
                parent->dismiss();
            };

        editor.onEscapeKey = [this] {
            if (auto* parent = findParentComponentOfClass<juce::CallOutBox>())
                parent->dismiss();
            };

        // Óâåëè÷åííûé ðàçìåð îêíà
        setSize(400, 70);
    }

    void resized() override
    {
        editor.setBounds(getLocalBounds().reduced(8));
    }

private:
    juce::TextEditor editor;
    std::function<void(const juce::String&)> onDoneCallback;
};
