#include"bank_editor.h"
#include "plugin_process_callback.h"     // ← без лишней точки!
#include "custom_audio_playhead.h"
#include "LearnController.h"
#include "cpu_load.h"
#include <memory>
#include <atomic>

namespace {
    CCMapping combineMapping(const CCMapping& global, const PresetCCMapping& preset)
    {
        CCMapping result;
        result.paramIndex = global.paramIndex;  // Глобальное назначение сохраняется
        result.name = global.name;          // Глобальное имя
        result.enabled = preset.enabled;       // Пресетное состояние
        result.ccValue = preset.ccValue;       // Пресетное значение
        result.invert = preset.invert;        // Пресетная инверсия
        return result;
    }
}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
static juce::File getBootConfigFile()
{
    auto sysDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("NEXUS_KONTROL_OS");
    sysDir.createDirectory();
    return sysDir.getChildFile("boot_config.xml");
}

static void writeBootConfig(const juce::File& targetFile)
{
    juce::XmlElement root("BootConfig");
    root.setAttribute("FilePath", targetFile.getFullPathName());
    root.setAttribute("FileName", targetFile.getFileName());
    getBootConfigFile().replaceWithText(root.toString());
}
static juce::File readBootConfig()
{
    auto bootFile = getBootConfigFile();
    if (!bootFile.existsAsFile())
        return juce::File(); // boot отсутствует → сигнал для loadSettings()

    std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(bootFile));
    if (!xml) return juce::File();

    juce::String path = xml->getStringAttribute("FilePath");
    if (path.isNotEmpty())
    {
        juce::File target(path);

        // ⚠️ защита: если путь указывает на сам boot_config.xml → игнорируем
        if (target.getFileName().equalsIgnoreCase("boot_config.xml"))
            return juce::File();

        return target;
    }

    return juce::File(); // если атрибут пустой
}
static juce::String normalizePluginId(const juce::String& rawId)
{
    juce::File f(rawId);

    if (f.getFileExtension() == ".so")
    {
        auto archDir = f.getParentDirectory();        // x86_64-linux
        auto contents = archDir.getParentDirectory();  // Contents
        auto vst3dir = contents.getParentDirectory(); // *.vst3

        if (vst3dir.hasFileExtension("vst3"))
            return vst3dir.getFullPathName();
    }

    return rawId;
}
// Кастомный LookAndFeel для всплывающего меню.ВЫБОРА БАНКОВ
class CustomPopupMenuLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Значение, которое можно задавать извне – минимальная ширина для пунктов меню.
    int minimumPopupWidth = 0;
    // Используем увеличенный шрифт для пунктов меню.
    juce::Font getPopupMenuFont() override
    {
        return juce::Font(22.0f);
    }
    // Переопределение отрисовки пункта меню с учетом минимальной ширины.
    void drawPopupMenuItem(juce::Graphics& g,
        const juce::Rectangle<int>& area,
        bool isSeparator,
        bool isActive,
        bool isHighlighted,
        bool isTicked,
        bool hasSubMenu,
        const juce::String& text,
        const juce::String& shortcutKeyText,
        const juce::Drawable* icon,
        const juce::Colour* textColour) override
    {
        juce::Rectangle<int> r(area);
        if (minimumPopupWidth > r.getWidth())
            r.setWidth(minimumPopupWidth);

        // Вызываем базовую реализацию с изменённой областью.
        LookAndFeel_V4::drawPopupMenuItem(g, r, isSeparator, isActive, isHighlighted,
            isTicked, hasSubMenu, text, shortcutKeyText, icon, textColour);
    }
};
//==============================================================================
BankEditor::BankEditor(PluginManager& pm, VSTHostComponent* host, bool loadDefaultFlag)
    : pluginManager(pm), vstHost(host), shouldLoadDefaultOnStartup(loadDefaultFlag)
{
  //  loadSettings();
    // говорим JUCE, что мы можем работать с VST и VST3
    formatManager.addDefaultFormats();
    // Row 0
    addAndMakeVisible(bankIndexLabel);
    bankIndexLabel.setJustificationType(juce::Justification::centred);
    bankIndexLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    bankIndexLabel.repaint();
    //
    addAndMakeVisible(libraryNameEditor);
    libraryNameEditor.setMultiLine(false);
    libraryNameEditor.setReturnKeyStartsNewLine(false);
    libraryNameEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0, 0, 80));
    libraryNameEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    libraryNameEditor.setOpaque(true);
    libraryNameEditor.setFont(juce::Font(40.0f));
    libraryNameEditor.setJustification(juce::Justification::centred);
    libraryNameEditor.onTextChange = [this]() {
        libraryName = libraryNameEditor.getText();
        if (onBankEditorChanged) onBankEditorChanged();
        };
    // Кнопки навигации по библиотеке
    prevButton.setLookAndFeel(&bigIcons);
    nextButton.setLookAndFeel(&bigIcons);
    prevButton.setButtonText(juce::String::fromUTF8("👈 Back"));
    nextButton.setButtonText(juce::String::fromUTF8("👉 Next"));
    addAndMakeVisible(prevButton);
    addAndMakeVisible(nextButton);
    prevButton.onClick = [this]() { navigateBank(false); };
    nextButton.onClick = [this]() { navigateBank(true); };
    // кнопка меню файлов конфига 
    // In BankEditor constructor
    configSelectButton.setButtonText(juce::String::fromUTF8("👇 Bank"));
    addAndMakeVisible(configSelectButton);
    configSelectButton.setLookAndFeel(&bigIcons);
    configSelectButton.onClick = [this]()
        {
            juce::PopupMenu menu;

            // declare bankDir properly
            juce::File bankDir;
            if (GetDriveTypeW(L"D:\\") == DRIVE_FIXED)
                bankDir = juce::File("D:\\NEXUS\\BANK");
            else
                bankDir = juce::File("C:\\NEXUS\\BANK");

            bankDir.createDirectory();

            auto files = bankDir.findChildFiles(juce::File::findFiles, false, "*.xml");

            if (files.isEmpty())
            {
                menu.addItem(1, "NO BANK", false, false);
            }
            else
            {
                int id = 1;
                for (auto& f : files)
                {
                    // show name without extension and tick the currently loaded file
                    menu.addItem(id++, f.getFileNameWithoutExtension(), true,
                        (f == currentlyLoadedBankFile));
                }
            }

            static CustomPopupMenuLookAndFeel customPopupLAF;
            customPopupLAF.minimumPopupWidth = configSelectButton.getWidth();
            menu.setLookAndFeel(&customPopupLAF);
            menu.showMenuAsync(
                juce::PopupMenu::Options()
                .withTargetComponent(&configSelectButton)
                .withPreferredPopupDirection(juce::PopupMenu::Options::PopupDirection::downwards),

                [this, files](int result)
                {
                    if (result <= 0 || result > files.size())
                        return;

                    juce::File chosen = files[result - 1];

                    // update editor with name without extension
                    libraryNameEditor.setText(chosen.getFileNameWithoutExtension(), juce::dontSendNotification);

                    // load config
                    loadSettingsFromFile(chosen);
                    currentlyLoadedBankFile = chosen;
                    writeBootConfig(chosen);
                });
        };


    // назначаем кастомный LookAndFeel кнопкам
    defaultButton.setLookAndFeel(&bigIcons);
    saveButton.setLookAndFeel(&bigIcons);
    storeButton.setLookAndFeel(&bigIcons);
    loadButton.setLookAndFeel(&bigIcons);
    cancelButton.setLookAndFeel(&bigIcons);
    selectBankButton.setLookAndFeel(&bigIcons);
    
    // Row 1: SELECT BANK / PRESET1…6 / VST
    addAndMakeVisible(selectBankButton);
    selectBankButton.setButtonText(juce::String::fromUTF8("👇 Preset"));
    selectBankButton.addListener(this);
    // PRESET1…6 
    for (int i = 0; i < numPresets; ++i)
    {
        presetButtons[i].setLookAndFeel(&bigIcons);
        addAndMakeVisible(presetButtons[i]);
        presetButtons[i].setButtonText("Scene " + juce::String(i + 1));
        presetButtons[i].addListener(this);
    }
    addAndMakeVisible(vstButton);
    vstButton.addListener(this);
    // Row 2: Bank name editor, Preset editors, Plugin label
    addAndMakeVisible(bankNameEditor);
    bankNameEditor.setMultiLine(false);
    bankNameEditor.setReturnKeyStartsNewLine(false);
    bankNameEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0, 0, 80));  // Изменяем цвет
    bankNameEditor.setOpaque(true);
    bankNameEditor.setFont(juce::Font(30.0f));        // Устанавливаем шрифт 18pt
    bankNameEditor.setJustification(juce::Justification::centred);  // Центрируем текст по горизонтали
    // Обработчик изменения текста
    bankNameEditor.onTextChange = [this]() {
        banks[activeBankIndex].bankName = bankNameEditor.getText();
        bankIndexLabel.setText(juce::String(activeBankIndex + 1), juce::dontSendNotification);
        if (onBankEditorChanged)
           onBankEditorChanged();
     };
    // Теперь для presetEditors:
    for (int i = 0; i < numPresets; ++i)
    {
        addAndMakeVisible(presetEditors[i]);
        presetEditors[i].setMultiLine(false);
        presetEditors[i].setReturnKeyStartsNewLine(false);
        presetEditors[i].setColour(juce::TextEditor::backgroundColourId, juce::Colour(0, 0, 80)); // Изменяем цвет 
        presetEditors[i].setOpaque(true);
        presetEditors[i].setFont(juce::Font(30.0f));     // Устанавливаем шрифт 16pt
        presetEditors[i].setJustification(juce::Justification::centred);  // Центрируем текст
        // Обработчик изменения текста 
        presetEditors[i].onTextChange = [this, i]()
            {
                banks[activeBankIndex].presetNames[i] = presetEditors[i].getText();
                if (i == activePreset)
                    updateSelectedPresetLabel(); // Обновляем отображаемое имя выбранного пресета
               
                    onActivePresetChanged(activePreset);
                if (onBankEditorChanged)
                    onBankEditorChanged();
                checkForChanges();
            };

    }
    // Row 2: Plugin label
    addAndMakeVisible(pluginLabel);
    pluginLabel.setJustificationType(juce::Justification::centredLeft);
    pluginLabel.setFont(juce::Font(12.0f, juce::Font::plain));// Устанавливаем шрифт 
    pluginLabel.repaint();

    // Set background colour for bankIndexLabel & pluginLabel to button colour
    auto btnBg = selectBankButton.findColour(juce::TextButton::buttonColourId);
  //  bankIndexLabel.setColour(juce::Label::backgroundColourId, btnBg);
    bankIndexLabel.setOpaque(false);
    pluginLabel.setColour(juce::Label::backgroundColourId, btnBg);
    pluginLabel.setOpaque(true);

    // Row 3: selectedPresetLabel
    addAndMakeVisible(selectedPresetLabel);
    selectedPresetLabel.setJustificationType(juce::Justification::centred);
    selectedPresetLabel.setOpaque(false);
    selectedPresetLabel.setFont(juce::Font(30.0f));// Устанавливаем шрифт

    // Row 4: SET CC buttons
    for (int i = 0; i < numCCParams; ++i)
    {
        addAndMakeVisible(setCCButtons[i]);
        setCCButtons[i].setButtonText("SET CC" + juce::String(i + 1));
        setCCButtons[i].addListener(this);
        setCCButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colour(213, 204, 175));
        setCCButtons[i].setColour(juce::TextButton::textColourOnId, juce::Colours::black);// Устанавливаем шрифт 16pt
        setCCButtons[i].setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    }

    // Row 5: CC name editors
    for (int i = 0; i < numCCParams; ++i)
    {
        addAndMakeVisible(ccNameEditors[i]);
        ccNameEditors[i].setMultiLine(false);
        ccNameEditors[i].setReturnKeyStartsNewLine(false);
        ccNameEditors[i].setFont(juce::Font(18.0f));
        ccNameEditors[i].setJustification(juce::Justification::centred);
        ccNameEditors[i].onTextChange = [this, i]()
            {
                // 1. Сохраняем новое имя в модель
                banks[activeBankIndex].globalCCMappings[i].name = ccNameEditors[i].getText();

                // 2. Уведомляем всех подписчиков (в т.ч. Rig_control)
                if (onBankEditorChanged)
                    onBankEditorChanged();

                // 3. Обновляем метку в VSTHostComponent
                if (vstHost != nullptr)
                    vstHost->updateCCLabel(i, ccNameEditors[i].getText());
            };

        ccNameEditors[i].setColour(juce::TextEditor::backgroundColourId, juce::Colour(0, 0, 80));// Изменяем цвет
        ccNameEditors[i].setOpaque(true);
    }

    // Row 6: CC toggle buttons (active = red, inactive = dark red)
    for (int i = 0; i < numCCParams; ++i)
    {
        addAndMakeVisible(ccToggleButtons[i]);
        ccToggleButtons[i].setButtonText("CC " + juce::String(i + 1));
        ccToggleButtons[i].setClickingTogglesState(true);
        ccToggleButtons[i].setToggleState(false, juce::dontSendNotification);
        ccToggleButtons[i].setColour(juce::TextButton::buttonOnColourId, juce::Colours::red); // Цвет кнопки в активном состоянии 
        ccToggleButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colour(50, 0, 0));// Цвет кнопки в неактивном состоянии 
        ccToggleButtons[i].addListener(this);
    }

    // --- Row 7: LEARN CC buttons
    for (int i = 0; i < numCCParams; ++i)
    {
        addAndMakeVisible(learnButtons[i]);
        learnButtons[i].setButtonText("LEARN " + juce::String(i + 1));
        learnButtons[i].setColour(juce::TextButton::buttonColourId,
            juce::Colour(0, 80, 0));
        learnButtons[i].setColour(juce::TextButton::textColourOffId,
            juce::Colours::white);
        learnButtons[i].addListener(this);
        learnButtons[i].setTooltip("Click and twist the plugin knob"
            + juce::String(i + 1));
    }
    
    // Row 11: Default, Load, Store, Save, Cancel
    addAndMakeVisible(defaultButton);
    defaultButton.setButtonText(juce::String::fromUTF8("🔄 Def"));
    defaultButton.addListener(this);
   
    addAndMakeVisible(storeButton);
    storeButton.setButtonText(juce::String::fromUTF8("💾 Save"));
    storeButton.addListener(this);

    addAndMakeVisible(loadButton);
    loadButton.setButtonText(juce::String::fromUTF8("📤 Load"));
    loadButton.addListener(this);

    addAndMakeVisible(saveButton);
    saveButton.setButtonText(juce::String::fromUTF8("📥 Save As"));
    saveButton.addListener(this);

    addAndMakeVisible(cancelButton);
    cancelButton.setButtonText(juce::String::fromUTF8("🔙 Back"));
    cancelButton.addListener(this);

    banks.assign(numBanks, Bank{});
    for (int i = 0; i < numBanks; ++i)
    {
        banks[i].bankName = "BANK" + juce::String(i + 1);
        for (int p = 0; p < numPresets; ++p)
            banks[i].presetNames[p] = "Scene " + juce::String(p + 1);
    }

    setActiveBankIndex(0);
    setActivePreset(0);
    // ─────────────── Pedal slots ───────────────
    // Pedal 1
    learnButtons[10].setButtonText("Learn Pedal 1");
    setCCButtons[10].setButtonText("Set Pedal 1");
    ccNameEditors[10].setText("Name Pedal 1");
    ccToggleButtons[10].setButtonText("Toggle Pedal 1");

    // Pedal 2
    learnButtons[11].setButtonText("Learn Pedal 2");
    setCCButtons[11].setButtonText("Set Pedal 2");
    ccNameEditors[11].setText("Name Pedal 2");
    ccToggleButtons[11].setButtonText("Toggle Pedal 2");

    // SW1
    learnButtons[12].setButtonText("Learn SW1");
    setCCButtons[12].setButtonText("Set SW1");
    ccNameEditors[12].setText("Name SW1");
    ccToggleButtons[12].setButtonText("Toggle SW1");

    // SW2
    learnButtons[13].setButtonText("Learn SW2");
    setCCButtons[13].setButtonText("Set SW2");
    ccNameEditors[13].setText("Name SW2");
    ccToggleButtons[13].setButtonText("Toggle SW2");

    addAndMakeVisible(pedalGroup);
    pedalGroup.setText("Pedals & Switches");
    pedalGroup.setTextLabelPosition(juce::Justification::centredTop);

    // >>> Здесь регулируешь цвет рамки и текста <<<
    pedalGroup.setColour(juce::GroupComponent::outlineColourId, juce::Colours::orange);
    pedalGroup.setColour(juce::GroupComponent::textColourId, juce::Colours::white);

     // 2. ПОДПИСКА на изменение параметров плагина  ▶▶▶  ДОБАВЬТЕ ЭТО
   
    if (vstHost != nullptr)
    {
        // 1) уже существующая подписка на параметры — оставляем без изменений
        vstHost->setParameterChangeCallback([this](int idx, float norm)
            {
                onPluginParameterChanged(idx, norm);
            });

        // 2) Preset: VSTHost → BankEditor
        vstHost->setPresetCallback([this](int idx)
            {
                if (isSettingPreset)            // guard-флаг: если мы внутри setActivePreset — выходим
                    return;

                isSettingPreset = true;         // блокируем повторные входы
                setActivePreset(idx);           // вызываем вашу «тяжёлую» логику
                isSettingPreset = false;        // снимаем блок
            });

        // 3) Убираем двустороннюю подписку — теперь UI не дергает хост напрямую
        onActivePresetChanged = nullptr;
    }

    // Learn-колл-бэк по той же схеме
    if (vstHost != nullptr)
    {
        vstHost->setLearnCallback([this](int cc, bool on)
            {
                if (isSettingLearn)             // guard-флаг для Learn
                    return;

                isSettingLearn = true;
                toggleLearnFromHost(cc, on);    // ваша внутренняя логика learn.begin()/cancel()
                isSettingLearn = false;
            });

        onLearnToggled = nullptr;          // убираем старый обратный зов
    }
    startTimerHz(2); // мигание 2 раза в секунду

}
BankEditor::~BankEditor()
{
    if (vstHost != nullptr)
        for (juce::Button* b : { &defaultButton, &storeButton, &loadButton,
                             &cancelButton, &saveButton, &vstButton,
                             &selectBankButton, &prevButton, &nextButton })
        {
            b->setLookAndFeel(nullptr);
        }

    for (int i = 0; i < numPresets; ++i)
        presetButtons[i].setLookAndFeel(nullptr);

}
//==============================================================================
void BankEditor::paint(juce::Graphics& g)
{
    g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));
}

