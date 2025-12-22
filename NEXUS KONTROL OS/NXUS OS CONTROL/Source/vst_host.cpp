#include "vst_host.h"
#include "plugin_process_callback.h"
#include "custom_audio_playhead.h"
#include "cpu_load.h"
#include "TunerComponent.h"
#include "OutControlComponent.h"
#include "InputControlComponent.h" 
//==============================================================================
juce::AudioDeviceManager& VSTHostComponent::getDefaultAudioDeviceManager()
{
    static juce::AudioDeviceManager instance;
    return instance;
}
//==============================================================================
using namespace juce;
//==============================================================================
// Конструктор с явной инъекцией AudioDeviceManager
//==============================================================================
//  vst_host.cpp
//==============================================================================
VSTHostComponent::VSTHostComponent(juce::AudioDeviceManager& adm,
    PluginManager& pm)
    : deviceMgr(adm),
    pluginManager(pm),                // <-- добавили инициализацию ссылки
    currentSampleRate(44100.0),
    currentBlockSize(512)
{
    processCb = std::make_unique<PluginProcessCallback>();
    processCb->setHostComponent(this);

    if (inputControl != nullptr)
        processCb->setInputControl(inputControl);

    // Подписываемся на события аудио-движка и запускаем обработку
    startAudio();

    // Загрузить настройки плагинов из XML
    {
        auto settingsFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("PluginSettings.xml");
        pluginManager.loadFromFile(settingsFile);
    }

    // Подписываемся на изменения списка плагинов
    pluginManager.addChangeListener(this);
    changeListenerCallback(nullptr);
    pluginManager.startScan();

    initialiseComponent();
}


//==============================================================================

VSTHostComponent::~VSTHostComponent() noexcept
{
    // 1) Снять коллбек и закрыть устройство
    stopAudio();
    deviceMgr.closeAudioDevice(); // <— гарантированно останавливает поток драйвера

    // 2) Разорвать связи внутри processCb, чтобы даже «заблудившийся» вызов был безопасен
    if (processCb)
    {
        processCb->setPlugin(nullptr);
        processCb->setLooperEngine(nullptr);
        processCb->setOutControl(nullptr);
        // Если есть метод очистки тюнеров:
        // processCb->clearTuners();
    }

    // 3) Уничтожить коллбек
    processCb.reset();

    // 4) Остальное — как у тебя
    if (auto* f = juce::Component::getCurrentlyFocusedComponent())
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

    pluginManager.removeChangeListener(this);

    {
        auto settingsFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("PluginSettings.xml");
        pluginManager.saveToFile(settingsFile);
    }

    resetBPM();
}


//==============================================================================
// Обновление загрузки CPU
void VSTHostComponent::updatePluginCpuLoad(double load) noexcept
{
    lastCpuLoad = load;
    globalCpuLoad.store(load);
}

//==============================================================================
// Прикрепить лейбл для BPM
void VSTHostComponent::setBpmDisplayLabel(Label* label) noexcept
{
    bpmLabel = label;
}

//==============================================================================
// Инжектировать looper engine в callback
void VSTHostComponent::setLooperEngine(LooperEngine* engine) noexcept
{
    if (processCb)
        processCb->setLooperEngine(engine);
}
// Инжектировать plaer engine в callback
void VSTHostComponent::setFilePlayerEngine(FilePlayerEngine* engine) noexcept
{
    if (processCb)
        processCb->setFilePlayerEngine(engine); // проброс в аудио-коллбэк
}

//==============================================================================
// Применить новые аудио-настройки
void VSTHostComponent::setAudioSettings(double sampleRate, int blockSize) noexcept
{
    if (!pluginInstance)
    {
        currentSampleRate = sampleRate;
        currentBlockSize = blockSize;
        return;
    }

    pluginInstance->releaseResources();
    pluginInstance->setPlayConfigDetails(2, 2, sampleRate, blockSize);
    pluginInstance->prepareToPlay(sampleRate, blockSize);

    currentSampleRate = sampleRate;
    currentBlockSize = blockSize;
}

