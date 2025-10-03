#pragma once
#include <JuceHeader.h>
#include "vst_host.h"
#include "Rig_control.h"
#include "bank_editor.h"
#include "cpu_load.h"
#include "LooperComponent.h"
#include "TunerComponent.h"
#include "OutControlComponent.h"
#include "InputControlComponent.h"
#include "MidiStartupShutdown.h"


// --- Панель с уведомлением об изменении видимости ---
class NotifyingSidePanel : public juce::SidePanel
{
public:
    using juce::SidePanel::SidePanel; // наследуем конструкторы

    std::function<void(bool)> onPanelVisibilityChanged; // true=open, false=closed

    void visibilityChanged() override
    {
        juce::SidePanel::visibilityChanged();

        if (onPanelVisibilityChanged)
            onPanelVisibilityChanged(isPanelShowing());
    }
};


using namespace juce;
class CallbackTabbedComponent : public TabbedComponent
{
public:
    CallbackTabbedComponent(TabbedButtonBar::Orientation orient)
        : TabbedComponent(orient) {}

    std::function<void(int)> onCurrentTabChanged;

    int tabBarIndent = 0; // ← добавляем поле

protected:
    void currentTabChanged(int newIndex, const String& newName) override
    {
        TabbedComponent::currentTabChanged(newIndex, newName);
        if (onCurrentTabChanged)
            onCurrentTabChanged(newIndex);
    }

    void resized() override
    {
        TabbedComponent::resized(); // сначала стандартный layout

        if (tabBarIndent > 0)
        {
            auto& bar = getTabbedButtonBar();
            bar.setBounds(bar.getBounds().withTrimmedLeft(tabBarIndent));
        }
    }
};

