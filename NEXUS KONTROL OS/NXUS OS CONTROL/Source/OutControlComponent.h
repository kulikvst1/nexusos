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

    // 1) флаг обхода EQ
    std::atomic<bool> eqBypassed{ false };
    // 2) кнопка-«тумблер» для обхода
    juce::TextButton bypassButton{ "EQ" };

    // UI-компоненты
    juce::ComboBox lowShelfFreqBox, peakFreqBox, highShelfFreqBox;
    juce::Label    lowShelfLabel, peakLabel, highShelfLabel;

    // Наборы предустановленных значений
    std::vector<float> lowShelfPresets{ 40.0f, 80.0f, 150.0f, 250.0f,300.0f, 350.0f };
    std::vector<float> peakPresets{ 300.0f, 350.0f, 400.0f, 450.0f, 500.0f, 550.0f, 600.0f, 650.0f, 700.0f, 750.0f, 800.0f, 850.0f, 900.0f, 950.0f,1000.0f, 1500.0f };
    std::vector<float> highShelfPresets{ 1000.0f, 1500.0f, 2000.0f, 2500.0f, 3000.0f, 3500.0f, 4000.0f, 4500.0f, 5000.0f, 6000.0f, 7000.0f, 8000.0f, 10000.0f, 15000.0f };

    // Атомарные хранилища текущего выбора
    std::atomic<float> lowShelfFreqVal{ lowShelfPresets[0] };
    std::atomic<float> peakFreqVal{ peakPresets[0] };
    std::atomic<float> highShelfFreqVal{ highShelfPresets[0] };
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
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
