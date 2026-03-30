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
#include "SerialReader.h"
#include "TunerComponent.h"
#include "RigLookAndFeel.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include "SerialSender.h"

std::unique_ptr<juce::Drawable> loadIcon(const void* data, size_t size);
// Предварительные объявления
class BankEditor;
class OutControlComponent;
class VSTHostComponent;
class StartupShutdown;
class SerialStartupShutdown;
class InputControlComponent;
class LibraryMode;    // внешний модуль логики библиотеки
class StompMode;      // внешний модуль логики стомпа
extern int packetDelayUs; // глобальная переменная

enum class VolumeMode
{
    Link,
    Left,
    Right
};

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
    void timerCallback() override;
    
    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;
    // Взаимодействие с внешними компонентами
    void setVstHostComponent(VSTHostComponent* host) { hostComponent = host; }
    void setBankEditor(BankEditor* editor);
    void setLooperEngine(LooperEngine& eng) noexcept;
    // Обновления UI
    void updatePresetDisplays();
    void setCurrentBpm(double bpm);
    void updateTapButton(double bpm);
    void updateAllSButtons();
    void updateSButton(int index, const juce::String& text, juce::Colour baseColour = juce::Colours::transparentBlack, juce::Colour textColour = juce::Colours::black);
    void updateActiveLibraryLabel();
    void updatePluginLabel();
    // Preset API
    void setPresetChangeCallback(std::function<void(int)> cb) noexcept { presetChangeCb = std::move(cb); }
    void handleExternalPresetChange(int newPresetIndex) noexcept;
    void updateVolumeLabel();
    // +++++++++++++++++++++ OutControl ++++++++++++++++++++++++
    void setOutControlComponent(OutControlComponent* oc) noexcept;
    void setVolumeMode(VolumeMode mode);
    void linkButtonClicked();
    //+++++++++++++++++++++ InControl ++++++++++++++++++++++++++
    void setInputControlComponent(InputControlComponent* ic) noexcept;

    // +++++++++++++++++ LOOPER CONTROL ++++++++++++++++++++++++
    std::function<void(bool)> onLooperButtonChanged;
    enum class LooperMode { Record, Stop, Play, TriggerActive };
    void setLooperState(bool on);
    void controlLooperState(bool on);
    bool getLooperState() const noexcept;
    void syncLooperStateToMidi();
    void setContinueClicks(bool enabled);
    //++++++++++++++++++++ TUNER CONTROL +++++++++++++++++++++
    void setTunerState(bool on);
    void setTunerComponent(TunerComponent* t) noexcept;
    std::function<void(bool /*isVisible*/)> onTunerVisibilityChanged;
    bool isTunerVisible() const noexcept { return externalTuner != nullptr && externalTuner->isVisible(); }

    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    bool isTriggeredFromRig() const { return triggeredFromRig; }

    //+++++++++++++++ Library sync ++++++++++++++++
    void syncLibraryToLoadedFile();
    void syncLibraryToLoadedFile(bool resetOffset);

    //++++++++++++++++++++++++++
    void setStompState(bool on);

    //++++++++++++++ Pedal control +++++++++++++++++++
    enum class PedalMode { None, SW1, SW2 };
    PedalMode currentPedalMode = PedalMode::None;
    void handlePedalConnected();
    void handlePedalDisconnected();
    void handlePedalSwitch1(bool isOn);
    void handlePedalSwitch2(bool isOn);
    void handlePedalAxis(int slot, int cc, int value);
    void handlePedalMin(int value);
    void handlePedalMax(int value);
    void handlePedalAutoConfig(bool isOn);
    void handlePedalThreshold(int value);
    void handlePedalSwitch(int switchIndex, bool isOn);
    // +++++++++++++ Library Mode API +++++++++++++++++
    void prevLibraryBlock();
    void nextLibraryBlock();
    void toggleLibraryMode();
    void updateLibraryDisplays();
    void updateBankSelector();
    void selectLibraryFile(int idx);
   
    //++++++++++++++ MIDI IN ++++++++++++++++++++++++
    void processMidiInput(int channel, int cc, int value, bool isOn, const juce::MidiMessage& msgCopy);
    void handleIncomingMidiMessage(juce::MidiInput* source,
    const juce::MidiMessage& message) override;

    //++++++++++++++ MIDI OUT +++++++++++++++++++++++++
    void sendLibraryState(int presetIndex, bool active);
    void sendLibraryMode(bool active);
    void sendStompButtonState(int ccIndex, bool state);
    void sendPresetChange(int active);
    void sendSButtonState(int ccNumber, bool pressed);
    void sendVolumeToController(int midiValue);
    void sendTunerMidi(bool on);
    void sendMidiCC(int channel, int cc, int value);
    void sendShiftState();
    void sendStompState();
    void sendBpmToMidi(double bpm);
    void sendSettingsMenuState(bool isOpen);
    void sendImpedanceCC(int ccNumber, bool on);
    // Pedal 
    void sendPedalSwitchState(int switchIndex, bool isOn);   // SW1 / SW2
    void sendPedalMin(int value);
    void sendPedalMax(int value);
    void sendPedalThreshold(int value);
    void sendPedalAutoConfig(bool isOn);
    void sendPedalConfigToMidi();
    // LOPER
    void sendLooperModeToMidi(LooperMode mode);         
    void sendLooperClearToMidi();                       
    void sendPlayerModeToMidi(bool isPlaying);          
    void sendLooperModeToMidi(LooperEngine::State state);
    void sendLooperToggle(bool on);
   void sendLooperTriggerActive();
    //
    std::function<void()> onMidiActivity;
    void sendAck(uint8_t chan, uint8_t cmd, uint16_t arg, uint8_t seq);
    void startAsyncRead();
    void sendPresetChangeSerial(int channel, int cmd, int value);
    uint8_t nextSeq();
    uint8_t calcChecksum(uint8_t chan, uint8_t cmd, uint16_t arg, uint8_t seq);
    void connectToSerialPort(const juce::String& portName);
    void processControlMessage(int CHAN, int CMD, int ARG);
    std::unique_ptr<SerialSender> sender;
    void sendBpmToSerial(double bpm);
