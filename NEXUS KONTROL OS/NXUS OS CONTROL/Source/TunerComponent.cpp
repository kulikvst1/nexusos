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

        const int written = totalWritten.load(std::memory_order_acquire);
        if (written - lastProcessed < analysisSize)
            continue;
        lastProcessed = written;

        int wp = writePos.load(std::memory_order_acquire);
        if (wp < 0 || wp >= bufSize) wp = 0;

        const int N = analysisSize;
        int start = wp - N;
        if (start < 0) start += bufSize;

        const int first = juce::jmin(N, bufSize - start);
        std::memcpy(tempBuffer.data(), ringBuffer.data() + start, (size_t)first * sizeof(float));
        std::memcpy(tempBuffer.data() + first, ringBuffer.data(), (size_t)(N - first) * sizeof(float));

        double mean = 0.0;
        for (int i = 0; i < N; ++i) mean += tempBuffer[i];
        mean /= (double)N;
        for (int i = 0; i < N; ++i) tempBuffer[i] -= (float)mean;

        double sumSq = 0.0;
        for (int i = 0; i < N; ++i) sumSq += tempBuffer[i] * tempBuffer[i];
        const float rms = std::sqrt(sumSq / N);

        if (rms >= rmsOn) { ++onCount; offCount = 0; }
        else if (rms <= rmsOff) { ++offCount; onCount = 0; }

        if (!audible && onCount >= onBlocksNeeded)
        {
            audible = true;
            lastAudibleTime = std::chrono::steady_clock::now();
            offCount = 0;
            gateOpenTime = lastAudibleTime;
            gateJustOpened = true;
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

        const PitchResult pr = detector->process(tempBuffer.data(), N);
        float freq = pr.hz;
        const float conf = pr.confidence;

        const auto now = std::chrono::steady_clock::now();
        const bool inAttack = (std::chrono::duration_cast<std::chrono::milliseconds>(now - gateOpenTime).count() < attackIgnoreMs);
        if (inAttack)
            continue;
        if (gateJustOpened) gateJustOpened = false;

        static bool confGate = false;
        const float confOn = 0.90f;
        const float confOff = 0.65f;
        if (!confGate && conf >= confOn) confGate = true;
        if (confGate && conf < confOff)  confGate = false;

        if (!std::isfinite(freq) || freq <= 0.0f || !confGate)
        {
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

        freq = juce::jlimit(70.0f, 1200.0f, freq);

        if (lastFreqSmooth > 0.0f)
        {
            const double centsDelta = 1200.0 * std::log2(freq / lastFreqSmooth);
            const bool recent = (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFreqOkTime).count() < freqHoldMs);
            if (recent && std::abs(centsDelta) < 6.0)
                freq = lastFreqSmooth;
        }

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
        lastFreqOkTime = now;

        // ── Автофиксация: подавление частот вне зафиксированной струны ───────────────
        if (lockedString >= 0)
        {
            const float lockedFreq = stringFreqs[lockedString] * (float)(getReferenceA4() / 440.0);
            const float cents = 1200.0f * std::log2(lastFreqSmooth / lockedFreq);
            const float lockRadius = 35.0f;

            if (std::abs(cents) > lockRadius)
            {
                lastFreqSmooth = -1.0f;
                lastFreq.store(-1.0f, std::memory_order_release);
                triggerAsyncUpdate();
                continue;
            }
        }

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

    // автофиксация всегда активна
    const auto now = std::chrono::steady_clock::now();

    if (lockedString < 0)
    {
        lockedString = bestIdx;
        stringLockTime = now;
    }

    // удержание по радиусу
    {
        const float targetLocked = stringFreqs[lockedString] * scale;
        const float centsLocked = 1200.0f * std::log2(f / targetLocked);
        const float keepRadius = 35.0f;
        if (std::abs(centsLocked) <= keepRadius)
        {
            bestIdx = lockedString;
            bestCents = centsLocked;
        }
    }

    // таймаут фиксации
    const bool lockExpired =
        (std::chrono::duration_cast<std::chrono::milliseconds>(now - stringLockTime).count() >= stringLockMs);

    if (!lockExpired && bestIdx != lockedString)
    {
        const float target = stringFreqs[lockedString] * scale;
        bestIdx = lockedString;
        bestCents = 1200.0f * std::log2(f / target);
    }
    else if (bestIdx != lockedString)
    {
        const float targetLocked = stringFreqs[lockedString] * scale;
        const float centsLocked = 1200.0f * std::log2(f / targetLocked);
        if (std::abs(centsLocked) > 90.0f)
            stringLockTime = now - std::chrono::milliseconds(stringLockMs);

        lockedString = bestIdx;
        stringLockTime = now;
        freqHistCount = 0;
        lastFreqSmooth = -1.0f;
        lastGoodFreq = -1.0f;
    }
    currentString = stringNames[bestIdx];
    currentCents = bestCents;
}
void TunerComponent::paint(juce::Graphics& g)
{
    if (visualStyle == TunerVisualStyle::Classic)
        drawClassicStyle(g);
    else
        drawTriangleStyle(g);
}
void TunerComponent::resized()
{
    auto b = getLocalBounds();
    const int h = b.getHeight();
    const int w = b.getWidth();

    const int topH = juce::jmax(24, h / 10);
    const int noteH = h / 4;
    const int scaleH = h / 3;
    const int controlH = juce::jmax(24, h / 12);

    auto top = b.removeFromTop(topH);
    auto note = b.removeFromTop(noteH);
    auto scale = b.removeFromTop(scaleH);
    auto rest = b;

    noteArea = note.reduced(w / 10, 0);
    scaleArea = scale.reduced(w / 20, 0);
    stringArea = rest;

    // Центрирование по горизонтали (ваша группа)
    const int btnW = juce::jmax(24, w / 12);
    const int labelW = juce::jmax(48, w / 6);
    const int spacing = 8;
    const int totalW = btnW * 2 + labelW + spacing * 2;
    const int x = (w - totalW) / 2;

    // Вертикальное размещение: ровно на половине между scaleArea и низом
    const int yStart = scaleArea.getBottom();
    const int yEnd = h;
    const int y = yStart + (yEnd - yStart - controlH) / 2;

    minusButton.setBounds(x, y, btnW, controlH);
    referenceLabel.setBounds(x + btnW + spacing, y, labelW, controlH);
    plusButton.setBounds(x + btnW + spacing + labelW + spacing, y, btnW, controlH);

    

    // Шрифт метки A4
    const int labelFontSize = juce::jmax(14, (int)(controlH * 0.6f));
    referenceLabel.setFont(juce::Font((float)labelFontSize, juce::Font::bold));
}

