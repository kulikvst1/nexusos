#pragma once

#include <JuceHeader.h>
#include "Grid.h"
#include "fount_label.h"
#include "LevelMeterComponent.h"    // <— ваши готовые level-meters
#include"LevelScaleComponent.h"
#include "eq_dsp.h"  

class OutControlComponent : public juce::Component
{
public:
    OutControlComponent();
    ~OutControlComponent() override;

    /** Вызывается перед стартом аудио — сбрасывает состояние уровнемеров */
    void prepare(double sampleRate, int blockSize) noexcept;
    void processAudioBlock(juce::AudioBuffer<float>& buffer) noexcept;
    void saveSettings() const;
    void loadSettings();
    void resized() override;
    //
    OutControlComponent* outControl = nullptr;
    void setOutControlComponent(OutControlComponent* o) noexcept { outControl = o; }


private:
    EqDsp eqDsp;
    // сюда добавляем атомарные значения для EQ
    std::atomic<float> atomicEqVals[5]{ 30.0f, 0.0f, 0.0f, 0.0f, 16000.0f };

    // флаг «параметры EQ изменились»
    std::atomic<bool> eqParamsDirty{ true };
    bool isLinking = false;
    // Rotary-EQ
    std::array<juce::Slider, 5> eqSliders;
    std::array<juce::Label, 5> eqLabels;
    juce::Label masterEqLabel;
    // выходные гейны + подписи
    juce::Slider gainSliderL, gainSliderR;
    juce::Label  gainLabelL, gainLabelR;
    // сюда добавляем два новых вертикальных метра
    LevelMeterComponent masterMeterL, masterMeterR;
    // подпись MASTER OUT внизу
    juce::Label masterOutLabel;
    LevelScaleComponent  masterScale;
    CustomLookAndFeel customLF;
    juce::TextButton muteButtonL, soloButtonL;
    juce::TextButton muteButtonR, soloButtonR;
    juce::TextButton linkButton;

    double lastSampleRate = 0.0;
    int    lastBlockSize = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutControlComponent)
};
