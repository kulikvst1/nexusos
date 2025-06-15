#pragma once
#include <JuceHeader.h>
#include <functional>
#include"plugin_process_callback.h."
#include "cpu_load.h" 
#include "custom_audio_playhead.h"

//==============================================================================
// Класс VSTHostComponent отвечает за загрузку, управление и выгрузку плагина.
class VSTHostComponent : public juce::Component,
   // public juce::ListBoxModel,
    public juce::Button::Listener,
    public juce::AudioProcessorParameter::Listener
{
public:
    using ParamChangeFn = std::function<void(int /*paramIdx*/, float /*0‥1*/)>;
    //────────────────── PRESET API ──────────────────
    static constexpr int kNumPresets = 6;
    static constexpr int kNumLearn = 10;
    using PresetCallback = std::function<void(int)>;         
    void setPresetCallback(PresetCallback cb) noexcept { presetCb = std::move(cb); }
    void setExternalPresetIndex(int idx);
    void updatePresetColours();               //  локальная перекраска
    void setParameterChangeCallback(ParamChangeFn cb) { paramChangeCb = std::move(cb); }
    void updateLearnColours();
    // ───── Learn API ────────────────────────────
    using LearnCallback = std::function<void(int /*cc*/, bool /*on*/)>;

    void setLearnCallback(LearnCallback cb) noexcept { learnCb = std::move(cb); }
    void setExternalLearnState(int cc, bool on);               // ← зовёт BankEditor


    // Конструктор по умолчанию с использованием дефолтного менеджера аудио устройств
    VSTHostComponent()
        : audioDeviceManager(getDefaultAudioDeviceManager()),
        currentSampleRate(44100),
        currentBlockSize(512)
    {
        initialiseComponent();
    }

    // Конструктор с заданным AudioDeviceManager
    VSTHostComponent(juce::AudioDeviceManager& adm)
        : audioDeviceManager(adm),
        currentSampleRate(44100),
        currentBlockSize(512)
    {
        initialiseComponent();
    }

    // Метод для обновления загрузки CPU плагина 
    void updatePluginCpuLoad(double load)
    {
        lastPluginCpuLoad = load;
        globalCpuLoad.store(load);
    }
    double getLastPluginCpuLoad() const { return lastPluginCpuLoad; }

    // Возвращает ссылку на менеджер аудио устройств
    juce::AudioDeviceManager& getAudioDeviceManagerRef()
    {
        return audioDeviceManager;
    }
    // Новый метод для установки указателя на метку
    void setBpmDisplayLabel(juce::Label* label)
    {
        bpmDisplay = label;
    }

    // Метод, который обновляет темп (BPM) в нашем CustomAudioPlayHead
    void updateBPM(double newBPM)
    {
        if (customAudioPlayHead)
            customAudioPlayHead->setBpm(newBPM);

        // Обновляем метку, если указатель установлен
        if (bpmDisplay != nullptr)
        {
            bpmDisplay->setText(juce::String(newBPM, 2) + " BPM", juce::dontSendNotification);
            bpmDisplay->repaint();  // на всякий случай
        }
    }
    // Метод для сброса BPM до значения по умолчанию (здесь 120 BPM)
    void resetBPM()
    {
        const double defaultBPM = 120.0;
        updateBPM(defaultBPM);

    }
    ~VSTHostComponent() override
    {
        unloadPlugin();
        resetBPM();
    }
    // Метод для управления параметрами плагина 
    void setPluginParameter(int ccNumber, int ccValue)
    {
        if (pluginInstance != nullptr)
        {
            auto& params = pluginInstance->getParameters();
            if (ccNumber >= 0 && ccNumber < params.size())
            {
                float normalizedValue = static_cast<float>(ccValue) / 127.f;
                pluginInstance->setParameterNotifyingHost(ccNumber, normalizedValue);
            }
        }
    }
   
    // --- Обработка нажатий кнопок ---
    void buttonClicked(juce::Button* button) override
    {
        // --- LEARN -------------------------------------------------
        for (int cc = 0; cc < kNumLearn; ++cc)
            if (button == &learnBtn[cc])
            {
                bool on = learnBtn[cc].getToggleState();
                updateLearnColours();          // ★ локально перекрасить
                if (learnCb) learnCb(cc, on); // ★ → BankEditor
                return;
            }

        // ---------- пресет-кнопки ----------
        for (int i = 0; i < kNumPresets; ++i)
        {
            if (button == &presetBtn[i])
            {
                if (activePreset != i && pluginInstance != nullptr)
                {
                    activePreset = i;
                    pluginInstance->setCurrentProgram(i);   // фактически меняем пресет
                    updatePresetColours();
                    if (presetCb) presetCb(i);              // → BankEditor
                }
                return;  // событие обработали
            }
        }
        
    }
    void VSTHostComponent::resized() override
    {
        if (pluginInstance != nullptr && pluginEditor != nullptr)
        {
            auto area = getLocalBounds();          // ← одна переменная на всё
            /* ── верхняя полоска ───────────────────────────── */
            auto topBar = area.removeFromTop(32);

            juce::FlexBox fb;
            fb.flexDirection = juce::FlexBox::Direction::row;
            fb.justifyContent = juce::FlexBox::JustifyContent::center;
            fb.alignItems = juce::FlexBox::AlignItems::center;

            constexpr int kBtnW = 120, kBtnH = 30, kMargin = 4;
            for (auto& b : presetBtn)
                fb.items.add(juce::FlexItem(b)
                    .withWidth(kBtnW)
                    .withHeight(kBtnH)
                    .withMargin(kMargin));

            fb.performLayout(topBar);

            /* ── нижняя полоса Learn ───────────────────────── */
            auto bottomBar = area.removeFromBottom(30);   // area, а не r

            juce::FlexBox learnFB;
            learnFB.flexDirection = juce::FlexBox::Direction::row;
            learnFB.justifyContent = juce::FlexBox::JustifyContent::center;
            learnFB.alignItems = juce::FlexBox::AlignItems::center;

            constexpr int w = 60, h = 24, gap = 2;
            for (auto& b : learnBtn)
                learnFB.items.add(juce::FlexItem(b)
                    .withWidth(w)
                    .withHeight(h)
                    .withMargin(gap));

            learnFB.performLayout(bottomBar);

            /* ── плагин-редактор в оставшейся области ─────── */
            layoutPluginEditor(area.reduced(4));
        }
    }

    // Метод для установки аудионастроек (sample rate и block size)
    void setAudioSettings(double sampleRate, int blockSize)
    {
        currentSampleRate = sampleRate;
        currentBlockSize = blockSize;
    }
    // --- Загрузка плагина ---
    void loadPlugin(const juce::File& pluginFile, double sampleRate, int blockSize)
    {
        unloadPlugin();
        juce::AudioPluginFormat* chosenFormat = nullptr;
        juce::OwnedArray<juce::PluginDescription> pluginDescs;

        for (auto* format : formatManager.getFormats())
        {
            format->findAllTypesForFile(pluginDescs, pluginFile.getFullPathName());
            if (pluginDescs.size() > 0)
            {
                chosenFormat = format;
                break;
            }
        }

        if (chosenFormat == nullptr || pluginDescs.size() == 0)
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                "Error",
                "No plugin description found in the file.");
            return;
        }
        juce::PluginDescription desc = *pluginDescs[0];
        juce::String error;
        pluginInstance = chosenFormat->createInstanceFromDescription(desc, sampleRate, blockSize, error);

        if (pluginInstance == nullptr)
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                "Error",
                "Failed to create plugin instance:\n" + error);
            return;
        }

        int numInputs = 2;
        int numOutputs = 2;
        pluginInstance->setPlayConfigDetails(numInputs, numOutputs, sampleRate, blockSize);
        currentSampleRate = sampleRate;
        currentBlockSize = blockSize;
        pluginInstance->prepareToPlay(currentSampleRate, currentBlockSize);

        // Создаём transport (CustomAudioPlayHead) и передаём его в плагин
        double defaultBPM = 120.0;
        customAudioPlayHead = std::make_unique<CustomAudioPlayHead>();
        customAudioPlayHead->setBpm(defaultBPM);
        customAudioPlayHead->setActive(true);
        pluginInstance->setPlayHead(customAudioPlayHead.get());

        // Создаём и регистрируем callback для обработки аудио
        pluginProcessCallback = std::make_unique<PluginProcessCallback>(pluginInstance.get(), currentSampleRate);
        pluginProcessCallback->setHostComponent(this);
        audioDeviceManager.addAudioCallback(pluginProcessCallback.get());

        // Если плагин предоставляет редактор, добавляем его на компонент
        pluginEditor = pluginInstance->createEditor();
        if (pluginEditor != nullptr)
        {
            addAndMakeVisible(pluginEditor);
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                "Info",
                "Plugin does not provide an editor.");
        }
        for (auto* p : pluginInstance->getParameters())
            p->addListener(this);//++++++++++++++++++++++++++++++++++++++++++++++++++++++
                
        for (int i = 0; i < kNumPresets; ++i)
            presetBtn[i].setVisible(true);
        for (int i = 0; i < kNumLearn; ++i)
            learnBtn[i].setVisible(true);

            activePreset = juce::jlimit(0, kNumPresets - 1,
            pluginInstance->getCurrentProgram());
            updatePresetColours();
            resized();
    }

    // Метод для получения указателя на AudioPluginInstance (если необходимо)
    juce::AudioPluginInstance* getPluginInstance() { return pluginInstance.get(); }
    
    juce::Array<juce::File> pluginFiles;

    //==================================================================
    void unloadPlugin()                                      
    {
        /*--------------------------------------------------*
         | 1. Если плагина нет – просто гарантируем дефолт  |
         *--------------------------------------------------*/
        if (pluginInstance == nullptr)
        {
            resetBPM();          // 120 BPM по-умолчанию
            return;
        }
        /*-------------------- 2. Audio callback ---------------------*/
        if (pluginProcessCallback)
        {
            audioDeviceManager.removeAudioCallback(pluginProcessCallback.get());
            pluginProcessCallback.reset();
            updatePluginCpuLoad(0.0);               // обнуляем индикатор CPU
        }
        /*--------------------- 3. Окно редактора --------------------*/
        if (pluginEditor != nullptr)
        {
            removeChildComponent(pluginEditor);
            delete pluginEditor;
            pluginEditor = nullptr;
        }
        /*------------------- 4. Слушатели параметров ---------------*/
        for (auto* p : pluginInstance->getParameters())
            p->removeListener(this);
        /*---------------- 5. Де-активируем play-head ----------------*/
        if (customAudioPlayHead)
            customAudioPlayHead->setActive(false);
        /*---------------------- 6. Финальный reset ------------------*/
        pluginInstance.reset();        // уничтожаем сам процессор
        resetBPM();                    // ВСЕГДА возвращаемся к 120

        for (int i = 0; i < kNumPresets; ++i)
            presetBtn[i].setVisible(false);
        for (int i = 0; i < kNumLearn; ++i)
            learnBtn[i].setVisible(false);

            activePreset = 0;
            updatePresetColours();
            resized();                     
    }

    void VSTHostComponent::parameterValueChanged(int parameterIndex, float newValue)
    {
        // приходит из АУДИО-потока → маршаллируем в GUI-поток
        if (paramChangeCb)
            juce::MessageManager::callAsync([=]
                {
                    if (paramChangeCb) paramChangeCb(parameterIndex, newValue);
                });
    }
   
    void VSTHostComponent::parameterGestureChanged(int, bool) {}   // заглушка
    const juce::Array<juce::File>& getPluginFiles() const noexcept
    {
        return pluginFiles;
    }
    const juce::AudioPluginInstance* getPluginInstance() const noexcept { return pluginInstance.get(); }
    int getPluginParametersCount() const noexcept;   
    
