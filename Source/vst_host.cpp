//#include "plugin_process_callback.h" 
#include "vst_host.h"
#include "custom_audio_playhead.h"
#include "cpu_load.h"
#include <memory>
#include <atomic>
using namespace juce;
static constexpr const char* kPluginCacheFile = "PluginCache.xml";
static juce::ThreadPool pluginScanPool(0);

//───────── локальные константы / цвета ────────────────────────────────
namespace {
    const Colour kPresetOff{ 0xFF404040 };
    const Colour kPresetOn{ 0xFF2A61FF };
    const Colour kLearnOff{ 0xFF0A2A0A };
    const Colour kLearnOn{ 0xFF24B324 };
}
//───────── Shared device manager ──────────────────────────────────────

AudioDeviceManager& VSTHostComponent::getDefaultAudioDeviceManager()
{
    static AudioDeviceManager mgr;
    static bool initialised = false;

    if (!initialised)
    {
        auto err = mgr.initialiseWithDefaultDevices(2, 2);
        jassert(err.isEmpty());
        initialised = true;
    }
    return mgr;
}
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
VSTHostComponent::VSTHostComponent()
    : deviceMgr(getDefaultAudioDeviceManager())
{
    processCb = std::make_unique<PluginProcessCallback>();
    processCb->setHostComponent(this);
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    formatManager.addFormat(new juce::VST3PluginFormat());
    appProps = std::make_unique<juce::ApplicationProperties>();
    juce::PropertiesFile::Options opt;
    opt.applicationName = "NexusOS";
    opt.filenameSuffix = "settings";
    appProps->setStorageParameters(opt);
    scanForPlugins();            // ← новый вызов
    initialiseComponent();
}
VSTHostComponent::VSTHostComponent(juce::AudioDeviceManager& adm)
    : deviceMgr(adm)
{
    processCb = std::make_unique<PluginProcessCallback>();
    processCb->setHostComponent(this);
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++
    /* 1. Форматы */
    static bool formatsAdded = false;
    if (!formatsAdded)
    {
        formatManager.addDefaultFormats();
        formatsAdded = true;
    }

    /* 2. ApplicationProperties … (остаётся без изменений) */
    appProps = std::make_unique<juce::ApplicationProperties>();
    juce::PropertiesFile::Options opt;
    opt.applicationName = "NexusOS";
    opt.filenameSuffix = "settings";
    appProps->setStorageParameters(opt);

    /* 3. UI-кнопки (скрыты/disabled) … */

    initialiseComponent();

    /* 4. Асинхронное сканирование — JUCE-8 style */
    pluginScanPool.addJob([this]
        {
            scanForPlugins();          // выполняется в фоне
        });
    
}

/*─────────────────────────────────────────────────────────────
  В деструкторе дожидаемся окончания фоновых задач
─────────────────────────────────────────────────────────────*/

