#pragma once

#include <JuceHeader.h>
#include"plugin_process_callback.h."
#include "cpu_load.h" // Если вы ещё не подключали ваш индикатор CPU загрузки
#include "custom_audio_playhead.h"
#include <vector>

/// Описывает один параметр плагина:
struct ParameterInfo
{
    int    index;      // тот самый номер параметра (0…N-1)
    juce::String name; // человеко-читаемое имя
};
//==============================================================================
// Компонент для редактирования параметров плагина (если плагин предоставляет редактор параметров)
class PluginParameterEditorComponent : public juce::Component,
    public juce::Slider::Listener
{
public:
    PluginParameterEditorComponent(juce::AudioPluginInstance* plugin)
        : pluginInstance(plugin)
    {
        if (pluginInstance != nullptr)
        {
            auto& params = pluginInstance->getParameters();
            for (int i = 0; i < params.size(); ++i)
            {
                // Создаем слайдер для изменения значения параметра (в диапазоне 0.0..1.0)
                auto* slider = new juce::Slider();
                slider->setRange(0.0, 1.0, 0.001);
                slider->setValue(params[i]->getValue(), juce::dontSendNotification);
                slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
                slider->addListener(this);
                addAndMakeVisible(slider);
                parameterSliders.add(slider);

                // Создаем метку с именем параметра
                auto* label = new juce::Label();
                label->setText(params[i]->getName(100), juce::dontSendNotification);
                addAndMakeVisible(label);
                parameterLabels.add(label);
            }
            setSize(400, params.size() * 40 + 20);
        }
    }

    ~PluginParameterEditorComponent() override { }

    void sliderValueChanged(juce::Slider* slider) override
    {
        auto& params = pluginInstance->getParameters();
        for (int i = 0; i < parameterSliders.size(); ++i)
        {
            if (parameterSliders[i] == slider)
            {
                float newValue = static_cast<float> (slider->getValue());
                if (pluginInstance != nullptr)
                    pluginInstance->setParameterNotifyingHost(i, newValue);
                break;
            }
        }
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        int y = 10;
        for (int i = 0; i < parameterSliders.size(); ++i)
        {
            auto area = juce::Rectangle<int>(10, y, getWidth() - 20, 30);
            parameterLabels[i]->setBounds(area.removeFromLeft(120));
            parameterSliders[i]->setBounds(area);
            y += 40;
        }
    }

private:
    juce::AudioPluginInstance* pluginInstance;
    juce::OwnedArray<juce::Slider> parameterSliders;
    juce::OwnedArray<juce::Label> parameterLabels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginParameterEditorComponent)
};