//==============================================================================
// Загрузка плагина и подготовка к проигрыванию
void VSTHostComponent::loadPlugin(const File& file,
    double   sampleRate,
    int      blockSize)
{
    unloadPlugin();

    if (formatManager.getNumFormats() == 0)
        formatManager.addDefaultFormats();

    OwnedArray<PluginDescription> descr;
    AudioPluginFormat* chosen = nullptr;

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

    pluginInstance = std::move(instance);
    currentSampleRate = sampleRate;
    currentBlockSize = blockSize;

    playHead = std::make_unique<CustomAudioPlayHead>();
    playHead->setBpm(120.0);
    playHead->setActive(true);
    pluginInstance->setPlayHead(playHead.get());

    pluginInstance->setPlayConfigDetails(2, 2, sampleRate, blockSize);
    pluginInstance->prepareToPlay(sampleRate, blockSize);

    processCb->setPlugin(pluginInstance.get());

    pluginEditor.reset(pluginInstance->createEditor());
    if (pluginEditor)
    {
        addAndMakeVisible(*pluginEditor);
        pluginEditor->toBack();
        pluginEditor->addComponentListener(this);
    }

    for (auto* p : pluginInstance->getParameters())
        p->addListener(this);

    for (auto& b : presetBtn) b.setVisible(true), b.setEnabled(true);
    for (auto& b : learnBtn)  b.setVisible(true), b.setEnabled(true);

    activePreset = jlimit(0, kNumPresets - 1,
        pluginInstance->getCurrentProgram());
    updatePresetColours();
    updateLearnColours();
    resized();
}

//==============================================================================
// Выгрузка плагина, сброс состояний и UI
void VSTHostComponent::unloadPlugin()
{
    DBG("🐶 Bulldog: unloadPlugin() called");
    if (!pluginInstance)
        return;

    processCb->setPlugin(nullptr);

    if (pluginEditor)
    {
        pluginEditor->removeComponentListener(this);
        removeChildComponent(pluginEditor.get());
        pluginEditor.reset();
        resetBPM();
    }

    for (auto* p : pluginInstance->getParameters())
        p->removeListener(this);

    if (playHead)
        playHead->setActive(false);

    pluginInstance->releaseResources();
    pluginInstance.reset();

    updatePluginCpuLoad(0.0);

    for (auto& b : presetBtn) b.setVisible(false);
    for (auto& b : learnBtn)  b.setVisible(false);

    activePreset = 0;
    updatePresetColours();
    updateLearnColours();
    resized();
}

//==============================================================================
// Обработка сообщений от PluginManager
void VSTHostComponent::changeListenerCallback(ChangeBroadcaster* src)
{
    if (src == &deviceMgr && pluginInstance)
    {
        if (auto* dev = deviceMgr.getCurrentAudioDevice())
        {
            auto sr = dev->getCurrentSampleRate();
            auto bs = dev->getCurrentBufferSizeSamples();
            auto ins = dev->getActiveInputChannels().countNumberOfSetBits();
            auto outs = dev->getActiveOutputChannels().countNumberOfSetBits();

            // 1) Перестраиваем сам плагин
            pluginInstance->releaseResources();
            pluginInstance->setPlayConfigDetails(ins, outs, sr, bs);
            pluginInstance->prepareToPlay(sr, bs);

            currentSampleRate = sr;
            currentBlockSize = bs;

            // 2) **Важно** — пересобираем весь PluginProcessCallback
            //    Это заново вызовет prepare() у looper, тюнеров и OutControl
            processCb->audioDeviceStopped();
            processCb->audioDeviceAboutToStart(dev);
        }
    }

    // 3) Ваш старый код по PluginManager…
    pluginFiles.clear();
    for (auto& e : pluginManager.getPluginsSnapshot())
        if (e.enabled)
            pluginFiles.emplace_back(File(e.getPath()));
    repaint();
}

