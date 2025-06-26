#include "plugin_process_callback.h" 
#include "vst_host.h"
#include "custom_audio_playhead.h"
#include "cpu_load.h"
#include <memory>
#include <atomic>

using namespace juce;
static constexpr const char* kPluginCacheFile = "PluginCache.xml";

// 1.--------------- Единственное определение пула
juce::ThreadPool pluginScanPool
{
    std::max(1, juce::SystemStats::getNumCpus() - 1)
};
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

VSTHostComponent::VSTHostComponent(): deviceMgr(getDefaultAudioDeviceManager())
{
    processCb = std::make_unique<PluginProcessCallback>();
    processCb->setHostComponent(this);
    formatManager.addFormat(new juce::VST3PluginFormat());
    appProps = std::make_unique<juce::ApplicationProperties>();
    juce::PropertiesFile::Options opt;
    opt.applicationName = "NexusOS";
    opt.filenameSuffix = "settings";
    appProps->setStorageParameters(opt);
    scanForPlugins();            // ← новый вызов
    initialiseComponent();
}

VSTHostComponent::VSTHostComponent(juce::AudioDeviceManager& adm): deviceMgr(adm)
{
    processCb = std::make_unique<PluginProcessCallback>();
    processCb->setHostComponent(this);
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
    initialiseComponent();
    scanForPlugins();
}
/*─────────────────────────────────────────────────────────────
  В деструкторе дожидаемся окончания фоновых задач
─────────────────────────────────────────────────────────────*/
//──────────────── setBpmDisplayLabel ───────────────────────
void VSTHostComponent::setBpmDisplayLabel(juce::Label* label) noexcept
{
    bpmLabel = label;
}
VSTHostComponent::~VSTHostComponent() noexcept
{
    // 0) Первым делом слейте (обнуляйте) все коллбэки,
    //    чтобы _Tidy() удалил корректную, пустую функцию:
    setParameterChangeCallback(nullptr);
    setPresetCallback(nullptr);
    setLearnCallback(nullptr);

    // 1) Уберите связь с bpmLabel, чтобы updateBPM ничего не сделал
    bpmLabel = nullptr;

    // 2) Только теперь можно спокойно подчищать плагин и GUI
    unloadPlugin();
}


//──────────────── getAudioDeviceManagerRef ────────────────
juce::AudioDeviceManager& VSTHostComponent::getAudioDeviceManagerRef() noexcept
{
    return deviceMgr;           //  deviceMgr — наш приватный ref
}
//──────────────── setAudioSettings ─────────────────────────
void VSTHostComponent::setAudioSettings(double sampleRate, int blockSize) noexcept
{
    if (!plugin)
    {
        currentSampleRate = sampleRate;
        currentBlockSize = blockSize;
        return;
    }
    // 1. Снимаем callback, чтобы аудиопоток не лез в плагин во время перестройки
    deviceMgr.removeAudioCallback(processCb.get());
    // 2. Безопасно останавливаем плагин
    plugin->releaseResources();
    // 3. Переподготавливаем под новые параметры
    plugin->setPlayConfigDetails(2, 2, sampleRate, blockSize);
    plugin->prepareToPlay(sampleRate, blockSize);
    // 4. Возвращаем callback
    deviceMgr.addAudioCallback(processCb.get());
    // 5. Обновляем наш внутренний state
    currentSampleRate = sampleRate;
    currentBlockSize = blockSize;
}
//──────────────── updateBPM ──────────────────────────────
void VSTHostComponent::updateBPM(double newBPM)
{
    if (playHead)
        playHead->setBpm(newBPM);

    if (bpmLabel != nullptr)  // <<< проверяем только на nullptr
    {
        bpmLabel->setText(String(newBPM, 2) + " BPM",
            dontSendNotification);
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
/*------------------------------------------------------------------
                        LOAD  UPLOAD
 ------------------------------------------------------------------*/
void VSTHostComponent::loadPlugin(const juce::File& file,
    double               sampleRate,
    int                  blockSize)
{
    unloadPlugin();   // убираем предыдущий плагин, если был

    /* 1. Выбираем нужный формат */
    if (formatManager.getNumFormats() == 0)
        formatManager.addDefaultFormats();

    juce::OwnedArray<juce::PluginDescription> descr;
    juce::AudioPluginFormat* chosen = nullptr;

    for (auto* fmt : formatManager.getFormats())
    {
        fmt->findAllTypesForFile(descr, file.getFullPathName());
        if (!descr.isEmpty())
        {
            chosen = fmt;
            break;
        }
    }

    if (!chosen)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Error",
            "Unsupported plugin file");
        return;
    }

    /* 2. Создаём экземпляр плагина */
    juce::String err;
    plugin = chosen->createInstanceFromDescription(*descr[0],
        sampleRate,
        blockSize,
        err);

    if (plugin == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Error",
            "Can't load plugin:\n" + err);
        return;
    }

    /* 3. Готовим аудио и play-head */
    currentSampleRate = sampleRate;
    currentBlockSize = blockSize;

    plugin->setPlayConfigDetails(2, 2, sampleRate, blockSize);

    playHead = std::make_unique<CustomAudioPlayHead>();
    playHead->setBpm(120.0);
    playHead->setActive(true);
    plugin->setPlayHead(playHead.get());
    plugin->prepareToPlay(sampleRate, blockSize);

    /* 4. AudioIODeviceCallback */
    processCb->setPlugin(plugin.get());
    deviceMgr.addAudioCallback(processCb.get());
    audioCbAdded = true;

    /* 5. GUI-редактор */
    pluginEditor = plugin->createEditor();
    if (pluginEditor != nullptr)
    {
        addAndMakeVisible(pluginEditor);
        pluginEditor->toBack();        // кладём под панели пресетов/learn
        pluginEditor->addComponentListener(this);//************************************************************************
    }

    /* 6. Слушаем параметры, чтобы передавать их в BankEditor */
    for (auto* p : plugin->getParameters())
        p->addListener(this);

    /* 7. Раскрашиваем и показываем кнопки */
    for (auto& b : presetBtn) { b.setVisible(true); b.setEnabled(true); }
    for (auto& b : learnBtn) { b.setVisible(true); b.setEnabled(true); }

    activePreset = juce::jlimit(0, kNumPresets - 1, plugin->getCurrentProgram());
    updatePresetColours();
    updateLearnColours();
    resized();
}

