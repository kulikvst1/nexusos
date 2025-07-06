//==============================================================================
//  vst_host.h
//==============================================================================

#pragma once

#include <JuceHeader.h>
#include "PluginManager.h"
#include "LooperEngine.h"
#include "plugin_process_callback.h"  // Audio I/O callback
#include "TunerComponent.h"           // UI tuner component

class PluginProcessCallback;
class CustomAudioPlayHead;

class VSTHostComponent : public juce::Component,
    private juce::Button::Listener,
    private juce::AudioProcessorParameter::Listener,
    private juce::ComponentListener,
    private juce::ChangeListener
{
public:
    VSTHostComponent();
    explicit VSTHostComponent(juce::AudioDeviceManager&);
    ~VSTHostComponent() noexcept override;

    //==============================================================================
    // Plugin load/unload
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
    const std::vector<juce::File>& getPluginFiles()     const noexcept { return pluginFiles; }
    int    getPluginParametersCount() const noexcept;
    double getLastPluginCpuLoad()      const noexcept { return lastCpuLoad; }

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
    // AudioDeviceManager access
    static juce::AudioDeviceManager& getDefaultAudioDeviceManager();
    juce::AudioDeviceManager& getAudioDeviceManagerRef() noexcept;
    PluginManager& getPluginManager() noexcept { return pluginManager; }

    //==============================================================================
    // LOOPER
    void setLooperEngine(LooperEngine* engine) noexcept;

    //==============================================================================
     /** Инжектирует UI-тюнер в аудио-колл-бэк */
    void setTuner(TunerComponent* t) noexcept;
private:
    //==========================================================================
    // Internal helpers
    void initialiseComponent();
    void updatePresetColours();
    void updateLearnColours();
    void layoutPluginEditor(juce::Rectangle<int> area);
    void clampEditorBounds();

    //==========================================================================
    // Button and parameter callbacks
    void buttonClicked(juce::Button*) override;
    void parameterValueChanged(int, float) override;
    void parameterGestureChanged(int, bool) override {}

    //==========================================================================
    // Preset / learn buttons
    static constexpr int kNumPresets = 6;
    static constexpr int kNumLearn = 10;
    juce::TextButton presetBtn[kNumPresets];
    juce::TextButton learnBtn[kNumLearn];

    //==========================================================================
    // State
    int    activePreset = 0;
    double currentSampleRate = 44100.0;
    int    currentBlockSize = 512;
    double lastCpuLoad = 0.0;

    //==========================================================================
    // Audio & plugin processing
    juce::AudioDeviceManager& deviceMgr;
    std::unique_ptr<juce::AudioPluginInstance> pluginInstance;
    std::unique_ptr<PluginProcessCallback>     processCb;
    std::unique_ptr<CustomAudioPlayHead>       playHead;
    std::unique_ptr<juce::Component>           pluginEditor;
    juce::Label* bpmLabel = nullptr;
    bool                                       audioCbAdded = false;

    //==========================================================================
    // Central plugin manager and file list
    PluginManager                              pluginManager;
    std::vector<juce::File>                    pluginFiles;

    //==========================================================================
    // BankEditor callbacks
    ParamChangeFn paramChangeCb;
    PresetCb      presetCb;
    LearnCb       learnCb;

    //==========================================================================
    // ChangeListener from PluginManager
    void changeListenerCallback(juce::ChangeBroadcaster*) override;

    //==========================================================================
    // Format manager for plugins
    juce::AudioPluginFormatManager formatManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VSTHostComponent)
};
