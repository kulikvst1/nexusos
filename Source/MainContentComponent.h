#pragma once
#include <JuceHeader.h>
#include "vst_host.h"
#include "Rig_control.h"
#include "bank_editor.h"
#include "cpu_load.h"
#include "LooperComponent.h"
#include "TunerComponent.h"
#include "OutControlComponent.h"

using namespace juce;

//==============================================================================
// Таб-компонент, который умеет коллбэком сообщать об изменении текущей вкладки
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
// Главное окно
//==============================================================================
class MainContentComponent : public Component
{
public:
    MainContentComponent()
        : tabs(TabbedButtonBar::TabsAtTop)
    {
        // 1) Инициализируем аудио-устройство
        deviceManager.initialise(2, 2, nullptr, true);

        // 2) Создаём VST-хост и регистрируем в нём единственный AudioCallback
        vstHostComponent = std::make_unique<VSTHostComponent>(deviceManager);

        // 3) UI-тюнеры
        tunerComponent = std::make_unique<TunerComponent>();
        rigTuner = std::make_unique<TunerComponent>();
        vstHostComponent->addTuner(tunerComponent.get());
        vstHostComponent->addTuner(rigTuner.get());

        // 4) Создаём OutControlComponent и сразу его «подключаем» в хост
        outControlComponent = std::make_unique<OutControlComponent>();

        // передаём указатель и показываем в UI
        vstHostComponent->setOutControlComponent(outControlComponent.get());
       // vstHostComponent->addAndMakeVisible(outControlComponent.get());

        // 5) Другие модули
        bankEditor = std::make_unique<BankEditor>(vstHostComponent.get());
        rigControl = std::make_unique<Rig_control>();
        looperEngine = std::make_unique<LooperEngine>();
        looperComponent = std::make_unique<LooperComponent>(*looperEngine);

        vstHostComponent->setLooperEngine(looperEngine.get());

        // 5) Синхрон A4 между тюнерами
        tunerComponent->onReferenceA4Changed = [rig = rigTuner.get()](double a4)
            { rig->setReferenceA4(a4, false); };
        rigTuner->onReferenceA4Changed = [tab = tunerComponent.get()](double a4)
            { tab->setReferenceA4(a4, false); };

        looperEngine->prepare(44100.0, 512);

        // 6) Настраиваем Rig_control
        rigControl->setBankEditor(bankEditor.get());
        rigControl->setVstHostComponent(vstHostComponent.get());
        rigControl->setLooperEngine(*looperEngine);
        rigControl->setTunerComponent(rigTuner.get());

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

        tabs.onCurrentTabChanged = [this](int) { updateTunerRouting(); };
        updateTunerRouting();
        rigControl->onTunerVisibilityChanged = [this](bool) { updateTunerRouting(); };

        // 9) CPU и BPM
        cpuIndicator = std::make_unique<CpuLoadIndicator>(globalCpuLoad);
        addAndMakeVisible(cpuIndicator.get());

        tapTempoDisplay.setText("120 BPM", dontSendNotification);
        tapTempoDisplay.setJustificationType(Justification::centred);
        addAndMakeVisible(&tapTempoDisplay);
        vstHostComponent->setBpmDisplayLabel(&tapTempoDisplay);

        setSize(900, 600);
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
        auto area = getLocalBounds();
        const int margin = 2, w = 100, h = 25;
        auto r = area;
        cpuIndicator->setBounds(r.removeFromRight(w + margin).removeFromTop(h));
        tapTempoDisplay.setBounds(r.removeFromRight(w + margin).removeFromTop(h));
        tabs.setBounds(area);
    }

    void setLooperTabVisible(bool shouldShow)
    {
        if (shouldShow && !looperTabVisible)
        {
            tabs.addTab("LOOPER", Colour::fromRGBA(50, 62, 68, 255), looperComponent.get(), false);
            looperTabVisible = true;
        }
        else if (!shouldShow && looperTabVisible)
        {
            if (auto i = findTabIndexFor(looperComponent.get()); i >= 0)
                tabs.removeTab(i);
            looperTabVisible = false;
            tabs.setCurrentTabIndex(0, sendNotification);
        }
        updateTunerRouting();
    }
    void MainContentComponent::setTunerTabVisible(bool shouldShow)
    {
        if (shouldShow && !tunerTabVisible)
        {
            tabs.addTab("TUNER", Colour::fromRGBA(50, 62, 68, 255), tunerComponent.get(), false);
            vstHostComponent->addTuner(tunerComponent.get());
            tunerTabVisible = true;
        }
        else if (!shouldShow && tunerTabVisible)
        {
            vstHostComponent->removeTuner(tunerComponent.get());
            if (auto i = findTabIndexFor(tunerComponent.get()); i >= 0)
                tabs.removeTab(i);
            tunerTabVisible = false;
            tabs.setCurrentTabIndex(0, sendNotification);
        }

        updateTunerRouting();
    }

    void setOutMasterTabVisible(bool shouldShow)
    {
        if (shouldShow && !outControlTabVisible)
        {
            tabs.addTab("OUT CONTROL", Colour::fromRGBA(50, 62, 68, 255),
                outControlComponent.get(), false);
            outControlTabVisible = true;
        }
        else if (!shouldShow && outControlTabVisible)
        {
            if (auto i = findTabIndexFor(outControlComponent.get()); i >= 0)
                tabs.removeTab(i);
            outControlTabVisible = false;
            tabs.setCurrentTabIndex(0, sendNotification);
        }
        updateTunerRouting();
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

    AudioDeviceManager                       deviceManager;
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