//──────────────── setBpmDisplayLabel ───────────────────────
void VSTHostComponent::setBpmDisplayLabel(juce::Label* label) noexcept
{
    bpmLabel = label;
}
VSTHostComponent::~VSTHostComponent()
{
    /* …остальной clean-up… */
    pluginScanPool.removeAllJobs(true, -1); // true = дождаться завершения
    unloadPlugin();
    resetBPM();
}
//───────── Public API ─────────────────────────────────────────────────
void VSTHostComponent::loadPlugin(const juce::File& file,
    double           sampleRate,
    int              blockSize)
{
    unloadPlugin();   // убираем предыдущий плагин, если был

    /*------------------------------------------------------------------
      1. Находим подходящий формат
    ------------------------------------------------------------------*/
    if (formatManager.getNumFormats() == 0)
        formatManager.addDefaultFormats();

    juce::OwnedArray<juce::PluginDescription> descr;
    juce::AudioPluginFormat* chosen = nullptr;

    for (auto* fmt : formatManager.getFormats())
    {
        fmt->findAllTypesForFile(descr, file.getFullPathName());
        if (!descr.isEmpty()) { chosen = fmt; break; }
    }
    if (!chosen)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Error", "Unsupported plugin file");
        return;
    }

    /*------------------------------------------------------------------
      2. Создаём экземпляр плагина
    ------------------------------------------------------------------*/
    juce::String err;
    plugin = chosen->createInstanceFromDescription(*descr[0],
        sampleRate,
        blockSize,
        err);
    if (plugin == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Error", "Can't load plugin:\n" + err);
        return;
    }

    /*------------------------------------------------------------------
      3. Готовим аудио и play-head
    ------------------------------------------------------------------*/
    currentSampleRate = sampleRate;
    currentBlockSize = blockSize;

    plugin->setPlayConfigDetails(2, 2, sampleRate, blockSize);

    playHead = std::make_unique<CustomAudioPlayHead>();
    playHead->setBpm(120.0);
    playHead->setActive(true);
    plugin->setPlayHead(playHead.get());

    plugin->prepareToPlay(sampleRate, blockSize);

    /*------------------------------------------------------------------
      4. AudioIODeviceCallback
    ------------------------------------------------------------------*/
    processCb->setPlugin(plugin.get());            // привязали новый экземпляр
    deviceMgr.addAudioCallback(processCb.get());
    audioCbAdded = true;
    processCb->setPlugin(plugin.get());
    /*------------------------------------------------------------------
      5. GUI-editor
    ------------------------------------------------------------------*/
    pluginEditor = plugin->createEditor();
    if (pluginEditor != nullptr)
        addAndMakeVisible(pluginEditor);

    /*------------------------------------------------------------------
      6. Слушаем параметры, чтобы передавать их в BankEditor
    ------------------------------------------------------------------*/
    for (auto* p : plugin->getParameters())
        p->addListener(this);

    /*------------------------------------------------------------------
      7. Показываем кнопки и раскрашиваем
    ------------------------------------------------------------------*/
    for (auto& b : presetBtn) { b.setVisible(true);  b.setEnabled(true); }
    for (auto& b : learnBtn) { b.setVisible(true);  b.setEnabled(true); }

    
    activePreset = juce::jlimit(0, kNumPresets - 1, plugin->getCurrentProgram());
    updatePresetColours();
    updateLearnColours();
    resized();
}

//──────────────── getAudioDeviceManagerRef ────────────────
juce::AudioDeviceManager& VSTHostComponent::getAudioDeviceManagerRef() noexcept
{
    return deviceMgr;           //  deviceMgr — наш приватный ref
}
//──────────────── setAudioSettings ─────────────────────────
void VSTHostComponent::setAudioSettings(double sampleRate, int blockSize) noexcept
{
    currentSampleRate = sampleRate;
    currentBlockSize = blockSize;
}
//──────────────── updateBPM ──────────────────────────────
void VSTHostComponent::updateBPM(double newBPM)
{
    if (playHead)
        playHead->setBpm(newBPM);

    if (bpmLabel)
    {
        bpmLabel->setText(juce::String(newBPM, 2) + " BPM",
            juce::dontSendNotification);
        bpmLabel->repaint();
    }
}
//──────────────── getPluginFiles ──────────────────────────
const juce::Array<juce::File>&
VSTHostComponent::getPluginFiles() const noexcept
{
    return pluginFiles;
}
//──────────────── getLastPluginCpuLoad ─────────────────────
double VSTHostComponent::getLastPluginCpuLoad() const noexcept
{
    return lastCpuLoad;
}
//──────────────── setPluginParameter ────────────────────────
void VSTHostComponent::setPluginParameter(int ccNumber, int ccValue) noexcept
{
    if (!plugin) return;
    auto& params = plugin->getParameters();
    if (ccNumber < 0 || ccNumber >= params.size()) return;
    float norm = juce::jlimit(0.f, 1.f, static_cast<float> (ccValue) / 127.f);
    plugin->setParameterNotifyingHost(ccNumber, norm);
}
void VSTHostComponent::unloadPlugin()
{
    // 0. если ничего не загружено – просто вернуть BPM к 120 и выйти
    if (plugin == nullptr)
    {
        resetBPM();
        return;
    }

    // 1. сообщаем аудиопотоку, что плагина больше нет
    if (processCb != nullptr)
        processCb->setPlugin(nullptr);

    // 2. снимаем callback с AudioDeviceManager
    deviceMgr.removeAudioCallback(processCb.get());
    //  processCb остаётся жить – он понадобится при следующей загрузке

    // 3. закрываем редактор (немодально – без dead-lock’а)
    if (pluginEditor != nullptr)
    {
        removeChildComponent(pluginEditor);
        delete pluginEditor;
        pluginEditor = nullptr;
    }

    // 4. убираем слушателей
    for (auto* p : plugin->getParameters())
        p->removeListener(this);

    // 5. выключаем play-head
    if (playHead)
        playHead->setActive(false);

    // 6. освобождаем сам экземпляр
    plugin->releaseResources();
    plugin.reset();

    // 7. UI-сброс
    updatePluginCpuLoad(0.0);
    resetBPM();

    for (auto& b : presetBtn) b.setVisible(false);
    for (auto& b : learnBtn)  b.setVisible(false);
    activePreset = 0;
    updatePresetColours();
    resized();
}


