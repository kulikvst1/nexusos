#pragma once

#include <JuceHeader.h>
#include "plugin_process_callback.h"
#include "Rig_control.h"
#include "bank_editor.h"
#include "vst_host.h"
#include "cpu_load.h"
#include "LooperComponent.h"
#include "TunerComponent.h"
#include "OutControlComponent.h"  

using namespace juce;

//==============================================================================

class CallbackTabbedComponent : public TabbedComponent
{
public:
    CallbackTabbedComponent(TabbedButtonBar::Orientation orient)
        : TabbedComponent(orient) {}

    std::function<void(int)> onCurrentTabChanged;

protected:
    void currentTabChanged(int newIndex, const String& newName) override
    {
        TabbedComponent::currentTabChanged(newIndex, newName);
        if (onCurrentTabChanged)
            onCurrentTabChanged(newIndex);
    }
};

//==============================================================================

class MainContentComponent : public Component
{
public:
    MainContentComponent()
        : tabs(TabbedButtonBar::TabsAtTop)
    {
        // 1) Audio callback + UI-тюнеры
        pluginCallback = std::make_unique<PluginProcessCallback>();
        tunerComponent = std::make_unique<TunerComponent>();
        rigTuner = std::make_unique<TunerComponent>();

        pluginCallback->setTuner(tunerComponent.get());
        pluginCallback->setTuner(rigTuner.get());

        // 2) AudioDeviceManager → callback
        deviceManager.initialise(2, 2, nullptr, true);
        deviceManager.addAudioCallback(pluginCallback.get());

        // 3) Другие модули
        vstHostComponent = std::make_unique<VSTHostComponent>();
        bankEditor = std::make_unique<BankEditor>(vstHostComponent.get());
        rigControl = std::make_unique<Rig_control>();
        looperEngine = std::make_unique<LooperEngine>();
        looperComponent = std::make_unique<LooperComponent>(*looperEngine);
        outControlComponent = std::make_unique<OutControlComponent>();

        pluginCallback->setHostComponent(vstHostComponent.get());
        pluginCallback->setLooperEngine(looperEngine.get());

        // 4) Синхрон A4
        tunerComponent->onReferenceA4Changed = [rig = rigTuner.get()](double a4)
            { rig->setReferenceA4(a4, false); };
        rigTuner->onReferenceA4Changed = [tab = tunerComponent.get()](double a4)
            { tab->setReferenceA4(a4, false); };

        // 5) Подготовка двигателей
        looperEngine->prepare(44100.0, 512);
        vstHostComponent->setLooperEngine(looperEngine.get());

        // 6) Rig_control
        rigControl->setBankEditor(bankEditor.get());
        rigControl->setVstHostComponent(vstHostComponent.get());
        rigControl->setLooperEngine(*looperEngine);
        rigControl->setTunerComponent(rigTuner.get());

        // 7) Синхрон пресетов
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

        // 8) Собираем табы
        addAndMakeVisible(tabs);
        tabs.addTab("RIG CONTROL", Colour::fromRGBA(50, 62, 68, 255), rigControl.get(), false);
        tabs.addTab("BANK EDITOR", Colour::fromRGBA(50, 62, 68, 255), bankEditor.get(), false);
        tabs.addTab("VST HOST", Colour::fromRGBA(50, 62, 68, 255), vstHostComponent.get(), false);
        tabs.addTab("OUT CONTROL", Colour::fromRGBA(50, 62, 68, 255), outControlComponent.get(), false);
        tabs.addTab("LOOPER", Colour::fromRGBA(50, 62, 68, 255), looperComponent.get(), false);
        tabs.addTab("TUNER", Colour::fromRGBA(50, 62, 68, 255), tunerComponent.get(), false);

        looperTabVisible = true;
        tunerTabVisible = true;
        outControlTabVisible = true;

        // 9) Роутинг тюнера при смене таба
        tabs.onCurrentTabChanged = [this](int)
            {
                updateTunerRouting();
            };

        // сразу настроим правильный тюнер
        updateTunerRouting();

        // ещё реагируем, если внешний rigControl показывает/скрывает свой тюнер
        rigControl->onTunerVisibilityChanged = [this](bool)
            {
                updateTunerRouting();
            };

        // 10) CPU и BPM
        cpuIndicator = std::make_unique<CpuLoadIndicator>(globalCpuLoad);
        addAndMakeVisible(cpuIndicator.get());

        tapTempoDisplay.setText("120 BPM", dontSendNotification);
        tapTempoDisplay.setJustificationType(Justification::centred);
        addAndMakeVisible(&tapTempoDisplay);
        vstHostComponent->setBpmDisplayLabel(&tapTempoDisplay);

        setSize(900, 600);
    }
    MainContentComponent::~MainContentComponent()
    {
        // 0) Найдём тот компонент, что сейчас держит фокус, и отберём у него клавишный фокус
        if (auto* focused = Component::getCurrentlyFocusedComponent())
            focused->giveAwayKeyboardFocus();

        // 1) Снимаем UI-тюнеры из аудиоколбэка
        pluginCallback->removeTuner(tunerComponent.get());
        pluginCallback->removeTuner(rigTuner.get());

        // 2) Убираем аудио-колбэк
        deviceManager.removeAudioCallback(pluginCallback.get());

        // 3) Уничтожаем сам колбэк, пока UI-компоненты ещё живы
        pluginCallback.reset();
    }


