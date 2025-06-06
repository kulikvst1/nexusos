#pragma once
#include <JuceHeader.h>

// Используем только предварительное объявление класса VSTHostComponent,
// чтобы исключить циклические зависимости.
class VSTHostComponent;

class PluginProcessCallback : public juce::AudioIODeviceCallback
{
public:
    // Объявляем конструктор и деструктор (реализация конструктора будет во .cpp)
    PluginProcessCallback(juce::AudioPluginInstance* p, double rate);
    ~PluginProcessCallback() override = default;

    // Объявляем необходимые методы аудио-callback:
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
        int numInputChannels,
        float* const* outputChannelData,
        int numOutputChannels,
        int numSamples,
        const juce::AudioIODeviceCallbackContext& context) override;

    // Версия без контекста — можно оставить пустой (без override).
    void audioDeviceIOCallback(const float** inputChannelData,
        int numInputChannels,
        float** outputChannelData,
        int numOutputChannels,
        int numSamples);

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    // Сеттер для указателя на хост-компонент
    void setHostComponent(VSTHostComponent* host);

private:
    juce::AudioPluginInstance* pluginInstance = nullptr;
    double currentSampleRate = 44100.0;
    VSTHostComponent* hostComponent = nullptr;
};
