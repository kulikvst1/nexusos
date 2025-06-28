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

//— сериализация
bool PluginManager::loadFromFile(const juce::File& f)
{
    if (!f.existsAsFile()) return false;
    auto xml = juce::XmlDocument::parse(f);
    if (!xml)            return false;

    scanPaths.clear();
    loadedPluginStates.clear();

    for (auto* pathNode = xml->getChildByName("Path"); pathNode;
        pathNode = pathNode->getNextElementWithTagName("Path"))
    {
        scanPaths.addIfNotAlreadyThere(juce::File(pathNode->getStringAttribute("folder")));
    }

    for (auto* pNode = xml->getChildByName("Plugin"); pNode;
        pNode = pNode->getNextElementWithTagName("Plugin"))
    {
        int uid = pNode->getIntAttribute("uid");
        bool enabled = pNode->getBoolAttribute("enabled", true);
        loadedPluginStates[uid] = enabled;
    }

    sendChangeMessage();
    return true;
}

bool PluginManager::saveToFile(const juce::File& f)
{
    // 1) Создаём объект XML-корня
    juce::XmlElement xmlRoot("PluginSettings");

    // 2) Добавляем узлы <Path folder="..."/>
    for (auto& folder : scanPaths)
    {
        // createNewChildElement возвращает XmlElement*
        juce::XmlElement* pathElem = xmlRoot.createNewChildElement("Path");
        pathElem->setAttribute("folder", folder.getFullPathName());
    }

    // 3) Добавляем узлы <Plugin uid="..." enabled="..."/>
    for (auto& entry : plugins)
    {
        juce::XmlElement* pluginElem = xmlRoot.createNewChildElement("Plugin");
        pluginElem->setAttribute("uid", entry.desc.uniqueId);
        pluginElem->setAttribute("enabled", entry.enabled);
    }

    // 4) Записываем в файл
    return xmlRoot.writeToFile(f, {});
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
