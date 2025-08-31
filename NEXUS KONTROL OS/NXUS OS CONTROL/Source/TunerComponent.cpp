#include "TunerComponent.h"
#include <numeric>
#include <cmath>
#include <cstring>
#include <limits>

TunerComponent::TunerComponent()
{
    addAndMakeVisible(minusButton);
    addAndMakeVisible(plusButton);
    minusButton.addListener(this);
    plusButton.addListener(this);

    addAndMakeVisible(referenceLabel);
    referenceLabel.setJustificationType(juce::Justification::centred);
    updateRefLabel();

    addAndMakeVisible(lockButton);
    lockButton.onClick = [this]
        {
            autoStringLock = !autoStringLock; // переключаем флаг
            lockedString = -1;              // сброс фикса при смене режима
            updateLockButtonAppearance();     // обновляем текст и цвет
            resized();
        };
    updateLockButtonAppearance();
}

TunerComponent::~TunerComponent()
{
    shouldTerminate.store(true, std::memory_order_release);
    hasNewData.store(true, std::memory_order_release);
    if (detectionThread.joinable())
        detectionThread.join();

    cancelPendingUpdate();
    minusButton.removeListener(this);
    plusButton.removeListener(this);
}

void TunerComponent::prepare(int blockSize, double sampleRate) noexcept
{
    // стоп предыдущего потока
    if (detectionThread.joinable())
    {
        shouldTerminate.store(true, std::memory_order_release);
        hasNewData.store(true, std::memory_order_release);
        detectionThread.join();
    }

    // окно анализа ~50 мс (можно 60–80 мс для повышения стабильности низов)
    const double targetWindowMs = 50.0;
    int desired = (int)std::round(sampleRate * targetWindowMs / 1000.0);

    // округление вверх до степени 2
    int pow2 = 1;
    while (pow2 < desired) pow2 <<= 1;
    analysisSize = juce::jlimit(1024, 8192, pow2);

    // кольцевой буфер = 2 окна
    bufSize = analysisSize * 2;
    halfSize = bufSize / 2;

    ringBuffer.assign(bufSize, 0.0f);
    tempBuffer.assign(analysisSize, 0.0f);
    diffBuffer.assign((analysisSize / 2) + 1, 0.0f);

    writePos.store(0, std::memory_order_relaxed);
    totalWritten.store(0, std::memory_order_relaxed);
    lastProcessed = 0;

    lastFreq.store(-1.0f, std::memory_order_relaxed);
    lastFreqSmooth = -1.0f;
    freqHist = { -1.0f, -1.0f, -1.0f };
    freqHistCount = 0;

    // сброс гейта и времён
    onCount = offCount = 0;
    audible = false;
    lastAudibleTime = std::chrono::steady_clock::now() - std::chrono::milliseconds(holdMs);
    lastGoodFreq = -1.0f;
    lastFreqOkTime = std::chrono::steady_clock::now() - std::chrono::milliseconds(freqHoldMs);

    // игнор атаки — сброс
    gateOpenTime = std::chrono::steady_clock::now();
    gateJustOpened = false;

    // детектор
    detector = std::make_unique<MPMDetector>();
    detector->prepare(analysisSize, sampleRate);

    shouldTerminate.store(false, std::memory_order_release);
    detectionThread = std::thread(&TunerComponent::detectionThreadFunction, this);
}

void TunerComponent::pushAudioData(const float* data, int numSamples) noexcept
{
    if (bufSize <= 0 || data == nullptr || numSamples <= 0)
        return;

    int pos = writePos.load(std::memory_order_relaxed);
    if (pos < 0 || pos >= bufSize) pos = 0;

    for (int i = 0; i < numSamples; ++i)
    {
        ringBuffer[pos] = data[i];
        if (++pos >= bufSize) pos = 0;
    }

    writePos.store(pos, std::memory_order_relaxed);
    totalWritten.fetch_add(numSamples, std::memory_order_release);
    hasNewData.store(true, std::memory_order_release);
}