//───────────────────────────────────────────────────────────────────────
//───────── Initialise GUI (called from ctor) ───────────────────────────
void VSTHostComponent::initialiseComponent()
{
    // preset buttons
    for (int i = 0; i < kNumPresets; ++i)
    {
        auto& b = presetBtn[i];
        b.setButtonText("PRESET " + String(i + 1));
        b.addListener(this);
        b.setVisible(false);
        addAndMakeVisible(b);
    }
    // learn buttons
    for (int i = 0; i < kNumLearn; ++i)
    {
        auto& b = learnBtn[i];
        b.setButtonText("LEARN " + String(i + 1));
        b.setClickingTogglesState(true);
        b.addListener(this);
        b.setVisible(false);
        addAndMakeVisible(b);
    }
}
//───────── Colours ─────────────────────────────────────────────────────
void VSTHostComponent::updatePresetColours()
{
    for (int i = 0; i < kNumPresets; ++i)
    {
        bool on = (i == activePreset);
        auto& b = presetBtn[i];
        b.setColour(TextButton::buttonColourId, on ? kPresetOn : kPresetOff);
        b.setColour(TextButton::textColourOffId, on ? Colours::white
            : Colours::black);
    }
}
void VSTHostComponent::updateLearnColours()
{
    for (auto& b : learnBtn)
    {
        b.setColour(TextButton::buttonColourId, kLearnOff);
        b.setColour(TextButton::buttonOnColourId, kLearnOn);
        b.setColour(TextButton::textColourOffId, Colours::white);
        b.setColour(TextButton::textColourOnId, Colours::black);
    }
}
//───────── Button handler ──────────────────────────────────────────────
void VSTHostComponent::buttonClicked(juce::Button* btn)
{
    /* ---------- PRESET ---------- */
    for (int i = 0; i < kNumPresets; ++i)
        if (btn == &presetBtn[i])
        {
            if (presetCb) presetCb(i);     // сообщаем BankEditor
            activePreset = i;
            updatePresetColours();
            return;
        }

    /* ---------- LEARN (toggle) ---------- */
    for (int i = 0; i < kNumLearn; ++i)
        if (btn == &learnBtn[i])
        {
            const bool newState = learnBtn[i].getToggleState(); // уже переключилось
            if (learnCb) learnCb(i, newState);                 // BankEditor узнаёт
            updateLearnColours();                               // подсветка
            return;
        }
}