//==============================================================================
// Главное окно
//==============================================================================
class MainContentComponent : public Component
{
public:
    MainContentComponent()
        : tabs(TabbedButtonBar::TabsAtTop)
    {
        // 1) Инициализация аудио
        deviceManager.initialise(2, 2, nullptr, true);

        // 2) VST Host
        vstHostComponent = std::make_unique<VSTHostComponent>(deviceManager);

        // 3) Тюнеры
        tunerComponent = std::make_unique<TunerComponent>();
        rigTuner = std::make_unique<TunerComponent>();
        vstHostComponent->addTuner(tunerComponent.get());
        vstHostComponent->addTuner(rigTuner.get());

        // 4) OutControl / InputControl
        outControlComponent = std::make_unique<OutControlComponent>();
        vstHostComponent->setOutControlComponent(outControlComponent.get());

        inputControlComponent = std::make_unique<InputControlComponent>();
        inputControlComponent->prepare(currentSampleRate, currentBlockSize);
        vstHostComponent->setInputControlComponent(inputControlComponent.get());

        // 5) Остальные модули
        bankEditor = std::make_unique<BankEditor>(vstHostComponent.get());
        rigControl = std::make_unique<Rig_control>(deviceManager);
        inputControlComponent->setRigControl(rigControl.get());
        rigControl->setInputControlComponent(inputControlComponent.get());
        // 🔹 Привязка логики переключения вкладки тюнера
        rigControl->onTunerVisibilityChanged = [this](bool show)
            {
                if (show)
                {
                    // Если вкладка тюнера уже есть и не активна — активируем
                    int tunerIndex = getTabIndexByName("TUNER");
                    if (tunerIndex >= 0 && tabs.getCurrentTabIndex() != tunerIndex)
                        tabs.setCurrentTabIndex(tunerIndex, true);
                }
                else
                {
                    // Не удаляем вкладку — просто переключаемся на RIG CONTROL
                    ensureRigControlActive();
                }
            };

        // 5b) MIDI init
        midiInit = std::make_unique<MidiStartupShutdown>(*rigControl);
        midiInit->sendStartupCommands();
        looperEngine = std::make_unique<LooperEngine>();
        looperComponent = std::make_unique<LooperComponent>(*looperEngine);
        looperEngine->prepare(44100.0, 512);
        vstHostComponent->setLooperEngine(looperEngine.get());

        // 5a) Синхронизация A4
        tunerComponent->onReferenceA4Changed = [rig = rigTuner.get()](double a4)
            { rig->setReferenceA4(a4, false); };
        rigTuner->onReferenceA4Changed = [tab = tunerComponent.get()](double a4)
            { tab->setReferenceA4(a4, false); };

        // 6) Настройка Rig_control
        rigControl->setBankEditor(bankEditor.get());
        rigControl->setVstHostComponent(vstHostComponent.get());
        vstHostComponent->onBpmChanged = [this](double bpm)
            { rigControl->sendBpmToMidi(bpm); };
        vstHostComponent->updateBPM(120.0);
        rigControl->setLooperEngine(*looperEngine);
        looperEngine->onPlayerStateChanged = [this](bool playing)
            { rigControl->sendPlayerModeToMidi(playing); };

        rigControl->setTunerComponent(rigTuner.get());
        rigControl->setOutControlComponent(outControlComponent.get());

        // 7) Синхронизация пресетов
        bankEditor->onActivePresetChanged = [this](int idx)
            {
                rigControl->handleExternalPresetChange(idx);
                if (!hostIsDriving)
                    vstHostComponent->setExternalPresetIndex(idx);
            };
        rigControl->setPresetChangeCallback([this](int idx)
            {
                bankEditor->setActivePreset(idx);
                vstHostComponent->setExternalPresetIndex(idx);
            });
        vstHostComponent->setPresetCallback([this](int idx)
            {
                hostIsDriving = true;
                bankEditor->setActivePreset(idx);
                rigControl->handleExternalPresetChange(idx);
                hostIsDriving = false;
            });

        // 8) Вкладки
        addAndMakeVisible(tabs);
        tabs.setTabBarDepth(40);
        tabs.addTab("INPUT CONTROL", Colour::fromRGBA(50, 62, 68, 255), inputControlComponent.get(), false);
        tabs.addTab("RIG CONTROL", Colour::fromRGBA(50, 62, 68, 255), rigControl.get(), false);
        tabs.addTab("BANK EDITOR", Colour::fromRGBA(50, 62, 68, 255), bankEditor.get(), false);
        tabs.addTab("VST HOST", Colour::fromRGBA(50, 62, 68, 255), vstHostComponent.get(), false);
        tabs.addTab("OUT CONTROL", Colour::fromRGBA(50, 62, 68, 255), outControlComponent.get(), false);
        tabs.addTab("LOOPER", Colour::fromRGBA(50, 62, 68, 255), looperComponent.get(), false);
        tabs.addTab("TUNER", Colour::fromRGBA(50, 62, 68, 255), tunerComponent.get(), false);

        // Сразу переставляем OUT CONTROL в конец
        ensureOutControlLast();

        looperTabVisible = true;
        tunerTabVisible = true;

        // Обработчик смены вкладок
        tabs.onCurrentTabChanged = [this](int newIndex)
            {
                updateTunerRouting();

                if (rigControl)
                {
                    auto tabName = tabs.getTabNames()[newIndex];

                    // INPUT CONTROL — как раньше
                    if (tabName == "INPUT CONTROL")
                        rigControl->sendSettingsMenuState(true);
                    else
                        rigControl->sendSettingsMenuState(false);

                }
            };


        // 11) CPU и BPM
        cpuIndicator = std::make_unique<CpuLoadIndicator>(globalCpuLoad);
        addAndMakeVisible(cpuIndicator.get());

        tapTempoDisplay.setText("120 BPM", dontSendNotification);
        tapTempoDisplay.setJustificationType(Justification::centred);
        addAndMakeVisible(&tapTempoDisplay);
        vstHostComponent->setBpmDisplayLabel(&tapTempoDisplay);

        setSize(900, 600);
    }

    // Гарантирует, что OUT CONTROL всегда последняя
    void MainContentComponent::ensureOutControlLast()
    {
        auto names = tabs.getTabNames();
        int outIndex = names.indexOf("OUT CONTROL");

        if (outIndex >= 0 && outIndex != tabs.getNumTabs() - 1)
            tabs.moveTab(outIndex, tabs.getNumTabs() - 1);
    }
    void MainContentComponent::ensureRigControlActive()
    {
        auto names = tabs.getTabNames();
        int rigIndex = names.indexOf("RIG CONTROL");

        if (rigIndex >= 0 && tabs.getCurrentTabIndex() != rigIndex)
            tabs.setCurrentTabIndex(rigIndex, true);
    }

