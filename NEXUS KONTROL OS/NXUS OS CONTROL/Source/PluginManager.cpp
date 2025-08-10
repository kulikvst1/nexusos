#include "PluginManager.h"

struct PluginSorter
{
    const std::unordered_set<int>& newlyAdded;

    PluginSorter(const std::unordered_set<int>& set) : newlyAdded(set) {}

    int compareElements(const PluginEntry& a, const PluginEntry& b)
    {
        bool aNew = newlyAdded.count(a.getUID()) > 0;
        bool bNew = newlyAdded.count(b.getUID()) > 0;

        if (aNew != bNew)
            return aNew ? -1 : 1;

        return a.getName().compareIgnoreCase(b.getName());
    }
};


PluginManager::PluginManager()
{
    formatManager.addDefaultFormats();
}

PluginManager::~PluginManager()
{
    stopTimer();
}

void PluginManager::addSearchPath(const juce::File& f)
{
    if (f.isDirectory())
    {
        scanPaths.addIfNotAlreadyThere(f);
        sendChangeMessage();
    }
}

void PluginManager::removeSearchPath(const juce::File& f)
{
    scanPaths.removeFirstMatchingValue(f);
    sendChangeMessage();
}
bool PluginManager::loadFromFile(const juce::File& file)
{
    if (!file.existsAsFile()) return false;

    auto xml = juce::XmlDocument::parse(file);
    if (!xml) return false;

    juce::ScopedLock sl(lock);

    scanPaths.clear();
    plugins.clear();
    pluginStatesById.clear();

    // Загружаем пути к папкам
    for (auto* pathNode = xml->getChildByName("Path"); pathNode;
        pathNode = pathNode->getNextElementWithTagName("Path"))
    {
        juce::File folder(pathNode->getStringAttribute("folder"));
        if (folder.isDirectory())
            scanPaths.addIfNotAlreadyThere(folder);
    }

    // Загружаем сохранённые плагины и их состояния
    for (auto* pNode = xml->getChildByName("Plugin"); pNode;
        pNode = pNode->getNextElementWithTagName("Plugin"))
    {
        PluginEntry e;
        e.desc.fileOrIdentifier = pNode->getStringAttribute("path");
        e.desc.name = pNode->getStringAttribute("name");
        e.desc.uniqueId = pNode->getIntAttribute("uid");
        e.enabled = pNode->getBoolAttribute("enabled", true);

        pluginStatesById[e.desc.uniqueId] = e.enabled;
        plugins.add(e);
    }

    sendChangeMessage();
    return true;
}

bool PluginManager::saveToFile(const juce::File& file)
{
    juce::XmlElement xmlRoot("PluginSettings");

    for (auto& folder : scanPaths)
        xmlRoot.createNewChildElement("Path")->setAttribute("folder", folder.getFullPathName());

    {
        juce::ScopedLock sl(lock);
        for (auto& entry : plugins)
        {
            auto* e = xmlRoot.createNewChildElement("Plugin");
            e->setAttribute("uid", entry.getUID());
            e->setAttribute("enabled", entry.enabled);
            e->setAttribute("path", entry.getPath());
            e->setAttribute("name", entry.getName());
        }
    }

    return xmlRoot.writeToFile(file, {});
}

void PluginManager::startScan()
{
    stopTimer();

    juce::ScopedLock sl(lock);
    filesToScan.clear();
    foundDescriptions.clear();

    for (auto& folder : scanPaths)
        folder.findChildFiles(filesToScan, juce::File::findFiles, true, "*.vst3;*.dll;*.component");

    currentFileIndex = 0;
    isScanning = true;
    startTimer(20); // адаптивный темп
}

void PluginManager::scanOnlyNew()
{
    stopTimer();

    juce::ScopedLock sl(lock);
    filesToScan.clear();
    foundDescriptions.clear();

    // Собираем уже известные пути плагинов
    std::unordered_set<juce::String> knownPaths;
    for (const auto& entry : plugins)
        knownPaths.insert(entry.getPath());

    // Ищем новые плагины среди всех указанных папок
    for (const auto& folder : scanPaths)
    {
        juce::Array<juce::File> found;
        folder.findChildFiles(found, juce::File::findFiles, true, "*.vst3");

        for (const auto& file : found)
        {
            if (knownPaths.find(file.getFullPathName()) == knownPaths.end())
                filesToScan.add(file);
        }
    }

    currentFileIndex = 0;
    isScanning = true;
    scanningModePreserveExisting = true; // 👈 отключаем очистку plugins
    startTimer(20);
}


void PluginManager::stopScan()
{
    stopTimer();
    isScanning = false;
}
void PluginManager::clearNewPluginFlags()
{
    juce::ScopedLock sl(lock);
    newlyAddedPlugins.clear();
}

void PluginManager::setPluginEnabled(int index, bool shouldBeEnabled)
{
    juce::ScopedLock sl(lock);
    if (juce::isPositiveAndBelow(index, plugins.size()))
    {
        auto& e = plugins.getReference(index);
        if (e.enabled != shouldBeEnabled)
        {
            e.enabled = shouldBeEnabled;
            sendChangeMessage();
        }
    }
}
void PluginManager::timerCallback()
{
    if (!isScanning)
        return;

    const int batchSize = 2;
    int processed = 0;

    while (processed++ < batchSize && currentFileIndex < filesToScan.size())
    {
        const auto& file = filesToScan.getReference(currentFileIndex++);
        for (auto* format : formatManager.getFormats())
        {
            if (format->fileMightContainThisPluginType(file.getFullPathName()))
                format->findAllTypesForFile(foundDescriptions, file.getFullPathName());
        }
    }

    if (currentFileIndex >= filesToScan.size())
    {
        stopScan();

        juce::ScopedLock sl(lock);

        if (!scanningModePreserveExisting)
            plugins.clear();

        std::unordered_set<juce::String> existingPaths;
        for (const auto& e : plugins)
            existingPaths.insert(e.getPath());

        for (auto* desc : foundDescriptions)
        {
            const auto& path = desc->fileOrIdentifier;

            if (scanningModePreserveExisting && existingPaths.count(path) > 0)
                continue;

            PluginEntry entry;
            entry.desc = *desc;

            auto it = pluginStatesById.find(entry.getUID());
            entry.enabled = (it != pluginStatesById.end()) ? it->second : true;

            plugins.add(entry);

            if (scanningModePreserveExisting)
                newlyAddedPlugins.insert(entry.getUID());
        }

        foundDescriptions.clear();

        if (scanningModePreserveExisting)
        {
            PluginSorter sorter(newlyAddedPlugins);
            plugins.sort(sorter);
        }

        sendChangeMessage();
    }
}

