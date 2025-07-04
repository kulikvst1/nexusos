#pragma once

#include <JuceHeader.h>
#include "Rig_control.h"
#include "bank_editor.h"
#include "vst_host.h"
#include "cpu_load.h"
#include "LooperComponent.h"

using namespace juce;

class MainContentComponent : public Component
{
public:
    MainContentComponent()
        : tabs(TabbedButtonBar::TabsAtTop)
    {
        vstHostComponent = std::make_unique<VSTHostComponent>();
        bankEditor       = std::make_unique<BankEditor>(vstHostComponent.get());
        rigControl       = std::make_unique<Rig_control>();
        looperEngine     = std::make_unique<LooperEngine>();
        looperEngine->prepare(44100.0, 512);
        vstHostComponent->setLooperEngine(looperEngine.get());
        looperComponent  = std::make_unique<LooperComponent>(*looperEngine);

        rigControl->setBankEditor(bankEditor.get());
        rigControl->setVstHostComponent(vstHostComponent.get());
        rigControl->setLooperEngine(*looperEngine);

        bankEditor->onActivePresetChanged = [this](int newIndex)
        {
            rigControl->handleExternalPresetChange(newIndex);
            if (! hostIsDriving)
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

        addAndMakeVisible(tabs);
        tabs.addTab("RIG CONTROL", Colour::fromRGBA(50, 62, 68, 255),
                    rigControl.get(),      true);
        tabs.addTab("BANK EDITOR",  Colour::fromRGBA(50, 62, 68, 255),
                    bankEditor.get(),     false);
        tabs.addTab("VST HOST",     Colour::fromRGBA(50, 62, 68, 255),
                    vstHostComponent.get(), false);
        tabs.addTab("LOOPER",       Colour::fromRGBA(50, 62, 68, 255),
                    looperComponent.get(), false);

        cpuIndicator = std::make_unique<CpuLoadIndicator>(globalCpuLoad);
        addAndMakeVisible(cpuIndicator.get());

        tapTempoDisplay.setText("120 BPM", dontSendNotification);
        tapTempoDisplay.setJustificationType(Justification::centred);
        addAndMakeVisible(tapTempoDisplay);
        vstHostComponent->setBpmDisplayLabel(&tapTempoDisplay);

        setSize(900, 600);
    }

    VSTHostComponent& getVstHostComponent() noexcept
    {
        return *vstHostComponent;
    }

    void resized() override
    {
        const auto area = getLocalBounds();
        const int margin = 2;
        const int indicatorW = 100;
        const int indicatorH = 25;

        auto r = area;
        cpuIndicator->setBounds(
            r.removeFromRight(indicatorW + margin)
            .removeFromTop(indicatorH));

        tapTempoDisplay.setBounds(
            r.removeFromRight(indicatorW + margin)
            .removeFromTop(indicatorH));

        tabs.setBounds(area);
    }
    /** Показывает или скрывает вкладку LOOPER и при удалении сразу выбирает первую вкладку */
    void setLooperTabVisible(bool shouldShow)
    {
        if (shouldShow && !looperTabVisible)
        {
            // добавляем LOOPER последней
            tabs.addTab("LOOPER",
                Colour::fromRGBA(50, 62, 68, 255),
                looperComponent.get(),
                false);
            looperTabVisible = true;
        }
        else if (!shouldShow && looperTabVisible)
        {
            // удаляем последнюю вкладку (LOOPER)
            const int lastIdx = tabs.getNumTabs() - 1;
            if (lastIdx >= 0)
                tabs.removeTab(lastIdx);

            looperTabVisible = false;

            // переключаемся на первую вкладку (индекс 0)
            if (tabs.getNumTabs() > 0)
                tabs.setCurrentTabIndex(0, juce::dontSendNotification);
        }
    }


private:
    TabbedComponent                             tabs;
    std::unique_ptr<VSTHostComponent>           vstHostComponent;
    std::unique_ptr<BankEditor>                 bankEditor;
    std::unique_ptr<Rig_control>                rigControl;
    std::unique_ptr<LooperEngine>               looperEngine;
    std::unique_ptr<LooperComponent>            looperComponent;
    std::unique_ptr<CpuLoadIndicator>           cpuIndicator;
    Label                                       tapTempoDisplay;

    bool                                        hostIsDriving    = false;
    bool                                        looperTabVisible = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};
