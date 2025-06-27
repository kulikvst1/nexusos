#pragma once

#include <JuceHeader.h>
#include "Rig_control.h"    
#include "bank_editor.h"     
#include "vst_host.h"
#include "cpu_load.h"

using namespace juce;

class MainContentComponent : public Component
{
public:
    MainContentComponent()
        : tabs(TabbedButtonBar::TabsAtTop)
    {
        vstHostComponent = std::make_unique<VSTHostComponent>();
        bankEditor = std::make_unique<BankEditor>(vstHostComponent.get());
        rigControl = std::make_unique<Rig_control>();

        // даём Rig_control доступ к BankEditor + VSTHost
        rigControl->setBankEditor(bankEditor.get());
        rigControl->setVstHostComponent(vstHostComponent.get());

        // === 1) BankEditor -> Rig_control + VSTHostComponent
        bankEditor->onActivePresetChanged = [this](int newIndex)
            {
                // внешний preset: сбрасываем manualShift и рисуем Rig_control
                rigControl->handleExternalPresetChange(newIndex);

                // и говорим VSTHostComponent переключить программу + UI
                vstHostComponent->setExternalPresetIndex(newIndex);
            };

        // === 2) Rig_control -> BankEditor + VSTHostComponent
        rigControl->setPresetChangeCallback([this](int newIndex)
            {
                // клик по кнопкам Rig_control
                bankEditor->setActivePresetIndex(newIndex);
                vstHostComponent->setExternalPresetIndex(newIndex);
            });

        // === 3) VSTHostComponent -> BankEditor + Rig_control
        vstHostComponent->setPresetCallback([this](int newIndex)
            {
                bankEditor->setActivePresetIndex(newIndex);
                rigControl->handleExternalPresetChange(newIndex);
            });

        // Добавляем вкладки в UI
        tabs.addTab("RIG CONTROL", juce::Colour::fromRGBA(50, 62, 68, 255),
            rigControl.get(), false);
        tabs.addTab("BANK EDITOR", juce::Colour::fromRGBA(50, 62, 68, 255),
            bankEditor.get(), false);
        tabs.addTab("VST HOST", juce::Colour::fromRGBA(50, 62, 68, 255),
            vstHostComponent.get(), false);



        // Ñîçäà¸ì èíäèêàòîð çàãðóçêè CPU
        cpuIndicator = std::make_unique<CpuLoadIndicator>(globalCpuLoad);
        addAndMakeVisible(cpuIndicator.get());

        // Ñîçäà¸ì èíäèêàòîð BPM (Label)
        tapTempoDisplay.setText("120 BPM", dontSendNotification);
        tapTempoDisplay.setJustificationType(Justification::centred);
        addAndMakeVisible(tapTempoDisplay);

        // Ïåðåäà¸ì óêàçàòåëü íà BPM-èíäèêàòîð â VST-õîñò äëÿ îáíîâëåíèÿ ïîêàçàòåëÿ
          vstHostComponent->setBpmDisplayLabel(&tapTempoDisplay);

        addAndMakeVisible(tabs);
    }
    void resized() override
    {
        // Çàäà¸ì ðàçìåðû è îòñòóïû äëÿ èíäèêàòîðîâ
        const int indicatorWidth = 100;
        const int indicatorHeight = 25;
        const int margin = 2;

        // Ðàçìåùàåì CPU-èíäèêàòîð â ïðàâîì âåðõíåì óãëó
        cpuIndicator->setBounds(getWidth() - indicatorWidth - margin,
            margin,
            indicatorWidth,
            indicatorHeight);

        // Ðàçìåùàåì BPM-èíäèêàòîð íåïîñðåäñòâåííî ïåðåä CPU-èíäèêàòîðîì
        tapTempoDisplay.setBounds(getWidth() - 2 * indicatorWidth - 2 * margin,
            margin,
            indicatorWidth,
            indicatorHeight);

        tabs.setBounds(getLocalBounds());
    }

private:
    TabbedComponent tabs;
    std::unique_ptr<Rig_control> rigControl;
    std::unique_ptr<BankEditor> bankEditor;
    std::unique_ptr<VSTHostComponent> vstHostComponent;

    // Èíäèêàòîð çàãðóçêè CPU èç cpu_load.h
    std::unique_ptr<CpuLoadIndicator> cpuIndicator;
    // Èíäèêàòîð BPM (Label)
    Label tapTempoDisplay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};