void BankEditor::resized()
{
    auto area = getLocalBounds().reduced(8);
    int baseX = area.getX();
    int baseY = area.getY();
    int W = area.getWidth();
    int H = area.getHeight();
    int sW = W / 20;    // ширина «ячейки»
    int sH = H / 12;    // высота «строки»

    float shrinkFactor = 0.85f; // коэффициент уменьшения высоты
    int gap = 4;                // зазор между компонентами
    int rowOffset = 1;           // смещение вниз на одну строку

    // Row 0 (без смещения): config(0), load(1), prev(2), editor(3–4), next(5), store(6), save(7)
    {
        int bw = W / 8 - gap;
        int rowHeight = int(sH * shrinkFactor);
        int catalogWidth = 2 * bw + gap; // editor занимает 2 ячейки (3–4)
        int catalogX = baseX + 3 * (bw + gap);
       // int catalogY = baseY + 0 * sH;
        int catalogY = baseY + sH / 2;

        libraryNameEditor.setBounds(catalogX, catalogY, catalogWidth, rowHeight);

        configSelectButton.setBounds(baseX + 0 * (bw + gap), catalogY, bw, rowHeight);
        loadButton.setBounds(baseX + 1 * (bw + gap), catalogY, bw, rowHeight);
        prevButton.setBounds(baseX + 2 * (bw + gap), catalogY, bw, rowHeight);
        nextButton.setBounds(baseX + 5 * (bw + gap), catalogY, bw, rowHeight);
        storeButton.setBounds(baseX + 6 * (bw + gap), catalogY, bw, rowHeight);
        saveButton.setBounds(baseX + 7 * (bw + gap), catalogY, bw, rowHeight);
    }

    // Row 1 → теперь Row 2
    {
        int bw = W / 8 - gap;
        int rowHeight = int(sH * shrinkFactor);
        for (int i = 0; i < 8; ++i)
        {
            juce::Button* b = (i == 0 ? &selectBankButton
                : i < 7 ? &presetButtons[i - 1]
                : &vstButton);
            b->setBounds(baseX + i * (bw + gap), baseY + (1 + rowOffset) * sH, bw, rowHeight);
        }
    }

    // Row 2 → теперь Row 3
    {
        int fw = W / 8 - gap;
        int fy = baseY + (2 + rowOffset) * sH;
        int rowHeight = int(sH * shrinkFactor);

        bankNameEditor.setBounds(baseX + 0 * (fw + gap), fy, fw, rowHeight);
        for (int i = 0; i < numPresets; ++i)
            presetEditors[i].setBounds(baseX + (i + 1) * (fw + gap), fy, fw, rowHeight);

        pluginLabel.setFont(juce::Font(sH * 0.45f, juce::Font::plain));
        pluginLabel.setJustificationType(juce::Justification::centredLeft);
        pluginLabel.setMinimumHorizontalScale(0.7f);
        pluginLabel.setBounds(baseX + 7 * (fw + gap), fy, fw, rowHeight);
    }

    // Row 3 → теперь Row 4 (без уменьшения)
    {
        // bankIndexLabel остаётся как есть
        bankIndexLabel.setFont(juce::Font(sH * 0.5f, juce::Font::bold));
        bankIndexLabel.setJustificationType(juce::Justification::centred);
        bankIndexLabel.setBounds(baseX,
            baseY + (3 + rowOffset) * sH,
            sW,
            sH);

        // selectedPresetLabel занимает всё оставшееся пространство, кроме последнего сектора
        selectedPresetLabel.setJustificationType(juce::Justification::centred);
        selectedPresetLabel.setOpaque(false);
        selectedPresetLabel.setFont(juce::Font(30.0f));
        selectedPresetLabel.setBounds(baseX + sW,
            baseY + (3 + rowOffset) * sH,
            W - 2 * sW,
            sH);
    }


    // Row 4–7 → теперь 5–8
    for (int i = 0; i < 10; ++i)
        learnButtons[i].setBounds(baseX + i * 2 * sW + gap, baseY + (4 + rowOffset) * sH, 2 * sW - gap, int(sH * shrinkFactor));

    for (int i = 0; i < 10; ++i)
        setCCButtons[i].setBounds(baseX + i * 2 * sW + gap, baseY + (5 + rowOffset) * sH, 2 * sW - gap, int(sH * shrinkFactor));

    for (int i = 0; i < 10; ++i)
        ccNameEditors[i].setBounds(baseX + i * 2 * sW + gap, baseY + (6 + rowOffset) * sH, 2 * sW - gap, int(sH * shrinkFactor));

    for (int i = 0; i < 10; ++i)
        ccToggleButtons[i].setBounds(baseX + i * 2 * sW + gap, baseY + (7 + rowOffset) * sH, 2 * sW - gap, int(sH * shrinkFactor));

    // Pedal block → теперь строки 9–10
    int startCol = 6;
    int offsetX = baseX + startCol * sW;
    int blockWidth = 8 * sW;
    int blockHeight = 2 * sH;
    int pedalYOffset = sH / 2;
    int pedalExtraWidth = sW / 4;
    int pedalExtraHeight = sH / 3;
    int y9 = baseY + (8 + rowOffset) * sH + pedalYOffset;
    int y10 = baseY + (9 + rowOffset) * sH + pedalYOffset;

    pedalGroup.setBounds(offsetX - pedalExtraWidth,
        y9 - pedalExtraHeight,
        blockWidth + 2 * pedalExtraWidth,
        blockHeight + 2 * pedalExtraHeight);

    learnButtons[10].setBounds(offsetX + 0 * sW, y9, sW, sH);
    setCCButtons[10].setBounds(offsetX + 1 * sW, y9, sW, sH);
    learnButtons[12].setBounds(offsetX + 2 * sW, y9, sW, sH);
    setCCButtons[12].setBounds(offsetX + 3 * sW, y9, sW, sH);
    learnButtons[11].setBounds(offsetX + 4 * sW, y9, sW, sH);
    setCCButtons[11].setBounds(offsetX + 5 * sW, y9, sW, sH);
    learnButtons[13].setBounds(offsetX + 6 * sW, y9, sW, sH);
    setCCButtons[13].setBounds(offsetX + 7 * sW, y9, sW, sH);

    ccNameEditors[10].setBounds(offsetX + 0 * sW, y10, 2 * sW, sH);
    ccNameEditors[12].setBounds(offsetX + 2 * sW, y10, 2 * sW, sH);
    ccNameEditors[11].setBounds(offsetX + 4 * sW, y10, 2 * sW, sH);
    ccNameEditors[13].setBounds(offsetX + 6 * sW, y10, 2 * sW, sH);

    // Row 11 (без изменений)
    defaultButton.setBounds(baseX + 0 * sW, baseY + 11 * sH, 2 * sW, sH);
    cancelButton.setBounds(baseX + 18 * sW, baseY + 11 * sH, 2 * sW, sH);
}

