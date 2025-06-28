#pragma once

#include <JuceHeader.h>

// «Запись» плагина + включён ли он
struct PluginEntry
{
    juce::PluginDescription desc;
    bool                    enabled = true;
};

class PluginManager : private juce::Timer,
    public juce::ChangeBroadcaster
{
public:
    PluginManager();
    ~PluginManager() override;

    // пути для сканирования
    void addSearchPath(const juce::File&);
    void removeSearchPath(const juce::File&);

    // сериализация (пути + флаги)
    bool loadFromFile(const juce::File&);
    bool saveToFile(const juce::File&);

    // фон-сканирование
    void startScan();
    void stopScan();

    // отмечаем/снимаем галочку
    void setPluginEnabled(int index, bool shouldBeEnabled);

    // результаты
    const juce::Array<juce::File>& getSearchPaths() const noexcept { return scanPaths; }
    const juce::Array<PluginEntry>& getPlugins()     const noexcept { return plugins; }

private:
    void timerCallback() override;

    juce::AudioPluginFormatManager      formatManager;
    juce::Array<juce::File>             scanPaths;
    juce::Array<juce::File>             filesToScan;
    juce::OwnedArray<juce::PluginDescription> foundDescriptions;
    std::map<int, bool>                 loadedPluginStates; // uid → enabled

    juce::Array<PluginEntry>            plugins;
    int                                 currentFileIndex = 0;
    bool                                isScanning = false;
};
