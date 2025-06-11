#pragma once

#include <JuceHeader.h>
#include "vst_host.h"
#include <functional>
#include <array>

// форвард
class VSTHostComponent;

// модель одного CC-слота
struct CCMapping
{
    int           paramIndex = -1;
    bool          enabled = false;   // флаг живёт здесь, но UI для него убран
    uint8_t       ccValue = 64;
    bool          invert = false;
    juce::String  name;
};

//==============================================================================
// Диалог редактирования одного CC-слота.
//==============================================================================
class SetCCDialog : public juce::DialogWindow
{
public:
    SetCCDialog(VSTHostComponent* hostComp,
        CCMapping          initial,
        const juce::String& slotName,
        std::function<void(CCMapping, bool)> onFinished)
        : juce::DialogWindow(slotName, juce::Colours::darkgrey, true),
        host(hostComp),
        mapping(initial),
        callback(std::move(onFinished))
    {
        // 1) Заголовок
        titleLabel.setText(slotName, juce::dontSendNotification);
        titleLabel.setFont({ 20.0f, juce::Font::bold });
        titleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(titleLabel);

        // 2) ComboBox с параметрами
        paramBox.addItem("<none>", 1);
        if (auto* p = host->getPluginInstance())
            for (int i = 0; i < p->getNumParameters(); ++i)
                paramBox.addItem(p->getParameterName(i), i + 2);

        paramBox.setSelectedId(mapping.paramIndex >= 0
            ? mapping.paramIndex + 2
            : 1,
            juce::dontSendNotification);
        addAndMakeVisible(paramBox);

        // 3) Инверсия
        invertToggle.setButtonText("Invert");
        invertToggle.setToggleState(mapping.invert, juce::dontSendNotification);
        addAndMakeVisible(invertToggle);

        // 4) CC-уровень (слайдер)
        levelSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        levelSlider.setRange(0, 127, 1);
        levelSlider.setValue(mapping.ccValue);
        levelSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 60, 30);
        levelSlider.setLookAndFeel(&sliderLnf);
        addAndMakeVisible(levelSlider);

        // 5) Кнопки Reset / OK / Cancel
        resetButton.setButtonText("Reset");
        resetButton.onClick = [this] { askResetConfirmation(); };
        addAndMakeVisible(resetButton);

        okButton.setButtonText("OK");
        okButton.onClick = [this] { finish(true); };
        addAndMakeVisible(okButton);

        cancelButton.setButtonText("Cancel");
        cancelButton.onClick = [this] { finish(false); };
        addAndMakeVisible(cancelButton);

        // финальные параметры окна
        setResizable(false, false);
        centreWithSize(450, 360);
        addToDesktop();
        enterModalState(true, nullptr);
    }

    ~SetCCDialog() override
    {
        levelSlider.setLookAndFeel(nullptr);
    }

    void closeButtonPressed() override
    {
        juce::DialogWindow::closeButtonPressed();
        delete this;
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(12);
        titleLabel.setBounds(area.removeFromTop(60));
        paramBox.setBounds(area.removeFromTop(50));
        invertToggle.setBounds(area.removeFromTop(45));
        levelSlider.setBounds(area.removeFromTop(80));

        auto btnArea = area.removeFromBottom(80);
        int  w = btnArea.getWidth() / 3;
        resetButton.setBounds(btnArea.removeFromLeft(w).reduced(8));
        okButton.setBounds(btnArea.removeFromLeft(w).reduced(8));
        cancelButton.setBounds(btnArea.removeFromLeft(w).reduced(8));
    }

private:
    VSTHostComponent* host;
    CCMapping                           mapping;
    std::function<void(CCMapping, bool)> callback;

    juce::Label       titleLabel;
    juce::ComboBox    paramBox;
    juce::ToggleButton invertToggle;    // ← единственный toggle-элемент теперь
    juce::Slider      levelSlider;
    juce::TextButton  resetButton, okButton, cancelButton;

    struct SliderLnf : juce::LookAndFeel_V4
    {
        int getSliderThumbRadius(juce::Slider&) override { return 16; }
    } sliderLnf;

    // Для подтверждения сброса
    struct ResetConfirmCallback : juce::ModalComponentManager::Callback
    {
        explicit ResetConfirmCallback(SetCCDialog* d) noexcept : dialog(d) {}
        void modalStateFinished(int resultCode) override
        {
            if (resultCode == 1)
                dialog->resetToDefaults();
        }
        SetCCDialog* dialog;
    };

    void askResetConfirmation()
    {
        auto* aw = new juce::AlertWindow(
            "Reset to Defaults",
            "Are you sure you want to reset all controls to default values?",
            juce::AlertWindow::WarningIcon);

        aw->addButton("Reset", 1, juce::KeyPress(juce::KeyPress::returnKey));
        aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        aw->enterModalState(true,
            new ResetConfirmCallback(this),
            true);
    }

    void resetToDefaults()
    {
        mapping.paramIndex = -1;
        // mapping.enabled = false; // флаг остаётся false, но UI для него больше нет
        mapping.invert = false;
        mapping.ccValue = 64;
        mapping.name = "<none>";

        paramBox.setSelectedId(1, juce::dontSendNotification);
        invertToggle.setToggleState(false, juce::dontSendNotification);
        levelSlider.setValue(mapping.ccValue);
    }

    void finish(bool okPressed)
    {
        auto sel = paramBox.getSelectedId();
        mapping.paramIndex = (sel >= 2 ? sel - 2 : -1);
        // mapping.enabled  // не меняем здесь, т.к. UI удалён
        mapping.invert = invertToggle.getToggleState();
        mapping.ccValue = static_cast<uint8_t>(levelSlider.getValue());

        if (mapping.paramIndex >= 0 && host->getPluginInstance() != nullptr)
            mapping.name = host->getPluginInstance()->getParameterName(mapping.paramIndex);
        else
            mapping.name = "<none>";

        if (callback)
            callback(mapping, okPressed);

        closeButtonPressed();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SetCCDialog)
};
