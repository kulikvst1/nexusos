#pragma once
#define NOMINMAX
#include <JuceHeader.h>
#include <vector>
#include <functional>
#include <array>
#include <unordered_map>
#include "SetCCDialog.h"    
#include "vst_host.h"
#include "LearnController.h" 
#include "FileManager.h" 
#include <windows.h>


class PluginManager; // ✅ добавлено: вперёд объявление

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
    static constexpr int numPresets = 6;
    static constexpr int numCCParams = 14;

    struct Bank
    {
        // --- Пользовательские данные ---
        juce::String  bankName{ "Untitled Bank" };
        juce::String  pluginName{ "None" };
        juce::String  presetNames[numPresets];
        int           activeProgram = -1;

        std::vector<std::vector<bool>> ccPresetStates;
        std::vector<float>             presetVolumes;
        std::array<CCMapping, numCCParams> globalCCMappings;
        std::vector<std::array<PresetCCMapping, numCCParams>> presetCCMappings;
        std::vector<float> pluginParamValues;

        // --- Технические данные ---
        juce::String  pluginId;
        juce::MemoryBlock pluginState;
        std::unordered_map<int, float> paramDiffs;

        // --- Конструктор ---
        Bank()
        {
            for (int i = 0; i < numPresets; ++i)
                presetNames[i] = "Scene " + juce::String(i + 1);

            ccPresetStates.assign(numPresets, std::vector<bool>(numCCParams, false));
            presetVolumes.assign(numPresets, 1.0f);

            for (int i = 0; i < numCCParams; ++i)
                globalCCMappings[i].name = "<none>";

            presetCCMappings.resize(numPresets);
            for (auto& ccArray : presetCCMappings)
                for (int i = 0; i < numCCParams; ++i)
                    ccArray[i] = PresetCCMapping{};
        }

        // --- Сравнение для мигания Store ---
        bool operator==(const Bank& other) const
        {
            // Строгое сравнение имени банка
            if (bankName != other.bankName) return false;

            if (pluginName != other.pluginName) return false;
            if (pluginId != other.pluginId) return false;
            if (activeProgram != other.activeProgram) return false;

            // Строгое сравнение имён пресетов
            for (int i = 0; i < numPresets; ++i)
                if (presetNames[i] != other.presetNames[i])
                    return false;

            if (ccPresetStates != other.ccPresetStates) return false;
            if (presetVolumes != other.presetVolumes) return false;

            // Сравнение всех полей CCMapping
            for (int i = 0; i < numCCParams; ++i)
            {
                if (globalCCMappings[i].name != other.globalCCMappings[i].name) return false;
                if (globalCCMappings[i].paramIndex != other.globalCCMappings[i].paramIndex) return false;
                if (globalCCMappings[i].enabled != other.globalCCMappings[i].enabled) return false;
               // if (globalCCMappings[i].defaultValue != other.globalCCMappings[i].defaultValue) return false;
            }

            // Сравнение пресетных CCMapping
            if (presetCCMappings.size() != other.presetCCMappings.size()) return false;
            for (size_t p = 0; p < presetCCMappings.size(); ++p)
            {
                for (int i = 0; i < numCCParams; ++i)
                {
                    if (presetCCMappings[p][i].ccValue != other.presetCCMappings[p][i].ccValue) return false;
                    if (presetCCMappings[p][i].invert != other.presetCCMappings[p][i].invert) return false;
                    if (presetCCMappings[p][i].enabled != other.presetCCMappings[p][i].enabled) return false;
                }
            }

            // Сравнение параметров плагина с допуском для float
            if (pluginParamValues.size() != other.pluginParamValues.size()) return false;
            for (size_t i = 0; i < pluginParamValues.size(); ++i)
            {
                if (std::fabs(pluginParamValues[i] - other.pluginParamValues[i]) > 1e-6f)
                    return false;
            }

            return true;
        }


        bool operator!=(const Bank& other) const
        {
            return !(*this == other);
        }
    };

    /** Возвращает индекс текущего (активного) пресета. */
    int getActivePresetIndex() const noexcept { return activePreset; }

    /** Прокинуть сюда midiOut из Main.cpp */
    void setMidiOutput(juce::MidiOutput* m) noexcept { midiOutput = m; }

    // Универсальное применение состояния CC к железу/плагину + уведомление UI
    void updateCCParameter(int index, bool state);

    /** Устанавливает активный пресет и оповещает о смене. */
    void setActivePresetIndex(int newPresetIndex);

    /** Возвращает список имён всех пресетов в указанном банке. */
    juce::StringArray getPresetNames(int bankIndex) const noexcept;

    /** Возвращает вектор CCMapping (включая признак enabled) для данного пресета. */
    std::vector<CCMapping> getCCMappings(int bankIndex, int presetIndex) const;

    void resetToDefaults() { resetAllDefaults(); updateUI(); sendChange(); }
    void refreshAllUI() { updateUI(); updateSelectedPresetLabel(); }
    void sendChange() { if (onBankEditorChanged) onBankEditorChanged(); }

    // ✅ Конструктор с менеджером и хостом
    explicit BankEditor(PluginManager& pm, VSTHostComponent* host = nullptr);

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

    /** Возвращает имя CC‑параметра по индексу (0..numCCParams-1) */
    juce::String getCCName(int ccIndex) const noexcept
    {
        if (ccIndex >= 0 && ccIndex < numCCParams)
            return ccNameEditors[ccIndex].getText();
        return {};
    }
    void toggleCC(int ccIndex, bool state);
    void unloadPluginEverywhere();
    void applyBankToPlugin(int bankIndex, bool synchronous = false);
    void applyPedalValue(int slot, float norm);
    void openPedalMappingDialog(int slot);

