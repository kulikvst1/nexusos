#pragma once

#include <JuceHeader.h>
#include <unordered_map>
#include <unordered_set>

struct PluginEntry
{
    juce::PluginDescription desc;
    bool enabled = true;

    juce::String getPath() const { return desc.fileOrIdentifier; }
    juce::String getName() const { return desc.name; }
    int getUID() const { return desc.uniqueId; }
};

class PluginManager : public juce::ChangeBroadcaster,
    private juce::Timer
{
public:
    PluginManager();
    ~PluginManager();

    void addSearchPath(const juce::File&);
    void removeSearchPath(const juce::File&);

    bool loadFromFile(const juce::File&);
    bool saveToFile(const juce::File&);

    // 🔹 режим сканирования
    enum class ScanMode { None, All, New };

    // 🔹 Сканирование — теперь принимаем режим
    void startScanAsync(ScanMode mode, juce::Component* parentComponent = nullptr);

    void setPluginEnabled(int index, bool shouldBeEnabled);
    void clearNewPluginFlags();

    const juce::Array<juce::File>& getSearchPaths() const noexcept { return scanPaths; }

    juce::Array<PluginEntry> getPluginsSnapshot() const
    {
        const juce::ScopedLock sl(lock);
        return plugins;
    }

    int getNumPlugins() const { const juce::ScopedLock sl(lock); return plugins.size(); }
    bool isPluginNew(int uid) const { return newlyAddedPlugins.find(uid) != newlyAddedPlugins.end(); }

    const PluginEntry& getPlugin(int index) const
    {
        const juce::ScopedLock sl(lock);
        return plugins.getReference(index);
    }

    double getScanProgress() const noexcept { return scanProgressValue; }
    double& getScanProgressRef() noexcept { return scanProgressValue; }
    bool isScanFinished() const noexcept { return scanFinished; } // флаг завершения
    void removePlugin(int index);
    juce::File getSettingsFile() const;
private:
    juce::AudioPluginFormatManager formatManager;
    juce::Array<juce::File> scanPaths, filesToScan;
    juce::OwnedArray<juce::PluginDescription> foundDescriptions;
    mutable juce::CriticalSection lock;
    juce::Array<PluginEntry> plugins;

    std::unordered_set<int> newlyAddedPlugins;

    std::unique_ptr<juce::PluginDirectoryScanner> scanner;
    std::unique_ptr<juce::KnownPluginList> pluginsList;
    std::unique_ptr<juce::FileSearchPath> searchPathList;
    std::unique_ptr<juce::Component> progressWindow;

    int totalFilesToScan{ 0 };
    int scannedFiles{ 0 };
    double scanProgressValue{ 0.0 };

    void timerCallback() override;
    void rebuildPluginEntries();
    bool scanFinished = false; 
   

    ScanMode currentScanMode{ ScanMode::None };
};
