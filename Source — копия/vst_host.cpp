#include "vst_host.h"
#include "plugin_process_callback.h"
#include "custom_audio_playhead.h"
#include "cpu_load.h"
#include "TunerComponent.h"
#include "OutControlComponent.h"
#include "InputControlComponent.h" 
#include "bank_editor.h"

struct BigEmojiLookAndFeel : juce::LookAndFeel_V4
{
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        // увеличиваем шрифт, эмодзи тоже станут больше
        return juce::Font ((float) buttonHeight * 0.8f);
    }
};

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
    pluginManager(pm),                // ссылка на уже инициализированный менеджер
    currentSampleRate(44100.0),
    currentBlockSize(512)
{
    processCb = std::make_unique<PluginProcessCallback>();
    processCb->setHostComponent(this);

    if (inputControl != nullptr)
        processCb->setInputControl(inputControl);

    // Подписываемся на события аудио-движка и запускаем обработку
    startAudio();

    // Подписываемся на изменения списка плагинов
    pluginManager.addChangeListener(this);
    changeListenerCallback(nullptr);
    initialiseComponent();

    for (int i = 0; i < 10; ++i)
    {
        auto* btn = new juce::TextButton("<none>");
        btn->setColour(juce::TextButton::buttonColourId, juce::Colours::maroon);
        btn->setColour(juce::TextButton::buttonOnColourId, juce::Colours::red);
        btn->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        btn->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        btn->setClickingTogglesState(true);

        ccButtons.add(btn);
        addChildComponent(btn);
        btn->setVisible(false);

        // 🔹 Добавляем обработчик клика
        btn->onClick = [this, i]() {
            bool state = ccButtons[i]->getToggleState();
            if (onCCButtonClicked)
                onCCButtonClicked(i, state);
            };
    }
// метки 
    activeBankLabel.setText("Preset: ---", juce::dontSendNotification);
    activeBankLabel.setJustificationType(juce::Justification::centred);
    activeBankLabel.setColour(juce::Label::backgroundColourId, juce::Colours::white);
    activeBankLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    activeBankLabel.setFont(juce::Font(25.0f, juce::Font::bold)); // увеличенный текст
    addAndMakeVisible(activeBankLabel);
    activeBankLabel.setVisible(false);

    activeLibraryLabel.setText("Bank: ---", juce::dontSendNotification);
    activeLibraryLabel.setJustificationType(juce::Justification::centred);
    activeLibraryLabel.setColour(juce::Label::backgroundColourId, juce::Colours::white);
    activeLibraryLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    activeLibraryLabel.setFont(juce::Font(25.0f, juce::Font::bold)); // увеличенный текст
    addAndMakeVisible(activeLibraryLabel);
    activeLibraryLabel.setVisible(false);

    addAndMakeVisible(presetBackground);
    presetBackground.setColour(juce::Label::backgroundColourId, juce::Colours::white);
    presetBackground.setText("", juce::dontSendNotification);
    presetBackground.toBack(); // сразу отправляем назад

    addAndMakeVisible(bottomBackground);
    presetBackground.setVisible(false);
    bottomBackground.setVisible(false);
    //
    addAndMakeVisible(toggleLearnBlockButton);
    toggleLearnBlockButton.setVisible(false);
    toggleLearnBlockButton.setButtonText(juce::String::fromUTF8("🔽")); // изначально блок виден
    toggleLearnBlockButton.setLookAndFeel(&bigIconLF);
    toggleLearnBlockButton.setBounds(10, 10, 80, 25); // левый верхний угол
    toggleLearnBlockButton.onClick = [this]()
        {
            learnBlockVisible = !learnBlockVisible;
            for (auto& b : learnBtn)  b.setVisible(learnBlockVisible);
            for (auto& b : ccButtons) b->setVisible(learnBlockVisible);
            bottomBackground.setVisible(learnBlockVisible);
            toggleLearnBlockButton.setButtonText(
                juce::String::fromUTF8(learnBlockVisible ? "🔽" : "🔼")
            );
            resized();
        };
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

    bpmEditor = nullptr;

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
    /*
    {
        auto settingsFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("PluginSettings.xml");
        pluginManager.saveToFile(settingsFile);
    }
    */
    toggleLearnBlockButton.setLookAndFeel(nullptr);
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
void VSTHostComponent::setBpmDisplayEditor(juce::TextEditor* editor) noexcept
{
    bpmEditor = editor;
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
    for (auto* btn : ccButtons) btn->setVisible(true);

    presetBackground.setVisible(true);
    bottomBackground.setVisible(true);
    activeBankLabel.setVisible(true);
    activeLibraryLabel.setVisible(true);
    toggleLearnBlockButton.setVisible(true);

    activePreset = jlimit(0, kNumPresets - 1,
        pluginInstance->getCurrentProgram());
    updatePresetColours();
    updateLearnColours();
    resized();

    // 🔹 Сброс BPM после загрузки плагина
    resetBPM(); // вызывает updateBPM(120.0) и пробрасывает onBpmChanged
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
    for (auto* lbl : ccButtons)lbl->setVisible(false);
    activeBankLabel.setVisible(false);
    activeLibraryLabel.setVisible(false);
    presetBackground.setVisible(false);
    bottomBackground.setVisible(false);
    toggleLearnBlockButton.setVisible(false);
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
    // Бледно-голубой для неактивных
    static const juce::Colour offColour = juce::Colour::fromRGBA(173, 216, 230, 255); // lightblue
    // Синий для активных
    static const juce::Colour onColour = juce::Colour::fromRGBA(42, 97, 255, 255);   // насыщенный синий

    for (int i = 0; i < kNumPresets; ++i)
    {
        bool isActive = (i == activePreset);
        auto& b = presetBtn[i];

        // фон кнопки
        b.setColour(juce::TextButton::buttonColourId, isActive ? onColour : offColour);

        // цвет текста
        if (isActive)
        {
            b.setColour(juce::TextButton::textColourOnId, juce::Colour::fromRGBA(255, 255, 255, 255)); // белый
            b.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGBA(255, 255, 255, 255)); // тоже белый, чтобы не мигало
        }
        else
        {
            b.setColour(juce::TextButton::textColourOnId, juce::Colour::fromRGBA(0, 0, 0, 255)); // чёрный
            b.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGBA(0, 0, 0, 255)); // чёрный
        }
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
   
}
void VSTHostComponent::resized()
{
    auto fullArea = getLocalBounds();

    // 🔹 Параметры для регулировки
    const int toggleSize = 40;  // размер кнопки
    const int marginX = 1;   // отступ от левого края
    const int marginYAdjust = -3;  // смещение кнопки вверх/вниз (от центра метки)
    const int backgroundHeight = 42;  // высота верхней панели
    const int labelHeight = 35;  // высота метки
    const int labelOffsetY = 4;   // вертикальный отступ метки внутри панели

    // Верхняя панель
    auto top = fullArea.removeFromTop(backgroundHeight);
    presetBackground.setBounds(top);

    // 🔹 Кнопка по центру метки, с регулируемым смещением
    int buttonY = top.getY() + (labelHeight - toggleSize) / 2 + labelOffsetY + marginYAdjust;
    toggleLearnBlockButton.setBounds(top.getX() + marginX,
        buttonY,
        toggleSize,
        toggleSize);

    {
        FlexBox fb;
        fb.flexDirection = FlexBox::Direction::row;
        fb.justifyContent = FlexBox::JustifyContent::center;
        fb.alignContent = FlexBox::AlignContent::center;

        for (auto& b : presetBtn)
            fb.items.add(FlexItem(b)
                .withWidth((float)100)
                .withHeight((float)35)
                .withMargin(FlexItem::Margin(4, 3, 4, 3)));

        fb.performLayout(top);
    }

    // Метки сверху
    int labelWidth = 100 * 3;
    activeLibraryLabel.setBounds(top.getX() + 10,
        top.getY() + labelOffsetY,
        labelWidth, labelHeight);
    activeBankLabel.setBounds(getWidth() - labelWidth - 10,
        top.getY() + labelOffsetY,
        labelWidth, labelHeight);

    // LEARN bar
    auto learnArea = fullArea.removeFromBottom(28);
    {
        FlexBox fb;
        fb.flexDirection = FlexBox::Direction::row;
        fb.justifyContent = FlexBox::JustifyContent::center;
        for (auto& b : learnBtn)
            fb.items.add(FlexItem(b).withMinWidth(110).withMinHeight(24));
        fb.performLayout(learnArea);
    }

    // CC Buttons
    juce::Rectangle<int> ccBoundsTotal;
    for (int i = 0; i < 10; ++i)
    {
        auto learnBounds = learnBtn[i].getBounds();
        auto ccBounds = learnBounds.withY(learnBounds.getY() - learnBounds.getHeight() - 4);
        ccButtons[i]->setBounds(ccBounds);

        if (i == 0) ccBoundsTotal = ccBounds;
        else        ccBoundsTotal = ccBoundsTotal.getUnion(ccBounds);
    }

    // Общая область Learn + CC
    juce::Rectangle<int> buttonsArea;
    for (auto& b : learnBtn)
        buttonsArea = buttonsArea.isEmpty() ? b.getBounds() : buttonsArea.getUnion(b.getBounds());
    buttonsArea = buttonsArea.getUnion(ccBoundsTotal);

    // Фон блока Learn+CC
    bottomBackground.setBounds(buttonsArea.expanded(3, 3));

    // 🔹 область редактора
    auto editorArea = getLocalBounds();
    editorArea.removeFromTop(backgroundHeight);

    if (learnBlockVisible && bottomBackground.isVisible())
    {
        editorArea.removeFromBottom(bottomBackground.getHeight());
        normalEditorBounds = editorArea.reduced(4);
    }
    else
    {
        expandedEditorBounds = editorArea.reduced(4);
    }

    if (pluginEditor)
    {
        if (learnBlockVisible)
            pluginEditor->setBounds(normalEditorBounds);
        else
            pluginEditor->setBounds(expandedEditorBounds);
    }

    // порядок отображения
    presetBackground.toBack();
    bottomBackground.toBack();
    for (auto& b : presetBtn) b.toFront(false);
    activeLibraryLabel.toFront(false);
    activeBankLabel.toFront(false);
    for (auto& b : learnBtn)  b.toFront(false);
    for (auto& b : ccButtons) b->toFront(false);

    // 🔹 кнопка скрытия панели всегда сверху
    toggleLearnBlockButton.toFront(true);
}