void TunerComponent::setVisualStyle(TunerVisualStyle style) noexcept
{
    visualStyle = style;
    repaint();
}
void TunerComponent::drawClassicStyle(juce::Graphics& g)
{
    const float  freq = lastFreq.load(std::memory_order_acquire);             // текущая частота
    const double a4 = referenceA4.load(std::memory_order_acquire);            // опорная частота A4
    const bool   hasSignal = freq > 0.0f;                                     // есть ли сигнал

    auto areaF = scaleArea.toFloat();                                         // область шкалы
    const float cx = areaF.getCentreX();                                      // центр по X
    const float halfW = areaF.getWidth() * 0.4f;                              // половина ширины шкалы (можно увеличить до 0.48f)
    const float lineY = areaF.getBottom() - areaF.getHeight() * 0.1f;        // вертикальная позиция шкалы (можно опустить ближе к низу)

    if (hasSignal)
    {
        const double midi = 69.0 + 12.0 * std::log2(freq / a4);               // вычисление MIDI-ноты
        const int    noteNum = (int)std::lround(midi);                        // округлённая нота
        const float  centsDeltaRaw = (float)((midi - noteNum) * 100.0);      // отклонение в центах
        const float  centsDelta = juce::jlimit(-50.0f, 50.0f, centsDeltaRaw); // ограничение ±50¢
        const float  absCents = std::abs(centsDelta);                         // модуль отклонения

        juce::Colour bg;                                                      // цвет фона по точности
        if (absCents <= perfectRange) bg = juce::Colours::green;
        else if (absCents <= warningRange)
            bg = juce::Colours::green.interpolatedWith(juce::Colours::yellow,
                juce::jlimit(0.0f, 1.0f, (absCents - perfectRange) / (warningRange - perfectRange)));
        else if (absCents <= dangerRange)
            bg = juce::Colours::yellow.interpolatedWith(juce::Colours::red,
                juce::jlimit(0.0f, 1.0f, (absCents - warningRange) / (dangerRange - warningRange)));
        else bg = juce::Colours::red;

        g.fillAll(bg);                                                        // заливка фона

        g.setColour(juce::Colours::white);
        g.setFont(noteArea.getHeight() * 0.7f);                               // крупный шрифт ноты
        g.drawText(currentString, noteArea, juce::Justification::centred);   // отображение ноты

        // стрелка: шкала ±50¢
        const float dx = (centsDelta / 50.0f) * halfW;                        // смещение стрелки

        constexpr float arrowHalfW = 8.0f, arrowHalfH = 20.0f;                // размеры стрелки (можно увеличить до 12/24)
        juce::Path arrow;
        arrow.startNewSubPath(cx + dx - arrowHalfW, lineY + arrowHalfH);
        arrow.lineTo(cx + dx + arrowHalfW, lineY + arrowHalfH);
        arrow.lineTo(cx + dx, lineY - arrowHalfH);
        arrow.closeSubPath();

        g.setColour(juce::Colours::blue);
        g.fillPath(arrow);                                                    // отрисовка стрелки

        // частота
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.setFont(juce::jmax(12.0f, noteArea.getHeight() * 0.18f));           // шрифт частоты
        g.drawText(juce::String(freq, 1) + " Hz", stringArea, juce::Justification::centredTop);
    }
    else
    {
        g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));     // фон при отсутствии сигнала
        g.setColour(juce::Colours::white);
        g.setFont(noteArea.getHeight() * 0.5f);
        g.drawText("No signal", noteArea, juce::Justification::centred);      // надпись при отсутствии сигнала
    }

    // шкала ±50¢
    g.setColour(juce::Colours::darkgreen);
    const float zoneW = (perfectRange / 50.0f) * halfW;                        // ширина зелёной зоны
    g.fillRect((int)(cx - zoneW), (int)(lineY - 2.0f), (int)(zoneW * 2.0f), 4); // зелёная зона

    g.setColour(juce::Colours::white);
    g.drawLine(cx - halfW, lineY, cx + halfW, lineY, 3.0f);                    // основная линия шкалы (можно увеличить до 3.0f)

    const int totalTicks = 11;                                                // количество делений
    const float step = (halfW * 2.0f) / (totalTicks - 1);                      // шаг между делениями
    for (int i = 0; i < totalTicks; ++i)
    {
        const float x = cx - halfW + i * step;
        const int centsLabel = (i - (totalTicks - 1) / 2) * 10;
        const bool major = (centsLabel % 50) == 0;
        const float tickH = major ? 12.0f : 6.0f;                              // высота деления (можно увеличить до 16/10)
        const float w = major ? 2.0f : 1.0f;                                   // толщина деления (можно увеличить до 2.5/1.5)

        g.drawLine(x, lineY - tickH * 0.5f, x, lineY + tickH * 0.5f, w);

        if (major)
        {
            g.setFont(14.0f);                                                  // шрифт метки (можно увеличить до 18.0f)
            g.drawText(juce::String(centsLabel),
                (int)(x - 15), (int)(lineY + 8), 30, 20,
                juce::Justification::centred);
        }
    }
}
void TunerComponent::drawTriangleStyle(juce::Graphics& g)
{
    const float  freq = lastFreq.load(std::memory_order_acquire);             // текущая частота
    const bool   hasSignal = (freq > 0.0f);                                   // есть ли сигнал

    // Фон окна постоянный
    g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));        // фон по LookAndFeel

    // Нота / частота
    g.setColour(juce::Colours::white);
    g.setFont(noteArea.getHeight() * 0.7f);                                   // крупный шрифт ноты
    g.drawText(hasSignal ? currentString : "No signal", noteArea, juce::Justification::centred);

    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.setFont(juce::jmax(12.0f, noteArea.getHeight() * 0.18f));               // шрифт частоты
    if (hasSignal)
        g.drawText(juce::String(freq, 1) + " Hz", stringArea, juce::Justification::centredTop);

    // Геометрия шкалы
    const auto s = scaleArea.toFloat();                                       // область шкалы
    const float centerX = s.getCentreX();                                     // центр по X
    const float yMid = s.getY() + s.getHeight() * 0.65f;                      // вертикальная позиция шкалы

    const int   N = 11;                                                       // треугольников на сторону
    const float maxCents = 50.0f;                                             // диапазон ±50¢
    const float spacing = s.getWidth() / (N * 2.0f + 1.0f);                   // расстояние между треугольниками
    const float triLen = spacing * 0.70f;                                     // длина треугольника (можно увеличить до 0.85f)
    const float triHeight = juce::jmin(s.getHeight() * 0.35f, spacing * 0.95f); // высота треугольника (можно увеличить до 0.45f)

    const float rectW = triLen * 0.80f;                                       // ширина центрального прямоугольника
    const float rectH = triHeight;                                           // высота прямоугольника
    const juce::Rectangle<float> centerRect(centerX - rectW * 0.5f, yMid - rectH * 0.5f, rectW, rectH);

    // Цветовая логика по точности
    auto colourForAbsCents = [this](float absC) -> juce::Colour
        {
            if (absC <= perfectRange) return juce::Colours::green;
            if (absC <= warningRange)
            {
                const float t = juce::jlimit(0.0f, 1.0f, (absC - perfectRange) / (warningRange - perfectRange));
                return juce::Colours::green.interpolatedWith(juce::Colours::yellow, t);
            }
            if (absC <= dangerRange)
            {
                const float t = juce::jlimit(0.0f, 1.0f, (absC - warningRange) / (dangerRange - warningRange));
                return juce::Colours::yellow.interpolatedWith(juce::Colours::red, t);
            }
            return juce::Colours::red;
        };

    const juce::Colour neutralFill = juce::Colours::darkgrey.withAlpha(0.35f);   // цвет неактивных треугольников
    const juce::Colour neutralOutline = juce::Colours::grey.withAlpha(0.85f);    // контур треугольников

    const float cents = currentCents;                                            // отклонение в центах
    const float absCents = std::abs(cents);
    const bool  centerHit = hasSignal && (absCents <= perfectRange);            // попадание в строй

    int  activeIdx = 0;
    bool activeLeft = false;

    if (hasSignal && !centerHit)
    {
        const float bin = maxCents / (float)N;                                   // ширина одного сегмента
        activeIdx = juce::jlimit(1, N, (int)std::lround(absCents / bin));        // индекс активного треугольника
        activeLeft = (cents < 0.0f);                                             // <0 — левый ряд
    }

    // Треугольники (всегда видны); вершины смотрят к центру
    auto drawTri = [&](float apexX, int dir, bool active)
        {
            juce::Path p;
            p.startNewSubPath(apexX, yMid);                                          // вершина у центра

            if (dir == +1)                                                           // левая сторона
            {
                p.lineTo(apexX - triLen, yMid - triHeight * 0.5f);
                p.lineTo(apexX - triLen, yMid + triHeight * 0.5f);
            }
            else                                                                     // правая сторона
            {
                p.lineTo(apexX + triLen, yMid - triHeight * 0.5f);
                p.lineTo(apexX + triLen, yMid + triHeight * 0.5f);
            }
            p.closeSubPath();

            g.setColour(active ? colourForAbsCents(absCents) : neutralFill);        // цвет по активности
            g.fillPath(p);
            g.setColour(neutralOutline);
            g.strokePath(p, juce::PathStrokeType(1.2f));                             // контур треугольника
        };

    for (int j = 1; j <= N; ++j)
    {
        const float xLeftApex = centerX - j * spacing;
        const float xRightApex = centerX + j * spacing;

        const bool isActiveLeft = (hasSignal && !centerHit && activeLeft && j == activeIdx);
        const bool isActiveRight = (hasSignal && !centerHit && !activeLeft && j == activeIdx);

        drawTri(xLeftApex, +1, isActiveLeft);                                    // рисуем левый
        drawTri(xRightApex, -1, isActiveRight);                                  // рисуем правый
    }

    // Центральный прямоугольник (всегда виден). Активен только при попадании.
    g.setColour(centerHit ? colourForAbsCents(absCents) : neutralFill);
    g.fillRect(centerRect);
    g.setColour(neutralOutline);
    g.drawRect(centerRect, 1.2f);                                                // контур прямоугольника
}



