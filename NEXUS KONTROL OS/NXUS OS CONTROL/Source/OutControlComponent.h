#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <limits>
#include "Grid.h"
#include "out_control_label.h"
#include "LevelMeterComponent.h"
#include "LevelScaleComponent.h"
#include "eq_dsp.h"
#include "LevelUtils.h"
class OutControlComponent : public juce::Component,
    private juce::Slider::Listener,
    private juce::Timer
{
public:
    OutControlComponent();
    ~OutControlComponent() override;

    /** Вызывается перед стартом аудио — сбрасывает состояние метеров и готовит DSP */
    void prepare(double sampleRate, int blockSize) noexcept;

    /** Основной аудио-поток: эквалайзер, gain, mute/solo, level-метры */
    void processAudioBlock(juce::AudioBuffer<float>& buffer) noexcept;

    void saveSettings() const;
    void loadSettings();

    void resized() override;
    void sliderValueChanged(juce::Slider* slider) override;

    /** Для связки между копиями компонента (если нужно) */
    void setOutControlComponent(OutControlComponent* o) noexcept { outControl = o; }
    //--
        /** Возвращает текущий гейн L/R в дБ */
    float getGainDbL() const noexcept { return (float)gainSliderL.getValue(); }
    float getGainDbR() const noexcept { return (float)gainSliderR.getValue(); }
     

    /** Коллбэк: мастер-гейн (среднее) изменился вручную */
    std::function<void(float /*newAvgDb*/)> onMasterGainChanged;

    /** Сместить оба гейна на deltaDb, сохраняя их относительную разницу */
    void offsetGainDb(float deltaDb) noexcept;

private:
    //==============================================================================
    // Ссылка на «другой» экземпляр (если используется)
    OutControlComponent* outControl = nullptr;
     bool isLinking = false;
    //==============================================================================
    // DSP-ядро эквалайзера
    EqDsp                             eqDsp;
    std::atomic<float>               atomicEqVals[5]{ 30.f, 0.f, 0.f, 0.f, 16000.f };
    std::atomic<bool>                eqParamsDirty{ true };
    std::atomic<bool>                eqBypassed{ false };

    // Таймер обновления DSP — по умолчанию ежем 10 блоков
    int                               dspUpdateCounter = 0;
    static constexpr int             dspUpdateInterval = 10;

    //==============================================================================
    // UI-слайдеры эквалайзера
    std::array<juce::Slider, 5>      eqSliders;
    std::array<juce::Label, 5>       eqLabels;
    juce::Label                       masterEqLabel;
    juce::TextButton                  bypassButton{ "EQ" };

    // Частотные предустановки и комбо-боксы
    std::vector<float>                lowShelfPresets{ 40,   80,   150, 250, 300, 350 };
    std::vector<float>                peakPresets{ 300,  350,  400, 450, 500, 550, 600, 650, 700, 750, 800, 850, 900, 950, 1000, 1500 };
    std::vector<float>                highShelfPresets{ 1000, 1500, 2000, 2500, 3000, 3500, 4000, 4500, 5000, 6000, 7000, 8000, 10000, 15000 };

    juce::ComboBox                    lowShelfFreqBox, peakFreqBox, highShelfFreqBox;
    juce::Label                       lowShelfLabel, peakLabel, highShelfLabel;
    std::atomic<float>               lowShelfFreqVal{ lowShelfPresets[0] };
    std::atomic<float>               peakFreqVal{ peakPresets[0] };
    std::atomic<float>               highShelfFreqVal{ highShelfPresets[0] };

    //==============================================================================
    // Атомарные параметры выхода
    std::atomic<float>               gainValL{ 1.0f }, gainValR{ 1.0f };
    std::atomic<bool>                atomicSoloL{ false }, atomicSoloR{ false };
    std::atomic<bool>                atomicMuteL{ false }, atomicMuteR{ false };
    std::atomic<bool>                atomicLink{ true };
    std::atomic<bool>                atomicMeterMode{ false };

    void syncButtonStatesToAtomics() noexcept
    {
        atomicMuteL.store(muteButtonL.getToggleState(), std::memory_order_relaxed);
        atomicSoloL.store(soloButtonL.getToggleState(), std::memory_order_relaxed);
        atomicMuteR.store(muteButtonR.getToggleState(), std::memory_order_relaxed);
        atomicSoloR.store(soloButtonR.getToggleState(), std::memory_order_relaxed);
        atomicLink.store(linkButton.getToggleState(), std::memory_order_relaxed);
        atomicMeterMode.store(meterModeButton.getToggleState(), std::memory_order_relaxed);
        eqBypassed.store(bypassButton.getToggleState(), std::memory_order_relaxed);
    }

    // UI-элементы управления выходом
    juce::Slider                      gainSliderL, gainSliderR;
    juce::Label                       gainLabelL, gainLabelR;
    juce::TextButton                  muteButtonL, soloButtonL;
    juce::TextButton                  muteButtonR, soloButtonR;
    juce::TextButton                  linkButton;
    juce::TextButton                meterModeButton;
    juce::Label clipLedL, clipLedR;
    //==============================================================================
    // Кэширование вычислений Gain:
    // — последние дБ-значения (чтобы не пересчитывать convert каждый блок)
    // — кэширующие линейные коэффициенты
    float                             lastGainDbL{ std::numeric_limits<float>::infinity() };
    float                             lastGainDbR{ std::numeric_limits<float>::infinity() };
    float                             cachedGainL{ 1.0f };
    float                             cachedGainR{ 1.0f };

    //==============================================================================
    // Мастер-метры и оформление
    LevelMeterComponent               masterMeterL, masterMeterR;
    LevelScaleComponent               masterScale;
    juce::Label                       masterOutLabel;
    OutLookAndFeel                    customLF, customLaf;
  
    //==============================================================================
    // Для prepare / resized
    double                            lastSampleRate{ 0.0 };
    int                               lastBlockSize{ 0 };
    bool meterWasUpdated = false;
    void timerCallback() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutControlComponent)
};
