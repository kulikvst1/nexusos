#pragma once
// audio_dsp.h
#pragma once
#include <JuceHeader.h>

class AudioDSP : public juce::AudioAppComponent
{
public:
    AudioDSP()
    {
        // Инициализируем аудиоустройство (например, 2 входа и 2 выхода)
        setAudioChannels(2, 2);

        // Создаем аудиограф и присваиваем его аудио-плееру
        audioGraph = std::make_unique<juce::AudioProcessorGraph>();
        audioProcessorPlayer.setProcessor(audioGraph.get());

        // Регистрируем аудио плеер как обратный вызов в устройстве
        deviceManager.addAudioCallback(&audioProcessorPlayer);
    }

    ~AudioDSP() override
    {
        deviceManager.removeAudioCallback(&audioProcessorPlayer);
        shutdownAudio();
    }

    /** Здесь вы вызываете этот метод при подготовке к воспроизведению.
        Он очищает граф и добавляет базовые узлы (вход и выход) и, если доступен,
        узел с плагином. Перед вызовом этого метода убедитесь, что плагин был загружен. */
    void prepareToPlayDSP(int samplesPerBlockExpected, double sampleRate, juce::AudioPluginInstance* pluginInstance)
    {
        // Очистим граф от предыдущих узлов:
        audioGraph->clear();

        // Создаем входной и выходной узел графа
        auto audioInputNode = audioGraph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(
            juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode));
        auto audioOutputNode = audioGraph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(
            juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));

        // Если плагин загружен, добавляем его в граф.
        if (pluginInstance != nullptr)
        {
            // Здесь важно: если плагин уже находится во владении другого компонента (например, VSTHostComponent),
            // то передать его в граф напрямую может вызвать проблемы с владением.
            // В этом примере рассматриваем вариант, когда он передается специально для аудио обработки.
            nodeWithPlugin = audioGraph->addNode(std::unique_ptr<juce::AudioPluginInstance>(pluginInstance));

            if (nodeWithPlugin != nullptr)
            {
                // Предполагаем стерео (2 канала).
                const int numChannels = 2;
                // Подключаем вход → плагин
                for (int ch = 0; ch < numChannels; ++ch)
                    audioGraph->addConnection({ { audioInputNode->nodeID, ch },
                                                  { nodeWithPlugin->nodeID, ch } });

                // Подключаем плагин → выход
                for (int ch = 0; ch < numChannels; ++ch)
                    audioGraph->addConnection({ { nodeWithPlugin->nodeID, ch },
                                                  { audioOutputNode->nodeID, ch } });
            }
        }
    }

    // Метод, который AudioAppComponent вызывает для получения аудиоданных.
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        audioProcessorPlayer.getNextAudioBlock(bufferToFill);
    }

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override
    {
        // Можно вызвать здесь некоторые базовые настройки.
        // Реальная настройка графа происходит через метод prepareToPlayDSP(),
        // когда у вас появится плагин.
    }

    void releaseResources() override
    {
        audioProcessorPlayer.releaseResources();
    }

private:
    // Устройства и плеер
    juce::AudioDeviceManager deviceManager;
    juce::AudioProcessorPlayer audioProcessorPlayer;
    std::unique_ptr<juce::AudioProcessorGraph> audioGraph;

    // Узел графа с загруженным плагином
    juce::AudioProcessorGraph::Node::Ptr nodeWithPlugin = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioDSP)
};

