#include "GlobalKeyboard.h"

// ---  онструктор / ƒеструктор ---
GlobalKeyboard::GlobalKeyboard(juce::TextEditor& targetEditor,
    std::function<void()> onAnyKey,
    std::function<void()> onClose)
    : editor(targetEditor), onAnyKey(std::move(onAnyKey)), onClose(std::move(onClose))
{
    setLookAndFeel(&lnf);
    buildLayout();
    for (auto& row : rows) for (auto* b : row) addAndMakeVisible(b);
    indicateShift();
}

GlobalKeyboard::~GlobalKeyboard()
{
    setLookAndFeel(nullptr);
}

// --- ќтрисовка ---
void GlobalKeyboard::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey);
}

// --- –аскладка ---
void GlobalKeyboard::resized()
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

    // центрируем строки
    std::vector<int> rowWidths;
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
        int x = area.getX() + (area.getWidth() - rowWidths[rowIndex]) / 2;

        for (auto* b : row)
        {
            float wFactor = (float)b->getProperties().getWithDefault("weight", 1.0f);
            int w = (int)(cellWidth * wFactor + gap * (wFactor - 1));
            b->setBounds(x, y, w, 64);
            x += w + gap;
        }

        y += 70;
    }

    // стрелки справа
    if (arrowUp && arrowDown && arrowLeft && arrowRight)
    {
        auto rightBlock = full.removeFromRight(200);
        auto center = rightBlock.getCentre();
        int btnSize = 50;

        arrowUp->setBounds(center.x - btnSize / 2, center.y - btnSize * 3 / 2, btnSize, btnSize);
        arrowLeft->setBounds(center.x - btnSize * 3 / 2, center.y - btnSize / 2, btnSize, btnSize);
        arrowRight->setBounds(center.x + btnSize / 2, center.y - btnSize / 2, btnSize, btnSize);
        arrowDown->setBounds(center.x - btnSize / 2, center.y + btnSize / 2, btnSize, btnSize);
    }
}
// --- ¬спомогательные методы ---
void GlobalKeyboard::addKey(std::vector<juce::TextButton*>& row,
    const juce::String& label,
    std::function<void()> action,
    float weight)
{
    auto* b = new juce::TextButton(label);
    b->setWantsKeyboardFocus(false);
    b->setColour(juce::TextButton::buttonColourId, keyColour);
    b->setColour(juce::TextButton::textColourOffId, textColour);
    b->getProperties().set("weight", weight);

    b->onClick = [this, action]()
        {
            if (action) action();
            if (onAnyKey) onAnyKey();
        };

    allKeys.add(b);
    row.push_back(b);
}

void GlobalKeyboard::addLetterKey(std::vector<juce::TextButton*>& row, wchar_t c, float weight)
{
    auto* b = new juce::TextButton(juce::String::charToString(c));
    b->setWantsKeyboardFocus(false);
    b->setColour(juce::TextButton::buttonColourId, keyColour);
    b->setColour(juce::TextButton::textColourOffId, textColour);
    b->getProperties().set("weight", weight);
    b->getProperties().set(juce::Identifier("baseChar"), juce::String::charToString(c));

    b->onClick = [this, b]()
        {
            juce::String stored = b->getProperties()[juce::Identifier("baseChar")].toString();
            wchar_t base = stored.isNotEmpty() ? stored[0] : L'?';

            bool upper = shiftActive; // как в исходнике: Shift даЄт верхний регистр
            wchar_t outChar = upper
                ? juce::CharacterFunctions::toUpperCase(base)
                : juce::CharacterFunctions::toLowerCase(base);

            handleCharKey(outChar);

            if (shiftActive)
            {
                shiftActive = false;
                updateLetterLabels();
                indicateShift();
            }

            if (onAnyKey) onAnyKey();
        };

    allKeys.add(b);
    row.push_back(b);
    letterButtons.push_back(b);
}

void GlobalKeyboard::addCharKey(std::vector<juce::TextButton*>& row, wchar_t c, float weight)
{
    auto* b = new juce::TextButton(juce::String::charToString(c));
    b->setWantsKeyboardFocus(false);
    b->setColour(juce::TextButton::buttonColourId, keyColour);
    b->setColour(juce::TextButton::textColourOffId, textColour);
    b->getProperties().set("weight", weight);

    b->onClick = [this, c]()
        {
            handleCharKey(c);
            if (onAnyKey) onAnyKey();
        };

    allKeys.add(b);
    row.push_back(b);
}

