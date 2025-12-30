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

#if JUCE_LINUX
    auto home = juce::File::getSpecialLocation(juce::File::userHomeDirectory);

    // Пользовательские папки
    addSearchPath(home.getChildFile(".vst"));
    addSearchPath(home.getChildFile(".vst3"));
    addSearchPath(home.getChildFile(".clap"));

    // Системные папки
    addSearchPath(juce::File("/usr/lib/vst"));
    addSearchPath(juce::File("/usr/local/lib/vst"));
    addSearchPath(juce::File("/usr/lib/vst3"));
    addSearchPath(juce::File("/usr/local/lib/vst3"));
#elif JUCE_WINDOWS
    addSearchPath(juce::File("C:\\Program Files\\VSTPlugins"));
    addSearchPath(juce::File("C:\\Program Files\\Common Files\\VST3"));
#endif
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
    if (!file.existsAsFile())
        return false;

    std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(file));
    if (!xml)
        return false;

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
// Синхронная синхронизация yabridge (Linux)
void PluginManager::syncYabridge()
{
   #if JUCE_LINUX
    auto yabridgeCtl = juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                          .getChildFile(".local/share/yabridge/yabridgectl");

    if (!yabridgeCtl.existsAsFile())
        return; // yabridgectl не найден — тихо выходим

    juce::ChildProcess proc;
    juce::StringArray args { yabridgeCtl.getFullPathName(), "sync" };

    if (proc.start(args))
        proc.waitForProcessToFinish(-1); // ждём завершения
   #endif
}

void PluginManager::startScan()
{
   #if JUCE_LINUX
    syncYabridge(); // гарантированно синхронизируем перед сканированием
   #endif

    stopTimer();

    juce::ScopedLock sl(lock);
    filesToScan.clear();
    foundDescriptions.clear();

    const char* fileExtensions = "*.dll;*.so;*.component;*.vst3";
    const char* bundleExtensions = "*.vst3";

    for (auto& folder : scanPaths)
    {
        // Файлы
        folder.findChildFiles(filesToScan,
            juce::File::findFiles,
            true,
            fileExtensions);

        // Директории-бандлы .vst3
        juce::Array<juce::File> vst3Dirs;
        folder.findChildFiles(vst3Dirs,
            juce::File::findDirectories,
            true,
            bundleExtensions);

        for (auto& d : vst3Dirs)
            filesToScan.add(d);
    }

    currentFileIndex = 0;
    isScanning = true;
    startTimer(20);
}

void PluginManager::scanOnlyNew()
{
   #if JUCE_LINUX
    syncYabridge(); // синхронизируем перед частичным сканированием
   #endif

    stopTimer();

    juce::ScopedLock sl(lock);
    filesToScan.clear();
    foundDescriptions.clear();

    std::unordered_set<juce::String> knownPaths;
    for (const auto& entry : plugins)
        knownPaths.insert(entry.getPath());

    const char* fileExtensions = "*.dll;*.so;*.component;*.vst3";
    const char* bundleExtensions = "*.vst3";

    for (const auto& folder : scanPaths)
    {
        // Файлы
        juce::Array<juce::File> foundFiles;
        folder.findChildFiles(foundFiles,
            juce::File::findFiles,
            true,
            fileExtensions);

        for (const auto& f : foundFiles)
            if (knownPaths.find(f.getFullPathName()) == knownPaths.end())
                filesToScan.add(f);

        // Директории-бандлы .vst3
        juce::Array<juce::File> foundDirs;
        folder.findChildFiles(foundDirs,
            juce::File::findDirectories,
            true,
            bundleExtensions);

        for (const auto& d : foundDirs)
            if (knownPaths.find(d.getFullPathName()) == knownPaths.end())
                filesToScan.add(d);
    }

    currentFileIndex = 0;
    isScanning = true;
    scanningModePreserveExisting = true;
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