//==============================================================================
void BankEditor::updatePresetButtons()
{
    for (int i = 0; i < numPresets; ++i)
    {
        if (i == activePreset)
        {
            // Для выбранного пресета: синий фон, белый текст
            presetButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colours::blue);
            presetButtons[i].setColour(juce::TextButton::textColourOnId, juce::Colours::white);
            presetButtons[i].setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        }
        else
        {
            // Для остальных: серый фон, чёрный текст
            presetButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colours::grey);
            presetButtons[i].setColour(juce::TextButton::textColourOnId, juce::Colours::black);
            presetButtons[i].setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        }
    }
}
void BankEditor::buttonClicked(juce::Button* b)
{
    // SELECT BANK
    if (b == &selectBankButton) { showBankSelectionMenu();return; }
    // VST
    if (b == &vstButton) { showVSTDialog();return; }

    // PRESET 1–6
    for (int i = 0; i < numPresets; ++i)
        if (b == &presetButtons[i])
        {
            isSettingPreset = true;
            setActivePreset(i);           // обновляем UI + saveSettings
            if (!isSettingPreset && vstHost)
                vstHost->setExternalPresetIndex(i);
            isSettingPreset = false;

            // здесь отсыпаем «событие» наружу
            if (onActivePresetChanged)
                onActivePresetChanged(i);

            return;
        }
    // SET CC 1…10
    for (int i = 0; i < numCCParams; ++i)
        if (b == &setCCButtons[i]) { editCCParameter(i);       return; }

    // TOGGLE CC 1…10
    for (int i = 0; i < numCCParams; ++i)
    {
        if (b == &ccToggleButtons[i])
        {
            bool state = ccToggleButtons[i].getToggleState();

            // 1) обновляем модель (используем индивидуальные настройки для активного пресета)
            banks[activeBankIndex].presetCCMappings[activePreset][i].enabled = state;

            // Если матрица ccPresetStates по-прежнему нужна для синхронизации UI:
            banks[activeBankIndex].ccPresetStates[activePreset][i] = state;

            // 2) гоняем CC → MIDI + плагин
            updateCCParameter(i, state);
             
            return;
        }
    }
    // LEARN 1–10 через хост
    for (int i = 0; i < numCCParams; ++i)
        if (b == &learnButtons[i])
        {
            if (isSettingLearn) return;

            isSettingLearn = true;
            bool newState = !(learn.isActive() && learn.slot() == i);
            vstHost->setExternalLearnState(i, newState);
            isSettingLearn = false;
            return;
        }
    // --- Обработчики нижних кнопок ---
    if (b == &defaultButton)
    {
        juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::WarningIcon, juce::String::fromUTF8("⚠️ Reset to Default"),
            "This will reset the bank to default settings. All current changes will be lost. Continue?",
            juce::String::fromUTF8("✔️ Yes"),
            juce::String::fromUTF8("❌ No"),
            nullptr,
            juce::ModalCallbackFunction::create([this](int result)
                {
                    if (result == 1) // Yes
                        resetAllDefaults();
                })
        );
    }
    else if (b == &loadButton)
    {
        juce::File bankDir = (GetDriveTypeW(L"D:\\") == DRIVE_FIXED)
            ? juce::File("D:\\NEXUS\\BANK")
            : juce::File("C:\\NEXUS\\BANK");
        bankDir.createDirectory();

        auto* fm = new FileManager(bankDir, FileManager::Mode::Load);
        fm->setMinimalUI(false);
        fm->setShowRunButton(false);
        fm->setWildcardFilter("*.xml");

        // Контекст Bank: Home должен вести в NEXUS/BANK
        fm->setHomeSubfolder("BANK");
        // Блокируем уход выше разрешённого физического корня
        fm->setRootLocked(true);

        fm->setConfirmCallback([this](const juce::File& file)
            {
                if (!file.existsAsFile())
                    return;

                // 🔹 Загружаем напрямую
                loadSettingsFromFile(file);

                // 🔹 Фиксируем рабочий файл
                currentlyLoadedBankFile = file;

                // 🔹 Обновляем boot_config.xml
                writeBootConfig(file);

                bankSnapshot = banks[activeBankIndex];
                updateUI();

                DBG("[Load] loaded bank file: " << file.getFullPathName());
            });

        juce::DialogWindow::LaunchOptions opts;
        opts.content.setOwned(fm);
        opts.dialogTitle = "Load Bank File";
        opts.dialogBackgroundColour = juce::Colours::darkgrey;
        opts.escapeKeyTriggersCloseButton = true;
        opts.useNativeTitleBar = false;
        opts.resizable = true;

        if (auto* dialog = opts.launchAsync())
        {
            fm->setDialogWindow(dialog);
            auto screenBounds = juce::Desktop::getInstance().getDisplays().getMainDisplay().userArea;
            int w = 800;
            int h = 400; // высота панели сверху
            int x = screenBounds.getCentreX() - w / 2;
            int y = screenBounds.getY() + 150; // верх экрана
            dialog->setBounds(x, y, w, h);
        }
    }
    else if (b == &storeButton)
    {
        // имя файла
        juce::String newName = libraryNameEditor.getText().trim();
        if (newName.isEmpty()) newName = "Default";

        // директория BANK
        juce::File bankDir = (GetDriveTypeW(L"D:\\") == DRIVE_FIXED)
            ? juce::File("D:\\NEXUS\\BANK")
            : juce::File("C:\\NEXUS\\BANK");
        bankDir.createDirectory();

        // целевой файл и флаг "новый файл"
        juce::File targetFile = bankDir.getChildFile(newName + ".xml");
        const bool isNewFile = !targetFile.existsAsFile();

        auto doStore = [this, targetFile]()
            {
                snapshotCurrentBank();
                bankSnapshot = banks[activeBankIndex];
                if (onBankChanged) onBankChanged();
                if (onBankEditorChanged) onBankEditorChanged();

                storeToBank();

                if (onActivePresetChanged)
                    onActivePresetChanged(activePreset);

                saveSettingsToFile(targetFile);
                currentlyLoadedBankFile = targetFile;
                loadedFileName = targetFile.getFileNameWithoutExtension();
                writeBootConfig(targetFile);
            };

        // твоя логика modified — без изменений
        bool modified = false;
        if (bankNameEditor.getText() != bankSnapshot.bankName)
            modified = true;
        for (int i = 0; i < numPresets && !modified; ++i)
            if (presetEditors[i].getText() != bankSnapshot.presetNames[i])
                modified = true;
        for (int i = 0; i < numCCParams && !modified; ++i)
            if (ccNameEditors[i].getText() != bankSnapshot.globalCCMappings[i].name)
                modified = true;
        if (!modified && libraryNameEditor.getText() != loadedFileName)
            modified = true;
        if (!modified && (banks[activeBankIndex] != bankSnapshot))
            modified = true;

        // правило: новый файл → без диалога; иначе — как у тебя (только при modified)
        if (isNewFile)
        {
            doStore();
        }
        else if (modified)
        {
            juce::AlertWindow::showOkCancelBox(
                juce::AlertWindow::WarningIcon, juce::String::fromUTF8("⚠️ Save Bank"),
                "Do you want to overwrite this bank? All current changes will be saved.",
                juce::String::fromUTF8("✔️ Yes"),
                juce::String::fromUTF8("❌ No"),
                nullptr,
                juce::ModalCallbackFunction::create([doStore](int result)
                    {
                        if (result == 1) // Yes
                            doStore();
                    })
            );
        }
        else
        {
            doStore();
        }
    }

    else if (b == &saveButton)
    {
        juce::File saveDir;

        // Определяем папку для сохранения
        if (GetDriveTypeW(L"D:\\") == DRIVE_FIXED)
            saveDir = juce::File("D:\\NEXUS\\BANK");
        else
            saveDir = juce::File("C:\\NEXUS\\BANK");

        saveDir.createDirectory();

        auto* fm = new FileManager(saveDir, FileManager::Mode::Save);
        fm->setMinimalUI(false);
        fm->setShowRunButton(false);
        fm->setWildcardFilter("*.xml");
        fm->setHomeSubfolder("BANK");
        fm->setRootLocked(true);

        fm->setConfirmCallback([this, saveDir](const juce::File& file)
            {
                // 🔹 теперь сохраняем только если пользователь ввёл имя
                if (file.getFileName().isEmpty())
                    return;

                juce::File targetFile = saveDir.getChildFile(file.getFileName());

                snapshotCurrentBank();
                allowSave = true;
                storeToBank();
                allowSave = false;

                saveSettingsToFile(targetFile);
                bankSnapshot = banks[activeBankIndex];
            });

        juce::DialogWindow::LaunchOptions opts;
        opts.content.setOwned(fm);
        opts.dialogTitle = "Save Bank File";
        opts.dialogBackgroundColour = juce::Colours::darkgrey;
        opts.escapeKeyTriggersCloseButton = true;
        opts.useNativeTitleBar = false;
        opts.resizable = false;

        if (auto* dialog = opts.launchAsync())
        {
            fm->setDialogWindow(dialog);
            auto screenBounds = juce::Desktop::getInstance().getDisplays().getMainDisplay().userArea;
            int w = 800;
            int h = 400;
            int x = screenBounds.getCentreX() - w / 2;
            int y = screenBounds.getY() + 150;
            dialog->setBounds(x, y, w, h);
        }
        }
    else if (b == &cancelButton) {
        auto restore = [this]() {
            DBG("[Cancel] Reloading bank file: " << currentlyLoadedBankFile.getFullPathName());
            loadSettingsFromFile(currentlyLoadedBankFile);
            updateUI();
            };

        juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::WarningIcon,
            juce::String::fromUTF8("⚠️ Cancel Changes"),
            "All unsaved changes will be lost. Do you want to continue?",
            juce::String::fromUTF8("✔️ Yes"),
            juce::String::fromUTF8("❌ No"),
            nullptr,
            juce::ModalCallbackFunction::create([restore](int result) {
                if (result == 1) // Yes
                    restore();
                })
        );
}

}

