#pragma once
#if JUCE_LINUX

#include <juce_core/juce_core.h>
#include <linux/uinput.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <map>
#include <stdexcept>

#include "KeySender.h"

class UInputKeySender : public IKeySender
{
public:
    UInputKeySender()
    {
        fd = ::open("/dev/uinput", O_WRONLY | O_NONBLOCK);
        if (fd < 0)
            throw std::runtime_error("Cannot open /dev/uinput. Check permissions.");

        ioctl(fd, UI_SET_EVBIT, EV_KEY);
        ioctl(fd, UI_SET_EVBIT, EV_SYN);

        for (int code = 0; code < 256; ++code)
            ioctl(fd, UI_SET_KEYBIT, code);

        struct uinput_user_dev uidev;
        std::memset(&uidev, 0, sizeof(uidev));
        std::snprintf(uidev.name, sizeof(uidev.name), "JUCE Virtual Keyboard");
        uidev.id.bustype = BUS_USB;
        uidev.id.vendor = 0x1234;
        uidev.id.product = 0x5678;
        uidev.id.version = 1;

        ssize_t res = ::write(fd, &uidev, sizeof(uidev));
        if (res < 0)
            throw std::runtime_error("Failed to write uinput_user_dev");

        if (ioctl(fd, UI_DEV_CREATE) < 0)
            throw std::runtime_error("UI_DEV_CREATE failed");

        initKeyMap();
    }

    ~UInputKeySender() override
    {
        if (fd >= 0)
        {
            ioctl(fd, UI_DEV_DESTROY);
            ::close(fd);
            fd = -1;
        }
    }

    void sendChar(juce_wchar ch) override
    {
        auto it = keyMap.find(ch);
        if (it == keyMap.end())
            return;
        sendKey(it->second);
    }

    void enter() override
    {
        sendKey(KEY_ENTER);
    }

    void backspace() override
    {
        sendKey(KEY_BACKSPACE);
    }

    void sendSpace() override
    {
        sendKey(KEY_SPACE);
    }

    void sendVk(uint16_t vk) override
    {
        sendKey(vk);
    }

    void sendVkCombo(uint16_t modifierVk, uint16_t keyVk) override
    {
        emit(EV_KEY, modifierVk, 1);
        emit(EV_SYN, SYN_REPORT, 0);

        emit(EV_KEY, keyVk, 1);
        emit(EV_SYN, SYN_REPORT, 0);

        emit(EV_KEY, keyVk, 0);
        emit(EV_SYN, SYN_REPORT, 0);

        emit(EV_KEY, modifierVk, 0);
        emit(EV_SYN, SYN_REPORT, 0);
    }

private:
    int fd = -1;
    std::map<juce_wchar, int> keyMap;

    void initKeyMap()
    {
        keyMap['a'] = KEY_A; keyMap['A'] = KEY_A;
        keyMap['b'] = KEY_B; keyMap['B'] = KEY_B;
        keyMap['c'] = KEY_C; keyMap['C'] = KEY_C;
        keyMap['d'] = KEY_D; keyMap['D'] = KEY_D;
        keyMap['e'] = KEY_E; keyMap['E'] = KEY_E;
        keyMap['f'] = KEY_F; keyMap['F'] = KEY_F;
        keyMap['g'] = KEY_G; keyMap['G'] = KEY_G;
        keyMap['h'] = KEY_H; keyMap['H'] = KEY_H;
        keyMap['i'] = KEY_I; keyMap['I'] = KEY_I;
        keyMap['j'] = KEY_J; keyMap['J'] = KEY_J;
        keyMap['k'] = KEY_K; keyMap['K'] = KEY_K;
        keyMap['l'] = KEY_L; keyMap['L'] = KEY_L;
        keyMap['m'] = KEY_M; keyMap['M'] = KEY_M;
        keyMap['n'] = KEY_N; keyMap['N'] = KEY_N;
        keyMap['o'] = KEY_O; keyMap['O'] = KEY_O;
        keyMap['p'] = KEY_P; keyMap['P'] = KEY_P;
        keyMap['q'] = KEY_Q; keyMap['Q'] = KEY_Q;
        keyMap['r'] = KEY_R; keyMap['R'] = KEY_R;
        keyMap['s'] = KEY_S; keyMap['S'] = KEY_S;
        keyMap['t'] = KEY_T; keyMap['T'] = KEY_T;
        keyMap['u'] = KEY_U; keyMap['U'] = KEY_U;
        keyMap['v'] = KEY_V; keyMap['V'] = KEY_V;
        keyMap['w'] = KEY_W; keyMap['W'] = KEY_W;
        keyMap['x'] = KEY_X; keyMap['X'] = KEY_X;
        keyMap['y'] = KEY_Y; keyMap['Y'] = KEY_Y;
        keyMap['z'] = KEY_Z; keyMap['Z'] = KEY_Z;

        keyMap['0'] = KEY_0; keyMap['1'] = KEY_1; keyMap['2'] = KEY_2;
        keyMap['3'] = KEY_3; keyMap['4'] = KEY_4; keyMap['5'] = KEY_5;
        keyMap['6'] = KEY_6; keyMap['7'] = KEY_7; keyMap['8'] = KEY_8;
        keyMap['9'] = KEY_9;

        keyMap[' '] = KEY_SPACE;
        keyMap['\n'] = KEY_ENTER;
    }

    void emit(int type, int code, int value)
    {
        struct input_event ev;
        std::memset(&ev, 0, sizeof(ev));
        ev.type = type;
        ev.code = code;
        ev.value = value;
        ssize_t res = ::write(fd, &ev, sizeof(ev));
        (void)res;
    }

    void sendKey(int keyCode)
    {
        emit(EV_KEY, keyCode, 1);
        emit(EV_SYN, SYN_REPORT, 0);
        emit(EV_KEY, keyCode, 0);
        emit(EV_SYN, SYN_REPORT, 0);
    }
};

#endif // JUCE_LINUX
