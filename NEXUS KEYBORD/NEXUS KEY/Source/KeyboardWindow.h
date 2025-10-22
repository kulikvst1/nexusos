#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "VirtualKeyboard.h"
#include "KeySender.h"
#include <windows.h>

// ================= WINDOWS =================
class KeyboardWindow : public juce::Component,
    private juce::Timer
{
public:
    KeyboardWindow()
    {
        setWantsKeyboardFocus(true); // ловим клавиши
        setAlwaysOnTop(true);
        setBroughtToFrontOnMouseClick(false);

        keySender = std::make_unique<WinKeySender>();

        keyboard = std::make_unique<VirtualKeyboard>(
            *keySender,
            [this]() { resetIdleTimer(); },
            [this]() { hideKeyboard(true); }
        );
        addAndMakeVisible(*keyboard);

        addAndMakeVisible(dockButton);
        dockButton.setButtonText("UP DOCK");
        dockButton.onClick = [this]() { toggleDock(); };

        setSize(800, 300);
        forceDockBottom();
        setVisible(true);

        startTimer(300);
    }

    void resized() override
    {
        if (keyboard) keyboard->setBounds(getLocalBounds());
        const int buttonW = 80, buttonH = 40;
        dockButton.setBounds(getWidth() - buttonW - 10, 10, buttonW, buttonH);
    }

    void paint(juce::Graphics&) override {}

    // обработка клавиш
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::returnKey) // Enter
        {
            hideKeyboard(true);
            return true;
        }
        return false;
    }

    void showDockRespectingState()
    {
        if (dockedTop) forceDockTop();
        else           forceDockBottom();
    }

    void forceDockBottom()
    {
        auto display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->totalArea;
        const int height = (int)(display.getHeight() * 0.35);
        setBounds(display.getX(), display.getBottom() - height, display.getWidth(), height);
        ensureOnDesktop();
        setVisible(true);
        resetIdleTimer();
        dockButton.setButtonText("UP DOCK");
    }

    void forceDockTop()
    {
        auto display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->totalArea;
        const int height = (int)(display.getHeight() * 0.35);
        setBounds(display.getX(), display.getY(), display.getWidth(), height);
        ensureOnDesktop();
        setVisible(true);
        resetIdleTimer();
        dockButton.setButtonText("DOWN DOCK");
    }

    void hideKeyboard(bool manual)
    {
        idleTicks = 0;
        setVisible(false);
        if (isOnDesktop()) removeFromDesktop();
        if (manual) suppressUntilCaretGone = true;
    }

private:
    std::unique_ptr<IKeySender> keySender;
    std::unique_ptr<VirtualKeyboard> keyboard;
    juce::TextButton dockButton{ "Dock" };
    bool dockedTop = false;
    bool suppressUntilCaretGone = false;
    int idleTicks = 0;

    RECT lastCaretRect{};
    HWND lastCaretHwnd = nullptr;
    bool hasLastCaret = false;

    void toggleDock()
    {
        dockedTop = !dockedTop;
        if (dockedTop) forceDockTop();
        else           forceDockBottom();
    }

    void ensureOnDesktop()
    {
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
    }

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
            else caretMoved = true;

            lastCaretRect = caretRect;
            lastCaretHwnd = caretHwnd;
            hasLastCaret = true;

            // теперь показываем клавиатуру при любом движении или появлении каретки
            if (caretMoved)
            {
                suppressUntilCaretGone = false; // сбрасываем блокировку
                showDockRespectingState();
            }
        }
        else
        {
            hasLastCaret = false;
            lastCaretHwnd = nullptr;
            suppressUntilCaretGone = false;
            if (isVisible()) hideKeyboard(false); // автозакрытие при исчезновении каретки
        }
    }

    void resetIdleTimer() { idleTicks = 0; }
};
