#pragma once
#include <JuceHeader.h>
#include <vector>
#include <string>
#include <functional>
#include "LooperEngine.h"
#include "LooperComponent.h"
#include "custom_audio_playhead.h"
#include "tap_tempo.h"
#include "vst_host.h"
#include "fount_label.h"
#include "TunerComponent.h"
#include "RigLookAndFeel.h"

// Предварительные объявления
class BankEditor;
class OutControlComponent;
class VSTHostComponent;
class MidiStartupShutdown;
class InputControlComponent;
class LibraryMode; // внешний модуль логики библиотеки

class Rig_control : public juce::Component,
    public juce::Button::Listener,
    public juce::Slider::Listener,
    public juce::MidiInputCallback,
    public juce::Timer
{
public:
    Rig_control(juce::AudioDeviceManager& adm);
    ~Rig_control() override;

    // JUCE
    void resized() override;
    void timerCallback() override;
    void handleIncomingMidiMessage(juce::MidiInput* source,
        const juce::MidiMessage& message) override;

    // UI callbacks
    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;

    // Взаимодействие с внешними компонентами
    void setVstHostComponent(VSTHostComponent* host) { hostComponent = host; }
    void setBankEditor(BankEditor* editor);
    void setLooperEngine(LooperEngine& eng) noexcept;

    // Обновления UI
    void updatePresetDisplays();
    void updateAllSButtons();
    void updateSButton(int index, const juce::String& text, juce::Colour colour);

    // Preset API
    void setPresetChangeCallback(std::function<void(int)> cb) noexcept { presetChangeCb = std::move(cb); }
    void handleExternalPresetChange(int newPresetIndex) noexcept;

    // Looper player
    enum class LooperMode { Record, Stop, Play };
    void sendLooperModeToMidi(LooperMode mode);          // CC21/22/23
    void sendLooperClearToMidi();                        // CC20
    void sendPlayerModeToMidi(bool isPlaying);           // CC для плеера
    void sendLooperModeToMidi(LooperEngine::State state);

    // Tuner
    void setTunerComponent(TunerComponent* t) noexcept;
    std::function<void(bool /*isVisible*/)> onTunerVisibilityChanged;
    bool isTunerVisible() const noexcept { return externalTuner != nullptr && externalTuner->isVisible(); }

    // OutControl
    void setOutControlComponent(OutControlComponent* oc) noexcept;

    // MIDI / CC
    void sendMidiCC(int channel, int cc, int value);
    void sendShiftState();
    void sendStompState();
    void sendBpmToMidi(double bpm);
    void sendSettingsMenuState(bool isOpen);
    void sendImpedanceCC(int ccNumber, bool on);

    void setInputControlComponent(InputControlComponent* ic) noexcept;

    enum class PedalMode { None, SW1, SW2 };
    PedalMode currentPedalMode = PedalMode::None;

    // Управление Looper-кнопкой
    std::function<void(bool)> onLooperButtonChanged;
    void setLooperState(bool on);
    void setLooperButtonState(bool state)
    {
        looperBtn.setToggleState(state, juce::sendNotification);
    }
    void controlLooperState(bool on)
    {
        setLooperState(on);
    }
    bool getLooperButtonState() const noexcept
    {
        return looperBtn.getToggleState();
    }
    void syncLooperStateToMidi()
    {
        if (enginePtr)
            sendLooperModeToMidi(enginePtr->getState());
    }
    void setTunerState(bool on);
    void sendTunerMidi(bool on);
    bool isTriggeredFromRig() const { return triggeredFromRig; }

    void syncLibraryToLoadedFile();
    void syncLibraryToLoadedFile(bool resetOffset);
    void sendLibraryState(int presetIndex, bool active);

private:
    friend class LibraryMode; // даём доступ модулю библиотеки к приватным полям

    // Внутренние режимы и служебные методы
    void selectPreset(int index);
    void setShiftState(bool on);
    void setStompState(bool on);
    void bankUp();
    void bankDown();
    void toggleLooper();
    void toggleTuner();
    void updateStompDisplays();

    bool pedalOn = false;

    // 1) LookAndFeel
    BankNameKomboBox                BankName;
    CustomLookPresetButtons         presetLP;
    CustomLookSWButtons             SWbutoon;
    CustomLookAndFeelA              custom;

    // 2) Внешние зависимости и состояние
    juce::AudioDeviceManager& deviceManager;
    LooperEngine* enginePtr = nullptr;
    VSTHostComponent* hostComponent = nullptr;
    BankEditor* bankEditor = nullptr;
    OutControlComponent* outControl = nullptr;
    TunerComponent* externalTuner = nullptr;
    std::function<void(int)>        presetChangeCb;

    // 3) Компоненты UI
    std::unique_ptr<juce::Component> mainTab;

    // Общие пресет-кнопки (используются во всех режимах)
    juce::OwnedArray<juce::TextButton> presetButtons;
    juce::Label                        presetLabel1_4, presetLabel2_5, presetLabel3_6;

    // Выбор банка
    std::unique_ptr<juce::ComboBox>    bankSelector;
    juce::Label                        bankNameLabel;

    // Невидимые универсальные кнопки
    std::unique_ptr<juce::TextButton>  shiftButton, tempoButton, upButton, downButton;

    // Микс/левел
    std::unique_ptr<juce::Slider>      volumeSlider;
    juce::Label                        volumeLabel;
    std::unique_ptr<juce::Slider>      mixSlider, levelSlider;

    // Плеер/статусы
    std::unique_ptr<juce::Slider>      progressSlider;
    juce::Label                        currentTimeLabel, totalTimeLabel;
    juce::Label                        stateLabel;

    // Looper
    juce::TextButton                   looperBtn{ "Looper" };
    std::unique_ptr<LooperComponent>   looperComponent;

    // Tuner
    juce::TextButton                   tunerBtn{ "Tuner" };

    // Stomp
    juce::TextButton                   stompBtn{ "Stomp" };

    // Switch control кнопки (S1..S4)
    std::array<std::unique_ptr<juce::TextButton>, 4> sButtons;
    juce::TextButton* s1Button = nullptr;
    juce::TextButton* s2Button = nullptr;
    juce::TextButton* s3Button = nullptr;
    juce::TextButton* s4Button = nullptr;

    // MIDI
    InputControlComponent* inputControl = nullptr;
    std::unique_ptr<juce::MidiOutput>  midiOut;
    std::unique_ptr<MidiStartupShutdown> midiInit;
    int                                lastSentPresetIndex = -1;

    // Состояния
    bool                               manualShift = false;
    bool                               stompMode = false;
    bool                               presetShiftState = false;
    float                              prevVolDb = 0.0f;

    // Вспомогательное
    TapTempo                           tapTempo;
    juce::Label                        pedalModeLabel;

    // Клипы
    juce::Label                        clipLedL, clipLedR;
    juce::Label                        inClipLedL, inClipLedR;

    
    // Делегат внешнего модуля библиотеки
    std::unique_ptr<LibraryMode>       libraryLogic;

  
    // Library Mode API
    void prevLibraryBlock();
    void nextLibraryBlock();
    void toggleLibraryMode();
    void updateLibraryDisplays();
    void updateBankSelector();
    void selectLibraryFile(int idx);
    bool triggeredFromRig = false;
    bool libraryModeActive = false;
    void sendLibraryMode(bool active);
    int lastActivePresetIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Rig_control)
};