//───────── Parameter listener ──────────────────────────────────────────
void VSTHostComponent::parameterValueChanged(int idx, float norm)
{
    SafePointer<VSTHostComponent> safe(this);
    MessageManager::callAsync([safe, idx, norm]
        {
            if (safe && safe->paramChangeCb) safe->paramChangeCb(idx, norm);
        });

}
//───────── Layout / paint ──────────────────────────────────────────────
void VSTHostComponent::paint(Graphics& g) { g.fillAll(Colours::black); }
void VSTHostComponent::layoutPluginEditor(Rectangle<int> area)
{
    if (!pluginEditor) return;
    int w = pluginEditor->getWidth();
    int h = pluginEditor->getHeight();
    if (w == 0 || h == 0) { w = 400; h = 300; }
    Rectangle<int> r(w, h);
    r.setCentre(area.getCentre());
    r.setX(jlimit(area.getX(), area.getRight() - w, r.getX()));
    r.setY(jlimit(area.getY(), area.getBottom() - h, r.getY()));
    pluginEditor->setBounds(r);
}
void VSTHostComponent::resized()
{
    auto area = getLocalBounds();
    // Top – presets
    auto top = area.removeFromTop(32);
    FlexBox pf; pf.flexDirection = FlexBox::Direction::row;
    pf.justifyContent = FlexBox::JustifyContent::center;
    for (auto& b : presetBtn) pf.items.add(FlexItem(b).withMinWidth(90).withMinHeight(26));
    pf.performLayout(top);
    // Bottom – learn
    auto bottom = area.removeFromBottom(30);
    FlexBox lf; lf.flexDirection = FlexBox::Direction::row;
    lf.justifyContent = FlexBox::JustifyContent::center;
    for (auto& b : learnBtn) lf.items.add(FlexItem(b).withMinWidth(60).withMinHeight(24));
    lf.performLayout(bottom);
    layoutPluginEditor(area.reduced(4));
}
//───────── External setters (BankEditor) ───────────────────────────────
void VSTHostComponent::setExternalPresetIndex(int idx)
{
    if (idx == activePreset || idx < 0 || idx >= kNumPresets) return;

    activePreset = idx;
    updatePresetColours();


}
void VSTHostComponent::setExternalLearnState(int cc, bool on)
{
    if (cc < 0 || cc >= kNumLearn) return;

    learnBtn[cc].setToggleState(on, juce::dontSendNotification);
    updateLearnColours();


}
//───────── Misc helpers ────────────────────────────────────────────────
void VSTHostComponent::updatePluginCpuLoad(double load)
{
    lastCpuLoad = load;
    globalCpuLoad.store(load);
}
void VSTHostComponent::resetBPM()
{
    if (bpmLabel) bpmLabel->setText("—", dontSendNotification);
}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//==============================================================================
//  VSTHostComponent :: scanForPlugins   (Windows-only, VST3, кеш в XML)
//==============================================================================
void VSTHostComponent::scanForPlugins()
{
    /* ---------- 1. читаем кеш ---------- */
    if (auto* settings = appProps->getUserSettings())
    {
        const juce::File cacheFile = settings->getFile()
            .getSiblingFile(kPluginCacheFile);
        if (cacheFile.existsAsFile())
        {
            std::unique_ptr<juce::XmlElement> xml            // ← явный unique_ptr
            { juce::XmlDocument::parse(cacheFile) };
            if (xml != nullptr)
                pluginList.recreateFromXml(*xml);
        }
    }
    /* ---------- 2. сканируем, если пусто ---------- */
    if (pluginList.getNumTypes() == 0)
    {
        using SL = juce::File::SpecialLocationType;
        juce::Array<juce::File> dirs =
        {
            juce::File::getSpecialLocation(SL::globalApplicationsDirectory)
                     .getChildFile("Common Files").getChildFile("VST3"),
            juce::File::getSpecialLocation(SL::userDocumentsDirectory)
                     .getChildFile("VST3"),
            juce::File::getSpecialLocation(SL::userHomeDirectory)
                     .getChildFile("VST3")
        };
        jassert(formatManager.getNumFormats() == 1);
        juce::AudioPluginFormat& vst3 = *formatManager.getFormat(0);
        for (const auto& dir : dirs)
        {
            if (!dir.isDirectory()) continue;
            juce::Array<juce::File> vstFiles;
            dir.findChildFiles(vstFiles, juce::File::findFiles,
                true, "*.vst3");
            for (const auto& f : vstFiles)
            {
                juce::OwnedArray<juce::PluginDescription> tmp;
                pluginList.scanAndAddFile(f.getFullPathName(),
                    true,        // dontRescan
                    tmp,
                    vst3);
            }
        }
        /* ---------- 3. сохраняем кеш ---------- */
        // Вариант 1 — оставить auto, но без *
        if (auto xml = pluginList.createXml())                 // xml — unique_ptr
        {
            if (auto* settings = appProps->getUserSettings())
            {
                const juce::File cacheFile = settings->getFile()
                    .getSiblingFile(kPluginCacheFile);
                cacheFile.replaceWithText(xml->toString());
            }
        }
    }
    /* ---------- 4. переносим в legacy-массив ---------- */
    pluginFiles.clear();
    for (juce::PluginDescription& desc : pluginList.getTypes())     // ← ссылка, без *
        pluginFiles.add(juce::File(desc.fileOrIdentifier));
}
int VSTHostComponent::getPluginParametersCount() const noexcept
{
#if JUCE_VERSION < 0x060000
    return plugin ? plugin->getNumParameters() : 0;
#else
    return plugin ? static_cast<int>(plugin->getParameters().size()) : 0;
#endif
}
