//==============================================================================
//  vst_host.h
//==============================================================================
#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <vector>

class PluginProcessCallback;
class CustomAudioPlayHead;

class VSTHostComponent : public juce::Component,
    private juce::Button::Listener,
    private juce::AudioProcessorParameter::Listener,
    private juce::ComponentListener
{
public:
    VSTHostComponent();
    explicit VSTHostComponent(juce::AudioDeviceManager&);
    ~VSTHostComponent() noexcept override;

    // загрузка/выгрузка плагина
    void loadPlugin(const juce::File& file,
        double sampleRate = 44100.0,
        int    blockSize = 512);
    void unloadPlugin();

    // GUI-хелперы для внешнего BankEditor
    void setExternalPresetIndex(int idx)         noexcept;
    void setExternalLearnState(int cc, bool on) noexcept;
    void setBpmDisplayLabel(juce::Label* label) noexcept;

    // колбэки от BankEditor
    using ParamChangeFn = std::function<void(int, float)>;
    using PresetCb = std::function<void(int)>;
    using LearnCb = std::function<void(int, bool)>;

    void setParameterChangeCallback(ParamChangeFn cb) noexcept { paramChangeCb = std::move(cb); }
    void setPresetCallback(PresetCb      cb) noexcept { presetCb = std::move(cb); }
    void setLearnCallback(LearnCb       cb) noexcept { learnCb = std::move(cb); }

    // смена audio-settings
    void setAudioSettings(double sampleRate, int blockSize) noexcept;
    void setPluginParameter(int ccNumber, int ccValue)        noexcept;

    // информация
    juce::AudioPluginInstance* getPluginInstance()       noexcept { return pluginInstance.get(); }
    const juce::AudioPluginInstance* getPluginInstance() const noexcept { return pluginInstance.get(); }
    const std::vector<juce::File>& getPluginFiles()     const noexcept { return pluginFiles; }
    int                              getPluginParametersCount() const noexcept;
    double                           getLastPluginCpuLoad()       const noexcept { return lastCpuLoad; }

    // Component overrides
    void paint(juce::Graphics&) override;
    void resized() override;
    void componentMovedOrResized(juce::Component&, bool, bool) override;

    // BPM/CPU
    void resetBPM() { updateBPM(120.0); }
    void updateBPM(double newBPM);
    void updatePluginCpuLoad(double load) noexcept;

    // общий AudioDeviceManager
    static juce::AudioDeviceManager& getDefaultAudioDeviceManager();
    juce::AudioDeviceManager& getAudioDeviceManagerRef() noexcept;

//+++++++++++++
    void clearPluginScanJobs() noexcept;                       // обёртка для removeAllJobs(...)
    void restartPluginScanPool();                             // сброс и пересоздание пула
    void addPluginScanJob(juce::ThreadPoolJob* job, bool hi); // чтобы закинуть новую задачу
    int getPluginScanJobCount() const noexcept;
private:
    // formats & cache
    juce::AudioPluginFormatManager               formatManager;
    juce::KnownPluginList                        pluginList;
    std::unique_ptr<juce::ApplicationProperties> appProps;
    static constexpr const char* kPluginCacheFile = "PluginCache.xml";

    // GUI
    void initialiseComponent();
    void updatePresetColours();
    void updateLearnColours();
    void layoutPluginEditor(juce::Rectangle<int> area);
    void clampEditorBounds();

    void buttonClicked(juce::Button*) override;
    void parameterValueChanged(int, float) override;
    void parameterGestureChanged(int, bool) override {}

    static constexpr int kNumPresets = 6;
    static constexpr int kNumLearn = 10;

    juce::TextButton presetBtn[kNumPresets];
    juce::TextButton learnBtn[kNumLearn];

    // state
    int    activePreset = 0;
    double currentSampleRate = 44100.0;
    int    currentBlockSize = 512;
    double lastCpuLoad = 0.0;

    // audio & plugin
    juce::AudioDeviceManager& deviceMgr;
    std::unique_ptr<juce::AudioPluginInstance> pluginInstance;

    std::unique_ptr<PluginProcessCallback>      processCb;
    std::unique_ptr<CustomAudioPlayHead>        playHead;
    std::unique_ptr<juce::Component>            pluginEditor;
    juce::Label* bpmLabel = nullptr;
    bool                                        audioCbAdded = false;

    // background scanner
    std::unique_ptr<juce::ThreadPool>           pluginScanPool;
    std::vector<juce::File>                     pluginFiles;

    // callbacks
    ParamChangeFn   paramChangeCb;
    PresetCb        presetCb;
    LearnCb         learnCb;

    // scanning
    void scanForPlugins();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VSTHostComponent)
};