void TunerComponent::detectionThreadFunction()
{
    juce::ScopedNoDenormals noDenormals;
    while (!shouldTerminate.load(std::memory_order_acquire))
    {
        if (!hasNewData.exchange(false, std::memory_order_acq_rel))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        if (bufSize <= 0 || analysisSize <= 0)
            continue;

        // ждём ровно analysisSize новых сэмплов поверх последнего обработанного
        const int written = totalWritten.load(std::memory_order_acquire);
        if (written - lastProcessed < analysisSize)
            continue;
        lastProcessed = written;

        // копируем ровно analysisSize сэмплов, заканчивая на текущем wp
        int wp = writePos.load(std::memory_order_acquire);
        if (wp < 0 || wp >= bufSize) wp = 0;

        const int N = analysisSize;
        int start = wp - N;
        if (start < 0) start += bufSize;

        const int first = juce::jmin(N, bufSize - start);
        std::memcpy(tempBuffer.data(), ringBuffer.data() + start, (size_t)first * sizeof(float));
        std::memcpy(tempBuffer.data() + first, ringBuffer.data(), (size_t)(N - first) * sizeof(float));

        // remove DC
        double mean = 0.0;
        for (int i = 0; i < N; ++i) mean += tempBuffer[i];
        mean /= (double)N;
        for (int i = 0; i < N; ++i) tempBuffer[i] = (float)(tempBuffer[i] - mean);

        // RMS
        double sumSq = 0.0;
        for (int i = 0; i < N; ++i) sumSq += (double)tempBuffer[i] * tempBuffer[i];
        const float rms = (float)std::sqrt(sumSq / N);

        // гейт с гистерезисом
        if (rms >= rmsOn) { ++onCount;  offCount = 0; }
        else if (rms <= rmsOff) { ++offCount; onCount = 0; }

        if (!audible && onCount >= onBlocksNeeded)
        {
            audible = true;
            lastAudibleTime = std::chrono::steady_clock::now();
            offCount = 0;

            // старт окна атаки
            gateOpenTime = lastAudibleTime;
            gateJustOpened = true;

            // чистый старт сглаживания и авто-струны
            freqHistCount = 0;
            lastFreqSmooth = -1.0f;
            lastGoodFreq = -1.0f;
            lockedString = -1;
        }

        if (audible && rms >= rmsOn)
            lastAudibleTime = std::chrono::steady_clock::now();

        if (audible)
        {
            const auto now = std::chrono::steady_clock::now();
            const bool holdExpired = (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastAudibleTime).count() >= holdMs);
            if (offCount >= offBlocksNeeded && holdExpired)
                audible = false;
        }

        if (!audible)
        {
            if (lastFreq.load(std::memory_order_relaxed) > 0.0f)
            {
                lastFreqSmooth = -1.0f;
                lastFreq.store(-1.0f, std::memory_order_release);
                triggerAsyncUpdate();
            }
            continue;
        }

        // детект строго на окне N
        const PitchResult pr = detector->process(tempBuffer.data(), N);
        float freq = pr.hz;
        const float conf = pr.confidence;

        // игнор первых attackIgnoreMs после открытия гейта
        {
            const auto now = std::chrono::steady_clock::now();
            const bool inAttack = (std::chrono::duration_cast<std::chrono::milliseconds>(now - gateOpenTime).count() < attackIgnoreMs);
            if (inAttack)
                continue;
            if (gateJustOpened) gateJustOpened = false;
        }

        // гейт по уверенности + hold
        static bool confGate = false;
        const float confOn = 0.90f;
        const float confOff = 0.65f;
        if (!confGate && conf >= confOn) confGate = true;
        if (confGate && conf < confOff)  confGate = false;

        if (!std::isfinite(freq) || freq <= 0.0f || !confGate)
        {
            const auto now = std::chrono::steady_clock::now();
            const bool expired = (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFreqOkTime).count() >= freqHoldMs);

            if (expired)
            {
                if (lastFreq.load(std::memory_order_relaxed) > 0.0f)
                {
                    lastFreqSmooth = -1.0f;
                    lastFreq.store(-1.0f, std::memory_order_release);
                    triggerAsyncUpdate();
                }
            }
            continue;
        }

        // ограничение диапазона
        freq = juce::jlimit(70.0f, 1200.0f, freq);

        // подавление мелких колебаний по центах
        if (lastFreqSmooth > 0.0f)
        {
            const double centsDelta = 1200.0 * std::log2((double)freq / (double)lastFreqSmooth);
            const auto now = std::chrono::steady_clock::now();
            const bool recent = (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFreqOkTime).count() < freqHoldMs);
            if (recent && std::abs(centsDelta) < 6.0)
                freq = lastFreqSmooth;
        }

        // медиана по 3 + EMA
        freqHist[freqHistCount % 3] = freq;
        ++freqHistCount;
        const int n = std::min(freqHistCount, 3);
        const float f0 = freqHist[0];
        const float f1 = (n > 1 ? freqHist[1] : freq);
        const float f2 = (n > 2 ? freqHist[2] : freq);
        auto med3 = [](float a, float b, float c)
            {
                return std::max(std::min(a, b), std::min(std::max(a, b), c));
            };
        const float fMed = (n == 1 ? f0 : (n == 2 ? 0.5f * (f0 + f1) : med3(f0, f1, f2)));

        lastFreqSmooth = (lastFreqSmooth < 0.0f)
            ? fMed
            : alphaSmooth * fMed + (1.0f - alphaSmooth) * lastFreqSmooth;

        lastGoodFreq = lastFreqSmooth;
        lastFreqOkTime = std::chrono::steady_clock::now();

        lastFreq.store(lastFreqSmooth, std::memory_order_release);
        triggerAsyncUpdate();
    }
}

