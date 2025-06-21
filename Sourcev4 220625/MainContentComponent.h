#pragma once

#include <JuceHeader.h>
#include "Rig_control.h"    
#include "bank_editor.h"     
#include "vst_host.h"
#include"cpu_load.h"
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

        tabs.addTab("RIG KONTROL", Colours::darkgrey, rigControl.release(), false);
        tabs.addTab("BANK EDITOR", Colours::darkgrey, bankEditor.release(), false);
        tabs.addTab("VST HOST", Colours::darkgrey, vstHostComponent.release(), false);

        addAndMakeVisible(tabs);


    }

    void resized() override
    {
        tabs.setBounds(getLocalBounds());
    }

private:
    TabbedComponent tabs;

   
    std::unique_ptr<Rig_control> rigControl;
    std::unique_ptr<BankEditor> bankEditor;
    std::unique_ptr<VSTHostComponent> vstHostComponent;
};
