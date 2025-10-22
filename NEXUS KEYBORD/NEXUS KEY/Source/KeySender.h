#pragma once

#include <juce_core/juce_core.h>
#include <cstdint>

// Базовый интерфейс отправки клавиш
struct IKeySender {
    virtual ~IKeySender() = default;
    virtual void sendChar(juce::juce_wchar ch) = 0;
    virtual void sendVk(uint16_t vk) = 0;
    virtual void backspace() = 0;
    virtual void enter() = 0;
    virtual void sendSpace() = 0;

    // Новый метод для отправки комбинаций (например Shift+цифра)
    virtual void sendVkCombo(uint16_t modifierVk, uint16_t keyVk) = 0;
};

// ================= WINDOWS =================
#include <windows.h>

class WinKeySender : public IKeySender {
public:
    void sendChar(juce::juce_wchar ch) override {
        SHORT vk = VkKeyScanW(ch);
        if (vk == -1) {
            // fallback: Unicode ввод
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

    void sendVk(uint16_t vk) override {
        keyPress(vk);
    }

    void backspace() override { sendVk(VK_BACK); }
    void enter() override { sendVk(VK_RETURN); }
    void sendSpace() override { sendVk(VK_SPACE); }

    // Реализация нового метода
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

    void keyPress(uint16_t vk) {
        keyDown(vk);
        keyUp(vk);
    }
};
