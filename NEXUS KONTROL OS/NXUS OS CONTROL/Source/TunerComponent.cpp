//==============================================================================
//  TunerComponent.cpp
//==============================================================================

#include "TunerComponent.h"
#include <numeric>
#include <cmath>
#include <cstring>

void TunerComponent::updateString()
{
    float f = lastFreq.load(std::memory_order_relaxed);
    if (f <= 0.0f)
    {
        currentString.clear();
        return;
    }

    float bestD = FLT_MAX;
    int   bestI = 0;
    for (int i = 0; i < 6; ++i)
    {
        float d = std::abs(f - stringFreqs[i]);
        if (d < bestD) { bestD = d; bestI = i; }
    }
    currentString = stringNames[bestI];
}

//==============================================================================
//  Constructor / Destructor
//==============================================================================
TunerComponent::TunerComponent() {}

TunerComponent::~TunerComponent()
{
    shouldTerminate.store(true, std::memory_order_release);
    hasNewData.store(true, std::memory_order_release);
    if (detectionThread.joinable())
        detectionThread.join();
}

//==============================================================================
//  prepare() — allocate buffers & launch detection thread
//==============================================================================
void TunerComponent::prepare(int blockSize, double sampleRate) noexcept
{
    bufSize = blockSize * 4;
    halfSize = bufSize / 2;

    ringBuffer.assign(bufSize, 0.0f);
    tempBuffer.assign(bufSize, 0.0f);
    diffBuffer.assign(halfSize, 0.0f);

    pitchEngine.prepare(bufSize, sampleRate);

    writePos.store(0, std::memory_order_relaxed);
    lastFreq.store(-1.0f, std::memory_order_relaxed);
    lastFreqSmooth = -1.0f;

    shouldTerminate.store(false, std::memory_order_release);
    if (!detectionThread.joinable())
        detectionThread = std::thread(&TunerComponent::detectionThreadFunction, this);
}

//==============================================================================
//  pushAudioData() — write into ringBuffer, wrap-around
//==============================================================================
void TunerComponent::pushAudioData(const float* data, int numSamples) noexcept
{
    if (bufSize <= 0 || data == nullptr)
        return;

    int pos = writePos.load(std::memory_order_relaxed);
    for (int i = 0; i < numSamples; ++i)
    {
        ringBuffer[pos] = data[i];
        if (++pos >= bufSize)
            pos = 0;
    }
    writePos.store(pos, std::memory_order_relaxed);
    hasNewData.store(true, std::memory_order_release);
}

//==============================================================================
//  detectionThreadFunction() — consume ringBuffer & detect pitch
//==============================================================================
void TunerComponent::detectionThreadFunction()
{
    while (!shouldTerminate.load(std::memory_order_acquire))
    {
        if (!hasNewData.exchange(false, std::memory_order_acq_rel))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        if (bufSize <= 0)
            continue;

        int wp = writePos.load(std::memory_order_relaxed);
        int tail = bufSize - wp;

        std::memcpy(tempBuffer.data(),
            ringBuffer.data() + wp,
            size_t(tail) * sizeof(float));
        std::memcpy(tempBuffer.data() + tail,
            ringBuffer.data(),
            size_t(wp) * sizeof(float));

        // RMS noise gate
        double sumSq = 0.0;
        for (auto& s : tempBuffer)
            sumSq += double(s) * s;
        float rms = float(std::sqrt(sumSq / bufSize));
        if (rms < rmsThreshold)
        {
            lastFreqSmooth = -1.0f;
            lastFreq.store(-1.0f, std::memory_order_relaxed);
            triggerAsyncUpdate();
            continue;
        }

        // remove DC offset
        double mean = std::accumulate(tempBuffer.begin(),
            tempBuffer.end(), 0.0) / bufSize;
        for (auto& s : tempBuffer)
            s = float(s - mean);

        // pitch detection
        float freq = pitchEngine.process(tempBuffer.data(),
            bufSize,
            diffBuffer.data());

        // exponential smoothing
        if (lastFreqSmooth < 0.0f)
            lastFreqSmooth = freq;
        else
            lastFreqSmooth = alphaSmooth * freq
            + (1.0f - alphaSmooth) * lastFreqSmooth;

        lastFreq.store(lastFreqSmooth, std::memory_order_relaxed);
        triggerAsyncUpdate();
    }
}

//==============================================================================
//  paint() — draw tuner UI + fix fillRect overload
//==============================================================================
void TunerComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey);

    // 1) Note name & cents calculation
    float freq = lastFreq.load(std::memory_order_relaxed);
    const double A4 = 440.0;
    double midi = (freq > 0.0)
        ? 69.0 + 12.0 * std::log2(freq / A4)
        : 69.0;
    int noteNum = int(std::round(midi));
    double cents = (midi - noteNum) * 100.0;
    static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    juce::String noteName = names[(noteNum + 1200) % 12]
        + juce::String(noteNum / 12 - 1);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(72.0f, juce::Font::bold));
    g.drawText(noteName,
        getLocalBounds().removeFromTop(100),
        juce::Justification::centred, false);

    // 2) Cents scale
    auto area = getLocalBounds().toFloat().reduced(20.0f);
    float lineY = area.getBottom() - 30.0f;
    float cx = area.getCentreX();
    float halfW = area.getWidth() * 0.4f;

    // 3) Zero zone (±5 cents)
    float zoneW = (5.0f / 50.0f) * halfW;
    g.setColour(juce::Colours::darkgreen);

    // <-- FIX: call fillRect with int args -->
    g.fillRect(
        int(cx - zoneW),
        int(lineY - 2.0f),
        int(zoneW * 2.0f),
        4
    );

    // 4) Main line
    g.setColour(juce::Colours::white);
    g.drawLine(cx - halfW, lineY, cx + halfW, lineY, 2.0f);

    // 5) Ticks and labels
    const int totalTicks = 21;
    float step = (halfW * 2) / (totalTicks - 1);
    for (int i = 0; i < totalTicks; ++i)
    {
        float x = cx - halfW + i * step;
        bool isMajor = ((i - (totalTicks - 1) / 2) % 5) == 0;
        float tickH = isMajor ? 12.0f : 6.0f;
        g.drawLine(x, lineY - tickH * 0.5f,
            x, lineY + tickH * 0.5f,
            isMajor ? 2.0f : 1.0f);
        if (isMajor)
        {
            int centsMark = (i - (totalTicks - 1) / 2) * 10;
            juce::String txt = centsMark == 0
                ? juce::String("0")
                : juce::String(centsMark);
            g.setFont(14.0f);
            g.drawText(txt,
                int(x - 15), int(lineY + 8),
                30, 20,
                juce::Justification::centred);
        }
    }

    // 6) Arrow
    float dx = juce::jlimit(-halfW, halfW,
        float(cents / 50.0 * halfW));
    juce::Path arrow;
    arrow.addTriangle(cx + dx - 8.0f, lineY + 16.0f,
        cx + dx + 8.0f, lineY + 16.0f,
        cx + dx, lineY - 16.0f);
    g.setColour(juce::Colours::yellow);
    g.fillPath(arrow);

    if (!currentString.empty())
    {
        g.setColour(juce::Colours::lightblue);
        g.setFont(24.0f);
        g.drawText("String: " + currentString,
            0, getHeight() - 40,
            getWidth(), 30,
            juce::Justification::centred, false);
    }
}
