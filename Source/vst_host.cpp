//==============================================================================
//  vst_host.cpp
//==============================================================================

#include "plugin_process_callback.h"
#include "vst_host.h"
#include "custom_audio_playhead.h"
#include "cpu_load.h"
#include "TunerComponent.h"    // ← подключаем UI-тюнер

using namespace juce;

//==============================================================================
// общий AudioDeviceManager
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

//==============================================================================
// Возвращает наш deviceMgr
juce::AudioDeviceManager& VSTHostComponent::getAudioDeviceManagerRef() noexcept
{
    return deviceMgr;
}
//==============================================================================
// конструктор без AudioDeviceManager
VSTHostComponent::VSTHostComponent()
    : deviceMgr(getDefaultAudioDeviceManager())
{
    currentSampleRate = 44100.0;
    currentBlockSize = 512;

    // Audio-callback
    processCb = std::make_unique<PluginProcessCallback>();
    processCb->setHostComponent(this);

    // Шаг 1: привязываем колбек сразу при старте хоста
    deviceMgr.addAudioCallback(processCb.get());
    audioCbAdded = true;

    // 1) загрузить файл плагинов + флаги из XML
    {
        auto settingsFile = File::getSpecialLocation(File::userApplicationDataDirectory)
            .getChildFile("PluginSettings.xml");
        pluginManager.loadFromFile(settingsFile);
    }

    // 2) подписаться на события PluginManager
    pluginManager.addChangeListener(this);
    changeListenerCallback(nullptr);

    // 3) стартовать сканирование плагинов
    pluginManager.startScan();

    // 4) инициализация GUI и остальных компонентов
    initialiseComponent();
}

//==============================================================================
// конструктор с внешним AudioDeviceManager
VSTHostComponent::VSTHostComponent(AudioDeviceManager& adm)
    : deviceMgr(adm)
{
    currentSampleRate = 44100.0;
    currentBlockSize = 512;

    processCb = std::make_unique<PluginProcessCallback>();
    processCb->setHostComponent(this);

    // Шаг 1: привязываем колбек сразу при старте хоста
    deviceMgr.addAudioCallback(processCb.get());
    audioCbAdded = true;

    // 1) загрузить файл плагинов + флаги из XML
    {
        auto settingsFile = File::getSpecialLocation(File::userApplicationDataDirectory)
            .getChildFile("PluginSettings.xml");
        pluginManager.loadFromFile(settingsFile);
    }

    // 2) подписаться на события PluginManager
    pluginManager.addChangeListener(this);
    changeListenerCallback(nullptr);

    // 3) стартовать сканирование плагинов
    pluginManager.startScan();

    // 4) инициализация GUI и остальных компонентов
    initialiseComponent();
}

//==============================================================================
// деструктор
VSTHostComponent::~VSTHostComponent() noexcept
{
    if (auto* f = Component::getCurrentlyFocusedComponent())
        f->giveAwayKeyboardFocus();

    setParameterChangeCallback(nullptr);
    setPresetCallback(nullptr);
    setLearnCallback(nullptr);

    bpmLabel = nullptr;

    if (pluginEditor)
    {
        pluginEditor->removeComponentListener(this);
        removeChildComponent(pluginEditor.get());
        pluginEditor.reset();
    }

    if (pluginInstance)
    {
        for (auto* p : pluginInstance->getParameters())
            p->removeListener(this);

        pluginInstance->releaseResources();
        pluginInstance.reset();
    }

    // (пока оставляем снятие колбека/закрытие аудио для следующего шага)
    if (processCb && audioCbAdded)
    {
        deviceMgr.removeAudioCallback(processCb.get());
        deviceMgr.closeAudioDevice();
        processCb.reset();
    }

    pluginManager.removeChangeListener(this);
    {
        auto settingsFile = File::getSpecialLocation(File::userApplicationDataDirectory)
            .getChildFile("PluginSettings.xml");
        pluginManager.saveToFile(settingsFile);
    }

    resetBPM();
}
//==============================================================================
// updatePluginCpuLoad
void VSTHostComponent::updatePluginCpuLoad(double load) noexcept
{
    lastCpuLoad = load;
    globalCpuLoad.store(load);
}

