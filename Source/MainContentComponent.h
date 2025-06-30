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
        // 1) создаём все блоки
        vstHostComponent = std::make_unique<VSTHostComponent>();
        bankEditor = std::make_unique<BankEditor>(vstHostComponent.get());
        rigControl = std::make_unique<Rig_control>();

        // 2) даём Rig_control ссылки на Host и BankEditor
        rigControl->setBankEditor(bankEditor.get());
        rigControl->setVstHostComponent(vstHostComponent.get());

        // === 3) BankEditor -> Rig_control + VSTHostComponent
        bankEditor->onActivePresetChanged = [this](int newIndex)
            {
                // всегда обновляем Rig_control
                rigControl->handleExternalPresetChange(newIndex);

                // Host меняем только если это не внутренний цикл из Host-а
                if (!hostIsDriving)
                    vstHostComponent->setExternalPresetIndex(newIndex);
            };

        // === 4) Rig_control -> BankEditor + VSTHostComponent
        rigControl->setPresetChangeCallback([this](int newIndex)
            {
                // клик из Rig_control: сразу BankEditor и Host
                bankEditor->setActivePreset(newIndex);
                vstHostComponent->setExternalPresetIndex(newIndex);
            });

        // === 5) VSTHostComponent -> BankEditor + Rig_control
        vstHostComponent->setPresetCallback([this](int newIndex)
            {
                // источник = Host, заходим в «режим хоста»
                hostIsDriving = true;

                // отрисовываем BankEditor (UI + CC → плагин)
                bankEditor->setActivePreset(newIndex);
                rigControl->handleExternalPresetChange(newIndex);

                // выходим из «режима хоста»
                hostIsDriving = false;
            });

        tabs.addTab("RIG CONTROL", juce::Colour::fromRGBA(50, 62, 68, 255), rigControl.get(), false);
        tabs.addTab("BANK EDITOR", juce::Colour::fromRGBA(50, 62, 68, 255), bankEditor.get(), false);
        tabs.addTab("VST HOST", juce::Colour::fromRGBA(50, 62, 68, 255), vstHostComponent.get(), false);

        // looper
        looperEngine = std::make_unique<LooperEngine>();
        looperEngine->prepare(44100.0, 512);
        // Прокидываем движок в Rig_control:
        rigControl->setLooperEngine(*looperEngine);

        looperComponent = std::make_unique<LooperComponent>(*looperEngine);

        // ─── ВСТАВКА ───
        vstHostComponent->setLooperEngine(looperEngine.get());

        // ───────────────

        tabs.addTab("LOOPER",
            juce::Colour(0xFF323E44),
            looperComponent.get(), false);





        addAndMakeVisible(tabs);

        cpuIndicator = std::make_unique<CpuLoadIndicator>(globalCpuLoad);
        addAndMakeVisible(cpuIndicator.get());

        tapTempoDisplay.setText("120 BPM", dontSendNotification);
        tapTempoDisplay.setJustificationType(Justification::centred);
        addAndMakeVisible(tapTempoDisplay);

        vstHostComponent->setBpmDisplayLabel(&tapTempoDisplay);
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
        cpuIndicator->setBounds(r.removeFromRight(indicatorW + margin)
            .removeFromTop(indicatorH));
        tapTempoDisplay.setBounds(r.removeFromRight(indicatorW + margin)
            .removeFromTop(indicatorH));
        tabs.setBounds(area);
    }

private:
    // UI
    TabbedComponent                      tabs;
    std::unique_ptr<VSTHostComponent>    vstHostComponent;
    std::unique_ptr<BankEditor>          bankEditor;
    std::unique_ptr<Rig_control>         rigControl;
    std::unique_ptr<CpuLoadIndicator>    cpuIndicator;
    Label                                tapTempoDisplay;

    // флаг, чтобы отличать «источник = Host» от «мы сами»
    bool hostIsDriving = false;

    //looper 

    std::unique_ptr<LooperEngine>    looperEngine;
    std::unique_ptr<LooperComponent> looperComponent;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};