void GlobalKeyboard::handleCharKey(wchar_t c)
{
    wchar_t outChar = c;

    // если Shift активен Ч замен€ем на верхний символ
    if (shiftActive)
    {
        auto it = symbolShiftMap().find(c);
        if (it != symbolShiftMap().end())
            outChar = it->second;
    }

    editor.insertTextAtCaret(juce::String::charToString(outChar));

    // Shift сбрасываетс€ после ввода
    if (shiftActive)
    {
        shiftActive = false;
        updateLetterLabels();
        indicateShift();
    }
}

void GlobalKeyboard::wireShiftButton(juce::TextButton& btn)
{
    btn.onClick = [this]()
        {
            shiftActive = !shiftActive; // временный Shift
            updateLetterLabels();
            indicateShift();
        };
}

void GlobalKeyboard::updateLetterLabels()
{
    for (auto* b : letterButtons)
    {
        juce::String stored = b->getProperties()[juce::Identifier("baseChar")].toString();
        if (stored.isEmpty()) continue;

        wchar_t base = stored[0];
        bool upper = shiftActive;
        wchar_t newChar = upper
            ? juce::CharacterFunctions::toUpperCase(base)
            : juce::CharacterFunctions::toLowerCase(base);

        b->setButtonText(juce::String::charToString(newChar));
    }
}

void GlobalKeyboard::indicateShift()
{
    if (!shiftLeft) return;
    auto colour = shiftActive ? juce::Colours::yellow : keyColour;
    shiftLeft->setColour(juce::TextButton::buttonColourId, colour);
    shiftLeft->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
}

const std::map<wchar_t, wchar_t>& GlobalKeyboard::symbolShiftMap()
{
    static const std::map<wchar_t, wchar_t> m = {
        { ';', ':' }, { '\'', '\"' }, { '[', '{' }, { ']', '}' },
        { '\\', '|' }, { ',', '<' }, { '.', '>' }, { '/', '?' },
        { '-', '_' }, { '=', '+' }
    };
    return m;
}
// --- ѕостроение раскладок ---
void GlobalKeyboard::buildLayout()
{
    switch (mode)
    {
    case Mode::Letters: buildLettersLayout(); break;
    case Mode::Digits:  buildDigitsLayout();  break;
    case Mode::Symbols: buildSymbolsLayout(); break;
    }
    createArrowKeys();
}

