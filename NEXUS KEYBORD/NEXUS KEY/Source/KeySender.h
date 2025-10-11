#pragma once
#include <juce_core/juce_core.h>
#include <cstdint>

struct IKeySender {
    virtual ~IKeySender() = default;
    virtual void sendChar(juce::juce_wchar ch) = 0;
    virtual void sendVk(uint16_t vk) = 0;
    virtual void backspace() = 0;
    virtual void enter() = 0;
    virtual void sendSpace() = 0; // <-- исправлено
};

#ifdef _WIN32
// ================= WINDOWS =================
#include <windows.h>

class WinKeySender : public IKeySender {
public:
    void sendChar(juce::juce_wchar ch) override {
        SHORT vk = VkKeyScanW(ch);
        if (vk == -1) {
            // fallback: Unicode
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
        // normal VK path
        INPUT ip{};
        ip.type = INPUT_KEYBOARD;
        ip.ki.wVk = LOBYTE(vk);
        ip.ki.wScan = MapVirtualKeyW(ip.ki.wVk, MAPVK_VK_TO_VSC);
        ip.ki.dwFlags = 0;
        SendInput(1, &ip, sizeof(INPUT));
        ip.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &ip, sizeof(INPUT));
    }

    void sendVk(uint16_t vk) override {
        INPUT ip{};
        ip.type = INPUT_KEYBOARD;
        ip.ki.wVk = static_cast<WORD>(vk);
        ip.ki.dwFlags = 0;
        SendInput(1, &ip, sizeof(INPUT));
        ip.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &ip, sizeof(INPUT));
    }

    void backspace() override { sendVk(VK_BACK); }
    void enter() override { sendVk(VK_RETURN); }
    void sendSpace() override { sendVk(VK_SPACE); }
};

#else
// ================= LINUX =================
#include <linux/uinput.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>

class LinuxKeySender : public IKeySender {
public:
    LinuxKeySender()
    {
        fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
        if (fd < 0)
            throw std::runtime_error("Не удалось открыть /dev/uinput (нужны права root или доступ через udev)");

        ioctl(fd, UI_SET_EVBIT, EV_KEY);
        ioctl(fd, UI_SET_EVBIT, EV_SYN);

        for (int i = 0; i < 256; ++i)
            ioctl(fd, UI_SET_KEYBIT, i);

        struct uinput_setup usetup {};
        memset(&usetup, 0, sizeof(usetup));
        snprintf(usetup.name, UINPUT_MAX_NAME_SIZE, "NEXUS KEY Virtual Keyboard");
        usetup.id.bustype = BUS_USB;
        usetup.id.vendor = 0x1234;
        usetup.id.product = 0x5678;
        usetup.id.version = 1;

        ioctl(fd, UI_DEV_SETUP, &usetup);
        ioctl(fd, UI_DEV_CREATE);
    }

    ~LinuxKeySender()
    {
        if (fd >= 0) {
            ioctl(fd, UI_DEV_DESTROY);
            close(fd);
        }
    }

    void sendChar(juce::juce_wchar ch) override
    {
        int keycode = charToKeycode(ch);
        if (keycode > 0) {
            sendKey(keycode, true);
            sendKey(keycode, false);
        }
    }

    void sendVk(uint16_t vk) override {
        sendKey(vk, true);
        sendKey(vk, false);
    }

    void backspace() override { sendVk(KEY_BACKSPACE); }
    void enter() override { sendVk(KEY_ENTER); }
    void sendSpace() override { sendVk(KEY_SPACE); }

private:
    int fd = -1;

    void sendKey(int keycode, bool press)
    {
        struct input_event ev {};
        memset(&ev, 0, sizeof(ev));
        ev.type = EV_KEY;
        ev.code = keycode;
        ev.value = press ? 1 : 0;
        write(fd, &ev, sizeof(ev));

        struct input_event syn {};
        memset(&syn, 0, sizeof(syn));
        syn.type = EV_SYN;
        syn.code = SYN_REPORT;
        syn.value = 0;
        write(fd, &syn, sizeof(syn));
    }

    int charToKeycode(juce::juce_wchar ch)
    {
        if (ch >= 'a' && ch <= 'z') return KEY_A + (ch - 'a');
        if (ch >= 'A' && ch <= 'Z') return KEY_A + (ch - 'A');
        if (ch >= '0' && ch <= '9') return KEY_0 + (ch - '0');
        if (ch == ' ') return KEY_SPACE;
        if (ch == '\n') return KEY_ENTER;
        if (ch == '\b') return KEY_BACKSPACE;
        return -1;
    }
};
#endif