//==============================================================================
// setBpmDisplayLabel
void VSTHostComponent::setBpmDisplayLabel(Label* label) noexcept
{
    bpmLabel = label;
}

//==============================================================================
// setLooperEngine
void VSTHostComponent::setLooperEngine(LooperEngine* engine) noexcept
{
    if (processCb)
        processCb->setLooperEngine(engine);
}

//==============================================================================
// setAudioSettings
void VSTHostComponent::setAudioSettings(double sampleRate, int blockSize) noexcept
{
    if (!pluginInstance)
    {
        currentSampleRate = sampleRate;
        currentBlockSize = blockSize;
        return;
    }

    deviceMgr.removeAudioCallback(processCb.get());
    pluginInstance->releaseResources();
    pluginInstance->setPlayConfigDetails(2, 2, sampleRate, blockSize);
    pluginInstance->prepareToPlay(sampleRate, blockSize);
    deviceMgr.addAudioCallback(processCb.get());

    currentSampleRate = sampleRate;
    currentBlockSize = blockSize;
}

//==============================================================================
// updateBPM
void VSTHostComponent::updateBPM(double newBPM)
{
    if (playHead)         playHead->setBpm(newBPM);
    if (bpmLabel != nullptr)
    {
        bpmLabel->setText(String(newBPM, 2) + " BPM", dontSendNotification);
        bpmLabel->repaint();
    }
}

//==============================================================================
// getPluginParametersCount
int VSTHostComponent::getPluginParametersCount() const noexcept
{
#if JUCE_VERSION < 0x060000
    return pluginInstance ? pluginInstance->getNumParameters() : 0;
#else
    return pluginInstance ? int(pluginInstance->getParameters().size()) : 0;
#endif
}

//==============================================================================
// setPluginParameter
void VSTHostComponent::setPluginParameter(int ccNumber, int ccValue) noexcept
{
    if (!pluginInstance) return;

    auto& params = pluginInstance->getParameters();
    if (ccNumber < 0 || ccNumber >= (int)params.size()) return;

    float norm = jlimit(0.0f, 1.0f, float(ccValue) / 127.0f);
    pluginInstance->setParameterNotifyingHost(ccNumber, norm);
}
//==============================================================================
// loadPlugin
void VSTHostComponent::loadPlugin(const File& file,
    double     sampleRate,
    int        blockSize)
{
    // сначала отключаем старый инстанс (но колбек остаётся)
    unloadPlugin();

    if (formatManager.getNumFormats() == 0)
        formatManager.addDefaultFormats();

    OwnedArray<PluginDescription> descr;
    AudioPluginFormat* chosen = nullptr;

    // ищем формат, поддерживающий этот файл
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
        AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
            "Error",
            "Unsupported plugin file");
        return;
    }

    String err;
    auto instance = chosen->createInstanceFromDescription(*descr[0],
        sampleRate,
        blockSize,
        err);
    if (!instance)
    {
        AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
            "Error",
            "Can't load plugin:\n" + err);
        return;
    }

    // сохраняем плагин
    pluginInstance = std::move(instance);

    // настраиваем дубль каналов, sample rate и блок-сайз
    currentSampleRate = sampleRate;
    currentBlockSize = blockSize;
    pluginInstance->setPlayConfigDetails(2, 2, sampleRate, blockSize);

    // подключаем свой playhead
    playHead = std::make_unique<CustomAudioPlayHead>();
    playHead->setBpm(120.0);
    playHead->setActive(true);
    pluginInstance->setPlayHead(playHead.get());

    // готовим плагин к обработке
    pluginInstance->prepareToPlay(sampleRate, blockSize);

    // передаём плагин в колбек (callback уже активен с конструктора)
    processCb->setPlugin(pluginInstance.get());

    // создаём и показываем GUI-редактор
    pluginEditor.reset(pluginInstance->createEditor());
    if (pluginEditor)
    {
        addAndMakeVisible(*pluginEditor);
        pluginEditor->toBack();
        pluginEditor->addComponentListener(this);
    }

    // подписываемся на параметры
    for (auto* p : pluginInstance->getParameters())
        p->addListener(this);

    // включаем кнопки PRESET/LEARN
    for (auto& b : presetBtn) b.setVisible(true), b.setEnabled(true);
    for (auto& b : learnBtn)  b.setVisible(true), b.setEnabled(true);

    // устанавливаем текущий пресет
    activePreset = jlimit(0, kNumPresets - 1,
        pluginInstance->getCurrentProgram());
    updatePresetColours();
    updateLearnColours();
    resized();
}
//==============================================================================
// unloadPlugin
void VSTHostComponent::unloadPlugin()
{
    if (!pluginInstance)
        return;

    // убираем плагин из колбека, но сам колбек остаётся в AudioDeviceManager
    processCb->setPlugin(nullptr);

    // скрываем и удаляем GUI
    if (pluginEditor)
    {
        pluginEditor->removeComponentListener(this);
        removeChildComponent(pluginEditor.get());
        pluginEditor.reset();
        resetBPM();
    }

    // отписываем параметр-слушателей
    for (auto* p : pluginInstance->getParameters())
        p->removeListener(this);

    // деактивируем playhead
    if (playHead)
        playHead->setActive(false);

    // освобождаем ресурсы плагина
    pluginInstance->releaseResources();
    pluginInstance.reset();

    // сбрасываем показатели загрузки CPU
    updatePluginCpuLoad(0.0);

    // скрываем кнопки PRESET/LEARN
    for (auto& b : presetBtn) b.setVisible(false);
    for (auto& b : learnBtn)  b.setVisible(false);

    activePreset = 0;
    updatePresetColours();
    updateLearnColours();
    resized();
}
//==============================================================================
//==============================================================================
// ChangeListener: сюда придёт sendChangeMessage() из PluginManager
void VSTHostComponent::changeListenerCallback(juce::ChangeBroadcaster*)
{
    pluginFiles.clear();

    for (const auto& entry : pluginManager.getPluginsSnapshot())
    {
        if (entry.enabled)
            pluginFiles.emplace_back(juce::File(entry.getPath()));
    }

    repaint();
}

