#pragma once
#include <JuceHeader.h>        // всё из JUCE
#include "vst_host.h"          // реальный хост-класс с getPluginInstance()
#include <functional>          // для std::function
#include <array>               // для CCMapArray, если он тут нужен
class VSTHostComponent;  // forward

// простая структура–модель одного CC-слота
struct CCMapping
{
    int     paramIndex = -1;    // индекс параметра в плагине (-1 = none)
    bool    enabled = false; // включён или нет
    uint8_t ccValue = 64;    // 0…127
};

// Массив из 10 маппингов, по одному на каждую CC-кнопку
using CCMapArray = std::array<CCMapping, 10>;
//==============================================================================
// SetCCDialog — редактирование одного слота CC: выбор параметра, уровень и ON/OFF
//==============================================================================
class SetCCDialog : public juce::DialogWindow
{
public:
    /** @param host       — указатель на VSTHostComponent (плагин-­хост)
        @param initial    — текущее состояние CCMapping (paramIndex, enabled, ccValue)
        @param slotName   — заголовок окна, например "Set CC1"
        @param onFinished — колбэк (mapping, ок/отмена)
    */
    SetCCDialog(VSTHostComponent* host,
        CCMapping                            initial,
        const juce::String& slotName,
        std::function<void(CCMapping, bool)>  onFinished)
        : DialogWindow(slotName, juce::Colours::darkgrey, true),
        host(host),
        mapping(initial),
        callback(std::move(onFinished))
    {
        // 1) ComboBox со списком параметров плагина
        if (auto* plugin = host->getPluginInstance())
        {
            paramBox.addItem("<none>", 1);
            for (int i = 0; i < plugin->getNumParameters(); ++i)
                paramBox.addItem(plugin->getParameterName(i), i + 2);

            if (mapping.paramIndex >= 0 && mapping.paramIndex < plugin->getNumParameters())
                paramBox.setSelectedId(mapping.paramIndex + 2);
            else
                paramBox.setSelectedId(1);
        }
        else
        {
            paramBox.addItem("<none>", 1);
            paramBox.setSelectedId(1);
        }
        addAndMakeVisible(&paramBox);

        // 2) Чекбокс включения/отключения слота
        enableToggle.setButtonText("Enabled");
        enableToggle.setToggleState(mapping.enabled, juce::dontSendNotification);
        addAndMakeVisible(&enableToggle);

        // 3) Слайдер для уровня CC-value
        levelSlider.setRange(0, 127, 1);
        levelSlider.setValue(mapping.ccValue);
        levelSlider.setTextBoxStyle(juce::Slider::TextBoxLeft,
            false, 50, 20);
        addAndMakeVisible(&levelSlider);

        // 4) Кнопки OK / Cancel
        okButton.setButtonText("OK");
        cancelButton.setButtonText("Cancel");
        okButton.onClick = [this] { finish(true);  };
        cancelButton.onClick = [this] { finish(false); };
        addAndMakeVisible(&okButton);
        addAndMakeVisible(&cancelButton);

        setResizable(false, false);
        centreWithSize(400, 200);
        setVisible(true);
    }

    ~SetCCDialog() override = default;

    void resized() override
    {
        auto r = getLocalBounds().reduced(10);
        paramBox.setBounds(r.removeFromTop(30));
        enableToggle.setBounds(r.removeFromTop(25));
        levelSlider.setBounds(r.removeFromTop(50));

        auto btnArea = r.removeFromBottom(30).withTrimmedLeft(r.getWidth() / 4);
        okButton.setBounds(btnArea.removeFromLeft(btnArea.getWidth() / 2).reduced(5));
        cancelButton.setBounds(btnArea.reduced(5));
    }

private:
    VSTHostComponent* host;
    CCMapping                           mapping;
    std::function<void(CCMapping, bool)> callback;

    juce::ComboBox     paramBox;
    juce::ToggleButton enableToggle;
    juce::Slider       levelSlider;
    juce::TextButton   okButton, cancelButton;

    void finish(bool isOk)
    {
        int sel = paramBox.getSelectedId();
        mapping.paramIndex = (sel >= 2 ? sel - 2 : -1);
        mapping.enabled = enableToggle.getToggleState();
        mapping.ccValue = (uint8_t)levelSlider.getValue();

        if (callback) callback(mapping, isOk);
        closeButtonPressed();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SetCCDialog)
};