//==============================================================================
void BankEditor::showBankSelectionMenu()
{
    juce::PopupMenu menu;
    // Заполняем меню пунктами: ID = i+1, текст = имя банка.
    for (int i = 0; i < numBanks; ++i)
        menu.addItem(i + 1, banks[i].bankName, true, (i == activeBankIndex));
    // Используем статический экземпляр нашего кастомного LookAndFeel.
    static CustomPopupMenuLookAndFeel customPopupLAF;
    customPopupLAF.minimumPopupWidth = selectBankButton.getWidth();
    // Назначаем кастомное оформление для меню.
    menu.setLookAndFeel(&customPopupLAF);
    // Отображаем меню под кнопкой.
    menu.showMenuAsync(juce::PopupMenu::Options()
        .withTargetComponent(&selectBankButton)
        .withPreferredPopupDirection(juce::PopupMenu::Options::PopupDirection::downwards),
        [this](int result)
        {
            if (result > 0)
                setActiveBankIndex(result - 1);
        });
}
void BankEditor::propagateInvert(int slot, bool newInvert)
{
    auto& bank = banks[activeBankIndex];
    bank.globalCCMappings[slot].invert = newInvert;   // чтобы редакторы имени видели
    for (int p = 0; p < numPresets; ++p)
        bank.presetCCMappings[p][slot].invert = newInvert;
}
void BankEditor::editCCParameter(int ccIndex)
{
    const CCMapping initialMap = combineMapping(
        banks[activeBankIndex].globalCCMappings[ccIndex],
        banks[activeBankIndex].presetCCMappings[activePreset][ccIndex]);

    juce::String slotName = "Set CC " + juce::String(ccIndex + 1);

    new SetCCDialog(vstHost, initialMap, slotName,
        [this, ccIndex](CCMapping newMap, bool ok)
        {
            if (!ok) return;

            // --- 1. Глобальный слой ------------------------------------------
            auto& global = banks[activeBankIndex].globalCCMappings[ccIndex];
            global.paramIndex = newMap.paramIndex;

            if (newMap.paramIndex >= 0 && vstHost != nullptr) {
                if (auto* inst = vstHost->getPluginInstance())
                    global.name = safeGetParamName(inst, newMap.paramIndex, 64);
            }
            else {
                global.name = "<none>";
            }

            // --- 2. Пресетный слой -------------------------------------------
            auto& preset = banks[activeBankIndex].presetCCMappings[activePreset][ccIndex];
            preset.ccValue = newMap.ccValue;
            propagateInvert(ccIndex, newMap.invert);
            preset.enabled = newMap.enabled;

            // --- 3. GUI ------------------------------------------------------
            ccNameEditors[ccIndex].setText(global.name, juce::dontSendNotification);
            ccToggleButtons[ccIndex].setToggleState(preset.enabled, juce::dontSendNotification);

            updateCCParameter(ccIndex, preset.enabled);

            // ✅ чтобы кнопка Store реагировала на изменения
            checkForChanges();
        });
}


//==============================================================================
void BankEditor::setActiveBankIndex(int newIdx)
{
    if (newIdx == activeBankIndex) return;

    snapshotCurrentBank(); // сохраняем старый банк

    activeBankIndex = juce::jlimit(0, numBanks - 1, newIdx);

    isSwitchingBank = true; // блокируем проверку dirty

    // Обычное переключение — асинхронно
    applyBankToPlugin(activeBankIndex, false);
    updateUI();
    setActivePreset(0);
    // Уведомляем подписчиков (Rig_control) о смене банка
    if (onBankChanged)
        onBankChanged();

    // Через 200 мс фиксируем эталон и снимаем блокировку
    juce::Timer::callAfterDelay(200, [this] {
        bankSnapshot = banks[activeBankIndex];
        isSwitchingBank = false;
        });
}
void BankEditor::setActiveBank(int newBank)
{
    activeBankIndex = juce::jlimit(0, numBanks - 1, newBank);
    updateUI();
}
void BankEditor::setActivePreset(int newPreset)
{
    if (banks.empty() || activeBankIndex < 0 || activeBankIndex >= (int)banks.size())
        return;
    if (newPreset < 0 || newPreset >= numPresets)
        return;

    activePreset = newPreset;
    updateSelectedPresetLabel();
    updatePresetButtons();

    auto& bank = banks[activeBankIndex];
    for (int i = 0; i < numCCParams; ++i)
    {
        bool state = bank.presetCCMappings[activePreset][i].enabled;
        ccToggleButtons[i].setToggleState(state, juce::dontSendNotification);
        updateCCParameter(i, state);
    }

    sendChange();

    if (onActivePresetChanged)
        onActivePresetChanged(activePreset);
}


void BankEditor::resetCCSlotState(int slot)
{
    auto& bank = banks[activeBankIndex];
    bank.globalCCMappings[slot].paramIndex = -1;
    bank.globalCCMappings[slot].name = "<none>";
    bank.globalCCMappings[slot].invert = false;
    for (int p = 0; p < numPresets; ++p)
    {
        auto& m = bank.presetCCMappings[p][slot];
        m.enabled = false;
        m.ccValue = 64;
        m.invert = false;
        bank.ccPresetStates[p][slot] = false;
    }
    ccNameEditors[slot].setText("<none>", juce::dontSendNotification);
    ccToggleButtons[slot].setToggleState(false, juce::dontSendNotification);
}
void BankEditor::clearCCMappingsForActiveBank()
{
    for (int s = 0; s < numCCParams; ++s)
        resetCCSlotState(s);
}
void BankEditor::updateUI()
{
    if (banks.empty() || activeBankIndex < 0 || activeBankIndex >= (int)banks.size())
        return;

    auto& bank = banks[activeBankIndex];

    libraryNameEditor.setText(currentlyLoadedBankFile.getFileNameWithoutExtension(),
        juce::dontSendNotification);

    bankIndexLabel.setText(juce::String(activeBankIndex + 1), juce::dontSendNotification);
    bankNameEditor.setText(bank.bankName, juce::dontSendNotification);

    if (onBankChanged)
        onBankChanged();

    // просто используем numPresets вместо .size()
    for (int i = 0; i < numPresets; ++i)
        presetEditors[i].setText(bank.presetNames[i], juce::dontSendNotification);

    updateVSTButtonLabel();

    pluginLabel.setText(bank.pluginName.isNotEmpty() ? bank.pluginName : "<no plugin>",
        juce::dontSendNotification);

    updateSelectedPresetLabel();

    for (int i = 0; i < numCCParams; ++i)
    {
        ccNameEditors[i].setText(bank.globalCCMappings[i].name, juce::sendNotification);

        ccToggleButtons[i].setToggleState(
            bank.presetCCMappings[activePreset][i].enabled,
            juce::dontSendNotification
        );
    }

}



void BankEditor::updateSelectedPresetLabel()
{
    if (banks.empty() || activeBankIndex < 0 || activeBankIndex >= banks.size())
        return;

    auto& bank = banks[activeBankIndex];
    if (activePreset < 0 || activePreset >= numPresets)
        return;

    selectedPresetLabel.setText(
        bank.presetNames[activePreset],
        juce::dontSendNotification);
}

//==============================================================================
void BankEditor::timerCallback()
{
    if (isSwitchingBank || isLoadingFromFile)
        return;

    checkForChanges(); // внутри сравнение UI ↔ snapshot и окраска Store
}
//==============================================================================
void BankEditor::loadSettings()
{
    // читаем путь из boot_config.xml
    juce::File targetFile = readBootConfig();

    // 🔹 если включён флаг или файл не существует → грузим дефолт
    if (shouldLoadDefaultOnStartup || !targetFile.existsAsFile())
    {
        juce::File bankDir;
        if (GetDriveTypeW(L"D:\\") == DRIVE_FIXED)
            bankDir = juce::File("D:\\NEXUS\\BANK");
        else
            bankDir = juce::File("C:\\NEXUS\\BANK");
        bankDir.createDirectory();

        juce::File defFile = bankDir.getChildFile("Default.xml");

        resetAllDefaults();
        saveSettingsToFile(defFile);
        writeBootConfig(defFile);

        targetFile = defFile;
    }

    // загружаем указанный файл
    loadSettingsFromFile(targetFile);
    currentlyLoadedBankFile = targetFile;
}

void BankEditor::saveSettings()
{
    if (!currentlyLoadedBankFile.existsAsFile())
    {
        // fallback: создаём дефолтный файл в BANK
        juce::File bankDir;
        if (GetDriveTypeW(L"D:\\") == DRIVE_FIXED)
            bankDir = juce::File("D:\\NEXUS\\BANK");
        else
            bankDir = juce::File("C:\\NEXUS\\BANK");
        bankDir.createDirectory();

        currentlyLoadedBankFile = bankDir.getChildFile("Default.xml");
    }

    // сохраняем текущее состояние в выбранный файл
    saveSettingsToFile(currentlyLoadedBankFile);

    // обновляем boot_config.xml, чтобы он указывал на этот файл
    writeBootConfig(currentlyLoadedBankFile);

    DBG("[Save] saved bank file: " << currentlyLoadedBankFile.getFullPathName());
}

