#include "TunerComponent.h"
#include <numeric>
#include <cmath>
#include <cstring>
#include <algorithm>

//==============================================================================
//  updateString() — выбираем ближайшую струну с учётом A4
//==============================================================================
void TunerComponent::updateString()
{
    float f = lastFreq.load(std::memory_order_relaxed);
    if (f <= 0.0f)
    {
        currentString.clear();
        currentCents = 0.0f;
        return;
    }

    double ratio = referenceA4.load(std::memory_order_relaxed) / 440.0;

    float bestD = FLT_MAX;
    int   bestI = 0;

    // ищем ближайшую целевую частоту
    for (int i = 0; i < 6; ++i)
    {
        float target = stringFreqs[i] * float(ratio);
        float d = std::abs(f - target);
        if (d < bestD)
        {
            bestD = d;
            bestI = i;
        }
    }

    // сохраняем имя струны
    currentString = stringNames[bestI];

    // вычисляем отклонение в центах относительно target
    float targetFreq = stringFreqs[bestI] * float(ratio);
    currentCents = 1200.0f * std::log2(f / targetFreq);
}
//==============================================================================
//  Constructor / Destructor
//==============================================================================
TunerComponent::TunerComponent()
{
    // UI-контролы для A4
    addAndMakeVisible(minusButton);
    addAndMakeVisible(plusButton);
    minusButton.addListener(this);
    plusButton.addListener(this);

    addAndMakeVisible(referenceLabel);
    referenceLabel.setJustificationType(juce::Justification::centred);
    updateRefLabel();
}