void GlobalKeyboard::buildLettersLayout()
{
    rows.clear();

    // Row 1: QWERTYUIOP
    {
        std::vector<juce::TextButton*> row;
        for (auto c : juce::String("qwertyuiop"))
            addLetterKey(row, c, 1.0f);
        rows.push_back(row);
    }

    // Row 2: ASDFGHJKL + Del
    {
        std::vector<juce::TextButton*> row;
        for (auto c : juce::String("asdfghjkl"))
            addLetterKey(row, c, 1.0f);

        addKey(row, "Del", [this] {
            auto text = editor.getText();
            int pos = editor.getCaretPosition();
            if (pos < text.length())
            {
                juce::String before = text.substring(0, pos);
                juce::String after = text.substring(pos + 1);
                editor.setText(before + after, juce::dontSendNotification);
                editor.setCaretPosition(pos);
            }
            }, 1.0f);

        rows.push_back(row);
    }

    // Row 3: Shift + ZXCVBNM + Backspace
    {
        std::vector<juce::TextButton*> row;

        // Shift
        shiftLeft = new juce::TextButton(juce::String::charToString((wchar_t)0x21E7));
        shiftLeft->getProperties().set("weight", 1.5f);
        shiftLeft->getProperties().set("fontSize", 50.0f);
        shiftLeft->setColour(juce::TextButton::buttonColourId, keyColour);
        shiftLeft->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        wireShiftButton(*shiftLeft);
        allKeys.add(shiftLeft);
        row.push_back(shiftLeft);

        for (auto c : juce::String("zxcvbnm"))
            addLetterKey(row, c, 1.0f);

        // Backspace ?
        auto* backBtn = new juce::TextButton(juce::String::charToString((wchar_t)0x232B));
        backBtn->getProperties().set("weight", 1.5f);
        backBtn->getProperties().set("fontSize", 40.0f);
        backBtn->setColour(juce::TextButton::buttonColourId, keyColour);
        backBtn->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        backBtn->onClick = [this] {
            auto text = editor.getText();
            int pos = editor.getCaretPosition();
            if (pos > 0)
            {
                juce::String before = text.substring(0, pos - 1);
                juce::String after = text.substring(pos);
                editor.setText(before + after, juce::dontSendNotification);
                editor.setCaretPosition(pos - 1);
            }
            };
        allKeys.add(backBtn);
        row.push_back(backBtn);

        rows.push_back(row);
    }

    // Row 4: ?123, ',', Space, '.', Enter
    {
        std::vector<juce::TextButton*> row;

        addKey(row, "?123", [this] { mode = Mode::Digits; rebuildLayout(); }, 1.5f);
        addCharKey(row, ',', 1.0f);

        addKey(row, "Space", [this] { editor.insertTextAtCaret(" "); }, 5.0f);

        addCharKey(row, '.', 1.0f);

        auto* enterBtn = new juce::TextButton(juce::String::charToString((wchar_t)0x21B5));
        enterBtn->getProperties().set("weight", 1.5f);
        enterBtn->getProperties().set("fontSize", 50.0f);
        enterBtn->setColour(juce::TextButton::buttonColourId, keyColour);
        enterBtn->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        enterBtn->onClick = [this] { if (onClose) onClose(); };
        allKeys.add(enterBtn);
        row.push_back(enterBtn);

        rows.push_back(row);
    }
}

void GlobalKeyboard::buildDigitsLayout()
{
    rows.clear();

    // Row 1: 1234567890 + Backspace
    {
        std::vector<juce::TextButton*> row;
        for (auto c : juce::String("1234567890"))
            addCharKey(row, c, 1.0f);

        auto* backBtn = new juce::TextButton(juce::String::charToString((wchar_t)0x232B));
        backBtn->getProperties().set("weight", 1.5f);
        backBtn->setColour(juce::TextButton::buttonColourId, keyColour);
        backBtn->setColour(juce::TextButton::textColourOffId, textColour);
        backBtn->onClick = [this] {
            auto text = editor.getText();
            int pos = editor.getCaretPosition();
            if (pos > 0)
            {
                juce::String before = text.substring(0, pos - 1);
                juce::String after = text.substring(pos);
                editor.setText(before + after, juce::dontSendNotification);
                editor.setCaretPosition(pos - 1);
            }
            };
        allKeys.add(backBtn);
        row.push_back(backBtn);

        rows.push_back(row);
    }

    // Row 2: -/:;()$&@"
    {
        std::vector<juce::TextButton*> row;
        for (auto c : juce::String("-/:;()$&@\""))
            addCharKey(row, c, 1.0f);
        rows.push_back(row);
    }

    // Row 3: !?.,' + Del
    {
        std::vector<juce::TextButton*> row;
        for (auto c : juce::String("!?.,'"))
            addCharKey(row, c, 1.0f);

        auto* delBtn = new juce::TextButton("Del");
        delBtn->getProperties().set("weight", 1.5f);
        delBtn->setColour(juce::TextButton::buttonColourId, keyColour);
        delBtn->setColour(juce::TextButton::textColourOffId, textColour);
        delBtn->onClick = [this] {
            auto text = editor.getText();
            int pos = editor.getCaretPosition();
            if (pos < text.length())
            {
                juce::String before = text.substring(0, pos);
                juce::String after = text.substring(pos + 1);
                editor.setText(before + after, juce::dontSendNotification);
                editor.setCaretPosition(pos);
            }
            };
        allKeys.add(delBtn);
        row.push_back(delBtn);

        rows.push_back(row);
    }

    // Row 4: ABC, Space, Enter
    {
        std::vector<juce::TextButton*> row;

        addKey(row, "ABC", [this] { mode = Mode::Letters; rebuildLayout(); }, 1.5f);
        addKey(row, "Space", [this] { editor.insertTextAtCaret(" "); }, 5.0f);

        auto* enterBtn = new juce::TextButton(juce::String::charToString((wchar_t)0x21B5));
        enterBtn->getProperties().set("weight", 1.5f);
        enterBtn->getProperties().set("fontSize", 50.0f);
        enterBtn->setColour(juce::TextButton::buttonColourId, keyColour);
        enterBtn->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        enterBtn->onClick = [this] { if (onClose) onClose(); };
        allKeys.add(enterBtn);
        row.push_back(enterBtn);

        rows.push_back(row);
    }
}

