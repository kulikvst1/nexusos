#pragma once

#include <JuceHeader.h>
#include "vst_host.h"
#include "SafeParamName.h"
#include <functional>

// LookAndFeel для крупных значков на кнопках
struct BigIcon : public juce::LookAndFeel_V4
{
    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
        bool, bool) override
    {
        auto font = juce::Font(20.0f, juce::Font::plain); // размер значка
        g.setFont(font);
        g.setColour(button.findColour(juce::TextButton::textColourOnId));
        g.drawFittedText(button.getButtonText(),
            button.getLocalBounds(),
            juce::Justification::centred,
            1);
    }
};
//─────────────────────────────────────
//  Модель CC-слота
//─────────────────────────────────────
struct CCMapping
{
    int          paramIndex = -1;
    uint8_t      ccValue = 64;
    bool         invert = false;
    bool         enabled = false;     // UI больше не показывает
    juce::String name;
};

class VSTHostComponent;

//─────────────────────────────────────
//  Диалог настройки CC
//─────────────────────────────────────
class SetCCDialog : public juce::DialogWindow
{
public:
    using Component::addAndMakeVisible;          // открываем ссылочные overload'ы

    SetCCDialog(VSTHostComponent* hostComp,
        CCMapping                           initial,
        const juce::String& slotName,
        std::function<void(CCMapping, bool)> onFinished)
        : DialogWindow(slotName, juce::Colours::darkgrey, true),
        host(hostComp),
        mapping(initial),
        callback(std::move(onFinished))
    {
        //── 1) Заголовок
        titleLabel.setText(slotName, juce::dontSendNotification);
        titleLabel.setFont({ 20.0f, juce::Font::bold });
        titleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(titleLabel);

        //── 2) ComboBox параметров
        buildParamList();            // кэшируем строки
        paramBox.addItemList(paramItems, 1);  // IDs = 1,2,3…
        paramBox.setSelectedId(mapping.paramIndex >= 0 ? mapping.paramIndex + 2 : 1,
            juce::dontSendNotification);
        addAndMakeVisible(paramBox);

        //── 3) Инверсия
        invertToggle.setButtonText("Invert value");
        invertToggle.setToggleState(mapping.invert, juce::dontSendNotification);
        addAndMakeVisible(invertToggle);

        //── 4) CC-уровень
        levelSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        levelSlider.setRange(0, 127, 1);
        levelSlider.setValue(mapping.ccValue);
        levelSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 60, 24);
        levelSlider.setLookAndFeel(&sliderLnf);
        addAndMakeVisible(levelSlider);

        //── 5) Кнопки
        resetButton.setLookAndFeel(&bigIcons);
        okButton.setLookAndFeel(&bigIcons);
        cancelButton.setLookAndFeel(&bigIcons);

        resetButton.setButtonText(juce::String::fromUTF8("❌ Reset"));
        okButton.setButtonText(juce::String::fromUTF8("✔️ OK"));
        cancelButton.setButtonText(juce::String::fromUTF8("✖️ Cancel"));

        resetButton.onClick = [this] { askResetConfirmation(); };
        okButton.onClick = [this] { finish(true);  };
        cancelButton.onClick = [this] { finish(false); };

        addAndMakeVisible(resetButton);
        addAndMakeVisible(okButton);
        addAndMakeVisible(cancelButton);

        //── Окно
        setResizable(false, false);
        centreWithSize(450, 360);
        addToDesktop();
        enterModalState(true, nullptr, true);   // deleteWhenDismissed = true
        setAlwaysOnTop(true);
    }

    ~SetCCDialog() override
    {
        levelSlider.setLookAndFeel(nullptr);
        resetButton.setLookAndFeel(nullptr);
        okButton.setLookAndFeel(nullptr);
        cancelButton.setLookAndFeel(nullptr);
    }

    void closeButtonPressed() override           // ESC или ✕
    {
        finish(false);                          // считается «Cancel»
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(12);

        titleLabel.setBounds(area.removeFromTop(60));
        paramBox.setBounds(area.removeFromTop(50));
        invertToggle.setBounds(area.removeFromTop(45));
        levelSlider.setBounds(area.removeFromTop(80));

        auto btn = area.removeFromBottom(80);
        const int w = btn.getWidth() / 3;

        resetButton.setBounds(btn.removeFromLeft(w).reduced(8));
        okButton.setBounds(btn.removeFromLeft(w).reduced(8));
        cancelButton.setBounds(btn.removeFromLeft(w).reduced(8));
    }

private:
    //──────────────── data
    VSTHostComponent* host;
    CCMapping                               mapping;
    std::function<void(CCMapping, bool)>    callback;

    //──────────────── UI
    juce::Label        titleLabel;
    juce::ComboBox     paramBox;
    juce::ToggleButton invertToggle;
    juce::Slider       levelSlider;
    juce::TextButton   resetButton, okButton, cancelButton;
    BigIcon bigIcons;
    juce::StringArray  paramItems;          // кэш строк для ComboBox

    struct SliderLnf : juce::LookAndFeel_V4
    {
        int getSliderThumbRadius(juce::Slider&) override { return 16; }
    } sliderLnf;

    //──────────────── helpers
    void buildParamList()
    {
        paramItems.clear();
        paramItems.add("<none>");

        if (auto* plug = host->getPluginInstance())
            for (int i = 0; i < plug->getNumParameters(); ++i)
            {
                auto txt = safeGetParamName(plug, i, 128).trim();
                if (txt.isEmpty())
                    txt = "Param " + juce::String(i + 1);

                paramItems.add(txt);
            }
    }

    struct ResetCB : juce::ModalComponentManager::Callback
    {
        explicit ResetCB(SetCCDialog* d) : dlg(d) {}
        void modalStateFinished(int r) override
        {
            if (r == 1) dlg->resetToDefaults();
        }
        SetCCDialog* dlg;
    };

    void askResetConfirmation()
    {
        auto* aw = new juce::AlertWindow("Reset to Defaults",
            "Reset all controls to default values?",
            juce::AlertWindow::WarningIcon);
        aw->addButton("Reset", 1, juce::KeyPress(juce::KeyPress::returnKey));
        aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        aw->enterModalState(true, new ResetCB(this), true);
    }

    void resetToDefaults()
    {
        mapping = {};
        paramBox.setSelectedId(1, juce::dontSendNotification);
        invertToggle.setToggleState(false, juce::dontSendNotification);
        levelSlider.setValue(64);
    }

    void finish(bool okPressed)
    {
        mapping.paramIndex = paramBox.getSelectedId() >= 2
            ? paramBox.getSelectedId() - 2
            : -1;
        mapping.invert = invertToggle.getToggleState();
        mapping.ccValue = static_cast<uint8_t> (levelSlider.getValue());

        if (mapping.paramIndex >= 0)
            mapping.name = safeGetParamName(host->getPluginInstance(),
                mapping.paramIndex, 64);
        else
            mapping.name = "<none>";

        if (callback) callback(mapping, okPressed);
        exitModalState(okPressed ? 1 : 0);   // окно закроется и удалится
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SetCCDialog)
};
