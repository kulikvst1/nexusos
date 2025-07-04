#pragma once

// 1) Сначала — JUCE
#include <JuceHeader.h>

// 2) Системные/стандартные
#include <vector>
#include <string>
#include <functional>  // std::function

// 3) Ваши утилиты и движок
//#include "Grid.h"           // утилитный грид
#include "LooperEngine.h"   // движок лупера

// 4) UI-компоненты, которые на них зависят
#include "LooperComponent.h"
#include "custom_audio_playhead.h"
#include "tap_tempo.h"
#include "vst_host.h"
#include "fount_label.h"
// Предварительное объявление BankEditor
class BankEditor;

class Rig_control : public juce::Component,
    public juce::Button::Listener,
    public juce::Slider::Listener,
    public juce::MidiInputCallback,
    public juce::Timer
{
public:
    Rig_control();
    ~Rig_control() override;

    void resized() override;
    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;
    void timerCallback() override;

    void setVstHostComponent(VSTHostComponent* host) { hostComponent = host; }

    // Новая функция для установки BankEditor
    void setBankEditor(BankEditor* editor);

    // Метод для обновления имен пресетов на кнопках и метках
    void updatePresetDisplays();
  
    // настраиваем обратный вызов при клике по пресету внутри Rig_control
    void setPresetChangeCallback(std::function<void(int)> cb) noexcept
    {
        presetChangeCb = std::move(cb);
    }

    // вызывается извне (из MainContentComponent), когда пресет меняют программно
    void handleExternalPresetChange(int newPresetIndex) noexcept;

    // ── Сеттер для движка лупера ────────────────────────────────
    void setLooperEngine(LooperEngine& eng) noexcept
    {
        enginePtr = &eng;

        // создаём UI лупера один раз, когда движок установлен
        if (!looperComponent)
        {
            looperComponent = std::make_unique<LooperComponent>(*enginePtr);
            addAndMakeVisible(*looperComponent);
            looperComponent->setVisible(false);
        }
    }

private:
    LooperEngine* enginePtr = nullptr;

    std::unique_ptr<juce::Component> mainTab;
    juce::OwnedArray<juce::TextButton> presetButtons;
    juce::Label bankNameLabel;
    std::unique_ptr<juce::TextButton> shiftButton, tempoButton, upButton, downButton;
    std::unique_ptr<juce::Slider> gainSlider, volumeSlider;
    juce::Label gainLabel, volumeLabel;

    TapTempo tapTempo;

    CustomLookAndFeel   custom;
    CustomLookButon   presetLF;
    juce::LookAndFeel_V4 customLF;
    VSTHostComponent* hostComponent = nullptr;

    // Указатель на BankEditor для получения списка имён пресетов
    BankEditor* bankEditor = nullptr;

    // Новые метки для пресетов (отображают альтернативную группу)
    juce::Label presetLabel1_4, presetLabel2_5, presetLabel3_6;

    bool manualShift = false;
    std::function<void(int)> presetChangeCb;

    /////////////////
    std::unique_ptr<juce::Slider>    mixSlider;        
    std::unique_ptr<juce::Slider>    levelSlider;      
    juce::Label                       stateLabel;
    std::unique_ptr<juce::Slider>    progressSlider;
    juce::Label                       currentTimeLabel;
    juce::Label                       totalTimeLabel;

    // ── добавляем для лупера ───────────────────────────────────
    juce::TextButton                  looperBtn{ "Looper" };
    std::unique_ptr<LooperComponent>  looperComponent;
   

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Rig_control)
};