void BankEditor::loadSettingsFromFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return;

    loadedFileName = file.getFileNameWithoutExtension();

    struct LoadingGuard {
        bool& flag;
        explicit LoadingGuard(bool& f) : flag(f) { flag = true; }
        ~LoadingGuard() { flag = false; }
    } guard(isLoadingFromFile);

    std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(file));
    if (!xml)
        return;

    // --- Чтение глобальных и банковских данных ---
    activeBankIndex = xml->getIntAttribute("activeBankIndex", 0);
    activePreset = xml->getIntAttribute("activePreset", 0);

    globalPluginName = xml->getStringAttribute("pluginName");
    globalPluginId = normalizePluginId(xml->getStringAttribute("pluginId"));
    globalActiveProgram = xml->getIntAttribute("activeProgram", -1);

    // --- Если в конфиге нет плагина, выгружаем старый ---
    if (vstHost != nullptr && vstHost->getPluginInstance() != nullptr)
    {
        auto* inst = vstHost->getPluginInstance();
        auto desc = inst->getPluginDescription();

        // Текущее имя и идентификатор из PluginDescription
        const auto currentName = desc.name.toLowerCase().trim();

        // В JUCE это строка: приводим к каноническому виду
        const auto currentId = normalizePluginId(desc.fileOrIdentifier);

        // Имя и id из конфига (у тебя путь в pluginId)
        const auto newName = globalPluginName.toLowerCase().trim();
        const auto newId = normalizePluginId(globalPluginId);

        if (newName.isEmpty() || newId.isEmpty())
        {
            DBG("🐶 Bulldog: Config has no plugin → unloading");
            vstHost->unloadPlugin();
        }
        else if (currentId == newId || currentName == newName)
        {
            DBG("🐶 Bulldog: Same plugin already loaded → apply saved state only");
            applyBankToPlugin(activeBankIndex, true);
        }
        else
        {
            DBG("🐶 Bulldog: Different plugin → unloading and loading new one");
            vstHost->unloadPlugin();
            vstHost->loadPlugin(juce::File(globalPluginId)); // грузим по пути из конфига
        }
    }

    globalPluginParamValues.clear();
    if (auto* paramsEl = xml->getChildByName("PluginParams"))
        forEachXmlChildElementWithTagName(*paramsEl, pe, "Param")
        globalPluginParamValues.push_back((float)pe->getDoubleAttribute("value", 0.0));

    globalPluginState.reset();
    if (auto* stateEl = xml->getChildByName("PluginState"))
        globalPluginState.fromBase64Encoding(stateEl->getAllSubText().trim());

    banks.assign(numBanks, Bank{});
    forEachXmlChildElementWithTagName(*xml, bankEl, "Bank")
    {
        int idx = bankEl->getIntAttribute("index", -1);
        if (idx < 0 || idx >= numBanks) continue;

        auto& b = banks[idx];
        b.bankName = bankEl->getStringAttribute("bankName");
        b.pluginName = bankEl->getStringAttribute("pluginName");
        b.pluginId = normalizePluginId(bankEl->getStringAttribute("pluginId"));
        b.activeProgram = bankEl->getIntAttribute("activeProgram", -1);

        b.pluginParamValues.clear();
        if (auto* paramsEl = bankEl->getChildByName("PluginParams"))
            forEachXmlChildElementWithTagName(*paramsEl, pe, "Param")
            b.pluginParamValues.push_back((float)pe->getDoubleAttribute("value", 0.0));

        b.pluginState.reset();
        if (auto* stateEl = bankEl->getChildByName("PluginState"))
            b.pluginState.fromBase64Encoding(stateEl->getAllSubText().trim());

        b.paramDiffs.clear();
        if (auto* diffsEl = bankEl->getChildByName("ParamDiffs"))
            forEachXmlChildElementWithTagName(*diffsEl, de, "Diff")
        {
            int pIdx = de->getIntAttribute("index", -1);
            if (pIdx >= 0)
                b.paramDiffs[pIdx] = (float)de->getDoubleAttribute("value", 0.0);
        }

        if (auto* presetsEl = bankEl->getChildByName("PresetNames"))
            forEachXmlChildElementWithTagName(*presetsEl, pe, "Preset")
        {
            int pIdx = pe->getIntAttribute("index", -1);
            if (pIdx >= 0 && pIdx < numPresets)
                b.presetNames[pIdx] = pe->getStringAttribute("name");
        }

        if (auto* ccStatesEl = bankEl->getChildByName("CCPresetStates"))
            forEachXmlChildElementWithTagName(*ccStatesEl, presetEl, "Preset")
        {
            int pIdx = presetEl->getIntAttribute("index", -1);
            if (pIdx >= 0 && pIdx < numPresets)
            {
                forEachXmlChildElementWithTagName(*presetEl, ccEl, "CC")
                {
                    int cc = ccEl->getIntAttribute("number", -1);
                    if (cc >= 0 && cc < numCCParams)
                    {
                        auto& presetMap = b.presetCCMappings[pIdx][cc];
                        presetMap.enabled = ccEl->getBoolAttribute("enabled", false);
                        presetMap.ccValue = (uint8_t)ccEl->getIntAttribute("ccValue", 64);
                        presetMap.invert = ccEl->getBoolAttribute("invert", false);

                        auto& globalMap = b.globalCCMappings[cc];
                        globalMap.paramIndex = ccEl->getIntAttribute("paramIndex", -1);
                        globalMap.name = ccEl->getStringAttribute("paramName");
                    }
                }
            }
        }
    }

    // фиксируем путь рабочего файла
    currentlyLoadedBankFile = file;

    // обновляем boot_config.xml только если это не сам boot_config.xml
    if (!file.getFileName().equalsIgnoreCase("boot_config.xml"))
        writeBootConfig(file);

    // метка: имя плагина если доступно, иначе имя файла
    if (auto* inst = vstHost ? vstHost->getPluginInstance() : nullptr)
        pluginLabel.setText(inst->getName(), juce::dontSendNotification);
    else if (banks[activeBankIndex].pluginName.isNotEmpty())
        pluginLabel.setText(banks[activeBankIndex].pluginName, juce::dontSendNotification);
    else
        pluginLabel.setText(file.getFileNameWithoutExtension(), juce::dontSendNotification);

    juce::Timer::callAfterDelay(200, [this] {
        applyBankToPlugin(activeBankIndex, true);
        bankSnapshot = banks[activeBankIndex];
        isLoadingFromFile = false;
        updateUI();
        });
    // 🔹 Обновляем UI кнопок пресетов
    if (onActivePresetChanged)
        onActivePresetChanged(activePreset);
    // 🔹 Уведомляем Rig_control о смене библиотеки
    if (onLibraryFileChanged)
        onLibraryFileChanged(file);
}

void BankEditor::saveSettingsToFile(const juce::File& file)
{
    DBG("Save: activeBankIndex = " << activeBankIndex);

    juce::XmlElement root("BanksConfig");
    root.setAttribute("version", 1);
    root.setAttribute("activeBankIndex", activeBankIndex);
    root.setAttribute("activePreset", activePreset);

    // --- Глобальные данные о плагине ---
    root.setAttribute("pluginName", globalPluginName);
    root.setAttribute("pluginId", normalizePluginId(globalPluginId));
    root.setAttribute("activeProgram", globalActiveProgram);

    // --- Полный state плагина (Base64) ---
    if (globalPluginState.getSize() > 0)
    {
        auto stateEl = std::make_unique<juce::XmlElement>("PluginState");
        stateEl->addTextElement(globalPluginState.toBase64Encoding());
        root.addChildElement(stateEl.release());
    }

    // --- Данные банков ---
    for (int i = 0; i < static_cast<int>(banks.size()); ++i)
    {
        const auto& b = banks[i];

        auto bankEl = std::make_unique<juce::XmlElement>("Bank");
        bankEl->setAttribute("index", i);
        bankEl->setAttribute("bankName", b.bankName);
        bankEl->setAttribute("activeProgram", b.activeProgram);

        // 🔹 сохраняем реальные pluginName и pluginId из банка
        bankEl->setAttribute("pluginName", b.pluginName);
        bankEl->setAttribute("pluginId", normalizePluginId(b.pluginId));

        // Параметры плагина
        {
            auto paramsEl = std::make_unique<juce::XmlElement>("PluginParams");
            for (float v : b.pluginParamValues)
            {
                auto pe = std::make_unique<juce::XmlElement>("Param");
                pe->setAttribute("value", v);
                paramsEl->addChildElement(pe.release());
            }
            bankEl->addChildElement(paramsEl.release());
        }

        // Полный state плагина для банка
        if (b.pluginState.getSize() > 0)
        {
            auto stateEl = std::make_unique<juce::XmlElement>("PluginState");
            stateEl->addTextElement(b.pluginState.toBase64Encoding());
            bankEl->addChildElement(stateEl.release());
        }

        // Отличающиеся параметры
        if (!b.paramDiffs.empty())
        {
            auto diffsEl = std::make_unique<juce::XmlElement>("ParamDiffs");
            for (auto& [paramIndex, value] : b.paramDiffs)
            {
                auto diffEl = std::make_unique<juce::XmlElement>("Diff");
                diffEl->setAttribute("index", paramIndex);
                diffEl->setAttribute("value", value);
                diffsEl->addChildElement(diffEl.release());
            }
            bankEl->addChildElement(diffsEl.release());
        }

        // Preset names
        {
            auto presetsEl = std::make_unique<juce::XmlElement>("PresetNames");
            for (int p = 0; p < numPresets; ++p)
            {
                auto pe = std::make_unique<juce::XmlElement>("Preset");
                pe->setAttribute("index", p);
                pe->setAttribute("name", b.presetNames[p]);
                presetsEl->addChildElement(pe.release());
            }
            bankEl->addChildElement(presetsEl.release());
        }

        // CC состояния и назначения
        {
            auto ccStatesEl = std::make_unique<juce::XmlElement>("CCPresetStates");
            for (int p = 0; p < numPresets; ++p)
            {
                auto presetEl = std::make_unique<juce::XmlElement>("Preset");
                presetEl->setAttribute("index", p);

                for (int cc = 0; cc < numCCParams; ++cc)
                {
                    const auto& presetMap = b.presetCCMappings[p][cc];
                    const auto& globalMap = b.globalCCMappings[cc];

                    auto ccEl = std::make_unique<juce::XmlElement>("CC");
                    ccEl->setAttribute("number", cc);
                    ccEl->setAttribute("ccValue", (int)presetMap.ccValue);
                    ccEl->setAttribute("invert", presetMap.invert);
                    ccEl->setAttribute("enabled", presetMap.enabled);
                    ccEl->setAttribute("paramIndex", globalMap.paramIndex);
                    ccEl->setAttribute("paramName", globalMap.name);

                    presetEl->addChildElement(ccEl.release());
                }

                ccStatesEl->addChildElement(presetEl.release());
            }
            bankEl->addChildElement(ccStatesEl.release());
        }

        root.addChildElement(bankEl.release());
    }

    // --- Снимок текущего банка ---
    bankSnapshot = banks[activeBankIndex];

    // 🔹 Перезаписываем переданный файл
    file.replaceWithText(root.toString());

    // 🔹 Обновляем ссылку на рабочий файл
    currentlyLoadedBankFile = file;

   

    DBG("[SaveSettings] saved file: " << file.getFullPathName());
}
void BankEditor::resetAllDefaults()
{
    DBG("[Default] Resetting to clean factory state");

    // 1) Определяем папку BANK
    juce::File bankDir = (GetDriveTypeW(L"D:\\") == DRIVE_FIXED)
        ? juce::File("D:\\NEXUS\\BANK")
        : juce::File("C:\\NEXUS\\BANK");
    bankDir.createDirectory();

    // 2) Пересоздаём чистый Default.xml
    juce::File defFile = bankDir.getChildFile("Default.xml");
    juce::XmlElement root("Banks");
    root.setAttribute("Name", "Default Bank");
    defFile.replaceWithText(root.toString());

    // 3) Обновляем boot_config.xml
    writeBootConfig(defFile);

    // 🔹 4) Выгружаем плагин, если был
    if (vstHost != nullptr)
        vstHost->unloadPlugin();

    // 5) Загружаем дефолт через стандартный механизм
    loadSettingsFromFile(defFile);
    currentlyLoadedBankFile = defFile;

    // 6) Обновляем UI
    libraryNameEditor.setText("Default", juce::dontSendNotification);

    // 7) Фиксируем snapshot и кнопки
    bankSnapshot = banks[activeBankIndex];
    checkForChanges();

    DBG("[Default] Clean Default.xml recreated, plugin unloaded, and loaded");
}