//==============================================================================
// Инициализация кнопок и UI
void VSTHostComponent::initialiseComponent()
{
    for (int i = 0; i < kNumPresets; ++i)
    {
        auto& b = presetBtn[i];
        b.setButtonText("SCENE " + String(i + 1));
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
// Обновление цветов preset-кнопок
void VSTHostComponent::updatePresetColours()
{
    static const Colour offColour{ 0xFF404040 };
    static const Colour onColour{ 0xFF2A61FF };

    for (int i = 0; i < kNumPresets; ++i)
    {
        bool isActive = (i == activePreset);
        auto& b = presetBtn[i];
        b.setColour(TextButton::buttonColourId, isActive ? onColour : offColour);
        b.setColour(TextButton::textColourOffId, isActive ? Colours::white : Colours::black);
    }
}

//==============================================================================
// Обновление цветов learn-кнопок
void VSTHostComponent::updateLearnColours()
{
    static const Colour offColour{ 0xFF0A2A0A };
    static const Colour onColour{ 0xFF24B324 };

    for (auto& b : learnBtn)
    {
        b.setColour(TextButton::buttonColourId, offColour);
        b.setColour(TextButton::buttonOnColourId, onColour);
        b.setColour(TextButton::textColourOffId, Colours::white);
        b.setColour(TextButton::textColourOnId, Colours::black);
    }
}

//==============================================================================
// Обработчики UI-кнопок
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

//==============================================================================
// Параметры плагина из аудио-потока
void VSTHostComponent::parameterValueChanged(int idx, float norm)
{
    auto safe = Component::SafePointer<VSTHostComponent>(this);
    MessageManager::callAsync([safe, idx, norm]()
        {
            if (safe && safe->paramChangeCb)
                safe->paramChangeCb(idx, norm);
        });
}

//==============================================================================
// Отрисовка и компоновка
void VSTHostComponent::paint(Graphics& g)
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

    layoutPluginEditor(area.reduced(4));

    for (auto& b : presetBtn) b.toFront(false);
    for (auto& b : learnBtn)  b.toFront(false);
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

void VSTHostComponent::componentMovedOrResized(Component& comp,
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
// Внешняя установка пресета
void VSTHostComponent::setExternalPresetIndex(int idx) noexcept
{
    if (pluginInstance)
        //     pluginInstance->setCurrentProgram(idx);/////////////////////////////////////////////////////////////////////////TEST 

        activePreset = idx;
    updatePresetColours();

    if (presetCb)
    {
        auto safe = Component::SafePointer<VSTHostComponent>(this);
        MessageManager::callAsync([safe, idx]()
            {
                if (auto* host = safe.getComponent())
                    host->presetCb(idx);
            });
    }
}

//==============================================================================
// Внешняя установка Learn-режима
void VSTHostComponent::setExternalLearnState(int cc, bool on) noexcept
{
    if (cc >= 0 && cc < kNumLearn)
        learnBtn[cc].setToggleState(on, dontSendNotification);

    updateLearnColours();

    if (learnCb)
    {
        auto safe = Component::SafePointer<VSTHostComponent>(this);
        MessageManager::callAsync([safe, cc, on]()
            {
                if (auto* host = safe.getComponent())
                    host->learnCb(cc, on);
            });
    }
}

//==============================================================================
//==============================================================================
// Добавить новый тюнер
void VSTHostComponent::addTuner(TunerComponent* t) noexcept
{
    if (!t) return;
    processCb->setTuner(t);
}

//==============================================================================
// Удалить тюнер
void VSTHostComponent::removeTuner(TunerComponent* t) noexcept
{
    if (!t) return;
    processCb->removeTuner(t);
}
//==============================================================================
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
void VSTHostComponent::setPluginParameter(int ccNumber, int ccValue) noexcept
{
    if (!pluginInstance) return;

    auto& params = pluginInstance->getParameters();
    if (ccNumber < 0 || ccNumber >= (int)params.size()) return;

    float norm = juce::jlimit(0.0f, 1.0f, float(ccValue) / 127.0f);

    if (auto* param = params[ccNumber])
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost(norm);
        param->endChangeGesture();
    }
}

//==============================================================================
// updateBPM
void VSTHostComponent::updateBPM(double newBPM)
{
    if (playHead)
        playHead->setBpm(newBPM);

    if (bpmLabel != nullptr)
    {
        bpmLabel->setText(String(newBPM, 2) + " BPM", dontSendNotification);
        bpmLabel->repaint();
    }

    if (onBpmChanged)
        onBpmChanged(newBPM);
}

double VSTHostComponent::getCurrentBPM() const
{
    if (playHead != nullptr)
        return playHead->getBpm(); // метод getBpm() мы добавили в CustomAudioPlayHead
    return 120.0; // дефолт, если playHead ещё не создан
}

void VSTHostComponent::setOutControlComponent(OutControlComponent* oc) noexcept
{
    outControl = oc;
    processCb->setOutControl(oc);
}
bool VSTHostComponent::isPluginEditorOpen() const noexcept
{
    return pluginEditor != nullptr;
}

void VSTHostComponent::closePluginEditorIfOpen()
{
    if (pluginEditor)
    {
        pluginEditor->removeComponentListener(this);
        removeChildComponent(pluginEditor.get());
        pluginEditor.reset();
    }
}

void VSTHostComponent::openPluginEditorIfNeeded()
{
    if (!pluginInstance || pluginEditor)
        return;

    if (auto* ed = pluginInstance->createEditor())
    {
        pluginEditor.reset(ed);
        addAndMakeVisible(*pluginEditor);
        pluginEditor->toBack();
        pluginEditor->addComponentListener(this);
        resized(); // чтобы разместить редактор
    }
}