TunerComponent::~TunerComponent()
{
    // 1) Останавливаем поток детекции
    shouldTerminate.store(true, std::memory_order_release);
    hasNewData.store(true, std::memory_order_release);
    if (detectionThread.joinable())
        detectionThread.join();

    // 2) Отменяем любой ожидающий AsyncUpdater-хук  
    cancelPendingUpdate();

    // 3) Снимаем слушатели от кнопок  
    minusButton.removeListener(this);
    plusButton.removeListener(this);
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
    diffBuffer.assign(halfSize + 1, 0.0f);


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
    // на всякий случай, чтобы pos точно лежал в [0, bufSize-1]
    if (pos < 0 || pos >= bufSize)
        pos = 0;

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
        // убираем «левые» значения
        if (wp < 0 || wp > bufSize)
            wp = 0;

        int tail = bufSize - wp;
        // гарантируем, что tail ∈ [0, bufSize]
        if (tail < 0)        tail = 0;
        else if (tail > bufSize) tail = bufSize;

        // теперь копируем строго в рамках границ
        std::memcpy(tempBuffer.data(),
            ringBuffer.data() + wp,
            size_t(tail) * sizeof(float));

        std::memcpy(tempBuffer.data() + tail,
            ringBuffer.data(),
            size_t(bufSize - tail) * sizeof(float));

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
//  paint() — draw tuner UI + use referenceA4 for cents & midi
//==============================================================================
void TunerComponent::paint(juce::Graphics& g)
{
    // 1) Получаем частоту и A4
    float freq = lastFreq.load(std::memory_order_relaxed);
    double a4 = referenceA4.load(std::memory_order_relaxed);

    // MIDI-номер (или дефолт 69)
    double midi = (freq > 0.0f)
        ? 69.0 + 12.0 * std::log2(freq / a4)
        : 69.0;
    int noteNum = int(std::round(midi));

    // смещение в центах
    float cents = std::fabs(float((midi - noteNum) * 100.0f));

    // 2) Вычисляем фон по порогам
    juce::Colour bg;
    if (cents <= perfectRange)
    {
        bg = juce::Colours::green;
    }
    else if (cents <= warningRange)
    {
        float t = (cents - perfectRange)
            / (warningRange - perfectRange);
        bg = juce::Colours::green
            .interpolatedWith(juce::Colours::yellow,
                juce::jlimit(0.0f, 1.0f, t));
    }
    else if (cents <= dangerRange)
    {
        float t = (cents - warningRange)
            / (dangerRange - warningRange);
        bg = juce::Colours::yellow
            .interpolatedWith(juce::Colours::red,
                juce::jlimit(0.0f, 1.0f, t));
    }
    else
    {
        bg = juce::Colours::red;
    }

    g.fillAll(bg);

    // 3) Рисуем название ноты
    static const char* names[] = {
        "C","C#","D","D#","E","F",
        "F#","G","G#","A","A#","B"
    };
    juce::String noteName =
        names[(noteNum + 1200) % 12]
        + juce::String(noteNum / 12 - 1);

    g.setColour(juce::Colours::white);
    g.setFont(noteArea.getHeight() * 0.7f);
    g.drawText(noteName, noteArea, juce::Justification::centred);

    // 4) Шкала центов
    auto areaF = scaleArea.toFloat();
    float cx = areaF.getCentreX();
    float halfW = areaF.getWidth() * 0.4f;
    float lineY = areaF.getBottom() - areaF.getHeight() * 0.1f;

    // зона ±perfectRange
    float zoneW = (perfectRange / dangerRange) * halfW;
    g.setColour(juce::Colours::darkgreen);
    g.fillRect(int(cx - zoneW), int(lineY - 2.0f),
        int(zoneW * 2.0f), 4);

    g.setColour(juce::Colours::white);
    g.drawLine(cx - halfW, lineY, cx + halfW, lineY, 2.0f);

    const int totalTicks = 21;
    float step = (halfW * 2.0f) / (totalTicks - 1);
    for (int i = 0; i < totalTicks; ++i)
    {
        float x = cx - halfW + i * step;
        bool  major = ((i - (totalTicks - 1) / 2) % 5) == 0;
        float tickH = major ? 12.0f : 6.0f;
        float w = major ? 2.0f : 1.0f;

        g.drawLine(x, lineY - tickH * 0.5f,
            x, lineY + tickH * 0.5f, w);

        if (major)
        {
            int centsLabel = (i - (totalTicks - 1) / 2) * 10;
            g.setFont(14.0f);
            g.drawText(juce::String(centsLabel),
                int(x - 15), int(lineY + 8),
                30, 20,
                juce::Justification::centred);
        }
    }

    // 5) Стрелка в прежнем месте и размере
    float rawDx = (midi - noteNum) * halfW;
    float dx = std::clamp(rawDx, -halfW, halfW);

    constexpr float arrowHalfW = 8.0f, arrowHalfH = 16.0f;
    juce::Path arrow;
    arrow.startNewSubPath(cx + dx - arrowHalfW, lineY + arrowHalfH);
    arrow.lineTo(cx + dx + arrowHalfW, lineY + arrowHalfH);
    arrow.lineTo(cx + dx, lineY - arrowHalfH);
    arrow.closeSubPath();

    g.setColour(juce::Colours::blue);
    g.fillPath(arrow);

    // 6) Отображаем текущую струну
    if (!currentString.empty())
    {
        g.setColour(juce::Colours::lightblue);
        g.setFont(stringArea.getHeight() * 0.6f);
        g.drawText("String: " + currentString,
            stringArea,
            juce::Justification::centred);
    }
}
//==============================================================================
//  resized() — разбиваем на зоны и раскладываем контролы
//==============================================================================
void TunerComponent::resized()
{
    Grid grid(getLocalBounds(), 5, 4);

    int mx = grid.getSector(0, 0).getWidth() / 20;
    int my = grid.getSector(0, 0).getHeight() / 20;

    auto sector = [&](int r, int c) { return grid.getSector(r, c).reduced(mx, my); };
    auto span = [&](int r, int c0, int c1) { return grid.getUnion(r, c0, c1).reduced(mx, my); };

    // Управление эталонной A4 в первой строке — кнопки уменьшены в 4 раза
    auto cell1 = sector(0, 1);
    auto cell3 = sector(0, 3);

    int btnW = cell1.getWidth() / 4;
    int btnH = cell1.getHeight() / 4;

    minusButton.setBounds(cell1.withSizeKeepingCentre(btnW, btnH));
    plusButton.setBounds(cell3.withSizeKeepingCentre(btnW, btnH));

    float fontSize = std::min(btnW, btnH) * 0.6f;
   
    referenceLabel.setBounds(sector(0, 2));

    // Основные зоны
    noteArea = sector(1, 2);
    scaleArea = span(2, 0, 4);
    stringArea = span(3, 1, 3);
}
