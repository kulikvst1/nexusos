#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cstring>

class VSTHostComponent;

/*───────── FIFO-события GUI → Audio ─────────*/
enum class EventType : uint8_t { Param, Midi };

struct Event
{
    EventType type = EventType::Param;
    union
    {
        struct { int index; float value; }                       param;
        struct { uint8_t data[3]; int size; int sampleOffset; }  midi;
    };

    static Event makeParam(int idx, float v) { Event e; e.param = { idx, v }; return e; }
    static Event makeMidi(const juce::MidiMessage& m, int ofs) {
        Event e; e.type = EventType::Midi;
        e.midi.size = m.getRawDataSize(); jassert(e.midi.size <= 3);
        std::memcpy(e.midi.data, m.getRawData(), e.midi.size);
        e.midi.sampleOffset = ofs; return e;
    }
};
using EventBuffer = std::array<Event, 512>;
extern EventBuffer        gEventBuf;
extern juce::AbstractFifo gEventFifo;

/*───────── RT-safe AudioIODeviceCallback ─────────*/
class PluginProcessCallback : public juce::AudioIODeviceCallback
{
public:
    /* новый, «правильный» конструктор */
    PluginProcessCallback(juce::AudioPluginInstance* inst,
        double                     initialSR,
        std::atomic<double>& cpuTarget) noexcept;

    /* обёртка-совместимость: старые вызовы с 2 аргументами
       делегируют внутрь, создавая dummy-атомик */
    PluginProcessCallback(juce::AudioPluginInstance* inst,
        double                     initialSR) noexcept;

    ~PluginProcessCallback() override = default;

    /* единственный override-метод, который существует в JUCE-8 */
    void audioDeviceIOCallbackWithContext(const float* const* input,
        int                                     numIn,
        float* const* output,
        int                                     numOut,
        int                                     numSamples,
        const juce::AudioIODeviceCallbackContext&) override;

    /* «старый» колбэк – прокси, override убран */
    void audioDeviceIOCallback(const float* const* input,
        int                  numIn,
        float* const* output,
        int                  numOut,
        int                  numSamples);

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    void setHostComponent(VSTHostComponent* host) noexcept { hostComponent = host; }

private:
    juce::AudioPluginInstance* pluginInstance = nullptr;
    double                     currentSR = 44100.0;
    std::atomic<double>& cpuLoadExt;
    VSTHostComponent* hostComponent = nullptr;

    juce::MidiBuffer midi;
    int              cpuCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessCallback)
};
