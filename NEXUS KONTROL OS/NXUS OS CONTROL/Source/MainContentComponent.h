#pragma once

#include <JuceHeader.h>
#include "Rig_control.h"
#include "bank_editor.h"
#include "vst_host.h"
#include "cpu_load.h"
#include "LooperComponent.h"
#include "TunerComponent.h"

using namespace juce;

class MainContentComponent : public Component
{
public:
    MainContentComponent()
        : tabs(TabbedButtonBar::TabsAtTop)
    {
        // Создаем все модули
        vstHostComponent = std::make_unique<VSTHostComponent>();
        bankEditor = std::make_unique<BankEditor>(vstHostComponent.get());
        rigControl = std::make_unique<Rig_control>();
        looperEngine = std::make_unique<LooperEngine>();
        looperComponent = std::make_unique<LooperComponent>(*looperEngine);
        tunerComponent = std::make_unique<TunerComponent>();

        // Готовим движки
        looperEngine->prepare(44100.0, 512);
        vstHostComponent->setLooperEngine(looperEngine.get());
        vstHostComponent->setTuner(tunerComponent.get());

        // Настраиваем связи Rig ↔ Bank ↔ Host
        rigControl->setBankEditor(bankEditor.get());
        rigControl->setVstHostComponent(vstHostComponent.get());
        rigControl->setLooperEngine(*looperEngine);

        bankEditor->onActivePresetChanged = [this](int newIndex)
            {
                rigControl->handleExternalPresetChange(newIndex);
                if (!hostIsDriving)
                    vstHostComponent->setExternalPresetIndex(newIndex);
            };

        rigControl->setPresetChangeCallback([this](int newIndex)
            {
                bankEditor->setActivePreset(newIndex);
                vstHostComponent->setExternalPresetIndex(newIndex);
            });

        vstHostComponent->setPresetCallback([this](int newIndex)
            {
                hostIsDriving = true;
                bankEditor->setActivePreset(newIndex);
                rigControl->handleExternalPresetChange(newIndex);
                hostIsDriving = false;
            });

        // Добавляем вкладки
        addAndMakeVisible(tabs);
        tabs.addTab("RIG CONTROL", Colour::fromRGBA(50, 62, 68, 255), rigControl.get(), true);
        tabs.addTab("BANK EDITOR", Colour::fromRGBA(50, 62, 68, 255), bankEditor.get(), false);
        tabs.addTab("VST HOST", Colour::fromRGBA(50, 62, 68, 255), vstHostComponent.get(), false);
        tabs.addTab("LOOPER", Colour::fromRGBA(50, 62, 68, 255), looperComponent.get(), false);
        tabs.addTab("TUNER", Colour::fromRGBA(50, 62, 68, 255), tunerComponent.get(), false);

        // CPU-индикатор и BPM
        cpuIndicator = std::make_unique<CpuLoadIndicator>(globalCpuLoad);
        addAndMakeVisible(cpuIndicator.get());

        tapTempoDisplay.setText("120 BPM", dontSendNotification);
        tapTempoDisplay.setJustificationType(Justification::centred);
        addAndMakeVisible(&tapTempoDisplay);
        vstHostComponent->setBpmDisplayLabel(&tapTempoDisplay);

        setSize(900, 600);
    }

    VSTHostComponent& getVstHostComponent() noexcept
    {
        return *vstHostComponent;
    }

    void resized() override
    {
        auto area = getLocalBounds();
        const int margin = 2, w = 100, h = 25;

        auto r = area;
        cpuIndicator->setBounds(
            r.removeFromRight(w + margin).removeFromTop(h));
        tapTempoDisplay.setBounds(
            r.removeFromRight(w + margin).removeFromTop(h));
        tabs.setBounds(area);
    }

    /** Показать/скрыть вкладку LOOPER */
    void setLooperTabVisible(bool shouldShow)
    {
        if (shouldShow && !looperTabVisible)
        {
            tabs.addTab("LOOPER",
                Colour::fromRGBA(50, 62, 68, 255),
                looperComponent.get(), false);
            looperTabVisible = true;
        }
        else if (!shouldShow && looperTabVisible)
        {
            if (int idx = findTabIndexFor(looperComponent.get()); idx >= 0)
                tabs.removeTab(idx);  // ← убрали лишний аргумент
            looperTabVisible = false;
            tabs.setCurrentTabIndex(0, dontSendNotification);
        }
    }

    /** Показать/скрыть вкладку TUNER */
    void setTunerTabVisible(bool shouldShow)
    {
        if (shouldShow && !tunerTabVisible)
        {
            tabs.addTab("TUNER",
                Colour::fromRGBA(50, 62, 68, 255),
                tunerComponent.get(), false);
            tunerTabVisible = true;
        }
        else if (!shouldShow && tunerTabVisible)
        {
            if (int idx = findTabIndexFor(tunerComponent.get()); idx >= 0)
                tabs.removeTab(idx);  // ← убрали лишний аргумент
            tunerTabVisible = false;
            tabs.setCurrentTabIndex(0, dontSendNotification);
        }
    }

private:
    // Ищет индекс вкладки по её контент-компоненту
    int findTabIndexFor(Component* comp) const
    {
        for (int i = 0; i < tabs.getNumTabs(); ++i)
            if (tabs.getTabContentComponent(i) == comp)
                return i;
        return -1;
    }

    TabbedComponent                         tabs;
    std::unique_ptr<VSTHostComponent>       vstHostComponent;
    std::unique_ptr<BankEditor>             bankEditor;
    std::unique_ptr<Rig_control>            rigControl;
    std::unique_ptr<LooperEngine>           looperEngine;
    std::unique_ptr<LooperComponent>        looperComponent;
    std::unique_ptr<TunerComponent>         tunerComponent;
    std::unique_ptr<CpuLoadIndicator>       cpuIndicator;
    Label                                   tapTempoDisplay;

    bool                                    hostIsDriving = false;
    bool                                    looperTabVisible = true;
    bool                                    tunerTabVisible = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};