private:
    // --- Друзья класса ---
    friend class LibraryMode; // доступ модулю библиотеки к приватным полям
    friend class StompMode;

    // --- Внутренние режимы и служебные методы ---
    void selectPreset(int index);
    void setShiftState(bool on);
    void bankUp();
    void bankDown();
    void toggleLooper();
    void toggleTuner();

    // --- Состояния ---
    bool pedalOn = false;
    bool manualShift = false;
    bool triggeredFromRig = false;
    bool libraryModeActive = false;
    bool shift = false;
    int  lastSentPresetIndex = -1;
    int  lastActivePresetIndex = -1;
    float prevVolDb = 0.0f;

    // --- LookAndFeel ---
    BankNameKomboBox                BankName;
    CustomLookPresetButtons         presetLP;
  
    CustomLookAndFeelA              custom;

    // --- Внешние зависимости и состояние ---
    juce::AudioDeviceManager& deviceManager;
    LooperEngine* enginePtr = nullptr;
    VSTHostComponent* hostComponent = nullptr;
    BankEditor* bankEditor = nullptr;
    OutControlComponent* outControl = nullptr;
    TunerComponent* externalTuner = nullptr;
    InputControlComponent* inputControl = nullptr;
    std::function<void(int)>        presetChangeCb;
    std::unique_ptr<LibraryMode>    libraryLogic;
    std::unique_ptr<StompMode>      stompModeController;

    // --- UI-компоненты ---
    std::unique_ptr<juce::Component> mainTab;

    // Пресет-кнопки
    juce::OwnedArray<juce::TextButton> presetButtons;
    juce::Label                        presetLabel1_4, presetLabel2_5, presetLabel3_6;

    // Выбор банка
    std::unique_ptr<juce::ComboBox>    bankSelector;
    juce::Label                        bankNameLabel;

    // Метки названия библиотеки,названия плагина 
    juce::Label activeLibraryLabel;
    juce::Label loadedPluginLabel;

    // Микс/левел
    std::unique_ptr<juce::Slider>      volumeSlider;
    juce::Label                        volumeLabel;
    std::unique_ptr<juce::Slider>      mixSlider, levelSlider;

    // Плеер/статусы
    std::unique_ptr<juce::Slider>      progressSlider;
    juce::Label                        currentTimeLabel, totalTimeLabel;
    juce::Label                        stateLabel;
    std::unique_ptr<LooperComponent>   looperComponent;

    // S-кнопки
    std::array<std::unique_ptr<juce::TextButton>, 4> sButtons;
    juce::TextButton* s1Button = nullptr;
    juce::TextButton* s2Button = nullptr;
    juce::TextButton* s3Button = nullptr;
    juce::TextButton* s4Button = nullptr;
    // иконки
    std::unique_ptr<juce::Drawable> iconFS;


    // --- MIDI ---
    std::unique_ptr<juce::MidiOutput>      midiOut;
    std::unique_ptr<StartupShutdown>   Start;
   
    // --- Вспомогательное ---
    TapTempo                               tapTempo;
    juce::Label                            pedalModeLabel;

    // Клипы
    juce::Label                            clipLedL, clipLedR;
    juce::Label                            inClipLedL, inClipLedR;
    double peakHoldTsL = 0.0;
    double peakHoldTsR = 0.0;
    // удерживаемые пики (в dB)
    float peakDbOutL = -100.0f;
    float peakDbOutR = -100.0f;
    float peakDbInL = -100.0f;
    float peakDbInR = -100.0f;
    double currentBpm{ 120.0 }; // значение по умолчанию
    //метки для каналов 
    juce::Label volumeLabelL;
    juce::Label volumeLabelR;
    VolumeMode currentVolumeMode = VolumeMode::Left;
    bool looperControlActive = true;
    
    // Serial
    std::unique_ptr<SerialReader> serialPort;
    std::vector<uint8_t> serialReadBuffer;
    void handleSerialFrame(const std::vector<uint8_t>& frame);
    std::vector<uint8_t> rxBuffer;
    uint8_t seqCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Rig_control)

};
