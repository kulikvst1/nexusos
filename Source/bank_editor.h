#pragma once

#include <JuceHeader.h>
#include "SetCCDialog.h"    // CCMapping, SetCCDialog
#include "vst_host.h"       // VSTHostComponent

#include <vector>
#include <functional>

//==============================================================================
// BankEditor — основной компонент для управления банками и CC-мэппингом.
//==============================================================================
class BankEditor : public juce::Component,
    private juce::Button::Listener,
    private juce::Timer
{
public:
    //==============================================================================
    /** Модель одного банка. */
    struct Bank
    {
        juce::String  bankName{ "Untitled Bank" };
        juce::String  pluginName{ "None" };
        juce::String  presetNames[6];
        CCMapping     ccMappings[10];

        Bank()
        {
            for (int i = 0; i < 6; ++i)
                presetNames[i] = "Preset " + juce::String(i + 1);

            // агрегатная инициализация CCMapping сбросит все поля, 
            // включая name == ""
            for (int i = 0; i < 10; ++i)
                ccMappings[i] = CCMapping{};
        }
    };

    //==============================================================================
    /**
      * @param host  — указатель на ваш VSTHostComponent (или nullptr, если не нужен).
      */
    explicit BankEditor(VSTHostComponent* host = nullptr);
    ~BankEditor() override;

    //==============================================================================  
    void paint(juce::Graphics& g) override;
    void resized() override;

    //==============================================================================
    // Публичный API для доступа к списку банков:
    const std::vector<Bank>& getBanks() const noexcept { return banks; }
    std::vector<Bank>& getBanks()       noexcept { return banks; }

    int  getActiveBankIndex() const noexcept { return activeBankIndex; }
    void setActiveBankIndex(int newIdx);

    void setPluginPresetSupported(bool supported);

    /** Вызывается внешним кодом при любом изменении модели. */
    std::function<void()> onBankEditorChanged;

    /** Загрузка/сохранение из файла. */
    void loadSettings();
    void saveSettings();

    const Bank& getBank(int idx) const noexcept { return banks.at(idx); }
    Bank& getBank(int idx)       noexcept { return banks.at(idx); }

private:
    //==============================================================================
    // Timer → автосохранение
    void timerCallback() override;
    void restartAutoSaveTimer();

    // Файловые операции
    void saveSettingsToFile(const juce::File& configFile);
    void loadSettingsFromFile(const juce::File& configFile);

    // Сброс и подсветка
    void resetAllDefaults();
    void updateUIHighlighting();

    //==============================================================================
    // Обработчик кнопок
    void buttonClicked(juce::Button* b) override;

    // Диалоги и всплывающие меню
    void showBankSelectionMenu();
    void showVSTDialog();
    void editCCParameter(int ccIndex);

    // Смена активных индексов
    void setActiveBank(int newBank);
    void setActivePreset(int newPreset);

    // Полное обновление UI
    void updateUI();
    void updateSelectedPresetLabel();

    // Кнопки Default/Load/Store/Save/Cancel
    void loadFromDisk();
    void saveToDisk();
    void storeToBank();
    void cancelChanges();

    //==============================================================================
    // Константы размеров
    static constexpr int numBanks = 20;
    static constexpr int numPresets = 6;
    static constexpr int numCCParams = 10;

    //==============================================================================
    // Сама модель + состояние
    std::vector<Bank> banks;
    int               activeBankIndex = 0;
    int               activePreset = 0;

    // Внешний VST-хост (для диалогов)
    VSTHostComponent* vstHost = nullptr;

    //==============================================================================
    // UI-компоненты
    juce::Label       bankIndexLabel, pluginLabel, selectedPresetLabel;
    juce::TextButton  selectBankButton, presetButtons[numPresets], vstButton;
    juce::TextEditor  bankNameEditor, presetEditors[numPresets];
    juce::TextButton  setCCButtons[numCCParams];
    juce::TextEditor  ccNameEditors[numCCParams];
    juce::TextButton  ccToggleButtons[numCCParams];
    juce::TextButton  defaultButton, loadButton, storeButton, saveButton, cancelButton;

    // --- новый менеджер форматов плагинов ---
    juce::AudioPluginFormatManager  formatManager;

    // --- popup-список плагинов, наполняется при открытии диалога ---
    juce::OwnedArray<juce::PluginDescription> availablePlugins;

    // =======================================================================
    

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BankEditor)
};