void VSTHostComponent::unloadPlugin()
{
    // 0) Если плагин уже nullptr — просто ничего не делаем
    if (plugin == nullptr)
        return;

    // 1) Сообщаем аудиопотоку, что плагина больше нет
    if (processCb != nullptr)
        processCb->setPlugin(nullptr);

    // 2) Отключаем AudioCallback от AudioDeviceManager
    deviceMgr.removeAudioCallback(processCb.get());
    //    processCb остаётся жив, понадобится при следующей загрузке

    // 3) Закрываем полноэкранный/немодальный редактор
    if (pluginEditor != nullptr)
    {
        pluginEditor->removeComponentListener(this);
        removeChildComponent(pluginEditor);
        delete pluginEditor;
        pluginEditor = nullptr;

        // теперь, когда label ещё не уничтожен, можно сбросить BPM
        resetBPM();
    }

    // 4) Снимаем слушателей со всех параметров
    for (auto* p : plugin->getParameters())
        p->removeListener(this);

    // 5) Выключаем play-head
    if (playHead)
        playHead->setActive(false);

    // 6) Освобождаем ресурсы плагина и сам экземпляр
    plugin->releaseResources();
    plugin.reset();

    // 7) Сбрасываем UI-индикацию загрузки/CPU-нагрузки
    updatePluginCpuLoad(0.0);

    // 8) Прячем все кнопки пресетов/learn-режима
    for (auto& b : presetBtn) b.setVisible(false);
    for (auto& b : learnBtn)  b.setVisible(false);

    // 9) Обновляем цвета пресетов, сбрасываем activePreset и layout
    updatePresetColours();
    activePreset = 0;
    resized();
}

//───────── Initialise GUI (called from ctor) ───────────────────────────
void VSTHostComponent::initialiseComponent()
{
    // preset buttons
    for (int i = 0; i < kNumPresets; ++i)
    {
        auto& b = presetBtn[i];
        b.setButtonText("PRESET " + String(i + 1));
        addAndMakeVisible(b);
        b.addListener(this);
        b.setVisible(false);
    }
    // learn buttons
    for (int i = 0; i < kNumLearn; ++i)
    {
        auto& b = learnBtn[i];
        b.setButtonText("LEARN " + String(i + 1));
        b.setClickingTogglesState(true);
        addAndMakeVisible(b);
        b.addListener(this);
        b.setVisible(false);
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
//**********************************************************************


//*********************************************************************
// вызывается каждый раз, когда плагин дернулся
void VSTHostComponent::componentMovedOrResized(juce::Component& c,
    bool /*wasMoved*/,
    bool wasResized)
{
    if (&c == pluginEditor && wasResized)
        clampEditorBounds();   // перекомпоновываем
}

void VSTHostComponent::clampEditorBounds()
{
    auto free = getLocalBounds();
    free.removeFromTop(32);   // высота preset-бара
    free.removeFromBottom(30);   // высота learn-бара
    free = free.reduced(4);

    if (!pluginEditor) return;

    auto b = pluginEditor->getBounds();
    b.setSize(juce::jmin(b.getWidth(), free.getWidth()),
        juce::jmin(b.getHeight(), free.getHeight()));
    b.setCentre(free.getCentre());
    pluginEditor->setBounds(b);
}

void VSTHostComponent::layoutPluginEditor(Rectangle<int> freeArea)
{
    if (!pluginEditor) return;
    auto desired = pluginEditor->getLocalBounds();
    if (desired.getWidth() == 0) desired.setWidth(800);
    if (desired.getHeight() == 0) desired.setHeight(400);
    // shrink-to-fit, но сохраняем пропорции
    Rectangle<int> fitted = desired;
    fitted.setSize(jmin(freeArea.getWidth(), desired.getWidth()),
    jmin(freeArea.getHeight(), desired.getHeight()));
    fitted.setCentre(freeArea.getCentre());
    pluginEditor->setBounds(fitted);
}
void VSTHostComponent::resized()
{
    auto area = getLocalBounds();

    // Верх: PRESET-панель
    auto top = area.removeFromTop(32);
    {
        FlexBox fb;
        fb.flexDirection = FlexBox::Direction::row;
        fb.justifyContent = FlexBox::JustifyContent::center;
        for (auto& b : presetBtn)
            fb.items.add(FlexItem(b).withMinWidth(100).withMinHeight(30));
        fb.performLayout(top);
    }

    // Низ: LEARN-панель
    auto bottom = area.removeFromBottom(30);
    {
        FlexBox fb;
        fb.flexDirection = FlexBox::Direction::row;
        fb.justifyContent = FlexBox::JustifyContent::center;
        for (auto& b : learnBtn)
            fb.items.add(FlexItem(b).withMinWidth(120).withMinHeight(30));
        fb.performLayout(bottom);
    }

    // Центральная область — редактор плагина
    layoutPluginEditor(area.reduced(4));

    // Гарантируем, что панели всегда поверх редактора
    for (auto& b : presetBtn) b.toFront(false);
    for (auto& b : learnBtn)  b.toFront(false);
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