void TunerComponent::handleAsyncUpdate()
{
    updateString();
    repaint();
}

void TunerComponent::setReferenceA4(double a4Hz, bool notify) noexcept
{
    referenceA4.store(a4Hz, std::memory_order_relaxed);
    updateRefLabel();
    if (notify && onReferenceA4Changed) onReferenceA4Changed(a4Hz);
}

void TunerComponent::buttonClicked(juce::Button* b)
{
    auto a4 = getReferenceA4();
    if (b == &minusButton)      a4 = std::max(400.0, a4 - 1.0);
    else if (b == &plusButton)  a4 = std::min(480.0, a4 + 1.0);
    setReferenceA4(a4);
}

void TunerComponent::updateRefLabel()
{
    referenceLabel.setText("A4: " + juce::String(getReferenceA4(), 0) + " Hz",
        juce::dontSendNotification);
}

void TunerComponent::updateString()
{
    const float f = lastFreq.load(std::memory_order_acquire);
    if (f <= 0.0f)
    {
        currentString.clear();
        currentCents = 0.0f;
        lockedString = -1; // сброс фиксатора при пропадании сигнала
        return;
    }

    const double a4 = getReferenceA4();

    if (chromaticMode)
    {
        const double midi = 69.0 + 12.0 * std::log2(f / a4);
        const int    noteNum = (int)std::lround(midi);
        currentCents = (float)((midi - noteNum) * 100.0);

        static const char* names[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        const int pc = ((noteNum % 12) + 12) % 12;
        const int oct = noteNum / 12 - 1;

        currentString = (juce::String(names[pc]) + juce::String(oct)).toStdString();
        return;
    }

    // режим 6 струн — авто-струна с фиксацией
    const float scale = (float)(a4 / 440.0f);

    // 1) ближайшая струна
    int   bestIdx = 0;
    float bestAbs = std::numeric_limits<float>::max();
    float bestCents = 0.0f;

    for (int i = 0; i < 6; ++i)
    {
        const float target = stringFreqs[i] * scale;
        const float cents = 1200.0f * std::log2(f / target);
        const float ac = std::abs(cents);
        if (ac < bestAbs) { bestAbs = ac; bestIdx = i; bestCents = cents; }
    }

    if (autoStringLock)
    {
        const auto now = std::chrono::steady_clock::now();

        // если залочка не установлена — установить
        if (lockedString < 0)
        {
            lockedString = bestIdx;
            stringLockTime = now;
        }

        // радиус удержания: если залоченная струна ещё близко — удерживаем её
        {
            const float targetLocked = stringFreqs[lockedString] * scale;
            const float centsLocked = 1200.0f * std::log2(f / targetLocked);
            const float keepRadius = 35.0f; // 25–45¢
            if (std::abs(centsLocked) <= keepRadius)
            {
                bestIdx = lockedString;
                bestCents = centsLocked;
            }
        }

        // проверка таймаута фиксации
        const bool lockExpired =
            (std::chrono::duration_cast<std::chrono::milliseconds>(now - stringLockTime).count() >= stringLockMs);

        if (!lockExpired && bestIdx != lockedString)
        {
            // удерживаем прежнюю
            const float target = stringFreqs[lockedString] * scale;
            bestIdx = lockedString;
            bestCents = 1200.0f * std::log2(f / target);
        }
        else if (bestIdx != lockedString)
        {
            // анти-болтанка: если ушли далеко — форсируем смену
            const float targetLocked = stringFreqs[lockedString] * scale;
            const float centsLocked = 1200.0f * std::log2(f / targetLocked);
            if (std::abs(centsLocked) > 90.0f)
                stringLockTime = now - std::chrono::milliseconds(stringLockMs);

            // смена фикса — сброс сглаживания
            if (lockedString != bestIdx)
            {
                lockedString = bestIdx;
                stringLockTime = now;
                freqHistCount = 0;
                lastFreqSmooth = -1.0f;
                lastGoodFreq = -1.0f;
            }
        }
    }

    currentString = stringNames[bestIdx];
    currentCents = bestCents;
}

void TunerComponent::paint(juce::Graphics& g)
{
    const float  freq = lastFreq.load(std::memory_order_acquire);
    const double a4 = referenceA4.load(std::memory_order_acquire);
    const bool   hasSignal = freq > 0.0f;

    auto areaF = scaleArea.toFloat();
    const float cx = areaF.getCentreX();
    const float halfW = areaF.getWidth() * 0.4f;
    const float lineY = areaF.getBottom() - areaF.getHeight() * 0.1f;

    if (hasSignal)
    {
        const double midi = 69.0 + 12.0 * std::log2(freq / a4);
        const int    noteNum = (int)std::lround(midi);
        const float  centsDeltaRaw = (float)((midi - noteNum) * 100.0);
        const float  centsDelta = juce::jlimit(-50.0f, 50.0f, centsDeltaRaw);
        const float  absCents = std::abs(centsDelta);

        juce::Colour bg;
        if (absCents <= perfectRange) bg = juce::Colours::green;
        else if (absCents <= warningRange)
            bg = juce::Colours::green.interpolatedWith(juce::Colours::yellow,
                juce::jlimit(0.0f, 1.0f, (absCents - perfectRange) / (warningRange - perfectRange)));
        else if (absCents <= dangerRange)
            bg = juce::Colours::yellow.interpolatedWith(juce::Colours::red,
                juce::jlimit(0.0f, 1.0f, (absCents - warningRange) / (dangerRange - warningRange)));
        else bg = juce::Colours::red;

        g.fillAll(bg);

        g.setColour(juce::Colours::white);
        g.setFont(noteArea.getHeight() * 0.7f);
        g.drawText(currentString, noteArea, juce::Justification::centred);

        // стрелка: шкала ±50¢
        const float dx = (centsDelta / 50.0f) * halfW;

        constexpr float arrowHalfW = 8.0f, arrowHalfH = 16.0f;
        juce::Path arrow;
        arrow.startNewSubPath(cx + dx - arrowHalfW, lineY + arrowHalfH);
        arrow.lineTo(cx + dx + arrowHalfW, lineY + arrowHalfH);
        arrow.lineTo(cx + dx, lineY - arrowHalfH);
        arrow.closeSubPath();

        g.setColour(juce::Colours::blue);
        g.fillPath(arrow);

        // частота
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.setFont(juce::jmax(12.0f, noteArea.getHeight() * 0.18f));
        g.drawText(juce::String(freq, 1) + " Hz", stringArea, juce::Justification::centredTop);
    }
    else
    {
        g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));
        g.setColour(juce::Colours::white);
        g.setFont(noteArea.getHeight() * 0.5f);
        g.drawText("No signal", noteArea, juce::Justification::centred);
    }

    // шкала ±50¢
    {
        g.setColour(juce::Colours::darkgreen);
        const float zoneW = (perfectRange / 50.0f) * halfW;
        g.fillRect((int)(cx - zoneW), (int)(lineY - 2.0f), (int)(zoneW * 2.0f), 4);

        g.setColour(juce::Colours::white);
        g.drawLine(cx - halfW, lineY, cx + halfW, lineY, 2.0f);

        const int totalTicks = 11; // -50..+50/10
        const float step = (halfW * 2.0f) / (totalTicks - 1);
        for (int i = 0; i < totalTicks; ++i)
        {
            const float x = cx - halfW + i * step;
            const int centsLabel = (i - (totalTicks - 1) / 2) * 10;
            const bool major = (centsLabel % 50) == 0;
            const float tickH = major ? 12.0f : 6.0f;
            const float w = major ? 2.0f : 1.0f;

            g.drawLine(x, lineY - tickH * 0.5f, x, lineY + tickH * 0.5f, w);

            if (major)
            {
                g.setFont(14.0f);
                g.drawText(juce::String(centsLabel),
                    (int)(x - 15), (int)(lineY + 8), 30, 20,
                    juce::Justification::centred);
            }
        }
    }
}