void GlobalKeyboard::buildSymbolsLayout()
{
    rows.clear();

    // Row 1: ~`|Х????ґ?
    {
        std::vector<juce::TextButton*> row;
        for (auto c : juce::String("~`|Х????ґ?"))
            addCharKey(row, c, 1.0f);
        rows.push_back(row);
    }

    // Row 2: ??И?∞©ЃЩ
    {
        std::vector<juce::TextButton*> row;
        for (auto c : juce::String("??И?∞©ЃЩ"))
            addCharKey(row, c, 1.0f);
        rows.push_back(row);
    }

    // Row 3: {}[]<>
    {
        std::vector<juce::TextButton*> row;
        for (auto c : juce::String("{}[]<>"))
            addCharKey(row, c, 1.0f);
        rows.push_back(row);
    }

    // Row 4: ABC, Space, Enter
    {
        std::vector<juce::TextButton*> row;

        addKey(row, "ABC", [this] { mode = Mode::Letters; rebuildLayout(); }, 1.5f);
        addKey(row, "Space", [this] { editor.insertTextAtCaret(" "); }, 5.0f);

        auto* enterBtn = new juce::TextButton(juce::String::charToString((wchar_t)0x21B5));
        enterBtn->getProperties().set("weight", 1.5f);
        enterBtn->getProperties().set("fontSize", 50.0f);
        enterBtn->setColour(juce::TextButton::buttonColourId, keyColour);
        enterBtn->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        enterBtn->onClick = [this] { if (onClose) onClose(); };
        allKeys.add(enterBtn);
        row.push_back(enterBtn);

        rows.push_back(row);
    }
}

void GlobalKeyboard::createArrowKeys()
{
    if (arrowLeft || arrowRight || arrowUp || arrowDown)
        return;

    arrowLeft = new juce::TextButton(juce::String::charToString((wchar_t)0x25C0)); // ?
    arrowUp = new juce::TextButton(juce::String::charToString((wchar_t)0x25B2)); // ?
    arrowDown = new juce::TextButton(juce::String::charToString((wchar_t)0x25BC)); // ?
    arrowRight = new juce::TextButton(juce::String::charToString((wchar_t)0x25B6)); // ?

    for (auto* b : { arrowLeft, arrowUp, arrowDown, arrowRight })
    {
        b->setWantsKeyboardFocus(false);
        b->setColour(juce::TextButton::buttonColourId, keyColour);
        b->setColour(juce::TextButton::textColourOffId, textColour);
        addAndMakeVisible(b);
        allKeys.add(b);
    }

    arrowLeft->onClick = [this] { editor.moveCaretLeft(false, false);  if (onAnyKey) onAnyKey(); };
    arrowRight->onClick = [this] { editor.moveCaretRight(false, false); if (onAnyKey) onAnyKey(); };
    arrowUp->onClick = [this] { editor.moveCaretUp(false);           if (onAnyKey) onAnyKey(); };
    arrowDown->onClick = [this] { editor.moveCaretDown(false);         if (onAnyKey) onAnyKey(); };

}
void GlobalKeyboard::rebuildLayout()
{
    // убрать старые кнопки
    for (auto& row : rows)
        for (auto* b : row)
            removeChildComponent(b);

    allKeys.clear(true);
    rows.clear();
    letterButtons.clear();

    // сброс спецклавиш
    arrowUp = arrowDown = arrowLeft = arrowRight = nullptr;
    shiftLeft = nullptr;

    // построить заново
    buildLayout();

    // добавить новые кнопки на экран
    for (auto& row : rows)
        for (auto* b : row)
            addAndMakeVisible(b);

    // пересчитать размеры
    resized();

    // обновить буквы (если есть)
    if (!letterButtons.empty())
        updateLetterLabels();
}