void VSTHostComponent::layoutPluginEditor(juce::Rectangle<int> freeArea)
{
    if (!pluginEditor) return;

    auto desired = pluginEditor->getLocalBounds();
    if (desired.getWidth() == 0)  desired.setWidth(freeArea.getWidth());
    if (desired.getHeight() == 0) desired.setHeight(freeArea.getHeight());

    juce::Rectangle<int> fitted = desired;

    fitted.setSize(jmin(freeArea.getWidth(), desired.getWidth()),
        jmin(freeArea.getHeight(), desired.getHeight()));

    // 🔹 верх всегда фиксирован на freeArea.getY()
    fitted.setX(freeArea.getCentreX() - fitted.getWidth() / 2);
    fitted.setY(freeArea.getY());

    pluginEditor->setBounds(fitted);
}
void VSTHostComponent::clampEditorBounds()
{
    auto r = getLocalBounds();
    r.removeFromTop(40);   // верхняя зона — запретная
    r.reduce(4, 4);

    if (!pluginEditor) return;

    auto b = pluginEditor->getBounds();

    int newWidth = jmin(b.getWidth(), r.getWidth());
    int newHeight = jmin(b.getHeight(), r.getHeight());

    int x = r.getCentreX() - newWidth / 2;
    int y = r.getY();

    b.setBounds(x, y, newWidth, newHeight);

    if (b.getBottom() > r.getBottom())
        b.setBottom(r.getBottom());

    pluginEditor->setBounds(b);
}
void VSTHostComponent::componentMovedOrResized(Component& comp,
    bool /*moved*/,
    bool wasResized)
{
    if (pluginEditor && &comp == pluginEditor.get() && wasResized)
        clampEditorBounds();
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
void VSTHostComponent::updateBPM(double newBPM)
{
    currentBpm = newBPM;

    if (playHead)
        playHead->setBpm(newBPM);

    if (onBpmChanged)
        onBpmChanged(newBPM);

    if (bpmEditor != nullptr && !bpmEditor->hasKeyboardFocus(true))
    {
        bpmEditor->setText(juce::String((int)newBPM) + " BPM", juce::dontSendNotification);
    }
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
void VSTHostComponent::updateCCLabel(int slot, const juce::String& text)
{
    if (slot >= 0 && slot < ccButtons.size())
        ccButtons[slot]->setButtonText(text.isNotEmpty() ? text : "<none>");
}
void VSTHostComponent::setCCButtonState(int slot, bool active)
{
    if (slot >= 0 && slot < ccButtons.size())
        ccButtons[slot]->setToggleState(active, juce::dontSendNotification);
}
void VSTHostComponent::updateActiveLibraryBankLabel(BankEditor* bankEditor)
{
    if (!bankEditor)
        return;
    // 🔹 Обновляем метку банка
    auto bankName = bankEditor->getBank(bankEditor->getActiveBankIndex()).bankName;
    if (bankName.isNotEmpty())
        activeBankLabel.setText("Preset: " + bankName, juce::dontSendNotification);
      // 🔹 Обновляем метку библиотеки
    juce::String libName = bankEditor->loadedFileName;
    if (!libName.isEmpty())
    {
        juce::File f = juce::File::getCurrentWorkingDirectory().getChildFile(libName);
        libName = f.getFileNameWithoutExtension();
    }
    activeLibraryLabel.setText("Bank: " + libName, juce::dontSendNotification);
}
void VSTHostComponent::setBankEditor(BankEditor* editor)
{
    bankEditor = editor;

    if (bankEditor)
    {
        bankEditor->onBankEditorChanged = [this]()
            {
                updateActiveLibraryBankLabel(bankEditor);
            };

        bankEditor->onLibraryFileChanged = [this](const juce::File& file)
            {
                updateActiveLibraryBankLabel(bankEditor);
            };
    }
}
