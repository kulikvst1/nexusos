#pragma once

#include <JuceHeader.h>
#include "SetCCDialog.h"    // CCMapping, SetCCDialog
#include "vst_host.h"       // VSTHostComponent

#include <vector>
#include <functional>
struct PresetCCMapping
{
    uint8_t ccValue = 64;
    bool enabled = false;
    bool invert = false; // добавляем поле invert
};
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
    

    struct Bank
    {
        juce::String  bankName{ "Untitled Bank" };
        juce::String  pluginName{ "None" };
        juce::String  presetNames[numPresets];
        std::vector<std::vector<bool>> ccPresetStates;  // если он вам еще нужен
        std::vector<float> presetVolumes;   // [numPresets]

        // Новая часть:
        // 1. Глобальные настройки для каждой из 10 кнопок CC (общие для всех пресетов)
        std::array<CCMapping, numCCParams> globalCCMappings;

        // 2. Пресетно-специфичные настройки для каждой CC-кнопки (каждый пресет – свой набор)
        std::vector<std::array<PresetCCMapping, numCCParams>> presetCCMappings;

        Bank()
        {
            // Инициируем имена пресетов
            for (int i = 0; i < numPresets; ++i)
                presetNames[i] = "Preset " + juce::String(i + 1);

            ccPresetStates.assign(numPresets, std::vector<bool>(numCCParams, false));
            presetVolumes.assign(numPresets, 1.0f);

            // Инициализация глобальных настроек:
            for (int i = 0; i < numCCParams; ++i)
            {
               // globalCCMappings[i].name = "<none> " + juce::String(i + 1);
                 globalCCMappings[i].name = "<none> ";
                // paramIndex по умолчанию = -1 (не назначен), invert = false
            }

            // Инициализация пресетных настроек:
            presetCCMappings.resize(numPresets);
            for (auto& ccArray : presetCCMappings)
            {
                for (int i = 0; i < numCCParams; ++i)
                    ccArray[i] = PresetCCMapping{}; // ccValue = 127, enabled = false
            }
        }
    };


    //==============================================================================  
/** Возвращает индекс текущего (активного) пресета. */
    int getActivePresetIndex() const noexcept { return activePreset; }

    /** Прокинуть сюда midiOut из Main.cpp */
    void setMidiOutput(juce::MidiOutput* m) noexcept { midiOutput = m; }

    /** Метод-калькулятор и отправщик CC-сообщений + VST-параметров */
    void updateCCParameter(int index, bool state);

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
    void setActivePreset(int newPreset);
    

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
    

    //==============================================================================
    // Обработчик кнопок
    void buttonClicked(juce::Button* b) override;
    void BankEditor::updatePresetButtons();
    // Диалоги и всплывающие меню
    void showBankSelectionMenu();
    void showVSTDialog();
    void editCCParameter(int ccIndex);

    // Смена активных индексов
    void setActiveBank(int newBank);
   // void setActivePreset(int newPreset);

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

    juce::MidiOutput* midiOutput = nullptr;  // новый указатель на MIDI-выход

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