    // При первом показе окна — активируем RIG CONTROL
    void MainContentComponent::parentHierarchyChanged()
    {
        if (isShowing())
        {
            ensureOutControlLast();
            ensureRigControlActive(); // если нужно, чтобы RIG CONTROL была активной
        }
    }


    ~MainContentComponent() override
    {
        // просто снимаем фокус с любого UI-элемента
        if (auto* f = Component::getCurrentlyFocusedComponent())
            f->giveAwayKeyboardFocus();
        // всё остальное (удаление AudioCallback и тюнеров) делает VSTHostComponent в своём деструкторе
    }
    VSTHostComponent& getVstHostComponent() noexcept { return *vstHostComponent; }
    void resized() override
    {
        auto area = getLocalBounds();           // Вся доступная область компонента

        // === Настройки размеров и отступов ===
        const int buttonWidth = 100;       // ← ширина кнопок и индикаторов
        const int buttonHeight = 28;        // ← высота кнопок и индикаторов
        const int margin = 2;         // ← отступ между элементами

        // === Гибкие сдвиги ===
        const int shiftIN_X = 3;         // → сдвиг кнопки IN по горизонтали
        const int shiftIN_Y = 5;         // ↓ сдвиг кнопки IN по вертикали

        const int shiftOUT_X = -3;        // ← сдвиг кнопки OUT по горизонтали
        const int shiftOUT_Y = 5;         // ↓ сдвиг кнопки OUT по вертикали

        const int shiftBlock_X = -5;        // ← сдвиг блока CPU + TEMPO по горизонтали
        const int shiftBlock_Y = 7;         // ↓ сдвиг блока CPU + TEMPO по вертикали

        const int cpuWidth = 100;       // ← ширина CPU индикатора
        const int tempoWidth = 100;       // ← ширина TEMPO дисплея
        const int blockHeight = 25;        // ← высота блока CPU/TEMPO

        // === Блок CPU + TEMPO ===
        auto blockArea = area;
        blockArea.reduce(buttonWidth + margin, 0); // Освобождаем боковые зоны под кнопки

        // CPU индикатор
        auto cpuBounds = blockArea.removeFromRight(cpuWidth + margin).removeFromTop(blockHeight);
        cpuIndicator->setBounds(cpuBounds.translated(shiftBlock_X, shiftBlock_Y)); // ← применяем сдвиги блока

        // TEMPO дисплей
        auto tempoBounds = blockArea.removeFromRight(tempoWidth + margin).removeFromTop(blockHeight);
        tapTempoDisplay.setBounds(tempoBounds.translated(shiftBlock_X, shiftBlock_Y)); // ← применяем сдвиги блока

        // === Вкладки ===
        tabs.setBounds(area); // ← занимают всю оставшуюся область
    }
    void setLooperTabVisible(bool shouldShow)
    {
        if (shouldShow && !looperTabVisible)
        {
            tabs.addTab("LOOPER", Colour::fromRGBA(50, 62, 68, 255), looperComponent.get(), false);
            looperTabVisible = true;
            ensureRigControlActive();
        }
        else if (!shouldShow && looperTabVisible)
        {
            if (auto i = findTabIndexFor(looperComponent.get()); i >= 0)
                tabs.removeTab(i);
            looperTabVisible = false;
            selectBestTabAfterChange(); // вместо tabs.setCurrentTabIndex(0, ...)
        }
        ensureOutControlLast();

        updateTunerRouting();
    }

    void setTunerTabVisible(bool shouldShow)
    {
        if (shouldShow && !tunerTabVisible)
        {
            tabs.addTab("TUNER", Colour::fromRGBA(50, 62, 68, 255), tunerComponent.get(), false);
            vstHostComponent->addTuner(tunerComponent.get());
            tunerTabVisible = true;
            ensureRigControlActive();
        }
        else if (!shouldShow && tunerTabVisible)
        {
            vstHostComponent->removeTuner(tunerComponent.get());
            if (auto i = findTabIndexFor(tunerComponent.get()); i >= 0)
                tabs.removeTab(i);
            tunerTabVisible = false;
            selectBestTabAfterChange(); // вместо tabs.setCurrentTabIndex(0, ...)
        }
        ensureOutControlLast();

        updateTunerRouting();
    }

