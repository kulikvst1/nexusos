#pragma once
#include <JuceHeader.h>
#include <vector>
#include <string>
#include <functional>  // std::function
#include "LooperEngine.h"        // движок лупера
#include "LooperComponent.h"
#include "custom_audio_playhead.h"
#include "tap_tempo.h"
#include "vst_host.h"
#include "fount_label.h"
#include "TunerComponent.h"

// Предварительное объявление
class BankEditor;
class Rig_control : public juce::Component,
    public juce::Button::Listener,
    public juce::Slider::Listener,
    public juce::MidiInputCallback,
    public juce::Timer
{
public:
    Rig_control(juce::AudioDeviceManager& adm);
    ~Rig_control() override;
    void resized() override;
    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;
    void sendMidiCC(int channel, int cc, int value);
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;
    void timerCallback() override;
    void setVstHostComponent(VSTHostComponent* host) { hostComponent = host; }
    void setBankEditor(BankEditor* editor);
    void updatePresetDisplays();
    void setPresetChangeCallback(std::function<void(int)> cb) noexcept
    {
        presetChangeCb = std::move(cb);
    }
    void handleExternalPresetChange(int newPresetIndex) noexcept;
    void setLooperEngine(LooperEngine& eng) noexcept;
    // Режимы лупера для отправки в MIDI
    enum class LooperMode { Record, Stop, Play };
    // Методы отправки
    void sendLooperModeToMidi(LooperMode mode); // CC21/22/23
    void sendLooperClearToMidi();               // CC20
    // ── Сеттер внешнего компонента тюнера ───────────────────────
    void setTunerComponent(TunerComponent* t) noexcept;
    /** Коллбек: вызывается при открытии/закрытии панели тюнера */
    std::function<void(bool /*isVisible*/)> onTunerVisibilityChanged;
    /** Возвращает, видима ли панель тюнера внутри Rig_control */
    bool isTunerVisible() const noexcept
    {
        return externalTuner != nullptr && externalTuner->isVisible();
    }
    //
    void setOutControlComponent(OutControlComponent* oc) noexcept;
    void updateSButton(int index, const juce::String& text, juce::Colour colour);
    void updateAllSButtons();
    // Отправка текущего состояния SHIFT в MIDI
    void sendShiftState();
    // Отправка состояния STOMP в MIDI
    void sendStompState();
    void sendBpmToMidi(double bpm);
    // Отправка состояния меню настроек (true = открыто, false = закрыто)
    void sendSettingsMenuState(bool isOpen);
    // Отправка команды смены импеданса
    void sendImpedanceCC(int ccNumber, bool on);

private:
    juce::AudioDeviceManager& deviceManager;
    LooperEngine* enginePtr = nullptr;
    std::unique_ptr<juce::Component>          mainTab;
    juce::OwnedArray<juce::TextButton>        presetButtons;
    juce::Label                               bankNameLabel;
    std::unique_ptr<juce::TextButton>         shiftButton, tempoButton, upButton, downButton;
    std::unique_ptr<juce::Slider>             gainSlider, volumeSlider;
    juce::Label                               gainLabel, volumeLabel;
    TapTempo                                  tapTempo;
    CustomLookAndFeel                         custom;
    CustomLookButon                           presetLF;
    juce::LookAndFeel_V4                      customLF;
    VSTHostComponent* hostComponent = nullptr;
    BankEditor* bankEditor = nullptr;
    juce::Label                               presetLabel1_4, presetLabel2_5, presetLabel3_6;
    bool                                      manualShift = false;
    std::function<void(int)>                  presetChangeCb;
    std::unique_ptr<juce::Slider>             mixSlider, levelSlider;
    juce::Label                               stateLabel;
    std::unique_ptr<juce::Slider>             progressSlider;
    juce::Label                               currentTimeLabel, totalTimeLabel;
    // Looper
    juce::TextButton                          looperBtn{ "Looper" };
    std::unique_ptr<LooperComponent>          looperComponent;
    // Tuner
    juce::TextButton                          tunerBtn{ "Tuner" };
    TunerComponent* externalTuner = nullptr;
    //
    OutControlComponent* outControl = nullptr;
    float                prevVolDb = 0.0f;
    //stomp
    juce::TextButton stompBtn{ "Stomp" };
    bool stompMode = false;
    void updateStompDisplays();
    bool presetShiftState = false; // хранит состояние Shift в пресет-режиме
    //switch control
    std::array<std::unique_ptr<juce::TextButton>, 4> sButtons;
    juce::TextButton* s1Button = nullptr;
    juce::TextButton* s2Button = nullptr;
    juce::TextButton* s3Button = nullptr;
    juce::TextButton* s4Button = nullptr;
    void selectPreset(int index); // единая логика переключения
    void setShiftState(bool on);
    void setStompState(bool on);
    void bankUp();
    void bankDown();
    void setLooperState(bool on);
    void toggleLooper(); // для режима toggle
    void setTunerState(bool on);
    void toggleTuner(); // если захочешь toggle
    std::unique_ptr<juce::MidiOutput> midiOut;
    int lastSentPresetIndex = -1; // -1 = ещё ничего не отправляли
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Rig_control)
};