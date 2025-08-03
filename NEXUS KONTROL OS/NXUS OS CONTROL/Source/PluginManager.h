#pragma once

#include <JuceHeader.h>

// «Запись» плагина + включён ли он
struct PluginEntry
{
    juce::PluginDescription desc;
    bool                    enabled = true;

    juce::String getPath() const { return desc.fileOrIdentifier; }
    juce::String getName() const { return desc.name; }
    int          getUID()  const { return desc.uniqueId; }
};

class PluginManager : private juce::Timer,
    public juce::ChangeBroadcaster
{
public:
    PluginManager();
    ~PluginManager() override;

    void addSearchPath(const juce::File&);
    void removeSearchPath(const juce::File&);

    bool loadFromFile(const juce::File&);
    bool saveToFile(const juce::File&);

    void startScan();
    void scanOnlyNew(); // 🔄 только для новых плагинов

    void stopScan();

    void setPluginEnabled(int index, bool shouldBeEnabled);
    void clearNewPluginFlags();


    const juce::Array<juce::File>& getSearchPaths() const noexcept { return scanPaths; }

    juce::Array<PluginEntry> getPluginsSnapshot() const
    {
        const juce::ScopedLock sl(lock);
        return plugins;
    }

    int getNumPlugins() const { const juce::ScopedLock sl(lock); return plugins.size(); }
    bool isPluginNew(int uid) const{ return newlyAddedPlugins.find(uid) != newlyAddedPlugins.end();
    }

    const PluginEntry& getPlugin(int index) const { const juce::ScopedLock sl(lock); return plugins.getReference(index); }

private:
    void timerCallback() override;

    juce::AudioPluginFormatManager formatManager;
    juce::Array<juce::File> scanPaths, filesToScan;
    juce::OwnedArray<juce::PluginDescription> foundDescriptions;
    std::unordered_map<int, bool> pluginStatesById;
    mutable juce::CriticalSection lock;
    juce::Array<PluginEntry> plugins;

    int currentFileIndex = 0;
    bool isScanning = false;
    bool scanningModePreserveExisting = false;
    std::unordered_set<int> newlyAddedPlugins;


};