    void setTunerStyleClassic(bool isClassic)
    {
        if (tunerComponent)
            tunerComponent->setVisualStyle(isClassic
                ? TunerComponent::TunerVisualStyle::Classic
                : TunerComponent::TunerVisualStyle::Triangles);

        if (rigTuner)
            rigTuner->setVisualStyle(isClassic
                ? TunerComponent::TunerVisualStyle::Classic
                : TunerComponent::TunerVisualStyle::Triangles);
    }
    void MainContentComponent::activateTunerTabIfVisible()
    {
        int tunerIndex = getTabIndexByName("TUNER");
        if (tunerIndex >= 0 && tabs.getCurrentTabIndex() != tunerIndex)
            tabs.setCurrentTabIndex(tunerIndex, true);
    }

    void MainContentComponent::closeTunerTabIfVisible()
    {
        int tunerIndex = getTabIndexByName("TUNER");
        if (tunerIndex >= 0)
        {
            tabs.removeTab(tunerIndex);
            tunerTabVisible = false;
            ensureRigControlActive();
        }
    }

    juce::AudioDeviceManager& getAudioDeviceManager() noexcept
    {
        return deviceManager;
    }
    InputControlComponent* getInputControlComponent() noexcept
    {
        return inputControlComponent.get();
    }

private:
    
    void MainContentComponent::updateTunerRouting()
    {
        vstHostComponent->removeTuner(tunerComponent.get());
        vstHostComponent->removeTuner(rigTuner.get());

        auto* current = tabs.getTabContentComponent(tabs.getCurrentTabIndex());
        if (current == tunerComponent.get())
            vstHostComponent->addTuner(tunerComponent.get());
        else if (rigControl->isTunerVisible())
            vstHostComponent->addTuner(rigTuner.get());
    }
    int findTabIndexFor(Component* comp) const
    {
        for (int i = 0; i < tabs.getNumTabs(); ++i)
            if (tabs.getTabContentComponent(i) == comp)
                return i;
        return -1;
    }
    int getTabIndexByName(const juce::String& name) const
    {
        auto names = tabs.getTabNames();
        return names.indexOf(name);
    }

    // Если удаляем/скрываем вкладку, держим/возвращаем RIG CONTROL
    void selectBestTabAfterChange()
    {
        // Если RIG CONTROL есть — активируем её
        int rigIndex = getTabIndexByName("RIG CONTROL");
        if (rigIndex >= 0)
        {
            tabs.setCurrentTabIndex(rigIndex, true);
            return;
        }

        // Иначе — если остались вкладки, остаёмся на текущей, либо на 0
        if (tabs.getNumTabs() > 0)
        {
            int cur = tabs.getCurrentTabIndex();
            if (cur < 0 || cur >= tabs.getNumTabs())
                tabs.setCurrentTabIndex(juce::jmin(0, tabs.getNumTabs() - 1), true);
        }
    }

    AudioDeviceManager                       deviceManager;
    std::unique_ptr<AudioMidiSettingsDialog> audioSettingsDialog;
    std::unique_ptr<VSTHostComponent>        vstHostComponent;
    std::unique_ptr<BankEditor>              bankEditor;
    std::unique_ptr<Rig_control>             rigControl;
    std::unique_ptr<LooperEngine>            looperEngine;
    std::unique_ptr<LooperComponent>         looperComponent;
    std::unique_ptr<TunerComponent>          tunerComponent, rigTuner;
    std::unique_ptr<OutControlComponent>     outControlComponent;
    std::unique_ptr<CpuLoadIndicator>        cpuIndicator;
    std::unique_ptr<InputControlComponent> inputControlComponent;
    Label                                    tapTempoDisplay;
    std::unique_ptr<MidiStartupShutdown> midiInit;
    CallbackTabbedComponent                   tabs;
    double currentSampleRate = 0.0;
    int    currentBlockSize = 0;
    bool                                      hostIsDriving = false;
    bool                                      looperTabVisible = false;
    bool                                      tunerTabVisible = false;
    bool                                      outControlTabVisible = false;
    bool                                      inControlTabVisible = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};