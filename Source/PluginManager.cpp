#include "PluginManager.h"

PluginManager::PluginManager()
{
    formatManager.addDefaultFormats();
    pluginsList = std::make_unique<juce::KnownPluginList>();
}

PluginManager::~PluginManager() {}

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

    for (auto* pathNode = xml->getChildByName("Path"); pathNode;
        pathNode = pathNode->getNextElementWithTagName("Path"))
    {
        juce::File folder(pathNode->getStringAttribute("folder"));
        if (folder.isDirectory())
            scanPaths.addIfNotAlreadyThere(folder);
    }

    for (auto* pNode = xml->getChildByName("Plugin"); pNode;
        pNode = pNode->getNextElementWithTagName("Plugin"))
    {
        PluginEntry e;
        e.desc.fileOrIdentifier = pNode->getStringAttribute("path");
        e.desc.name = pNode->getStringAttribute("name");
        e.desc.uniqueId = pNode->getIntAttribute("uid");
        e.enabled = pNode->getBoolAttribute("enabled", true);

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
void PluginManager::startScanAsync(ScanMode mode, juce::Component* parentComponent)
{
    if (scanner != nullptr)
        return;

    currentScanMode = mode;

    juce::FileSearchPath searchPaths;
    for (auto& f : scanPaths)
        if (f.isDirectory())
            searchPaths.add(f);

    // 🔹 Если пользователь не указал пути — используем только дефолтный VST3 каталог
    if (searchPaths.toString().isEmpty())
    {
        DBG("PluginManager: no valid search paths, using default VST3 folder");

        juce::File vst3Dir("C:/Program Files/Common Files/VST3");
        if (vst3Dir.isDirectory())
            searchPaths.add(vst3Dir);
    }

    if (searchPaths.toString().isEmpty())
    {
        DBG("PluginManager: still no valid paths — scan aborted");
        return;
    }

    auto dmpFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("PluginScan_DMP.txt");

    juce::AudioPluginFormat* format = nullptr;
    for (int i = 0; i < formatManager.getNumFormats(); ++i)
        if (auto* f = formatManager.getFormat(i))
            if (f->getName().containsIgnoreCase("VST3")) { format = f; break; }

    if (format == nullptr)
        format = formatManager.getFormat(0);

    if (format == nullptr)
    {
        DBG("PluginManager: no plugin formats available — scan aborted");
        return;
    }

    if (pluginsList == nullptr)
        pluginsList = std::make_unique<juce::KnownPluginList>();

    if (currentScanMode == ScanMode::All)
        pluginsList->clear();

    scanner = std::make_unique<juce::PluginDirectoryScanner>(
        *pluginsList, *format, searchPaths, true, dmpFile, true);

    // 🔹 прогресс обнуляем
    scanProgressValue = 0.0;

    // 🔹 запускаем таймер
    startTimer(100);
}
void PluginManager::timerCallback()
{
    if (scanner != nullptr)
    {
        juce::String pluginName;
        const bool dontRescanIfAlreadyInList = (currentScanMode == ScanMode::New);
        const bool finished = !scanner->scanNextFile(dontRescanIfAlreadyInList, pluginName);

        // 🔹 берём прогресс напрямую из сканера
        scanProgressValue = scanner->getProgress();

        if (finished)
        {
            scanFinished = true;   // <-- флаг завершения
            DBG("PluginManager: scan finished");
            scanner.reset();
            stopTimer();

            juce::String detectedPlugins;
            juce::Array<juce::PluginDescription*> newPlugins;

            for (int i = 0; i < pluginsList->getNumTypes(); ++i)
            {
                if (auto* desc = pluginsList->getType(i))
                {
                    bool alreadyExists = false;
                    for (auto& e : plugins)
                        if (e.getUID() == desc->uniqueId || e.getPath() == desc->fileOrIdentifier)
                            alreadyExists = true;

                    if (currentScanMode == ScanMode::All || !alreadyExists)
                    {
                        newPlugins.add(desc);
                        detectedPlugins << "• " << desc->name << "\n";
                    }
                }
            }

            if (currentScanMode == ScanMode::New)
            {
                if (newPlugins.isEmpty())
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::InfoIcon,
                        juce::String::fromUTF8("✅ Scan Finished"),
                        juce::String::fromUTF8("No new plugins detected."),
                        juce::String::fromUTF8("OK")
                    );
                    sendChangeMessage();
                }
                else
                {
                    juce::AlertWindow::showOkCancelBox(
                        juce::AlertWindow::InfoIcon,
                        juce::String::fromUTF8("✅ Scan Finished"),
                        juce::String::fromUTF8("New plugins detected. Add them to the list?\n\nDetected:\n") + detectedPlugins,
                        juce::String::fromUTF8("✔️ OK"),
                        juce::String::fromUTF8("❌ Cancel"),
                        nullptr,
                        juce::ModalCallbackFunction::create([this, newPlugins](int result)
                            {
                                if (result == 1)
                                {
                                    juce::ScopedLock sl(lock);
                                    for (auto* desc : newPlugins)
                                    {
                                        PluginEntry entry;
                                        entry.desc = *desc;
                                        entry.enabled = true;
                                        plugins.add(entry);
                                        newlyAddedPlugins.insert(entry.getUID());
                                    }
                                    sendChangeMessage();
                                }
                                else
                                {
                                    sendChangeMessage();
                                }
                            })
                    );
                }
            }
            else // ScanMode::All
            {
                if (pluginsList->getNumTypes() == 0)
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::InfoIcon,
                        juce::String::fromUTF8("✅ Scan Finished"),
                        juce::String::fromUTF8("No plugins were detected."),
                        juce::String::fromUTF8("OK")
                    );
                    sendChangeMessage();
                }
                else
                {
                    juce::AlertWindow::showOkCancelBox(
                        juce::AlertWindow::InfoIcon,
                        juce::String::fromUTF8("✅ Scan Finished"),
                        juce::String::fromUTF8("Scanning completed. Add found plugins to the list?\n\nDetected:\n") + detectedPlugins,
                        juce::String::fromUTF8("✔️ OK"),
                        juce::String::fromUTF8("❌ Cancel"),
                        nullptr,
                        juce::ModalCallbackFunction::create([this](int result)
                            {
                                if (result == 1)
                                {
                                    rebuildPluginEntries();
                                    sendChangeMessage();
                                }
                                else
                                {
                                    if (pluginsList)
                                        pluginsList->clear();
                                    sendChangeMessage();
                                }
                            })
                    );
                }
            }
        }
        else
        {
            scanFinished = false;   // <-- пока идёт процесс
            sendChangeMessage();    // 🔹 обновляем UI на каждом шаге
        }
    }
}

void PluginManager::rebuildPluginEntries()
{
    juce::ScopedLock sl(lock);
    plugins.clear();

    for (int i = 0; i < pluginsList->getNumTypes(); ++i)
    {
        if (auto* desc = pluginsList->getType(i))
        {
            PluginEntry entry;
            entry.desc = *desc;
            entry.enabled = true;
            plugins.add(entry);
        }
    }
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

void PluginManager::clearNewPluginFlags()
{
    juce::ScopedLock sl(lock);
    newlyAddedPlugins.clear();
}
