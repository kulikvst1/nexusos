#pragma once
#include <JuceHeader.h>
#include "PluginManager.h"
#include "LooperEngine.h"
#include "plugin_process_callback.h"
#include "TunerComponent.h"
#include "InputControlComponent.h" 

class PluginProcessCallback;
class CustomAudioPlayHead;

class VSTHostComponent : public juce::Component,
    private juce::Button::Listener,
    private juce::AudioProcessorParameter::Listener,
    private juce::ComponentListener,
    private juce::ChangeListener
{
public:
    //==============================================================================
    explicit VSTHostComponent(juce::AudioDeviceManager& deviceManager);
    ~VSTHostComponent() noexcept override;

    //==============================================================================
    // Plugin load / unload
    void loadPlugin(const juce::File& file,
        double            sampleRate = 44100.0,
        int               blockSize = 512);
    void unloadPlugin();

    //==============================================================================
    // GUI helpers for BankEditor
    void setExternalPresetIndex(int idx) noexcept;
    void setExternalLearnState(int cc, bool on) noexcept;
    void setBpmDisplayLabel(juce::Label* label) noexcept;

    //==============================================================================
    // Callbacks from BankEditor
    using ParamChangeFn = std::function<void(int, float)>;
    using PresetCb = std::function<void(int)>;
    using LearnCb = std::function<void(int, bool)>;

    void setParameterChangeCallback(ParamChangeFn cb) noexcept { paramChangeCb = std::move(cb); }
    void setPresetCallback(PresetCb     cb) noexcept { presetCb = std::move(cb); }
    void setLearnCallback(LearnCb      cb) noexcept { learnCb = std::move(cb); }

    //==============================================================================
    // Audio settings
    void setAudioSettings(double sampleRate, int blockSize) noexcept;
    void setPluginParameter(int ccNumber, int ccValue) noexcept;

    //==============================================================================
    // Information getters
    juce::AudioPluginInstance* getPluginInstance()       noexcept { return pluginInstance.get(); }
    const juce::AudioPluginInstance* getPluginInstance() const noexcept { return pluginInstance.get(); }
    const std::vector<juce::File>& getPluginFiles()    const noexcept { return pluginFiles; }
    int                               getPluginParametersCount() const noexcept;
    double                            getLastPluginCpuLoad()      const noexcept { return lastCpuLoad; }

    //==============================================================================
    // Component overrides
    void paint(juce::Graphics&) override;
    void resized() override;
    void componentMovedOrResized(juce::Component&, bool, bool) override;

    //==============================================================================
    // BPM / CPU load
    void resetBPM() { updateBPM(120.0); }
    void updateBPM(double newBPM);
    void updatePluginCpuLoad(double load) noexcept;

    //==============================================================================
    //==============================================================================
// AudioDeviceManager control
//==============================================================================

    /** Единственный AudioDeviceManager для всего приложения */
    static juce::AudioDeviceManager& getDefaultAudioDeviceManager();

    // Запускаем поток и подписываемся
    void startAudio()
    {
        deviceMgr.addChangeListener(this);
        deviceMgr.addAudioCallback(processCb.get());
    }

    // Останавливаем поток и отписываемся
    void stopAudio()
    {
        deviceMgr.removeAudioCallback(processCb.get()); // 1) Снять коллбек первым
        deviceMgr.removeChangeListener(this);           // 2) Отписаться от изменений
        // Важно: закрыть девайс либо здесь, либо сразу после stopAudio() в деструкторе/shutdown
        // Я рекомендую закрывать здесь, чтобы инвариант был один:
        deviceMgr.closeAudioDevice();                   // 3) Остановить device thread
    }

    juce::AudioDeviceManager& getAudioDeviceManagerRef() noexcept { return deviceMgr; }

    //==============================================================================
    // LOOPER
    void setLooperEngine(LooperEngine* engine) noexcept;

    //==============================================================================
    // Inject UI tuner into audio callback
    void addTuner(TunerComponent* t) noexcept;
    void removeTuner(TunerComponent* t) noexcept;

    //==============================================================================
    // PluginManager access (non-static)
    PluginManager& getPluginManager()       noexcept { return pluginManager; }
    const PluginManager& getPluginManager() const noexcept { return pluginManager; }

    // 1) Объявляем сеттер для InputControlComponent
    void setInputControlComponent(InputControlComponent* ic) noexcept
    {
        inputControl = ic;
        if (processCb)  // пробрасываем в аудио-колбэк
            processCb->setInputControl(ic);
    }

    // 1) Объявляем сеттер для OutControlComponent
    void setOutControlComponent(OutControlComponent* oc) noexcept;


private:
    //==============================================================================
    // ** Удалили **
    // static juce::AudioDeviceManager defaultAudioDeviceManager;
    // static PluginManager           defaultPluginManager;

    // Internal helpers
    void initialiseComponent();
    void updatePresetColours();
    void updateLearnColours();
    void layoutPluginEditor(juce::Rectangle<int> area);
    void clampEditorBounds();

    // Button & parameter callbacks
    void buttonClicked(juce::Button*) override;
    void parameterValueChanged(int, float) override;
    void parameterGestureChanged(int, bool) override {}

    // Preset / learn buttons
    static constexpr int kNumPresets = 6;
    static constexpr int kNumLearn = 10;
    juce::TextButton presetBtn[kNumPresets];
    juce::TextButton learnBtn[kNumLearn];

    // State
    int    activePreset = 0;
    double currentSampleRate = 44100.0;
    int    currentBlockSize = 512;
    double lastCpuLoad = 0.0;

    // Audio & plugin processing
    juce::AudioDeviceManager& deviceMgr;
    std::unique_ptr<juce::AudioPluginInstance> pluginInstance;
    std::unique_ptr<PluginProcessCallback>     processCb;
    std::unique_ptr<CustomAudioPlayHead>       playHead;
    std::unique_ptr<juce::Component>           pluginEditor;
    juce::Label* bpmLabel = nullptr;

    // Plugin manager and file list
    PluginManager                   pluginManager;
    std::vector<juce::File>         pluginFiles;

    // BankEditor callbacks
    ParamChangeFn                   paramChangeCb;
    PresetCb                        presetCb;
    LearnCb                         learnCb;

    // ChangeListener from PluginManager
    void changeListenerCallback(juce::ChangeBroadcaster*) override;

    // Format manager for plugins
    juce::AudioPluginFormatManager  formatManager;

    // Список всех активных тюнеров
    //std::vector<TunerComponent*>    tuners;
    InputControlComponent* inputControl = nullptr;
    OutControlComponent* outControl = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VSTHostComponent)
};