// initialiseComponent
void VSTHostComponent::initialiseComponent()
{
    for (int i = 0; i < kNumPresets; ++i)
    {
        auto& b = presetBtn[i];
        b.setButtonText("PRESET " + String(i + 1));
        addAndMakeVisible(b);
        b.addListener(this);
        b.setVisible(false);
    }

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

//==============================================================================
// изменение цвета кнопок
void VSTHostComponent::updatePresetColours()
{
    static const Colour kOff{ 0xFF404040 }, kOn{ 0xFF2A61FF };

    for (int i = 0; i < kNumPresets; ++i)
    {
        bool on = (i == activePreset);
        auto& b = presetBtn[i];
        b.setColour(TextButton::buttonColourId, on ? kOn : kOff);
        b.setColour(TextButton::textColourOffId, on ? Colours::white : Colours::black);
    }
}

void VSTHostComponent::updateLearnColours()
{
    static const Colour kOff{ 0xFF0A2A0A }, kOn{ 0xFF24B324 };

    for (auto& b : learnBtn)
    {
        b.setColour(TextButton::buttonColourId, kOff);
        b.setColour(TextButton::buttonOnColourId, kOn);
        b.setColour(TextButton::textColourOffId, Colours::white);
        b.setColour(TextButton::textColourOnId, Colours::black);
    }
}

//==============================================================================
// Button & Parameter callbacks
void VSTHostComponent::buttonClicked(juce::Button* btn)
{
    for (int i = 0; i < kNumPresets; ++i)
        if (btn == &presetBtn[i])
        {
            if (presetCb) presetCb(i);
            activePreset = i;
            updatePresetColours();
            return;
        }

    for (int i = 0; i < kNumLearn; ++i)
        if (btn == &learnBtn[i])
        {
            bool newState = learnBtn[i].getToggleState();
            if (learnCb) learnCb(i, newState);
            updateLearnColours();
            return;
        }
}

void VSTHostComponent::parameterValueChanged(int idx, float norm)
{
    auto safe = juce::Component::SafePointer<VSTHostComponent>(this);
    MessageManager::callAsync([safe, idx, norm]
        {
            if (safe && safe->paramChangeCb)
                safe->paramChangeCb(idx, norm);
        });
}

//==============================================================================
// paint / resized / layout
void VSTHostComponent::paint(juce::Graphics& g)
{
    g.fillAll(Colours::black);
}

void VSTHostComponent::resized()
{
    auto area = getLocalBounds();

    // PRESET bar
    auto top = area.removeFromTop(32);
    {
        FlexBox fb;
        fb.flexDirection = FlexBox::Direction::row;
        fb.justifyContent = FlexBox::JustifyContent::center;
        for (auto& b : presetBtn)
            fb.items.add(FlexItem(b).withMinWidth(100).withMinHeight(30));
        fb.performLayout(top);
    }

    // LEARN bar
    auto bottom = area.removeFromBottom(30);
    {
        FlexBox fb;
        fb.flexDirection = FlexBox::Direction::row;
        fb.justifyContent = FlexBox::JustifyContent::center;
        for (auto& b : learnBtn)
            fb.items.add(FlexItem(b).withMinWidth(120).withMinHeight(30));
        fb.performLayout(bottom);
    }

    // plugin editor
    layoutPluginEditor(area.reduced(4));

    for (auto& b : presetBtn) b.toFront(false);
    for (auto& b : learnBtn) b.toFront(false);
}

void VSTHostComponent::layoutPluginEditor(Rectangle<int> freeArea)
{
    if (!pluginEditor) return;
    auto desired = pluginEditor->getLocalBounds();
    if (desired.getWidth() == 0) desired.setWidth(800);
    if (desired.getHeight() == 0) desired.setHeight(400);
    Rectangle<int> fitted = desired;
    fitted.setSize(jmin(freeArea.getWidth(), desired.getWidth()),
        jmin(freeArea.getHeight(), desired.getHeight()));
    fitted.setCentre(freeArea.getCentre());
    pluginEditor->setBounds(fitted);
}

void VSTHostComponent::componentMovedOrResized(juce::Component& comp,
    bool /*moved*/,
    bool wasResized)
{
    if (pluginEditor && &comp == pluginEditor.get() && wasResized)
        clampEditorBounds();
}

void VSTHostComponent::clampEditorBounds()
{
    auto r = getLocalBounds();
    r.removeFromTop(32);
    r.removeFromBottom(30);
    r.reduce(4, 4);

    if (!pluginEditor) return;
    auto b = pluginEditor->getBounds();
    b.setSize(jmin(b.getWidth(), r.getWidth()),
        jmin(b.getHeight(), r.getHeight()));
    b.setCentre(r.getCentre());
    pluginEditor->setBounds(b);
}
//==============================================================================
// вызывается извне (bank_editor), чтобы переключить пресет
void VSTHostComponent::setExternalPresetIndex(int idx) noexcept
{
    if (pluginInstance != nullptr)
        pluginInstance->setCurrentProgram(idx);

    activePreset = idx;
    updatePresetColours();

    if (presetCb)
    {
        auto safeThis = juce::Component::SafePointer<VSTHostComponent>(this);
        juce::MessageManager::callAsync([safeThis, idx]()
            {
                if (auto* host = safeThis.getComponent())
                    host->presetCb(idx);
            });
    }
}

//==============================================================================
// вызывается извне (bank_editor), чтобы включить/выключить Learn-режим
void VSTHostComponent::setExternalLearnState (int cc, bool on) noexcept
{
    if (cc >= 0 && cc < kNumLearn)
        learnBtn[cc].setToggleState (on, juce::dontSendNotification);

    updateLearnColours();

    if (learnCb)
    {
        auto safeThis = juce::Component::SafePointer<VSTHostComponent> (this);
        juce::MessageManager::callAsync ([safeThis, cc, on]()
        {
            if (auto* host = safeThis.getComponent())
                host->learnCb (cc, on);
        });
    }
}
///TUNER
void VSTHostComponent::setTuner (TunerComponent* t) noexcept
{
    if (processCb)
        processCb->setTuner (t);
}