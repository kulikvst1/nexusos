// audio_dsp.h
#pragma once
#include <JuceHeader.h>

class AudioDSP
{
public:
    AudioDSP(juce::AudioDeviceManager& adm)
        : deviceManager(adm)
    {
        // Создаем аудиограф
        audioGraph = std::make_unique<juce::AudioProcessorGraph>();
        audioProcessorPlayer.setProcessor(audioGraph.get());

        // Регистрируем audioProcessorPlayer как аудио callback в общем AudioDeviceManager
        deviceManager.addAudioCallback(&audioProcessorPlayer);
    }

    ~AudioDSP()
    {
        deviceManager.removeAudioCallback(&audioProcessorPlayer);
    }

    void prepareToPlayDSP(int samplesPerBlockExpected, double sampleRate, juce::AudioPluginInstance* pluginInstance)
    {
        // Очистим граф от предыдущих узлов
        audioGraph->clear();

        // Создаем входной узел графа
        auto audioInputNode = audioGraph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(
            juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode));

        // Создаем выходной узел графа
        auto audioOutputNode = audioGraph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(
            juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));

        const int numChannels = 2;

        if (pluginInstance != nullptr)
        {
            // Плагин загружен – создаем его узел и соединяем: вход → плагин → выход
            nodeWithPlugin = audioGraph->addNode(std::unique_ptr<juce::AudioPluginInstance>(pluginInstance));
            if (nodeWithPlugin != nullptr)
            {
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    audioGraph->addConnection({ { audioInputNode->nodeID, ch },
                                                  { nodeWithPlugin->nodeID, ch } });
                }
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    audioGraph->addConnection({ { nodeWithPlugin->nodeID, ch },
                                                  { audioOutputNode->nodeID, ch } });
                }
            }
        }
        else
        {
            // Плагин не загружен – делаем bypass: соединяем вход напрямую с выходом
            for (int ch = 0; ch < numChannels; ++ch)
            {
                audioGraph->addConnection({ { audioInputNode->nodeID, ch },
                                              { audioOutputNode->nodeID, ch } });
            }
        }
    }


    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
    {
        if (bufferToFill.buffer != nullptr && audioGraph)
        {
            juce::MidiBuffer dummyMidi;
            audioGraph->processBlock(*bufferToFill.buffer, dummyMidi);
        }
    }

    void releaseResources()
    {
        // Здесь можно добавить, если что-либо нужно освободить из audioGraph
    }

private:
    juce::AudioDeviceManager& deviceManager;
    juce::AudioProcessorPlayer audioProcessorPlayer;
    std::unique_ptr<juce::AudioProcessorGraph> audioGraph;
    juce::AudioProcessorGraph::Node::Ptr nodeWithPlugin = nullptr;
};