private:
    bool isSettingPreset = false;
    bool isSettingLearn = false;

    void onPluginParameterChanged(int paramIdx, float normalised); // ← без BankEditor:: и без ;
    // Timer → автосохранение
    void timerCallback() override;

    // Файловые операции
    void saveSettingsToFile(const juce::File& configFile);
    void loadSettingsFromFile(const juce::File& configFile);

    // Сброс и подсветка
    void resetAllDefaults();
    // Обработчик кнопок
    void buttonClicked(juce::Button* b) override;
    void updatePresetButtons();
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

    // ✅ добавлено: ссылка на глобальный менеджер
    PluginManager& pluginManager;

    VSTHostComponent* vstHost = nullptr;
    juce::Label       bankIndexLabel, pluginLabel, selectedPresetLabel;
    juce::TextButton  selectBankButton, presetButtons[numPresets], vstButton;
    juce::TextEditor  bankNameEditor, presetEditors[numPresets];
    juce::TextButton  setCCButtons[numCCParams];
    juce::TextEditor  ccNameEditors[numCCParams];
    juce::TextButton  ccToggleButtons[numCCParams];
    juce::TextButton  defaultButton, loadButton, storeButton, saveButton, cancelButton;
    juce::TextButton  learnButtons[numCCParams]; // 10 зелёных кнопок LEARN
    juce::AudioPluginFormatManager  formatManager;
    juce::OwnedArray<juce::PluginDescription> availablePlugins;
    std::function<void(int /*cc*/, bool /*on*/)> onLearnToggled;
    void toggleLearnFromHost(int cc, bool on);

    /*  LEARN  */
    LearnController  learn{ *this };
    juce::Colour     oldLearnColour;
    // —- реализации Listener —-
    void learnStarted(int) override;
    void learnFinished(int, int, const juce::String&) override;
    void learnCancelled(int) override;
    // --- Работа с дефолтным конфигом ---
    juce::File getDefaultConfigFile() const;
    void ensureDefaultConfigExists();
    void writeDefaultConfig();   // создать дефолтный конфиг на диске из текущих banks[]
    void loadDefaultConfig();    // загрузить дефолтный конфиг в память

    static constexpr const char* kDefaultConfigName = "banks_config.xml";
    // --- Глобальные данные о плагине (один плагин для всех банков) ---
    juce::String globalPluginName;
    juce::String globalPluginId;
    int globalActiveProgram = -1;
    std::vector<float> globalPluginParamValues;
    juce::MemoryBlock globalPluginState;
    juce::XmlElement* serializeBank(const Bank& b, int index) const;
    void deserializeBank(Bank& b, const juce::XmlElement& bankEl);
    void applyBankToPlugin(int bankIndex);
    void snapshotCurrentBank();       // Сохраняет изменения текущего банка///раб

    std::unique_ptr<juce::FileChooser> fileChooser;

    bool allowSave = false; // по умолчанию запрещаем запись
    juce::String lastLoadedPluginId;
    Bank bankSnapshot; // копия активного банка после загрузки/сохранения
    juce::String getCurrentPluginDisplayName() const;
    bool isApplyingState = false;
    bool isSwitchingBank = false;
    bool isLoadingFromFile = false;
    // ─────────────── Pedal slots ───────────────
    juce::GroupComponent pedalGroup;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BankEditor)
};