//==============================================================================
// Класс VSTHostComponent отвечает за загрузку, управление и выгрузку плагина.
class VSTHostComponent : public juce::Component,
    public juce::ListBoxModel,
    public juce::Button::Listener
{
public:
    


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

    // Метод для обновления загрузки CPU плагина (не относится к BPM, оставлен как есть)
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
    // и одновременно обновляет текст метки отображения BPM
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
    // Метод для управления параметрами плагина (например, через MIDI CC)
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
    // --- Реализация интерфейса ListBoxModel ---
    int getNumRows() override { return pluginFiles.size(); }

    void paintListBoxItem(int rowNumber, juce::Graphics& g,
        int width, int height, bool rowIsSelected) override
    {
        if (rowNumber < pluginFiles.size())
        {
            if (rowIsSelected)
                g.fillAll(juce::Colours::lightblue);
            g.setColour(juce::Colours::white);
            g.drawText(pluginFiles[rowNumber].getFileName(), 2, 0, width - 4, height,
                juce::Justification::centredLeft);
        }
    }

    void listBoxItemClicked(int row, const juce::MouseEvent&) override
    {
        selectedIndex = row;
    }

    // --- Обработка нажатий кнопок ---
    void buttonClicked(juce::Button* button) override
    {
        if (button == &loadButton)
        {
            if (selectedIndex >= 0 && selectedIndex < pluginFiles.size())
            {
                auto pluginFile = pluginFiles[selectedIndex];
                loadPlugin(pluginFile, currentSampleRate, currentBlockSize);
                saveSelectedPluginFile(pluginFile);
            }
        }
        else if (button == &closeButton)
        {
            unloadPlugin();
            clearSavedPluginFile();
            repaint();
            // Сброс BPM при закрытии плагина
            resetBPM();
        }
        else if (button == &parametersButton)
        {
            if (pluginInstance != nullptr)
            {
                auto* paramEditor = new PluginParameterEditorComponent(pluginInstance.get());
                juce::DialogWindow::showDialog("Plugin Parameters",
                    paramEditor,
                    nullptr,
                    juce::Colours::lightgrey,
                    true);
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                    "Info",
                    "Plugin is not loaded!");
            }
        }

    }

    void resized() override
    {
        auto r = getLocalBounds();
        if (pluginInstance != nullptr && pluginEditor != nullptr)
        {
            auto topBar = r.removeFromTop(30);
            closeButton.setBounds(topBar.removeFromLeft(100).reduced(4));
            parametersButton.setBounds(topBar.removeFromLeft(150).reduced(4));
            pluginEditor->setBounds(r.reduced(4));
        }
        else
        {
            auto listArea = r.removeFromTop(r.getHeight() - 40);
            pluginListBox.setBounds(listArea.reduced(4));
            auto buttonArea = r;
            loadButton.setBounds(buttonArea.removeFromLeft(buttonArea.getWidth() / 2).reduced(4));
            parametersButton.setVisible(false);
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
        DBG(juce::String::formatted("Plugin setPlayConfigDetails called with numInputs = %d, numOutputs = %d, sampleRate = %f, blockSize = %d",
            numInputs, numOutputs, sampleRate, blockSize));

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

        pluginListBox.setVisible(false);
        loadButton.setVisible(false);
        closeButton.setVisible(true);
        parametersButton.setVisible(true);
        resized();
    }

    // Метод для получения указателя на AudioPluginInstance (если необходимо)
    juce::AudioPluginInstance* getPluginInstance() { return pluginInstance.get(); }

    void switchPresetUp()
    {
        if (pluginInstance != nullptr)
        {
            int currentProgram = pluginInstance->getCurrentProgram();
            int numPrograms = pluginInstance->getNumPrograms();

            if (numPrograms > 0 && currentProgram < (numPrograms - 1))
            {
                pluginInstance->setCurrentProgram(currentProgram + 1);
                DBG("Switched preset UP to: " + juce::String(pluginInstance->getCurrentProgram()));
            }
            else
            {
                DBG("Already at highest preset or no presets available.");
            }
        }
        else
        {
            DBG("Plugin is not loaded - cannot switch preset.");
        }
    }

    // Переключение на предыдущий пресет (DOWN)
    void switchPresetDown()
    {
        if (pluginInstance != nullptr)
        {
            int currentProgram = pluginInstance->getCurrentProgram();

            if (pluginInstance->getNumPrograms() > 0 && currentProgram > 0)
            {
                pluginInstance->setCurrentProgram(currentProgram - 1);
                DBG("Switched preset DOWN to: " + juce::String(pluginInstance->getCurrentProgram()));
            }
            else
            {
                DBG("Already at lowest preset or no presets available.");
            }
        }
        else
        {
            DBG("Plugin is not loaded - cannot switch preset.");
        }
    }

    std::vector<ParameterInfo> getCurrentPluginParameters() const;

private:
    // Общий метод инициализации компонента
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
        // Например, задаём начальное значение отображения BPM.



        addAndMakeVisible(pluginListBox);
        pluginListBox.setModel(this);

        loadButton.setButtonText("Load Plugin");
        loadButton.addListener(this);
        addAndMakeVisible(loadButton);

        closeButton.setButtonText("Close Plugin");
        closeButton.addListener(this);
        addAndMakeVisible(closeButton);
        closeButton.setVisible(false);

        parametersButton.setButtonText("Edit Parameters");
        parametersButton.addListener(this);
        addAndMakeVisible(parametersButton);
        parametersButton.setVisible(false);

        // Автозагрузка сохранённого плагина:
        juce::AudioIODevice* currentDevice = audioDeviceManager.getCurrentAudioDevice();
        double newSR = (currentDevice != nullptr) ? currentDevice->getCurrentSampleRate() : currentSampleRate;
        int newBS = (currentDevice != nullptr) ? currentDevice->getCurrentBufferSizeSamples() : currentBlockSize;
        juce::File lastPlugin = loadLastSelectedPluginFile();
        if (lastPlugin.existsAsFile())
        {
            loadPlugin(lastPlugin, newSR, newBS);
        }
    }

    void saveSelectedPluginFile(const juce::File& file)
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "NexusOS";
        options.filenameSuffix = "3settingshost";
        options.osxLibrarySubFolder = "Preferences";
        juce::PropertiesFile propertiesFile(options);
        propertiesFile.setValue("lastPluginPath", file.getFullPathName());
        propertiesFile.saveIfNeeded();
    }

    juce::File loadLastSelectedPluginFile()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "NexusOS";
        options.filenameSuffix = "3settingshost";
        options.osxLibrarySubFolder = "Preferences";
        juce::PropertiesFile propertiesFile(options);
        juce::String lastPluginPath = propertiesFile.getValue("lastPluginPath", "");
        return juce::File(lastPluginPath);
    }

    void clearSavedPluginFile()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "NexusOS";
        options.filenameSuffix = "3settingshost";
        options.osxLibrarySubFolder = "Preferences";
        juce::PropertiesFile propertiesFile(options);
        propertiesFile.removeValue("lastPluginPath");
        propertiesFile.saveIfNeeded();
    }

    void unloadPlugin()/////////////////////////////////////////////////////////////////////

    {
        if (pluginProcessCallback)
        {
            audioDeviceManager.removeAudioCallback(pluginProcessCallback.get());
            pluginProcessCallback.reset();
            updatePluginCpuLoad(0.0);
        }
        if (pluginEditor != nullptr)
        {
            removeChildComponent(pluginEditor);
            delete pluginEditor;
            pluginEditor = nullptr;

        }
        pluginInstance.reset();
        pluginListBox.setVisible(true);
        loadButton.setVisible(true);
        closeButton.setVisible(false);
        parametersButton.setVisible(false);

        // Явно обновляем компоновку после выгрузки плагина:
        resized();   // или можно вызвать repaint();
    }
    static juce::AudioDeviceManager& getDefaultAudioDeviceManager()
    {
        static juce::AudioDeviceManager defaultManager;
        static bool isInitialised = false;
        if (!isInitialised)
        {
            juce::String err = defaultManager.initialiseWithDefaultDevices(2, 2);
            if (!err.isEmpty())
                DBG("Default AudioDeviceManager error: " << err);
            isInitialised = true;
        }
        return defaultManager;
    }





    double lastPluginCpuLoad = 0.0;

    std::unique_ptr<PluginProcessCallback> pluginProcessCallback;
    std::unique_ptr<CpuLoadIndicator> cpuLoadIndicator;


    juce::AudioPluginFormatManager formatManager;
    juce::ListBox       pluginListBox;
    juce::TextButton    loadButton;
    juce::TextButton    closeButton;
    juce::TextButton    parametersButton;
    juce::Array<juce::File> pluginFiles;
    int selectedIndex = -1;
    std::unique_ptr<juce::AudioPluginInstance> pluginInstance;
    juce::Component* pluginEditor = nullptr;
    juce::AudioDeviceManager& audioDeviceManager;
    double              currentSampleRate;
    int                 currentBlockSize;

    std::unique_ptr<CustomAudioPlayHead> customAudioPlayHead;

    juce::Label* bpmDisplay = nullptr;



   // JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VSTHostComponent)
};
inline std::vector<ParameterInfo> VSTHostComponent::getCurrentPluginParameters() const
{
    std::vector<ParameterInfo> list;
    if (pluginInstance != nullptr)
    {
        auto& params = pluginInstance->getParameters();
        list.reserve(params.size());

        for (int i = 0; i < params.size(); ++i)
        {
            // getName(100) — строка до 100 символов
            juce::String nm = params[i]->getName(100);
            list.push_back({ i, nm });
        }
    }
    return list;
}
