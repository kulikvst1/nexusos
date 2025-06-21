#pragma once

#include <JuceHeader.h>
#include "AudioMidiSettingsDialog.h"

// Если компилируемся под Windows, подключаем заголовки для замены оконной процедуры (для блокировки перемещения).
#ifdef JUCE_WINDOWS
#include <windows.h>
namespace
{
    static WNDPROC originalWndProc = nullptr;
}

LRESULT CALLBACK myWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCLBUTTONDOWN || message == WM_NCLBUTTONDBLCLK)
    {
        if (wParam == HTCAPTION)
            return 0; // Блокируем перемещение окна при клике по заголовку.
    }
    return CallWindowProc(originalWndProc, hwnd, message, wParam, lParam);
}
#endif

class MainWindow : public juce::DocumentWindow,
    public juce::MenuBarModel
{
public:
    MainWindow(const juce::String& title)
        : DocumentWindow(title,
            juce::Desktop::getInstance().getDefaultLookAndFeel()
            .findColour(juce::ResizableWindow::backgroundColourId),
            0) // 0 означает отсутствие стандартных кнопок
    {
        // Инициализируем AudioDeviceManager (например, 2 входа и 2 выхода).
        auto err = deviceManager.initialiseWithDefaultDevices(2, 2);
        jassert(err.isEmpty());

        // Загружаем настройки в рамках проекта можно не делать (сохранение происходит при закрытии диалога).
        // Используем нативный заголовок и оставляем заголовок пустым.
        setUsingNativeTitleBar(true);
        setName("");

        // Устанавливаем основной контент окна.
        setContentOwned(new juce::Component(), true);

        // Устанавливаем меню. Главное меню имеет имя "NEXUS OS" и содержит два пункта:
        // «Audio/MIDI Settings…» и «Exit».
        setMenuBar(this, 25);
        setTitleBarButtonsRequired(0, false);

        setResizable(false, false);
        setFullScreen(true);
        setAlwaysOnTop(true);
        setVisible(true);
       

#ifdef JUCE_WINDOWS
        if (auto* peer = getPeer())
        {
            HWND hwnd = static_cast<HWND> (peer->getNativeHandle());
            // Заменяем оконную процедуру, чтобы заблокировать перемещение окна.
            originalWndProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)myWindowProc);
            // Здесь не изменяем цвет титлбара — оставляем его системным.
        }
#endif
    }

    ~MainWindow() override
    {
        setMenuBar(nullptr, 0);
#ifdef JUCE_WINDOWS
        if (auto* peer = getPeer())
        {
            HWND hwnd = static_cast<HWND> (peer->getNativeHandle());
            if (originalWndProc != nullptr)
            {
                SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)originalWndProc);
                originalWndProc = nullptr;
            }
        }
#endif
    }

    // Реализация MenuBarModel:
    // Единственное верхнее меню называется "NEXUS OS".
    juce::StringArray getMenuBarNames() override
    {
        juce::StringArray names;
        names.add("NEXUS OS");
        return names;
    }

    // Для меню "NEXUS OS" создаём всплывающее меню с нужными пунктами.
    juce::PopupMenu getMenuForIndex(int /*menuIndex*/, const juce::String& menuName) override
    {
        juce::PopupMenu menu;
        if (menuName == "NEXUS OS")
        {
            menu.addItem(2, "Audio/MIDI Settings...", true);
            menu.addSeparator();
            menu.addItem(1, "Exit", true);
        }
        return menu;
    }

    // Обработка выбора пункта меню.
    void menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/) override
    {
        if (menuItemID == 1)
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
        else if (menuItemID == 2)
        {
            showAudioMidiSettings();
        }
    }

    void closeButtonPressed() override { }

    void resized() override { DocumentWindow::resized(); }

private:
    // Функция для открытия модального окна настроек.
    // Фон диалогового окна устанавливается равным фону основного окна.
    void showAudioMidiSettings()
    {
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(new AudioMidiSettingsDialog(deviceManager));
        options.dialogTitle = "Audio/MIDI Settings";
        auto bgColour = this->getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
        options.dialogBackgroundColour = bgColour;
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;

        options.launchAsync();
    }

    juce::AudioDeviceManager deviceManager;
};