void BankEditor::storeToBank()
{
    if (activeBankIndex < 0 || activeBankIndex >= (int)banks.size())
        return;

    auto& b = banks[activeBankIndex];

    // Имена из UI
    b.bankName = bankNameEditor.getText();
    for (int i = 0; i < numPresets; ++i)
        b.presetNames[i] = presetEditors[i].getText();

    // 🔹 сохраняем пользовательские имена CC
    for (int i = 0; i < numCCParams; ++i)
        b.globalCCMappings[i].name = ccNameEditors[i].getText();

    // 🔹 глобальное имя библиотеки
    libraryName = libraryNameEditor.getText();

    if (vstHost != nullptr)
    {
        if (auto* inst = vstHost->getPluginInstance())
        {
            juce::PluginDescription desc;
            inst->fillInPluginDescription(desc);

            b.pluginName = desc.name;
            globalPluginName = desc.name;

            if (desc.fileOrIdentifier.isNotEmpty())
            {
                b.pluginId = desc.fileOrIdentifier;
                globalPluginId = desc.fileOrIdentifier;
            }
            else
            {
                DBG("StoreToBank: fileOrIdentifier пустой, оставляем старый pluginId = " << b.pluginId);
                globalPluginId = b.pluginId;
            }

            b.activeProgram = inst->getCurrentProgram();
            globalActiveProgram = b.activeProgram;

            b.pluginState.reset();
            inst->getStateInformation(b.pluginState);

            const auto& params = inst->getParameters();
            const int N = (int)params.size();
            if ((int)b.pluginParamValues.size() != N)
                b.pluginParamValues.assign(N, 0.0f);

            static constexpr float eps = 1.0e-4f;
            std::unordered_map<int, float> newDiffs;

            for (int i = 0; i < N; ++i)
            {
                const float v = params[i]->getValue();
                const float oldBaseline = b.pluginParamValues[i];

                if (std::abs(v - oldBaseline) > eps)
                    newDiffs[i] = v;

                b.pluginParamValues[i] = v;
            }
            b.paramDiffs = std::move(newDiffs);
        }
    }
}
 
void BankEditor::cancelChanges()
{
    loadSettings();
}
//------------------------------------------------------------------------------
// Заглушки для кнопок Default/Load/Store/Save/Cancel
//------------------------------------------------------------------------------
void BankEditor::loadFromDisk()
{
    // просто переложим на ваш метод loadSettings()
    loadSettings();
    updateUI();

}
void BankEditor::saveToDisk()
{
    // аналогично — на saveSettings()
    saveSettings();
  
}
void BankEditor::showVSTDialog()
{
    juce::PopupMenu::dismissAllActiveMenus();
    if (vstHost == nullptr)
        return;

    // A: plugin not loaded -> build menu
    if (vstHost->getPluginInstance() == nullptr)
    {
        auto entries = vstHost->getPluginManager().getPluginsSnapshot();

        juce::PopupMenu menu;
        std::vector<int> indexMap;
        int itemId = 1;

        for (int i = 0; i < (int)entries.size(); ++i)
        {
            if (!entries[i].enabled)
                continue;

            menu.addItem(itemId, entries[i].desc.name);
            indexMap.push_back(i);
            ++itemId;
        }

        if (indexMap.empty())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "No Plugins Enabled",
                "Please enable the plugin(s) in the Plugin Manager.");
            return;
        }

        // apply custom style
        static CustomPopupMenuLookAndFeel customPopupLAF;
        customPopupLAF.minimumPopupWidth = vstButton.getWidth();
        menu.setLookAndFeel(&customPopupLAF);

        menu.showMenuAsync(
            juce::PopupMenu::Options().withTargetComponent(&vstButton)
            .withPreferredPopupDirection(juce::PopupMenu::Options::PopupDirection::downwards),
            [this, entries, indexMap](int choice)
            {
                if (choice < 1 || choice >(int)indexMap.size())
                    return;

                int realIdx = indexMap[choice - 1];
                const auto& desc = entries[realIdx].desc;

                auto& adm = vstHost->getAudioDeviceManagerRef();
                auto* dev = adm.getCurrentAudioDevice();
                double sr = dev ? dev->getCurrentSampleRate() : 44100.0;
                int    bs = dev ? dev->getCurrentBufferSizeSamples() : 512;

                vstHost->loadPlugin(juce::File(normalizePluginId(desc.fileOrIdentifier)), sr, bs);

                auto* inst = vstHost->getPluginInstance();
                int   nParam = inst ? (int)inst->getParameters().size() : 0;
                for (int s = 0; s < numCCParams; ++s)
                {
                    auto& map = banks[activeBankIndex].globalCCMappings[s];
                    if (map.paramIndex < 0 || map.paramIndex >= nParam)
                    {
                        map.paramIndex = -1;
                        map.name = "<none>";
                    }
                }

                banks[activeBankIndex].pluginName = desc.name;
                pluginLabel.setText(desc.name, juce::dontSendNotification);

                updateVSTButtonLabel();

                // уведомляем Rig_control о смене плагина
                if (onPluginChanged)
                    onPluginChanged();
            });

        return;
    }

    // B: plugin already loaded -> ask to close
    juce::AlertWindow::showOkCancelBox(
        juce::AlertWindow::WarningIcon, juce::String::fromUTF8("⚠️ Close Plugin"),
        "Are you sure you want to close the current plugin?",
        juce::String::fromUTF8("✔️ Yes"),
        juce::String::fromUTF8("❌ No"),
        nullptr,
        juce::ModalCallbackFunction::create([this](int result)
            {
                if (result == 1)
                {
                    unloadPluginEverywhere();
                    pluginLabel.setText("<no plugin>", juce::dontSendNotification);
                    updateVSTButtonLabel();

                    // уведомляем Rig_control о закрытии плагина
                    if (onPluginChanged)
                        onPluginChanged();
                }
            }));
}

//===== PUBLIC API IMPLEMENTATION =============================================
void BankEditor::setActivePresetIndex(int newPresetIndex)
{
    if (newPresetIndex >= 0 && newPresetIndex < numPresets)
    {
        activePreset = newPresetIndex;
        updateSelectedPresetLabel();
        updatePresetButtons();
        repaint();                     
        sendChange();
        
    }
}
juce::StringArray BankEditor::getPresetNames(int bankIndex) const noexcept
{
    juce::StringArray names;
    if (bankIndex >= 0 && bankIndex < (int)banks.size())
    {
        for (int i = 0; i < numPresets; ++i)
            names.add(banks[bankIndex].presetNames[i]);
    }
    return names;
}
std::vector<CCMapping> BankEditor::getCCMappings(int bankIndex, int presetIndex) const
{
    std::vector<CCMapping> mappings;
    if (bankIndex >= 0 && bankIndex < static_cast<int>(banks.size()) &&
        presetIndex >= 0 && presetIndex < numPresets)
    {
        const auto& bank = banks[bankIndex];
        for (int cc = 0; cc < numCCParams; ++cc)
        {
            CCMapping combined;
            // Берем глобальные данные, назначенные для данной CC-кнопки:
            const auto& global = bank.globalCCMappings[cc];
            // Берем пресетные данные (уровень, состояние включения) из текущего пресета:
            const auto& preset = bank.presetCCMappings[presetIndex][cc];
            combined.paramIndex = global.paramIndex;
            combined.invert = global.invert;
            combined.name = global.name;
            // Поле enabled теперь берём из пресетной структуры:
            combined.enabled = preset.enabled;
            // ccValue берется из пресетных настроек:
            combined.ccValue = preset.ccValue;
            mappings.push_back(combined);
        }
    }
    return mappings;
}
void BankEditor::updateCCParameter(int index, bool state)
{
    if (banks.empty() || activeBankIndex < 0 || activeBankIndex >= (int)banks.size())
        return;
    auto& bank = banks[activeBankIndex];
    if (activePreset < 0 || activePreset >= numPresets)
        return;
    if (index < 0 || index >= numCCParams)
        return;

    auto& globalMapping = bank.globalCCMappings[index];
    auto& presetMapping = bank.presetCCMappings[activePreset][index];

    bank.ccPresetStates[activePreset][index] = state;
    presetMapping.enabled = state;

    uint8_t effective = (!presetMapping.invert)
        ? (state ? presetMapping.ccValue : 0)
        : (state ? 0 : presetMapping.ccValue);

    if (vstHost != nullptr && globalMapping.paramIndex >= 0)
    {
        vstHost->setPluginParameter(globalMapping.paramIndex, effective);
        sendChange();
    }
}


// -------------------------------------------------------------------
void BankEditor::onPluginParameterChanged(int paramIdx, float normalised)
{
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      1.  РЕЖИМ LEARN
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    juce::String paramName;

    if (auto* inst = (vstHost != nullptr ? vstHost->getPluginInstance() : nullptr))
        paramName = safeGetParamName(inst, paramIdx, 128); // без deprecated-API

    if (paramName.isEmpty())
        paramName = "<unnamed>";          // запасное имя, если строка пуста

    learn.parameterTouched(paramIdx, paramName);

    if (learn.isActive())                 // событие поглощено Learn-режимом
        return;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      2.  ПРОВЕРКИ И ПРЕОБРАЗОВАНИЯ (АУДИО-ТРЕД, БЕЗ GUI)
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    if (vstHost == nullptr || vstHost->getPluginInstance() == nullptr
        || isSettingPreset)
        return;

    const int v127 = juce::jlimit(0, 127,
        int(std::lround(normalised * 127.0f)));

    // ищем CC-слот
    int slot = -1;
    for (int i = 0; i < numCCParams; ++i)
        if (banks[activeBankIndex].globalCCMappings[i].paramIndex == paramIdx)
        {
            slot = i; break;
        }

    if (slot < 0) return;

    auto& preset = banks[activeBankIndex]
        .presetCCMappings[activePreset][slot];

    const bool invert = preset.invert;
    const bool shouldBeEnabled = invert ? (v127 == 0) : (v127 > 0);
    const int  newCCValue = invert ? (127 - v127) : v127;

    // записываем результаты в модель (это ПОКА что не GUI)
    preset.enabled = shouldBeEnabled;
    if (shouldBeEnabled)
        preset.ccValue = newCCValue;

    banks[activeBankIndex].ccPresetStates[activePreset][slot] = shouldBeEnabled;

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      3.  ПЕРЕДАЁМ ОБНОВЛЕНИЕ GUI В MessageThread
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    juce::MessageManager::callAsync([this, slot, shouldBeEnabled]
        {
            ccToggleButtons[slot].setToggleState(shouldBeEnabled,
                juce::dontSendNotification);
            updatePresetButtons();
            repaint();
        });
}
//────────────────────────────  Learn callbacks  ────────────────────────────
void BankEditor::learnStarted(int slot)
{
    auto& btn = learnButtons[slot];
    oldLearnColour = btn.findColour(juce::TextButton::buttonColourId);
    btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0, 200, 0));

    selectedPresetLabel.setText("Move any control on the plugin",
        juce::dontSendNotification);

    if (onLearnToggled) onLearnToggled(slot, true);
}

void BankEditor::learnCancelled(int slot)
{
    learnButtons[slot].setColour(juce::TextButton::buttonColourId, oldLearnColour);
    updateSelectedPresetLabel();

    if (onLearnToggled) onLearnToggled(slot, false);
}

