
#pragma once
#include <juce_core/juce_core.h>
#include <linux/uinput.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>

struct IKeySender {
    virtual ~IKeySender() = default;
    virtual void sendChar(juce::juce_wchar ch) = 0;
    virtual void sendVk(uint16_t vk) = 0;
    virtual void backspace() = 0;
    virtual void enter() = 0;
    virtual void space() = 0;
};

class LinuxKeySender : public IKeySender {
public:
    LinuxKeySender()
    {
        fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
        if (fd < 0)
            throw std::runtime_error("Не удалось открыть /dev/uinput (нужны права root или доступ через udev)");

        ioctl(fd, UI_SET_EVBIT, EV_KEY);
        ioctl(fd, UI_SET_EVBIT, EV_SYN);

        // Разрешаем все клавиши
        for (int i = 0; i < 256; ++i)
            ioctl(fd, UI_SET_KEYBIT, i);

        struct uinput_setup usetup{};
        memset(&usetup, 0, sizeof(usetup));
        snprintf(usetup.name, UINPUT_MAX_NAME_SIZE, "NEXUS KEY Virtual Keyboard");
        usetup.id.bustype = BUS_USB;
        usetup.id.vendor  = 0x1234;
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
        // Простейший маппинг: только латиница и цифры
        int keycode = charToKeycode(ch);
        if (keycode > 0)
        {
            sendKey(keycode, true);
            sendKey(keycode, false);
        }
    }

    void sendVk(uint16_t vk) override
    {
        sendKey(vk, true);
        sendKey(vk, false);
    }

    void backspace() override { sendVk(KEY_BACKSPACE); }
    void enter() override     { sendVk(KEY_ENTER); }
    void space() override     { sendVk(KEY_SPACE); }

private:
    int fd = -1;

    void sendKey(int keycode, bool press)
    {
        struct input_event ev{};
        memset(&ev, 0, sizeof(ev));
        ev.type = EV_KEY;
        ev.code = keycode;
        ev.value = press ? 1 : 0;
        write(fd, &ev, sizeof(ev));

        struct input_event syn{};
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
        return -1; // пока без расширенной раскладки
    }
};
