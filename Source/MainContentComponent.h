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
        // В MainContentComponent:
        vstHostComponent = std::make_unique<VSTHostComponent>();
        bankEditor = std::make_unique<BankEditor>(vstHostComponent.get());
        rigControl = std::make_unique<Rig_control>();
        rigControl->setVstHostComponent(vstHostComponent.get());

        // Устанавливаем связь для синхронизации пресетов:
        rigControl->setBankEditor(bankEditor.get());

        tabs.addTab("RIG KONTROL", juce::Colours::darkgrey, rigControl.get(), false);
        tabs.addTab("BANK EDITOR", juce::Colours::darkgrey, bankEditor.release(), false);
        tabs.addTab("VST HOST", juce::Colours::darkgrey, vstHostComponent.get(), false);



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
