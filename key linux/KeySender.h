#pragma once

#include <juce_core/juce_core.h>
#include <cstdint>

// Базовый интерфейс
struct IKeySender {
    virtual ~IKeySender() = default;
    virtual void sendChar(juce::juce_wchar ch) = 0;
    virtual void sendVk(uint16_t vk) = 0;
    virtual void backspace() = 0;
    virtual void enter() = 0;
    virtual void sendSpace() = 0;
    virtual void sendVkCombo(uint16_t modifierVk, uint16_t keyVk) = 0;
};

// ======================================================
// WINDOWS
// ======================================================
#if JUCE_WINDOWS

#include <windows.h>

class WinKeySender : public IKeySender {
public:
    void sendChar(juce::juce_wchar ch) override {
        SHORT vk = VkKeyScanW(ch);
        if (vk == -1) {
            INPUT ip{};
            ip.type = INPUT_KEYBOARD;
            ip.ki.wVk = 0;
            ip.ki.wScan = static_cast<WORD>(ch);
            ip.ki.dwFlags = KEYEVENTF_UNICODE;
            SendInput(1, &ip, sizeof(INPUT));
            ip.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
            SendInput(1, &ip, sizeof(INPUT));
            return;
        }
        BYTE shiftState = HIBYTE(vk);
        WORD vkCode = LOBYTE(vk);
        if (shiftState & 1) keyDown(VK_SHIFT);
        keyPress(vkCode);
        if (shiftState & 1) keyUp(VK_SHIFT);
    }

    void sendVk(uint16_t vk) override { keyPress(vk); }
    void backspace() override { sendVk(VK_BACK); }
    void enter() override { sendVk(VK_RETURN); }
    void sendSpace() override { sendVk(VK_SPACE); }

    void sendVkCombo(uint16_t modifierVk, uint16_t keyVk) override {
        keyDown(modifierVk);
        keyPress(keyVk);
        keyUp(modifierVk);
    }

private:
    void keyDown(uint16_t vk) {
        INPUT ip{};
        ip.type = INPUT_KEYBOARD;
        ip.ki.wVk = vk;
        ip.ki.dwFlags = 0;
        SendInput(1, &ip, sizeof(INPUT));
    }
    void keyUp(uint16_t vk) {
        INPUT ip{};
        ip.type = INPUT_KEYBOARD;
        ip.ki.wVk = vk;
        ip.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &ip, sizeof(INPUT));
    }
    void keyPress(uint16_t vk) { keyDown(vk); keyUp(vk); }
};

// ======================================================
// LINUX (X11)
// ======================================================
#elif JUCE_LINUX

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

// убираем конфликт макроса KeyPress
#ifdef KeyPress
#undef KeyPress
#endif

// Определяем Windows VK_* коды для Linux
#ifndef VK_LEFT
#define VK_LEFT   0x25
#define VK_UP     0x26
#define VK_RIGHT  0x27
#define VK_DOWN   0x28
#define VK_DELETE 0x2E
#define VK_RETURN 0x0D
#define VK_SPACE  0x20
#define VK_BACK   0x08
#endif

class X11KeySender : public IKeySender {
public:
    X11KeySender() { display = XOpenDisplay(nullptr); }
    ~X11KeySender() override { if (display) XCloseDisplay(display); }

    void sendChar(juce::juce_wchar ch) override {
        if (!display) return;
        juce::String s = juce::String::charToString(ch);
        KeySym ks = XStringToKeysym(s.toRawUTF8());
        if (ks == NoSymbol) {
            if (ch >= 32 && ch <= 126) ks = static_cast<KeySym>(ch);
            else return;
        }
        sendKeysym(ks);
    }

    void sendVk(uint16_t vk) override {
        if (!display) return;
        KeySym ks = translateWindowsVkToKeysym(vk);
        if (ks == NoSymbol) return;
        sendKeysym(ks);
    }

    void backspace() override { sendVk(VK_BACK); }
    void enter() override { sendVk(VK_RETURN); }
    void sendSpace() override { sendVk(VK_SPACE); }

    void sendVkCombo(uint16_t modifierVk, uint16_t keyVk) override {
        if (!display) return;
        KeySym modKs = translateWindowsVkToKeysym(modifierVk);
        KeySym keyKs = translateWindowsVkToKeysym(keyVk);
        if (modKs == NoSymbol || keyKs == NoSymbol) return;
        KeyCode mod = XKeysymToKeycode(display, modKs);
        KeyCode kc = XKeysymToKeycode(display, keyKs);
        if (!mod || !kc) return;
        XTestFakeKeyEvent(display, mod, True, 0);
        XTestFakeKeyEvent(display, kc, True, 0);
        XTestFakeKeyEvent(display, kc, False, 0);
        XTestFakeKeyEvent(display, mod, False, 0);
        XFlush(display);
    }

private:
    Display* display = nullptr;

    void sendKeysym(KeySym ks) {
        KeyCode kc = XKeysymToKeycode(display, ks);
        if (kc == 0) return;
        XTestFakeKeyEvent(display, kc, True, 0);
        XTestFakeKeyEvent(display, kc, False, 0);
        XFlush(display);
    }

    KeySym translateWindowsVkToKeysym(uint16_t vk) {
        switch (vk) {
        case VK_LEFT:   return XK_Left;
        case VK_UP:     return XK_Up;
        case VK_RIGHT:  return XK_Right;
        case VK_DOWN:   return XK_Down;
        case VK_DELETE: return XK_Delete;
        case VK_RETURN: return XK_Return;
        case VK_SPACE:  return XK_space;
        case VK_BACK:   return XK_BackSpace;
        default:        return NoSymbol;
        }
    }
};

using PlatformKeySender = X11KeySender;

#else
#error "Unsupported platform"
#endif