private:
    void initialiseComponent()
    {
        formatManager.addDefaultFormats();
#ifdef JUCE_WINDOWS
        juce::File pluginDirectory("C:\\Program Files\\Common Files\\VST3\\");
#else
        juce::File pluginDirectory("C:\\Program Files\\Common Files\\VST3\\");
#endif
        if (pluginDirectory.exists())
            pluginDirectory.findChildFiles(pluginFiles, juce::File::findFiles, true, "*.vst3");

        // Здесь создаются и настраиваются все элементы управления.
        // создаём 6 пресет-кнопок, пока прячем
        for (int i = 0; i < kNumPresets; ++i)
        {
            presetBtn[i].setButtonText("PRESET " + juce::String(i + 1));
            presetBtn[i].addListener(this);
            presetBtn[i].setVisible(false);
            addAndMakeVisible(presetBtn[i]);
        }
        // --- 10 learn-тумблеров, пока скрыты ---------------------------
        for (int i = 0; i < kNumLearn; ++i)
        {
            learnBtn[i].setButtonText("LEARN " + juce::String(i + 1));
            learnBtn[i].setClickingTogglesState(true);
            learnBtn[i].addListener(this);
            learnBtn[i].setVisible(false);          // появятся после loadPlugin()
            addAndMakeVisible(learnBtn[i]);
            
        }


    }
    // Расставляет pluginEditor по центру и гарантирует
    void layoutPluginEditor(juce::Rectangle<int> contentArea)
    {
        if (pluginEditor == nullptr)
        return;
        int w = pluginEditor->getWidth(), h = pluginEditor->getHeight();
        if (w == 0 || h == 0)
        {
            auto b = pluginEditor->getBounds();
            w = (b.getWidth() > 0 ? b.getWidth() : 400);
            h = (b.getHeight() > 0 ? b.getHeight() : 300);
        }
        juce::Rectangle<int> e(w, h);
        e.setCentre(contentArea.getCentre());
        e.setX(juce::jlimit(contentArea.getX(),
        contentArea.getRight() - e.getWidth(), e.getX()));
        e.setY(juce::jlimit(contentArea.getY(),
        contentArea.getBottom() - e.getHeight(), e.getY()));
        pluginEditor->setBounds(e);
    }
    
    static juce::AudioDeviceManager& getDefaultAudioDeviceManager()
    {
        static juce::AudioDeviceManager defaultManager;
        static bool isInitialised = false;
        if (!isInitialised)
        {
            juce::String err = defaultManager.initialiseWithDefaultDevices(2, 2);
            if (!err.isEmpty())
            isInitialised = true;
        }
        return defaultManager;
    }

    ParamChangeFn paramChangeCb;   // хранит внешний слушатель
    int                 currentBlockSize;
    int selectedIndex = -1;
    int              activePreset = 0;      // какой preset сейчас подсвечен
    double lastPluginCpuLoad = 0.0;
    double              currentSampleRate;
    std::unique_ptr<PluginProcessCallback> pluginProcessCallback;
    std::unique_ptr<CpuLoadIndicator> cpuLoadIndicator;
    std::unique_ptr<CustomAudioPlayHead> customAudioPlayHead;
    std::unique_ptr<juce::AudioPluginInstance> pluginInstance;
    juce::AudioPluginFormatManager formatManager;
    juce::AudioDeviceManager& audioDeviceManager;
    juce::Component* pluginEditor = nullptr;
    juce::Label* bpmDisplay = nullptr;
    juce::TextButton presetBtn[kNumPresets];
    PresetCallback   presetCb{};

    juce::TextButton learnBtn[kNumLearn];
    LearnCallback    learnCb{};
    

    //JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VSTHostComponent)
};
//   приходит из BankEditor → просто перекрасить
inline void VSTHostComponent::setExternalPresetIndex(int idx)
{
    if (idx == activePreset || idx < 0 || idx >= kNumPresets) return;
    activePreset = idx;
    updatePresetColours();                 // тихо, без presetCb
}
//   перекрасить все кнопки
inline void VSTHostComponent::updatePresetColours()
{
    for (int i = 0; i < kNumPresets; ++i)
    {
        bool on = (i == activePreset);
        presetBtn[i].setColour(juce::TextButton::buttonColourId,
            on ? juce::Colours::blue : juce::Colours::grey);
        presetBtn[i].setColour(juce::TextButton::textColourOffId,
            on ? juce::Colours::white : juce::Colours::black);
    }
}
inline void VSTHostComponent::setExternalLearnState(int cc, bool on)
{
    if (cc < 0 || cc >= kNumLearn) return;
    learnBtn[cc].setToggleState(on, juce::dontSendNotification);
    updateLearnColours();
}
inline void VSTHostComponent::updateLearnColours()
{
    for (int i = 0; i < kNumLearn; ++i)
    {
        bool on = learnBtn[i].getToggleState();

        learnBtn[i].setColour(juce::TextButton::buttonColourId,
            on ? juce::Colours::green      // фон
            : juce::Colours::darkgrey);

        learnBtn[i].setColour(juce::TextButton::textColourOffId,
            on ? juce::Colours::black          // текст
            : juce::Colours::white);
    }
}

// ────────────────────────  inline-реализация  ────────────────────────
inline int VSTHostComponent::getPluginParametersCount() const noexcept
{
    if (auto* inst = getPluginInstance())          // здесь уже пакетно совпадают const-квалификаторы
    {
#if JUCE_VERSION < 0x060000
        return inst->getNumParameters();
#else
        return static_cast<int>(inst->getParameters().size());
#endif
    }
    return 0;
}
