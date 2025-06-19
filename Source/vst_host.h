#pragma once
#include <JuceHeader.h>
#include <functional>
#include <memory>
//#include "plugin_process_callback.h" 

//--- Forward declarations (чтобы .h не тянул тяжёлые .h) ───────────────
class PluginProcessCallback;
class CustomAudioPlayHead;

//--- Хост-компонент ──────────────────────────────────────────────────────
class VSTHostComponent : public juce::Component,
    private juce::Button::Listener,
    private juce::AudioProcessorParameter::Listener
{
public:
    VSTHostComponent();
    explicit VSTHostComponent(juce::AudioDeviceManager&);
    ~VSTHostComponent() override;

    //–– загрузка / выгрузка плагина ––------------------------------------
    void loadPlugin(const juce::File& file,
        double sampleRate = 44100,
        int    blockSize = 512);
    void unloadPlugin();

    //–– GUI-helpers для BankEditor ––-------------------------------------
    void setExternalPresetIndex(int idx);
    void setExternalLearnState(int cc, bool on);

    using ParamChangeFn = std::function<void(int, float)>;
    using PresetCb = std::function<void(int)>;
    using LearnCb = std::function<void(int, bool)>;

    void setParameterChangeCallback(ParamChangeFn cb) noexcept { paramChangeCb = std::move(cb); }
    void setPresetCallback(PresetCb      cb) noexcept { presetCb = std::move(cb); }
    void setLearnCallback(LearnCb       cb) noexcept { learnCb = std::move(cb); }

    void setAudioSettings(double sampleRate, int blockSize) noexcept;
    void setPluginParameter(int ccNumber, int ccValue)        noexcept;
    void setBpmDisplayLabel(juce::Label* label)               noexcept;

    //–– геттеры ––--------------------------------------------------------
    juce::AudioPluginInstance* getPluginInstance()       noexcept { return plugin.get(); }
    const juce::AudioPluginInstance* getPluginInstance() const noexcept { return plugin.get(); }
    const juce::Array<juce::File>& getPluginFiles() const noexcept;
    int  getPluginParametersCount()  const noexcept;

    //-- juce::Component ---------------------------------------------------
    void paint(juce::Graphics&) override;
    void resized() override;

    juce::AudioDeviceManager& getAudioDeviceManagerRef() noexcept;
    double getLastPluginCpuLoad() const noexcept;

    //  BPM-лейбл и управление
    void resetBPM() { updateBPM(120.0); }
    void updateBPM(double newBPM);

    // CPU LOAD
    void updatePluginCpuLoad(double);
       
private:
    /* ===== КЕШ + ФОРМАТЫ ===== */
    juce::AudioPluginFormatManager               formatManager;     // ← единственный экземпляр
    juce::KnownPluginList                        pluginList;
    std::unique_ptr<juce::ApplicationProperties> appProps;
    static constexpr const char* kPluginCacheFile = "PluginCache.xml";

    juce::Array<juce::File> pluginFiles;
    void scanForPlugins();

    /* ===== UI-и вспомогательные ===== */
    void initialiseComponent();               // вызывается из ctor
    void updatePresetColours();
    void updateLearnColours();
    void layoutPluginEditor(juce::Rectangle<int>);

    //-- juce callbacks ----------------------------------------------------
    void buttonClicked(juce::Button*) override;
    void parameterValueChanged(int, float)    override;
    void parameterGestureChanged(int, bool)     override {}

    /* ===== константы ===== */
    static constexpr int kNumPresets = 6;
    static constexpr int kNumLearn = 10;

    /* ===== runtime-данные ===== */
    juce::AudioDeviceManager& deviceMgr;
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    std::unique_ptr<PluginProcessCallback>      processCb;
    std::unique_ptr<CustomAudioPlayHead>        playHead;

    juce::Component* pluginEditor = nullptr;
    juce::Label* bpmLabel = nullptr;
    juce::TextButton  presetBtn[kNumPresets];
    juce::TextButton  learnBtn[kNumLearn];

    int    activePreset = 0;
    double currentSampleRate = 44100;
    int    currentBlockSize = 512;
    double lastCpuLoad = 0.0;

    /* ===== callbacks ===== */
    ParamChangeFn paramChangeCb;
    PresetCb      presetCb;
    LearnCb       learnCb;

    /* ===== static helpers ===== */
    static juce::AudioDeviceManager& getDefaultAudioDeviceManager();
    bool                                    audioCbAdded = false;
   
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VSTHostComponent)
};
