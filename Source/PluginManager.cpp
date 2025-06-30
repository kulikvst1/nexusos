#include "PluginManager.h"

PluginManager::PluginManager()
{
    formatManager.addDefaultFormats();
}

PluginManager::~PluginManager()
{
    stopTimer();
}

//— пути
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

bool PluginManager::loadFromFile (const juce::File& f)
{
    if (! f.existsAsFile())     return false;
    auto xml = juce::XmlDocument::parse (f);
    if (! xml)                  return false;

    scanPaths.clear();
    loadedPluginStates.clear();
    loadedPluginPaths .clear();
    loadedPluginNames .clear();

    // 1) скан–папки
    for (auto* pathNode = xml->getChildByName("Path"); pathNode;
         pathNode = pathNode->getNextElementWithTagName("Path"))
    {
        scanPaths.addIfNotAlreadyThere (
            juce::File (pathNode->getStringAttribute("folder")));
    }

    // 2) плагины: uid, enabled, path, name
    for (auto* pNode = xml->getChildByName("Plugin"); pNode;
         pNode = pNode->getNextElementWithTagName("Plugin"))
    {
        int uid    = pNode->getIntAttribute   ("uid");
        bool en    = pNode->getBoolAttribute  ("enabled", true);
        auto path  = pNode->getStringAttribute("path");
        auto name  = pNode->getStringAttribute("name");

        loadedPluginStates[uid] = en;
        loadedPluginPaths [uid] = path;
        loadedPluginNames [uid] = name;
    }

    sendChangeMessage();
    return true;
}

//==============================================================================
// Сохранение состояния в XML (Path + полный PluginEntry)
bool PluginManager::saveToFile(const juce::File& f)
{
    juce::XmlElement xmlRoot ("PluginSettings");

    // 1) <Path folder="..."/>
    for (auto& folder : scanPaths)
    {
        auto* e = xmlRoot.createNewChildElement("Path");
        e->setAttribute("folder", folder.getFullPathName());
    }

    // 2) <Plugin uid="..." enabled="..." path="..." name="..."/>
    for (auto& entry : plugins)
    {
        auto* e = xmlRoot.createNewChildElement("Plugin");
        e->setAttribute("uid",     entry.desc.uniqueId);
        e->setAttribute("enabled", entry.enabled);
        e->setAttribute("path",    entry.desc.fileOrIdentifier);
        e->setAttribute("name",    entry.desc.name);
    }

    return xmlRoot.writeToFile (f, {});
}


//— фон-скан
void PluginManager::startScan()
{
    stopTimer();
    filesToScan.clear();
    foundDescriptions.clear();
    plugins.clear();

    for (auto& folder : scanPaths)
    {
        juce::Array<juce::File> tmp;
        folder.findChildFiles(tmp,
            juce::File::findFiles, true,
            "*.vst3;*.dll;*.component");
        filesToScan.addArray(tmp);
    }

    currentFileIndex = 0;
    isScanning = true;
    startTimer(10);
}

void PluginManager::stopScan()
{
    stopTimer();
    isScanning = false;
}

//— устанавливаем флаг и оповещаем слушателей
void PluginManager::setPluginEnabled(int index, bool shouldBeEnabled)
{
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

//— таймерный колбэк для по-файлового скана
void PluginManager::timerCallback()
{
    if (!isScanning)
        return;

    if (currentFileIndex >= filesToScan.size())
    {
        // конец скана — merge +通知
        stopTimer();
        isScanning = false;

        plugins.clear();
        for (auto* d : foundDescriptions)
        {
            PluginEntry e;
            e.desc = *d;
            e.enabled = loadedPluginStates[d->uniqueId];
            plugins.add(e);
        }
        loadedPluginStates.clear();
        foundDescriptions.clear();

        sendChangeMessage();
        return;
    }

    // сканируем файл
    auto& f = filesToScan.getReference(currentFileIndex++);
    for (auto* fmt : formatManager.getFormats())
        fmt->findAllTypesForFile(foundDescriptions, f.getFullPathName());
}
