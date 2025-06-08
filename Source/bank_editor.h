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
    //------------------------------------------------------------------------------
// Внутри class BankEditor:
    static constexpr int numPresets = 6;
    static constexpr int numCCParams = 10;

    //------------------------------------------------------------------------------  
    /** Модель одного банка. */
    struct Bank
    {
        juce::String  bankName{ "Untitled Bank" };
        juce::String  pluginName{ "None" };
        juce::String  presetNames[numPresets];
        CCMapping     ccMappings[numCCParams];
        std::vector<std::vector<bool>> ccPresetStates;  // [numPresets][numCCParams]
        std::vector<float>             presetVolumes;   // [numPresets]

        Bank()
        {
            // 1) Имена пресетов
            for (int i = 0; i < numPresets; ++i)
                presetNames[i] = "Preset " + juce::String(i + 1);

            // 2) Сброс CC-мэппингов
            for (int i = 0; i < numCCParams; ++i)
                ccMappings[i] = CCMapping{};

            // 3) Инициализация матрицы состояний CC [numPresets][numCCParams]
            ccPresetStates.assign(numPresets,
                std::vector<bool>(numCCParams, false));

            // 4) Инициализация громкостей каждого пресета (по умолчанию = 1.0f)
            presetVolumes.assign(numPresets, 1.0f);
        }
    };

    //==============================================================================  
/** Возвращает индекс текущего (активного) пресета. */
    int getActivePresetIndex() const noexcept { return activePreset; }

    /** Устанавливает активный пресет и оповещает о смене. */
    void setActivePresetIndex(int newPresetIndex);

    /** Возвращает список имён всех пресетов в указанном банке. */
    juce::StringArray getPresetNames(int bankIndex) const noexcept;

    /** Возвращает вектор CCMapping (включая признак enabled) для данного пресета. */
    std::vector<CCMapping> getCCMappings(int bankIndex,
        int presetIndex) const;

    /** Сброс всех банков/пресетов к дефолтным значениям + обновление UI + уведомление. */
    void resetToDefaults() { resetAllDefaults(); updateUI(); sendChange(); }

    /** Принудительно перерисовать весь UI и метку выбранного пресета. */
    void refreshAllUI() { updateUI(); updateSelectedPresetLabel(); }

    /** Оповестить внешний код об изменениях (вызывает onBankEditorChanged). */
    void sendChange() { if (onBankEditorChanged) onBankEditorChanged(); }

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
  //  static constexpr int numPresets = 6;
  //  static constexpr int numCCParams = 10;

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