    VSTHostComponent& getVstHostComponent() noexcept { return *vstHostComponent; }

    void resized() override
    {
        auto area = getLocalBounds();
        const int margin = 2, w = 100, h = 25;

        auto r = area;
        cpuIndicator->setBounds(r.removeFromRight(w + margin).removeFromTop(h));
        tapTempoDisplay.setBounds(r.removeFromRight(w + margin).removeFromTop(h));
        tabs.setBounds(area);
    }

    // Показывать/прятать LOOPER
    void setLooperTabVisible(bool shouldShow)
    {
        if (shouldShow && !looperTabVisible)
        {
            tabs.addTab("LOOPER", Colour::fromRGBA(50, 62, 68, 255), looperComponent.get(), false);
            looperTabVisible = true;
        }
        else if (!shouldShow && looperTabVisible)
        {
            if (auto idx = findTabIndexFor(looperComponent.get()); idx >= 0)
                tabs.removeTab(idx);

            looperTabVisible = false;
            tabs.setCurrentTabIndex(0, sendNotification);
        }

        updateTunerRouting();
    }

    // Показывать/прятать TUNER
    void setTunerTabVisible(bool shouldShow)
    {
        if (shouldShow && !tunerTabVisible)
        {
            tabs.addTab("TUNER", Colour::fromRGBA(50, 62, 68, 255), tunerComponent.get(), false);
            tunerTabVisible = true;
        }
        else if (!shouldShow && tunerTabVisible)
        {
            pluginCallback->removeTuner(tunerComponent.get());

            if (auto idx = findTabIndexFor(tunerComponent.get()); idx >= 0)
                tabs.removeTab(idx);

            tunerTabVisible = false;
            tabs.setCurrentTabIndex(0, sendNotification);
        }

        updateTunerRouting();
    }

    // Показывать/прятать OUT CONTROL
    void setOutMasterTabVisible(bool shouldShow)
    {
        if (shouldShow && !outControlTabVisible)
        {
            tabs.addTab("OUT CONTROL",
                Colour::fromRGBA(50, 62, 68, 255),
                outControlComponent.get(), false);
            outControlTabVisible = true;
        }
        else if (!shouldShow && outControlTabVisible)
        {
            if (auto idx = findTabIndexFor(outControlComponent.get()); idx >= 0)
                tabs.removeTab(idx);

            outControlTabVisible = false;
            tabs.setCurrentTabIndex(0, sendNotification);
        }

        updateTunerRouting();
    }

private:
    void MainContentComponent::updateTunerRouting()
    {
        // Убираем всех тюнеров
        pluginCallback->removeTuner(tunerComponent.get());
        pluginCallback->removeTuner(rigTuner.get());

        // Какой сейчас индекс?
        int currentIndex = tabs.getCurrentTabIndex();

        // Получаем указатель на текущий таб-компонент
        Component* currentComp = nullptr;
        if (currentIndex >= 0 && currentIndex < tabs.getNumTabs())
            currentComp = tabs.getTabContentComponent(currentIndex);

        // Если открыт UI-тюнер – подключаем его
        if (currentComp == tunerComponent.get())
        {
            pluginCallback->setTuner(tunerComponent.get());
        }
        // Иначе, если rigControl всё ещё хочет rig-тюнер – подключаем rigTuner
        else if (rigControl->isTunerVisible())
        {
            pluginCallback->setTuner(rigTuner.get());
        }
    }

    int findTabIndexFor(Component* comp) const
    {
        for (int i = 0; i < tabs.getNumTabs(); ++i)
            if (tabs.getTabContentComponent(i) == comp)
                return i;
        return -1;
    }

    AudioDeviceManager                       deviceManager;
    std::unique_ptr<PluginProcessCallback>   pluginCallback;
    std::unique_ptr<VSTHostComponent>        vstHostComponent;
    std::unique_ptr<BankEditor>              bankEditor;
    std::unique_ptr<Rig_control>             rigControl;
    std::unique_ptr<LooperEngine>            looperEngine;
    std::unique_ptr<LooperComponent>         looperComponent;
    std::unique_ptr<TunerComponent>          tunerComponent, rigTuner;
    std::unique_ptr<OutControlComponent>     outControlComponent;
    std::unique_ptr<CpuLoadIndicator>        cpuIndicator;
    Label                                     tapTempoDisplay;

    CallbackTabbedComponent                   tabs;
    bool                                      hostIsDriving = false;
    bool                                      looperTabVisible = false;
    bool                                      tunerTabVisible = false;
    bool                                      outControlTabVisible = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};
