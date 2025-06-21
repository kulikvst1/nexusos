#pragma once

#include <JuceHeader.h>
#include "Rig_control.h"     // Êîìïîíåíò äëÿ Rig (åñëè èñïîëüçóåòñÿ)
#include "bank_editor.h"     // Â ýòîì ôàéëå îïðåäåëÿåòñÿ êëàññ BankEditor, ñî âñåìè íóæíûìè ìåòîäàìè è êîíñòðóêòîðîì
#include "vst_host.h"        // Çäåñü îáúÿâëåí VSTHostComponent

using namespace juce;

class MainContentComponent : public Component
{
public:
    MainContentComponent()
        : tabs(TabbedButtonBar::TabsAtTop)
    {
        // Ñîçäà¸ì VST-õîñò (óêàçûâàåì ðåàëüíûé òèï)
        vstHostComponent = std::make_unique<VSTHostComponent>();

        // Ñîçäàåì BankEditor, ïåðåäàâàÿ åìó óêàçàòåëü íà VST-õîñò.
        // Ýòî îáåñïå÷èâàåò ïðÿìîå óïðàâëåíèå – âíóòðè BankEditor âûçûâàþòñÿ
        // ìåòîäû òèïà vstHost->setPluginParameter(...), setParameterChangeCallback è ïðî÷åå.
        bankEditor = std::make_unique<BankEditor>(vstHostComponent.get());

        // Ñîçäà¸ì êîìïîíåíò Rig_control (åñëè îí íóæåí)
        rigControl = std::make_unique<Rig_control>();

        // Äîáàâëÿåì êîìïîíåíòû âî âêëàäêè (ïîðÿäîê ìîæíî ìåíÿòü ïî íåîáõîäèìîñòè)
        tabs.addTab("RIG KONTROL", Colours::darkgrey, rigControl.release(), false);
        tabs.addTab("BANK EDITOR", Colours::lightgrey, bankEditor.release(), false);
        tabs.addTab("VST HOST", Colours::lightgrey, vstHostComponent.release(), false);

        addAndMakeVisible(tabs);
    }

    void resized() override
    {
        tabs.setBounds(getLocalBounds());
    }

private:
    TabbedComponent tabs;

    // Õðàíèì Rig_control, BankEditor è VSTHostComponent ñ èõ ðåàëüíûìè òèïàìè.
    // Ýòî âàæíî: ïîñêîëüêó BankEditor îæèäàåò óêàçàòåëü òèïà VSTHostComponent*,
    // ïåðåìåííàÿ vstHostComponent äîëæíà áûòü èìåííî òàêîãî òèïà.
    std::unique_ptr<Rig_control> rigControl;
    std::unique_ptr<BankEditor> bankEditor;
    std::unique_ptr<VSTHostComponent> vstHostComponent;
};