void BankEditor::learnFinished(int slot, int paramIdx, const juce::String& pname)
{
    learnCancelled(slot); // вернуть кнопку

    if (vstHost)
        vstHost->setExternalLearnState(slot, false);

    if (slot >= 0 && slot < numCCParams)
    {
        auto& ccMap = banks[activeBankIndex].globalCCMappings[slot];
        ccMap.paramIndex = paramIdx;
        ccMap.name = pname.isEmpty() ? "<none>" : pname;

        // обновляем подпись в UI
        if (slot < (int)std::size(ccNameEditors))
        {
            ccNameEditors[slot].setText(ccMap.name, juce::sendNotification);

            if (vstHost != nullptr)
                vstHost->updateCCLabel(slot, ccMap.name);
        }
    }

    saveSettings();
}


void BankEditor::toggleLearnFromHost(int cc, bool on)
{
    if (cc < 0 || cc >= numCCParams) return;
    if (on)  learn.begin(cc);
    else     learn.cancel();
}
void BankEditor::toggleCC(int ccIndex, bool state)
{
    if (activeBankIndex < 0 || activeBankIndex >= (int)banks.size()) return;
    auto& bank = banks[activeBankIndex];
    if (activePreset < 0 || activePreset >= (int)bank.ccPresetStates.size()) return;
    if (ccIndex < 0 || ccIndex >= numCCParams) return;

    // 1. Обновляем модель
    bank.presetCCMappings[activePreset][ccIndex].enabled = state;
    bank.ccPresetStates[activePreset][ccIndex] = state;

    // 2. Применяем к железу/плагину
    updateCCParameter(ccIndex, state);

    sendChange();
}
juce::File BankEditor::getDefaultConfigFile() const
{
    // Вместо жёсткого banks_config.xml возвращаем рабочий файл
    // Если он есть — используем его, если нет — Default.xml
    auto baseDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("NEXUS_KONTROL_OS");
    baseDir.createDirectory();

    if (currentlyLoadedBankFile.existsAsFile())
        return currentlyLoadedBankFile;

    return baseDir.getChildFile("Default.xml");
}

void BankEditor::writeDefaultConfig()
{
    auto file = getDefaultConfigFile();
    DBG("Writing default config to: " << file.getFullPathName());
    saveSettingsToFile(file);
}


void BankEditor::loadDefaultConfig()
{
    // Загружаем banks[] из XML через уже существующую функцию
    loadSettingsFromFile(getDefaultConfigFile());
}
juce::XmlElement* BankEditor::serializeBank(const Bank& b, int index) const
{
    auto* bankEl = new juce::XmlElement("Bank");
    bankEl->setAttribute("index", index);
    bankEl->setAttribute("bankName", b.bankName);

    // --- Нормализация pluginId ---
    juce::String id = b.pluginId;
    juce::File f(id);

    if (f.getFileExtension() == ".so")
    {
        auto archDir = f.getParentDirectory();        // x86_64-linux
        auto contents = archDir.getParentDirectory();  // Contents
        auto vst3dir = contents.getParentDirectory(); // *.vst3

        if (vst3dir.hasFileExtension("vst3"))
            id = vst3dir.getFullPathName();
    }

    bankEl->setAttribute("pluginName", b.pluginName);
    bankEl->setAttribute("pluginId", id);
    bankEl->setAttribute("activeProgram", b.activeProgram);

    // Preset names
    {
        auto* presetsEl = new juce::XmlElement("PresetNames");
        for (int p = 0; p < numPresets; ++p)
        {
            auto* pe = new juce::XmlElement("Preset");
            pe->setAttribute("index", p);
            pe->setAttribute("name", b.presetNames[p]);
            presetsEl->addChildElement(pe);
        }
        bankEl->addChildElement(presetsEl);
    }

    // CC состояния и назначения
    {
        auto* ccStatesEl = new juce::XmlElement("CCPresetStates");
        for (int p = 0; p < numPresets; ++p)
        {
            auto* presetEl = new juce::XmlElement("Preset");
            presetEl->setAttribute("index", p);

            for (int cc = 0; cc < numCCParams; ++cc)
            {
                const auto& presetMap = b.presetCCMappings[p][cc];
                const auto& globalMap = b.globalCCMappings[cc];

                auto* ccEl = new juce::XmlElement("CC");
                ccEl->setAttribute("number", cc);
                ccEl->setAttribute("ccValue", (int)presetMap.ccValue);
                ccEl->setAttribute("invert", presetMap.invert);
                ccEl->setAttribute("enabled", presetMap.enabled);
                ccEl->setAttribute("paramIndex", globalMap.paramIndex);
                ccEl->setAttribute("paramName", globalMap.name);

                presetEl->addChildElement(ccEl);
            }
            ccStatesEl->addChildElement(presetEl);
        }
        bankEl->addChildElement(ccStatesEl);
    }

    // Полный state плагина
    if (b.pluginState.getSize() > 0)
    {
        auto* stateEl = new juce::XmlElement("PluginState");
        stateEl->addTextElement(b.pluginState.toBase64Encoding());
        bankEl->addChildElement(stateEl);
    }

    // Baseline параметров
    {
        auto* paramsEl = new juce::XmlElement("PluginParams");
        for (float v : b.pluginParamValues)
        {
            auto* pe = new juce::XmlElement("Param");
            pe->setAttribute("value", v);
            paramsEl->addChildElement(pe);
        }
        bankEl->addChildElement(paramsEl);
    }

    // Diff’ы параметров
    if (!b.paramDiffs.empty())
    {
        auto* diffsEl = new juce::XmlElement("ParamDiffs");
        for (const auto& [idx, val] : b.paramDiffs)
        {
            auto* de = new juce::XmlElement("Diff");
            de->setAttribute("index", idx);
            de->setAttribute("value", val);
            diffsEl->addChildElement(de);
        }
        bankEl->addChildElement(diffsEl);
    }

    return bankEl;
}

void BankEditor::deserializeBank(Bank& b, const juce::XmlElement& bankEl)
{
    b.bankName = bankEl.getStringAttribute("bankName");
    b.pluginName = bankEl.getStringAttribute("pluginName");

    // --- Нормализация pluginId ---
    {
        juce::String rawId = bankEl.getStringAttribute("pluginId");
        juce::File f(rawId);

        if (f.getFileExtension() == ".so")
        {
            auto archDir = f.getParentDirectory();        // x86_64-linux
            auto contents = archDir.getParentDirectory();  // Contents
            auto vst3dir = contents.getParentDirectory(); // *.vst3

            if (vst3dir.hasFileExtension("vst3"))
                rawId = vst3dir.getFullPathName();
        }

        b.pluginId = rawId; // теперь всегда .vst3
    }

    b.activeProgram = bankEl.getIntAttribute("activeProgram", -1);

    // Preset names
    if (auto* presetsEl = bankEl.getChildByName("PresetNames"))
    {
        forEachXmlChildElementWithTagName(*presetsEl, pe, "Preset")
        {
            int pIdx = pe->getIntAttribute("index", -1);
            if (pIdx >= 0 && pIdx < numPresets)
                b.presetNames[pIdx] = pe->getStringAttribute("name");
        }
    }

    // CC состояния и назначения
    if (auto* ccStatesEl = bankEl.getChildByName("CCPresetStates"))
    {
        forEachXmlChildElementWithTagName(*ccStatesEl, presetEl, "Preset")
        {
            int pIdx = presetEl->getIntAttribute("index", -1);
            if (pIdx >= 0 && pIdx < numPresets)
            {
                forEachXmlChildElementWithTagName(*presetEl, ccEl, "CC")
                {
                    int cc = ccEl->getIntAttribute("number", -1);
                    if (cc >= 0 && cc < numCCParams)
                    {
                        auto& presetMap = b.presetCCMappings[pIdx][cc];
                        presetMap.enabled = ccEl->getBoolAttribute("enabled", false);
                        presetMap.ccValue = (uint8_t)ccEl->getIntAttribute("ccValue", 64);
                        presetMap.invert = ccEl->getBoolAttribute("invert", false);

                        // 🔹 фикс: сразу синхронизируем ccPresetStates
                        b.ccPresetStates[pIdx][cc] = presetMap.enabled;

                        auto& globalMap = b.globalCCMappings[cc];
                        globalMap.paramIndex = ccEl->getIntAttribute("paramIndex", -1);
                        globalMap.name = ccEl->getStringAttribute("paramName");
                    }
                }
            }
        }
    }

    // Полный state плагина
    b.pluginState.reset();
    if (auto* stateEl = bankEl.getChildByName("PluginState"))
        b.pluginState.fromBase64Encoding(stateEl->getAllSubText().trim());

    // Baseline параметров
    b.pluginParamValues.clear();
    if (auto* paramsEl = bankEl.getChildByName("PluginParams"))
    {
        forEachXmlChildElementWithTagName(*paramsEl, pe, "Param")
            b.pluginParamValues.push_back((float)pe->getDoubleAttribute("value", 0.0));
    }

    // Diff’ы параметров
    b.paramDiffs.clear();
    if (auto* diffsEl = bankEl.getChildByName("ParamDiffs"))
    {
        forEachXmlChildElementWithTagName(*diffsEl, de, "Diff")
        {
            int idx = de->getIntAttribute("index", -1);
            if (idx >= 0)
                b.paramDiffs[idx] = (float)de->getDoubleAttribute("value", 0.0);
        }
    }
}


void BankEditor::applyBankToPlugin(int bankIndex, bool synchronous /* = false */)
{
    if (bankIndex < 0 || bankIndex >= (int)banks.size())
        return;
    if (!vstHost)
        return;

    const auto& b = banks[bankIndex];
    auto* currentInst = vstHost->getPluginInstance();
    bool needLoad = false;

    if (!currentInst)
        needLoad = b.pluginId.isNotEmpty();
    else if (b.pluginId.isNotEmpty() && b.pluginId != lastLoadedPluginId)
    {
        vstHost->unloadPlugin();
        needLoad = true;
    }

    if (needLoad)
    {
        juce::File pluginDir(b.pluginId);

        if (pluginDir.exists())
        {
            DBG("Loading plugin: " << pluginDir.getFullPathName());
            vstHost->loadPlugin(pluginDir);

            // фиксируем именно .vst3 как идентификатор
            lastLoadedPluginId = b.pluginId;
        }
        else
        {
            DBG("Plugin not found: " << pluginDir.getFullPathName());
            return;
        }
    }

    auto* instNow = vstHost->getPluginInstance();
    if (!instNow)
        return;

    // --- Если есть полный state
    if (b.pluginState.getSize() > 0)
    {
        auto stateCopy = b.pluginState;
        auto targetId = lastLoadedPluginId;
        bool justLoaded = needLoad;

        auto applyState = [this, targetId, stateCopy]()
            {
                if (!vstHost) return;
                if (targetId.isEmpty() || targetId != lastLoadedPluginId) return;

                auto* instCheck = vstHost->getPluginInstance();
                if (!instCheck) return;

                const int sz = (int)stateCopy.getSize();
                if (sz <= 0 || stateCopy.getData() == nullptr) return;

                bool wasOpen = vstHost->isPluginEditorOpen();
                if (wasOpen) vstHost->closePluginEditorIfOpen();

                DBG("Applying full plugin state (" << sz << " bytes)");
                try { instCheck->setStateInformation(stateCopy.getData(), sz); }
                catch (...) { DBG("Exception in setStateInformation — skipped"); }

                if (wasOpen)
                {
                    juce::Timer::callAfterDelay(80, [this, targetId]()
                        {
                            if (!vstHost) return;
                            if (targetId != lastLoadedPluginId) return;
                            if (vstHost->getPluginInstance())
                                vstHost->openPluginEditorIfNeeded();
                        });
                }
            };

        if (synchronous)
            applyState();
        else if (justLoaded)
            juce::MessageManager::callAsync([applyState]() {
            juce::Timer::callAfterDelay(150, applyState);
                });
        else
        {
            if (juce::MessageManager::getInstance()->isThisTheMessageThread())
                applyState();
            else
                juce::MessageManager::callAsync(applyState);
        }

        return;
    }

    // --- Если state пуст — baseline + diffs
    if (b.activeProgram >= 0)
        instNow->setCurrentProgram(b.activeProgram);

    auto params = instNow->getParameters();
    const int n = std::min((int)params.size(), (int)b.pluginParamValues.size());
    for (int i = 0; i < n; ++i)
        params[i]->setValueNotifyingHost(b.pluginParamValues[i]);

    for (const auto& kv : b.paramDiffs)
    {
        int idx = kv.first;
        float val = kv.second;
        if (idx >= 0 && idx < (int)params.size())
            params[idx]->setValueNotifyingHost(val);
    }
}


