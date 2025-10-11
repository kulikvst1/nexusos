#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <windows.h>
#include "VirtualKeyboard.h"
#include "KeySender.h"

class KeyboardWindow : public juce::Component,
    private juce::Timer
{
public:
    KeyboardWindow()
    {
        setWantsKeyboardFocus(false);
        setAlwaysOnTop(true);
        setBroughtToFrontOnMouseClick(false);

        keySender = std::make_unique<WinKeySender>();
        keyboard = std::make_unique<VirtualKeyboard>(
            *keySender,
            [this]() { resetIdleTimer(); },   // любое нажатие → сброс idle
            [this]() { hideKeyboard(true); }  // Enter или ✖ → скрыть (ручное)
        );
        addAndMakeVisible(*keyboard);

        startTimer(300); // проверка каждые 300 мс
    }

    void resized() override
    {
        if (keyboard)
            keyboard->setBounds(getLocalBounds());
    }

    void paint(juce::Graphics&) override {}

    void showDockedBottom()
    {
        if (isVisible() || suppressUntilCaretGone) return;

        auto display = juce::Desktop::getInstance().getDisplays().getMainDisplay().totalArea;
        const int height = (int)(display.getHeight() * 0.35);
        setBounds(display.getX(), display.getBottom() - height, display.getWidth(), height);

        if (!isOnDesktop())
        {
            addToDesktop(juce::ComponentPeer::windowIsTemporary
                | juce::ComponentPeer::windowIgnoresKeyPresses);
            if (auto* peer = getPeer())
            {
                HWND hwnd = (HWND)peer->getNativeHandle();
                LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
                exStyle |= WS_EX_NOACTIVATE | WS_EX_TOPMOST;
                SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
            }
        }

        setVisible(true);
        resetIdleTimer();
    }

    void hideKeyboard(bool manual)
    {
        idleTicks = 0;
        setVisible(false);
        if (isOnDesktop())
            removeFromDesktop();

        if (manual)
            suppressUntilCaretGone = true; // блокируем до исчезновения курсора
    }

    void setIdleHideEnabled(bool enabled) { disableIdleHide = !enabled; }

private:
    std::unique_ptr<IKeySender> keySender;
    std::unique_ptr<VirtualKeyboard> keyboard;

    int idleTicks = 0;
    bool suppressUntilCaretGone = false;
    bool disableIdleHide = false;

    // отслеживание каретки
    RECT lastCaretRect{};
    HWND lastCaretHwnd = nullptr;
    bool hasLastCaret = false;

    bool getCaretInfo(bool& visible, RECT& rect, HWND& caretHwnd)
    {
        GUITHREADINFO gi{};
        gi.cbSize = sizeof(GUITHREADINFO);
        HWND fg = GetForegroundWindow();
        DWORD tid = fg ? GetWindowThreadProcessId(fg, nullptr) : 0;
        if (tid != 0 && GetGUIThreadInfo(tid, &gi) && gi.hwndCaret != nullptr)
        {
            visible = true;
            rect = gi.rcCaret;
            caretHwnd = gi.hwndCaret;
            return true;
        }
        visible = false;
        caretHwnd = nullptr;
        return false;
    }

    void timerCallback() override
    {
        bool caretVisible = false;
        RECT caretRect{};
        HWND caretHwnd = nullptr;

        getCaretInfo(caretVisible, caretRect, caretHwnd);

        if (caretVisible)
        {
            bool caretMoved = false;
            if (hasLastCaret)
            {
                caretMoved = (caretRect.left != lastCaretRect.left) ||
                    (caretRect.top != lastCaretRect.top) ||
                    (caretRect.right != lastCaretRect.right) ||
                    (caretRect.bottom != lastCaretRect.bottom) ||
                    (caretHwnd != lastCaretHwnd);
            }
            else
            {
                caretMoved = true; // caret появился впервые
            }

            lastCaretRect = caretRect;
            lastCaretHwnd = caretHwnd;
            hasLastCaret = true;

            if (!isVisible() && !suppressUntilCaretGone && caretMoved)
                showDockedBottom();

            // 🔥 idle‑таймер убран — клавиатура больше не скрывается сама
        }
        else
        {
            hasLastCaret = false;
            lastCaretHwnd = nullptr;
            suppressUntilCaretGone = false;

            if (isVisible())
                hideKeyboard(false);
        }

    }

    void resetIdleTimer()
    {
        idleTicks = 0;
    }
};
