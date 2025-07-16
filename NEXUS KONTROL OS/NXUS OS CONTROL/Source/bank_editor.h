#pragma once
#include <JuceHeader.h>
#include <vector>
#include <functional>
#include "SetCCDialog.h"    
#include "vst_host.h"
#include "LearnController.h" 

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
    private LearnController::Listener,
    private juce::Timer
{
public:
    // Внутри class BankEditor:
    static constexpr int numPresets = 6;
    static constexpr int numCCParams = 10;
    struct Bank
    {
        juce::String  bankName{ "Untitled Bank" };
        juce::String  pluginName{ "None" };
        juce::String  presetNames[numPresets];
        std::vector<std::vector<bool>> ccPresetStates;  
        std::vector<float> presetVolumes;  
        std::array<CCMapping, numCCParams> globalCCMappings;
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
            globalCCMappings[i].name = "<none> ";
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
   
    void resetToDefaults() { resetAllDefaults(); updateUI(); sendChange(); }
    void refreshAllUI() { updateUI(); updateSelectedPresetLabel(); }
    void sendChange() { if (onBankEditorChanged) onBankEditorChanged(); }
    explicit BankEditor(VSTHostComponent* host = nullptr);

    ~BankEditor() override;
   
    void paint(juce::Graphics& g) override;
    void resized() override;
    const std::vector<Bank>& getBanks() const noexcept { return banks; }
    std::vector<Bank>& getBanks()       noexcept { return banks; }
    int  getActiveBankIndex() const noexcept { return activeBankIndex; }
    void setActiveBankIndex(int newIdx);
    void setActivePreset(int newPreset);
    std::function<void()> onBankEditorChanged;
    void loadSettings();
    void saveSettings();
    const Bank& getBank(int idx) const noexcept { return banks.at(idx); }
    Bank& getBank(int idx)       noexcept { return banks.at(idx); }
    std::function<void(int /*newPresetIndex*/)> onActivePresetChanged;
    std::function<void()> onBankChanged;
    

    
private:

    bool isSettingPreset = false;
    bool isSettingLearn = false;

    void onPluginParameterChanged(int paramIdx, float normalised); // ← без BankEditor:: и без ;
    // Timer → автосохранение
    void timerCallback() override;
    void restartAutoSaveTimer();
    // Файловые операции
    void saveSettingsToFile(const juce::File& configFile);
    void loadSettingsFromFile(const juce::File& configFile);
    // Сброс и подсветка
    void resetAllDefaults();
    // Обработчик кнопок
    void buttonClicked(juce::Button* b) override;
    void BankEditor::updatePresetButtons();
    // Диалоги и всплывающие меню
    void showBankSelectionMenu();
    void showVSTDialog();
    void editCCParameter(int ccIndex);
    // Смена активных индексов
    void setActiveBank(int newBank);
    // Полное обновление UI
    void updateUI();
    void updateSelectedPresetLabel();
    void loadFromDisk();
    void saveToDisk();
    void storeToBank();
    void cancelChanges();
    void propagateInvert(int slot, bool newInvert);
    void clearCCMappingsForActiveBank();
    void resetCCSlotState(int slot);
     
    static constexpr int numBanks = 20;
    std::vector<Bank> banks;
    int               activeBankIndex = 0;
    int               activePreset = 0;
    juce::MidiOutput* midiOutput = nullptr;  // новый указатель на MIDI-выход
    VSTHostComponent* vstHost = nullptr;
    juce::Label       bankIndexLabel, pluginLabel, selectedPresetLabel;
    juce::TextButton  selectBankButton, presetButtons[numPresets], vstButton;
    juce::TextEditor  bankNameEditor, presetEditors[numPresets];
    juce::TextButton  setCCButtons[numCCParams];
    juce::TextEditor  ccNameEditors[numCCParams];
    juce::TextButton  ccToggleButtons[numCCParams];
    juce::TextButton  defaultButton, loadButton, storeButton, saveButton, cancelButton;
    juce::TextButton learnButtons[numCCParams]; // 10 зелёных кнопок LEARN
    juce::AudioPluginFormatManager  formatManager;
    juce::OwnedArray<juce::PluginDescription> availablePlugins;
    std::function<void(int /*cc*/, bool /*on*/)> onLearnToggled;
    void toggleLearnFromHost(int cc, bool on);

    /*  LEARN  */
    LearnController  learn{ *this };
    juce::Colour     oldLearnColour;
   // bool             isSettingPreset = false;   // переносим сюда (раньше был глобал)
    // —- реализации Listener —-
    void learnStarted(int) override;
    void learnFinished(int, int, const juce::String&) override;
    void learnCancelled(int) override;
  
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BankEditor)
};
