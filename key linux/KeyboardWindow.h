#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "VirtualKeyboard.h"
#include "KeySender.h"

#if JUCE_LINUX
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>   // для XWMHints
#include "UInputKeySender.h"
#endif

class KeyboardWindow : public juce::Component,
    private juce::Timer
{
public:
    KeyboardWindow()
    {
        setWantsKeyboardFocus(false);   // не перехватываем фокус
        setAlwaysOnTop(true);
        setBroughtToFrontOnMouseClick(false);

#if JUCE_WINDOWS
        keySender = std::make_unique<WinKeySender>();
#elif JUCE_LINUX
        keySender = std::make_unique<UInputKeySender>();
#endif

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

        // Викликаємо ensureOnDesktop асинхронно, щоб peer вже існував
        juce::MessageManager::callAsync([this] { ensureOnDesktop(); });

        startTimer(300);
    }

    void resized() override
    {
        if (keyboard) keyboard->setBounds(getLocalBounds());
        const int buttonW = 80, buttonH = 40;
        dockButton.setBounds(getWidth() - buttonW - 10, 10, buttonW, buttonH);
    }

    void paint(juce::Graphics&) override {}

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::returnKey)
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
#if JUCE_WINDOWS
        if (manual) suppressUntilCaretGone = true;
#endif
    }

private:
    std::unique_ptr<IKeySender> keySender;
    std::unique_ptr<VirtualKeyboard> keyboard;
    juce::TextButton dockButton{ "Dock" };
    bool dockedTop = false;
    int idleTicks = 0;

#if JUCE_WINDOWS
    bool suppressUntilCaretGone = false;
    RECT lastCaretRect{};
    HWND lastCaretHwnd = nullptr;
    bool hasLastCaret = false;
#endif

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
#if JUCE_WINDOWS
            addToDesktop(juce::ComponentPeer::windowIsTemporary
                | juce::ComponentPeer::windowIgnoresKeyPresses);

            if (auto* peer = getPeer())
            {
                HWND hwnd = (HWND)peer->getNativeHandle();
                LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
                exStyle |= WS_EX_NOACTIVATE | WS_EX_TOPMOST;
                SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
            }
#elif JUCE_LINUX
            if (!isOnDesktop())
            {
                // создаём peer обычным способом
                addToDesktop(juce::ComponentPeer::windowIsTemporary
                    | juce::ComponentPeer::windowIgnoresKeyPresses);

                setWantsKeyboardFocus(false);
                setOpaque(false);
                setVisible(true);

                if (auto* peer = getPeer())
                {
                    if (Display* dpy = XOpenDisplay(nullptr))
                    {
                        ::Window w = (Window)peer->getNativeHandle();

                        // включаем override_redirect, чтобы окно не крало фокус
                        XSetWindowAttributes attrs{};
                        attrs.override_redirect = True;
                        XChangeWindowAttributes(dpy, w, CWOverrideRedirect, &attrs);

                        // форсируем окно поверх
                        Atom wmState = XInternAtom(dpy, "_NET_WM_STATE", False);
                        Atom keepAbove = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);
                        Atom states[] = { keepAbove };
                        XChangeProperty(dpy, w, wmState, XA_ATOM, 32,
                            PropModeReplace,
                            reinterpret_cast<const unsigned char*>(states), 1);

                        XFlush(dpy);
                        XCloseDisplay(dpy);
                    }
                }
            }
#endif


    }
}

    void timerCallback() override
    {
#if JUCE_WINDOWS
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

            if (caretMoved)
            {
                suppressUntilCaretGone = false;
                showDockRespectingState();
            }
        }
        else
        {
            hasLastCaret = false;
            lastCaretHwnd = nullptr;
            suppressUntilCaretGone = false;
            if (isVisible()) hideKeyboard(false);
        }
#elif JUCE_LINUX
        Display* dpy = XOpenDisplay(nullptr);
        if (!dpy) return;

        Window focused;
        int revert;
        XGetInputFocus(dpy, &focused, &revert);

        if (focused != None && focused != PointerRoot)
        {
            if (auto* peer = getPeer())
            {
                ::Window selfWin = (Window)peer->getNativeHandle();
                if (focused != selfWin)
                    showDockRespectingState();
            }
        }
        else
        {
            if (isVisible())
                hideKeyboard(false);
        }

        XCloseDisplay(dpy);
#endif
    }

#if JUCE_WINDOWS
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
#endif

    void resetIdleTimer() { idleTicks = 0; }
};
