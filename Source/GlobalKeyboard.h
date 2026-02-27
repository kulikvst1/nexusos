#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>
#include <map>

// LookAndFeel для крупных кнопок (как в исходнике)
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

class GlobalKeyboard : public juce::Component
{
public:
    enum class Mode { Letters, Digits, Symbols };

    GlobalKeyboard(juce::TextEditor& targetEditor,
        std::function<void()> onAnyKey = {},
        std::function<void()> onClose = {});
    ~GlobalKeyboard() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // пересоздать раскладку при смене режима (?123 / ABC / #+=)
    void rebuildLayout();

private:
    // цель ввода
    juce::TextEditor& editor;
    std::function<void()> onAnyKey;
    std::function<void()> onClose;

    // UI коллекции
    juce::OwnedArray<juce::TextButton> allKeys;
    std::vector<std::vector<juce::TextButton*>> rows;
    std::vector<juce::TextButton*> letterButtons;

    // состояние
    bool shiftActive = false;      // как в исходнике: временный Shift (без Caps)
    Mode mode = Mode::Letters;

    // спецклавиши
    juce::TextButton* shiftLeft = nullptr;
    juce::TextButton* arrowUp = nullptr;
    juce::TextButton* arrowDown = nullptr;
    juce::TextButton* arrowLeft = nullptr;
    juce::TextButton* arrowRight = nullptr;

    // визуал и параметры
    BigFontLookAndFeel lnf;
    float scaleFactor = 0.85f;
    const juce::Colour keyColour = juce::Colours::lightgrey;
    const juce::Colour textColour = juce::Colours::black;

    // построение
    void buildLayout();
    void buildLettersLayout();
    void buildDigitsLayout();
    void buildSymbolsLayout();
    void createArrowKeys();

    // вспомогательные
    void addKey(std::vector<juce::TextButton*>& row,
        const juce::String& label,
        std::function<void()> action,
        float weight = 1.0f);

    void addLetterKey(std::vector<juce::TextButton*>& row, wchar_t c, float weight = 1.0f);
    void addCharKey(std::vector<juce::TextButton*>& row, wchar_t c, float weight = 1.0f);

    void handleCharKey(wchar_t c);
    void wireShiftButton(juce::TextButton& btn);
    void updateLetterLabels();
    void indicateShift();

    static const std::map<wchar_t, wchar_t>& symbolShiftMap();
};