void BankEditor::snapshotCurrentBank()
{
    if (!vstHost || activeBankIndex < 0 || activeBankIndex >= (int)banks.size())
        return;

    auto* inst = vstHost->getPluginInstance();
    if (!inst)
        return;

    auto& b = banks[activeBankIndex];
    const auto& params = inst->getParameters();
    const int N = (int)params.size();

    // Инициализация/переразмер базовой копии параметров банка
    if ((int)b.pluginParamValues.size() != N)
        b.pluginParamValues.assign(N, 0.0f);

    static constexpr float eps = 1.0e-4f;
    std::unordered_map<int, float> newDiffs;
    newDiffs.reserve((size_t)N / 4); // эвристика, чтобы меньше реаллокаций

    for (int i = 0; i < N; ++i)
    {
        const float v = params[i]->getValue();       // текущее значение в плагине [0..1]
        const float old = b.pluginParamValues[i];       // сохранённый baseline банка

        if (std::abs(v - old) > eps)
            newDiffs[i] = v;                            // зафиксировать как отличающееся

        b.pluginParamValues[i] = v;                     // обновить baseline банка
    }

    b.paramDiffs = std::move(newDiffs);
    b.activeProgram = inst->getCurrentProgram();
}
void BankEditor::unloadPluginEverywhere()
{
    // 1. Выгружаем сам плагин
    if (vstHost)
        vstHost->unloadPlugin();

    // 2. Сбрасываем все банки в дефолт
    for (int i = 0; i < numBanks; ++i)
    {
        banks[i] = Bank(); // пересоздаём структуру
        banks[i].bankName = "BANK" + juce::String(i + 1);
    }

    // 3. Чистим глобальные данные плагина
    globalPluginName.clear();
    globalPluginId.clear();
    globalPluginState.reset();
    globalPluginParamValues.clear();
    globalActiveProgram = -1;

    // 4. Сбрасываем активные индексы
    activeBankIndex = 0;
    activePreset = 0;

    // 5. Обновляем UI
    updateUI();

   
}

juce::String BankEditor::getCurrentPluginDisplayName() const
{
    if (vstHost != nullptr)
        if (auto* inst = vstHost->getPluginInstance())
            return inst->getName().isEmpty() ? "PLUGIN: <unnamed>" : inst->getName();

    return "PLUGIN: NONE";
}
/////педаль 
void BankEditor::applyPedalValue(int slot, float norm)
{
    if (slot < 0 || slot >= numCCParams || vstHost == nullptr)
        return;

    auto& bank = banks[activeBankIndex];
    CCMapping& m = bank.globalCCMappings[slot];

    // 1. Применение к плагину
    if (m.paramIndex >= 0)
    {
        if (auto* plug = vstHost->getPluginInstance())
        {
            if (auto* param = plug->getParameters()[m.paramIndex])
            {
                float v = m.invert ? (1.0f - norm) : norm;
                param->setValueNotifyingHost(v);
            }
        }
    }

    // 2. Отправка MIDI CC (если нужно)
    if (midiOutput != nullptr)
    {
        int ccValue = juce::jlimit(0, 127, static_cast<int>(norm * 127.0f));
        auto msg = juce::MidiMessage::controllerEvent(1, slot + 1, ccValue);
        midiOutput->sendMessageNow(msg);
    }

    // 3. Обновление модели (если нужно)
    if (activePreset >= 0 && activePreset < (int)bank.ccPresetStates.size())
    {
        bank.ccPresetStates[activePreset][slot] = (norm > 0.0f);
        bank.presetCCMappings[activePreset][slot].enabled = (norm > 0.0f);
    }

    sendChange();
}
void BankEditor::openPedalMappingDialog(int slot)
{
    if (slot < 0 || slot >= numCCParams)
        return;

    CCMapping& m = banks[activeBankIndex].globalCCMappings[slot];

    auto dlg = std::make_unique<SetCCDialog>(
        vstHost,
        m,
        juce::String("Pedal Mapping ") + juce::String(slot),
        [this, slot](CCMapping newMap, bool ok)
        {
            if (ok)
                banks[activeBankIndex].globalCCMappings[slot] = newMap;
        });

    dlg->setVisible(true);
}
void BankEditor::checkForChanges()
{
    bool modified = false;

    // --- сравнение текста из редакторов ---
    if (bankNameEditor.getText() != bankSnapshot.bankName) {
        DBG("[CheckChanges] bankName changed");
        modified = true;
    }

    for (int i = 0; i < numPresets && !modified; ++i) {
        if (presetEditors[i].getText() != bankSnapshot.presetNames[i]) {
            DBG("[CheckChanges] presetName[" << i << "] changed");
            modified = true;
        }
    }

    for (int i = 0; i < numCCParams && !modified; ++i) {
        if (ccNameEditors[i].getText() != bankSnapshot.globalCCMappings[i].name) {
            DBG("[CheckChanges] ccName[" << i << "] changed");
            modified = true;
        }
    }

    // --- проверка имени библиотеки ---
    if (!modified && libraryNameEditor.getText() != loadedFileName) {
        DBG("[CheckChanges] libraryName changed");
        modified = true;
    }

    // --- проверка плагина ---
    const auto& current = banks[activeBankIndex];
    const auto& snap = bankSnapshot;

    if (!modified && current.pluginName != snap.pluginName) {
        DBG("[CheckChanges] pluginName changed");
        modified = true;
    }
    if (!modified && normalizePluginId(current.pluginId) != normalizePluginId(snap.pluginId)) {
        DBG("[CheckChanges] pluginId changed");
        modified = true;
    }
    if (!modified && current.pluginParamValues != snap.pluginParamValues) {
        DBG("[CheckChanges] pluginParamValues changed");
        modified = true;
    }
    if (!modified && current.paramDiffs != snap.paramDiffs) {
        DBG("[CheckChanges] paramDiffs changed");
        modified = true;
    }
    if (!modified && current.pluginState.toBase64Encoding() != snap.pluginState.toBase64Encoding()) {
        DBG("[CheckChanges] pluginState changed");
        modified = true;
    }

    // --- fallback: сравнение всей модели ---
    if (!modified && (banks[activeBankIndex] != bankSnapshot)) {
        DBG("[CheckChanges] Bank model changed");
        modified = true;
    }

    // --- мигание кнопки Store ---
    if (modified) {
        bool blink = (juce::Time::getMillisecondCounter() / 500) % 2;
        storeButton.setColour(juce::TextButton::buttonColourId,
            blink ? juce::Colours::red : juce::Colours::transparentBlack);

        // --- активируем Cancel ---
        cancelButton.setEnabled(true);
        cancelButton.setColour(juce::TextButton::buttonColourId, juce::Colours::orange);
    }
    else {
        storeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);

        // --- деактивируем Cancel ---
        cancelButton.setEnabled(false);
        cancelButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    }

    storeButton.repaint();
    cancelButton.repaint();
}



void BankEditor::updateVSTButtonLabel()
{
    vstButton.setLookAndFeel(&bigIcons);

    if (vstHost != nullptr && vstHost->getPluginInstance() != nullptr)
    {
        vstButton.setButtonText(juce::String::fromUTF8("❌ VST"));
     
    }
    else
    {
        vstButton.setButtonText(juce::String::fromUTF8("👇 VST"));
       
    }
}
// Папка BANK (D если фиксированный, иначе C)
juce::File BankEditor::getBankDir() const
{
    if (GetDriveTypeW(L"D:\\") == DRIVE_FIXED)
        return juce::File("D:\\NEXUS\\BANK");
    return juce::File("C:\\NEXUS\\BANK");
}

// Извлекаем начальный числовой префикс из имени (без расширения)
int BankEditor::getNumericPrefix(const juce::String& name) const
{
    int i = 0;
    while (i < name.length() && juce::CharacterFunctions::isDigit(name[i]))
        ++i;
    if (i == 0) return -1;
    return name.substring(0, i).getIntValue();
}

// Компаратор для сортировки
struct FileComparator
{
    const BankEditor* editor;
    int compareElements(const juce::File& a, const juce::File& b) const
    {
        int ap = editor->getNumericPrefix(a.getFileNameWithoutExtension());
        int bp = editor->getNumericPrefix(b.getFileNameWithoutExtension());

        if (ap != bp)
            return (ap < bp) ? -1 : 1;

        return a.getFileNameWithoutExtension()
            .compare(b.getFileNameWithoutExtension());
    }
};

// Сканируем BANK и сортируем по префиксу
juce::Array<juce::File> BankEditor::scanNumericBankFiles() const
{
    auto bankDir = getBankDir();
    juce::Array<juce::File> result;

    auto files = bankDir.findChildFiles(juce::File::findFiles, false, "*.xml");
    for (auto& f : files)
    {
        if (getNumericPrefix(f.getFileNameWithoutExtension()) >= 0)
            result.add(f);
    }

    FileComparator comp{ this };
    result.sort(comp, true);
    return result;
}
void BankEditor::loadBankFile(const juce::File& sourceFile)
{
    if (!sourceFile.existsAsFile())
        return;

    loadSettingsFromFile(sourceFile);
    currentlyLoadedBankFile = sourceFile;

    writeBootConfig(sourceFile);

    loadedFileName = sourceFile.getFileNameWithoutExtension();

}
// Навигация вперёд/назад
void BankEditor::navigateBank(bool forward)
{
    auto files = scanNumericBankFiles();
    if (files.isEmpty())
        return;

    auto currentBase = currentlyLoadedBankFile.getFileNameWithoutExtension();
    int currentPrefix = getNumericPrefix(currentBase);

    // Если текущий файл без префикса → крайний
    if (currentPrefix < 0)
    {
        loadBankFile(forward ? files.getFirst() : files.getLast());
        return;
    }

    int currentIndex = -1;
    for (int i = 0; i < files.size(); ++i)
    {
        if (files[i].getFileNameWithoutExtension() == currentBase)
        {
            currentIndex = i;
            break;
        }
    }
    if (currentIndex == -1) return;

    int targetIndex = forward
        ? (currentIndex < files.size() - 1 ? currentIndex + 1 : 0)
        : (currentIndex > 0 ? currentIndex - 1 : files.size() - 1);

    loadBankFile(files[targetIndex]);
}