void TunerComponent::resized()
{
    // Простой симметричный лэйаут без Grid
    auto b = getLocalBounds();
    const int h = b.getHeight();
    const int w = b.getWidth();

    const int topH = juce::jmax(24, h / 10);
    const int noteH = h / 4;
    const int scaleH = h / 3;

    auto top = b.removeFromTop(topH);
    auto note = b.removeFromTop(noteH);
    auto scale = b.removeFromTop(scaleH);
    auto rest = b;

    const int btnW = juce::jmax(24, top.getWidth() / 12);
    minusButton.setBounds(top.removeFromLeft(btnW).reduced(2));
    plusButton.setBounds(top.removeFromRight(btnW).reduced(2));
    referenceLabel.setBounds(top);

    noteArea = note.reduced(w / 10, 0);
    scaleArea = scale.reduced(w / 20, 0);
    stringArea = rest;

    // кнопка под шкалой по центру, слегка ниже середины, размер как у плюс/минус
    {
        const int rowH = topH;
        const int btnWsame = juce::jmax(24, w / 12);
        const int offsetY = rowH / 6;

        const int x = (w - btnWsame) / 2;
        const int yCenter = stringArea.getY() + stringArea.getHeight() / 2;
        const int y = yCenter - rowH / 2 + offsetY;

        lockButton.setBounds(juce::Rectangle<int>(x, y, btnWsame, rowH).reduced(2));
    }
}

void TunerComponent::updateLockButtonAppearance() noexcept
{
    lockButton.setButtonText(autoStringLock ? "Auto-lock: ON" : "Auto-lock: OFF");

    const auto onBg = juce::Colours::limegreen;
    const auto offBg = juce::Colours::darkgrey;
    const auto onTx = juce::Colours::black;
    const auto offTx = juce::Colours::white;

    lockButton.setColour(juce::TextButton::buttonColourId, autoStringLock ? onBg : offBg);
    lockButton.setColour(juce::TextButton::textColourOnId, autoStringLock ? onTx : offTx);
    lockButton.setColour(juce::TextButton::textColourOffId, autoStringLock ? onTx : offTx);
}
