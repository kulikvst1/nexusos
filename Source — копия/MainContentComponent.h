#pragma once
#include <JuceHeader.h>
#include "vst_host.h"
#include "Rig_control.h"
#include "bank_editor.h"
#include "cpu_load.h"
#include "LooperComponent.h"
#include "TunerComponent.h"
#include "OutControlComponent.h"
#include "InputControlComponent.h"
#include "StartupShutdown.h"
#include "InlineTextInputOverlay.h"
#include "Metronome.h"
#include "MetronomeWindow.h"
#include "TabColourSettings.h"

class GlobalTextInputWatcher : public juce::FocusChangeListener
{
public:
    void globalFocusChanged(juce::Component* newFocus) override
    {
        if (newFocus == nullptr)
            return;

        // 🚫 Фильтр: если фокус внутри FileManager → выходим
        if (newFocus->findParentComponentOfClass<FileManager>() != nullptr)
            return;

        if (auto* editor = dynamic_cast<juce::TextEditor*>(newFocus))
        {
            // Игнорируем, если это наш оверлей
            if (editor->findParentComponentOfClass<InlineTextInputOverlay>() != nullptr)
                return;

            if (auto* mainWin = juce::TopLevelWindow::getActiveTopLevelWindow())
            {
                auto bounds = mainWin->getScreenBounds();

                int w = 400;
                int h = 80;

                // Центрируем по экрану
                auto targetRect = juce::Rectangle<int>(
                    bounds.getCentreX() - w / 2,
                    bounds.getCentreY() - h / 2,
                    w, h
                );

                juce::CallOutBox& box = juce::CallOutBox::launchAsynchronously(
                    std::make_unique<InlineTextInputOverlay>(
                        editor->getText(),
                        [editor](const juce::String& newText) {
                            editor->setText(newText, juce::NotificationType::sendNotification);
                        }
                    ),
                    targetRect,
                    mainWin
                );


                box.setArrowSize(0.0f);
            }
        }
    }
};
// --- Панель с уведомлением об изменении видимости ---
class NotifyingSidePanel : public juce::SidePanel
{
public:
    using juce::SidePanel::SidePanel; // наследуем конструкторы

    std::function<void(bool)> onPanelVisibilityChanged; // true=open, false=closed

    void visibilityChanged() override
    {
        juce::SidePanel::visibilityChanged();

        if (onPanelVisibilityChanged)
            onPanelVisibilityChanged(isPanelShowing());
    }
};
using namespace juce;
class CallbackTabbedComponent : public TabbedComponent
{
public:
    CallbackTabbedComponent(TabbedButtonBar::Orientation orient)
        : TabbedComponent(orient) {}

    std::function<void(int)> onCurrentTabChanged;

    int tabBarIndent = 0; // ← добавляем поле

protected:
    void currentTabChanged(int newIndex, const String& newName) override
    {
        TabbedComponent::currentTabChanged(newIndex, newName);
        if (onCurrentTabChanged)
            onCurrentTabChanged(newIndex);
    }

    void resized() override
    {
        TabbedComponent::resized(); // сначала стандартный layout

        if (tabBarIndent > 0)
        {
            auto& bar = getTabbedButtonBar();
            bar.setBounds(bar.getBounds().withTrimmedLeft(tabBarIndent));
        }
    }
};
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class IconTextTabLookAndFeel : public juce::LookAndFeel_V4
{
public:
    IconTextTabLookAndFeel(int iconSizePx = 20, int iconPaddingPx = 28)
        : iconSize(iconSizePx), iconPadding(iconPaddingPx)
    {
        icons["INPUT"] = loadImage(BinaryData::Input_png, BinaryData::Input_pngSize);
        icons["RIG CONTROL"] = loadImage(BinaryData::RigControl_png, BinaryData::RigControl_pngSize);
        icons["BANK EDITOR"] = loadImage(BinaryData::BankEdit_png, BinaryData::BankEdit_pngSize);
        icons["VST EDITOR"] = loadImage(BinaryData::vstEdit_png, BinaryData::vstEdit_pngSize);
        icons["OUTPUT"] = loadImage(BinaryData::Output_png, BinaryData::Output_pngSize);
        icons["LOOPER"] = loadImage(BinaryData::looper_png, BinaryData::looper_pngSize);
        icons["TUNER"] = loadImage(BinaryData::Tuning_png, BinaryData::Tuning_pngSize);
    }

    void drawTabButton(juce::TabBarButton& button, juce::Graphics& g,
        bool isMouseOver, bool isMouseDown) override
    {
        auto area = button.getLocalBounds();

        // фон и рамка
        auto backgroundColour = button.isFrontTab() ? juce::Colour::fromRGBA(50, 62, 68, 255)//juce::Colours::darkgrey
            : juce::Colours::grey;//juce::Colours::grey
        g.setColour(backgroundColour);
        g.fillRect(area);

        g.setColour(juce::Colours::black);
        g.drawRect(area);

        // ищем иконку по имени вкладки
        auto it = icons.find(button.getButtonText());
        if (it != icons.end() && it->second.isValid())
        {
            auto iconBounds = area.removeFromLeft(iconPadding+20)
                .withSizeKeepingCentre(iconSize+15, iconSize+15);

            g.drawImageWithin(it->second,
                iconBounds.getX(), iconBounds.getY(),
                iconBounds.getWidth(), iconBounds.getHeight(),
                juce::RectanglePlacement::centred);
        }

        // текст справа
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(15.0f));
        g.drawFittedText(button.getButtonText(), area, juce::Justification::centredLeft, 1);
    }

    int getTabButtonBestWidth(juce::TabBarButton& button, int tabDepth) override
    {
        int textWidth = juce::LookAndFeel_V4::getTabButtonBestWidth(button, tabDepth);
        if (icons.find(button.getButtonText()) != icons.end())
        return textWidth + iconPadding;
        return textWidth;
    }
private:
    juce::Image loadImage(const void* data, size_t size)
    {
        juce::MemoryInputStream mis(data, size, false);
        return juce::PNGImageFormat().decodeImage(mis);
    }
    std::map<juce::String, juce::Image> icons;
    int iconSize;
    int iconPadding;
};
//
class MetronomeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    juce::Drawable* icon = nullptr;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
        const juce::Colour&, bool, bool) override
    {
        auto area = button.getLocalBounds();
        auto colour = button.getToggleState() ? juce::Colours::green : juce::Colour::fromRGBA(50, 62, 68, 255);
        g.setColour(colour);
        g.fillRect(area);
        g.setColour(juce::Colours::black);
        g.drawRect(area);
        if (icon != nullptr)
        {
            auto iconArea = area.withSizeKeepingCentre(area.getWidth() * 0.9f,
            area.getHeight() * 0.9f);
            icon->drawWithin(g, iconArea.toFloat(),
            juce::RectanglePlacement::centred, 1.0f);
        }
    }

};
//
class BpmButtonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
        const juce::Colour&, bool, bool) override
    {
        auto area = button.getLocalBounds();

        // прозрачный фон
        g.setColour(juce::Colours::transparentBlack);
        g.fillRect(area);

        // чёрная рамка
        g.setColour(juce::Colours::black);
        g.drawRect(area, 1.0f);
    }
};

//==============================================================================
// Главное окно
//==============================================================================
class MainContentComponent : public Component
{
public:
    MainContentComponent(PluginManager& pm, bool loadDefaultFlag = false)
        : pluginManager(pm), tabs(TabbedButtonBar::TabsAtTop)

    {
        // 1) Инициализация аудио
        deviceManager.initialise(2, 2, nullptr, true);
        // 2) VST Host
        vstHostComponent = std::make_unique<VSTHostComponent>(deviceManager, pluginManager);

        // 3) BankEditor — пробрасываем флаг
        bankEditor = std::make_unique<BankEditor>(pluginManager, vstHostComponent.get(), loadDefaultFlag);

        // 3) Тюнеры
        tunerComponent = std::make_unique<TunerComponent>();
        rigTuner = std::make_unique<TunerComponent>();
        tunerComponent->getRmsSlider().getValueObject().referTo(rmsValue);
        rigTuner->getRmsSlider().getValueObject().referTo(rmsValue);

        tunerComponent->getConfSlider().getValueObject().referTo(confValue);
        rigTuner->getConfSlider().getValueObject().referTo(confValue);

        vstHostComponent->addTuner(tunerComponent.get());
        vstHostComponent->addTuner(rigTuner.get());

        // 4) OutControl / InputControl
        outControlComponent = std::make_unique<OutControlComponent>();
        vstHostComponent->setOutControlComponent(outControlComponent.get());

        inputControlComponent = std::make_unique<InputControlComponent>();
        inputControlComponent->prepare(currentSampleRate, currentBlockSize);
        vstHostComponent->setInputControlComponent(inputControlComponent.get());

        // 5) Остальные модули
        rigControl = std::make_unique<Rig_control>(deviceManager);
        inputControlComponent->setRigControl(rigControl.get());
        rigControl->setInputControlComponent(inputControlComponent.get());
        //rig switch
        rigControl->onMidiActivity = [this]()
            {
                if (autoSwitchToRig)
                {
                    auto currentTabName = tabs.getTabNames()[tabs.getCurrentTabIndex()];
                    if (currentTabName != "LOOPER")
                        ensureRigControlActive();
                }
            };

        // 🔹 Привязка логики переключения вкладки тюнера
        rigControl->onTunerVisibilityChanged = [this](bool show)
            {
                updateTunerRouting(); // 🔹 сразу обновляем маршрутизацию

                if (show)
                {
                    int tunerIndex = getTabIndexByName("TUNER");
                    if (tunerIndex >= 0 && tabs.getCurrentTabIndex() != tunerIndex)
                        tabs.setCurrentTabIndex(tunerIndex, true);
                }
                else
                {
                    ensureRigControlActive();
                }
            };
        // 🔹 Привязка логики переключения вкладки лупера
        rigControl->onLooperButtonChanged = [this](bool state)
            {
                int looperIndex = getTabIndexByName("LOOPER");

                if (state)
                {
                    if (looperIndex >= 0)
                        tabs.setCurrentTabIndex(looperIndex, true);
                    else
                        ensureRigControlActive();
                }
                else
                {
                    if (looperIndex >= 0)
                        ensureRigControlActive();
                }
            };

        // 5b) MIDI init
        Start = std::make_unique<StartupShutdown>(*rigControl);
        Start->sendStartupCommands();
        looperEngine = std::make_unique<LooperEngine>();
        looperComponent = std::make_unique<LooperComponent>(*looperEngine);
        looperEngine->prepare(44100.0, 512);
        vstHostComponent->setLooperEngine(looperEngine.get());

        // 5a) Синхронизация A4
        tunerComponent->onReferenceA4Changed = [rig = rigTuner.get()](double a4)
            { rig->setReferenceA4(a4, false); };
        rigTuner->onReferenceA4Changed = [tab = tunerComponent.get()](double a4)
            { tab->setReferenceA4(a4, false); };
        // 6) Настройка Rig_control
        rigControl->setBankEditor(bankEditor.get());
        rigControl->setVstHostComponent(vstHostComponent.get());

        // Связь BankEditor → VSTHostComponent
        juce::Component::SafePointer<VSTHostComponent> safeHost{ vstHostComponent.get() };
        bankEditor->onCCStateChanged = [safeHost](int slot, bool active) {
            if (safeHost != nullptr)
                safeHost->setCCButtonState(slot, active);
            };

        // Связь VSTHostComponent → BankEditor
        juce::Component::SafePointer<BankEditor> safeBank{ bankEditor.get() };
        vstHostComponent->onCCButtonClicked = [safeBank](int slot, bool active) {
            if (safeBank != nullptr)
                safeBank->updateCCParameter(slot, active);
            };

        vstHostComponent->setBankEditor(bankEditor.get());

        // 🔹 Подписка на смену файла в BankEditor
        bankEditor->onLibraryFileChanged = [this](const juce::File& file)
            {
                if (rigControl->isTriggeredFromRig())
                    rigControl->syncLibraryToLoadedFile(false); // вызов из Rig → offset не трогаем
                else
                    rigControl->syncLibraryToLoadedFile(true);  // вызов из BankEditor → сброс offset
            };


        // Безопасная слабая ссылка на Rig_control
        juce::Component::SafePointer<Rig_control> safeRig{ rigControl.get() };

        // BPM: не захватываем this, работаем через SafePointer
        vstHostComponent->onBpmChanged = [this](double bpm)
            {
                tapTempoDisplay.setButtonText(juce::String((int)bpm) + " BPM");

                if (rigControl)
                {
                    rigControl->sendBpmToSerial(bpm);
                    rigControl->sendBpmToMidi(bpm);
                    rigControl->setCurrentBpm(bpm);
                    rigControl->updateTapButton(bpm);
                }

                if (metronomeComponent)
                    metronomeComponent->setBpm(bpm);
            };


        // Можно вызвать первичное обновление, но лучше делать это после полной инициализации
        vstHostComponent->updateBPM(120.0);

        // Looper: тоже не захватываем this
        rigControl->setLooperEngine(*looperEngine);
        looperEngine->onPlayerStateChanged = [safeRig](bool playing)
            {
                if (safeRig != nullptr)
                    safeRig->sendPlayerModeToMidi(playing);
            };

        rigControl->setTunerComponent(rigTuner.get());
        rigControl->setOutControlComponent(outControlComponent.get());

        // 7) Синхронизация пресетов
        bankEditor->onActivePresetChanged = [this](int idx)
            {
                rigControl->handleExternalPresetChange(idx);
                if (!hostIsDriving)
                    vstHostComponent->setExternalPresetIndex(idx);
            };
        rigControl->setPresetChangeCallback([this](int idx)
            {
                bankEditor->setActivePreset(idx);
                vstHostComponent->setExternalPresetIndex(idx);
            });
        vstHostComponent->setPresetCallback([this](int idx)
            {
                hostIsDriving = true;
                bankEditor->setActivePreset(idx);
                rigControl->handleExternalPresetChange(idx);
                hostIsDriving = false;
            });

        metronomeComponent = std::make_unique<Metronome>();
        metronomeComponent->setHost(vstHostComponent.get()); // 🔹 проброс хоста
      
        audioSourcePlayer.setSource(metronomeComponent.get());
        deviceManager.addAudioCallback(&audioSourcePlayer);

        // Связка LooperEngine ↔ Metronome для Count-In
        looperEngine->onCountInRequested = [this]()
            {
                if (metronomeComponent)
                {
                    metronomeComponent->startCountIn(
                        looperEngine->getCountInCycles() * metronomeComponent->getBeatsPerBar(),
                        // ⚠️ теперь не сразу запись, а выставляем флаг
                        [this]() { looperEngine->beginRecordingAfterCountIn(); },
                        looperEngine->isMetronomeContinueEnabled()
                    );
                }
            };

        // Связка LooperEngine ↔ Metronome для остановки после Count-In
        looperEngine->onStopMetronome = [this]()
            {
                if (metronomeComponent)
                    metronomeComponent->stop();
            };

        // 🔹 Новая связка: старт записи на первую долю
        metronomeComponent->onBarStart = [this]()
            {
                if (looperEngine->pendingRecordStart)
                {
                    looperEngine->recTriggered = !looperEngine->getTriggerEnabled();
                    looperEngine->recPos = 0;
                    looperEngine->state = LooperEngine::Recording;
                    looperEngine->pendingRecordStart = false;

                    if (looperEngine->onStateChanged)
                        looperEngine->onStateChanged(LooperEngine::Recording);

                    looperEngine->notify();
                }
            };

        looperComponent = std::make_unique<LooperComponent>(*looperEngine, false);
        // 8) Вкладки
        addAndMakeVisible(tabs);
        tabs.setTabBarDepth(40);
        tabs.setLookAndFeel(&inputTabLF);
        tabs.addTab("INPUT", juce::Colour::fromRGBA(50, 62, 68, 255), inputControlComponent.get(), false);
        tabs.addTab("RIG CONTROL", juce::Colour::fromRGBA(50, 62, 68, 255), rigControl.get(), false);
        tabs.addTab("BANK EDITOR", juce::Colour::fromRGBA(50, 62, 68, 255), bankEditor.get(), false);
        tabs.addTab("VST EDITOR", juce::Colour::fromRGBA(50, 62, 68, 255), vstHostComponent.get(), false);
        tabs.addTab("OUTPUT", juce::Colour::fromRGBA(50, 62, 68, 255), outControlComponent.get(), false);
        tabs.addTab("LOOPER", juce::Colour::fromRGBA(50, 62, 68, 255), looperComponent.get(), false);
        tabs.addTab("TUNER", juce::Colour::fromRGBA(50, 62, 68, 255), tunerComponent.get(), false);
        applySavedTabColours();

        // Сразу переставляем OUT CONTROL в конец
        ensureOutControlLast();
        looperTabVisible = true;
        tunerTabVisible = true;
        // Обработчик смены вкладок
        tabs.onCurrentTabChanged = [this](int newIndex)
            {
                updateTunerRouting();

                if (rigControl)
                {
                    auto tabName = tabs.getTabNames()[newIndex];
                    rigControl->sendSettingsMenuState(tabName == "INPUT");

                    // 🔹 Лупер: при входе активируем
                    if (tabName == "LOOPER")
                    {
                        // если лупер ещё не активен → включаем (шлём CC102)
                        if (!rigControl->getLooperState())
                            rigControl->setLooperState(true);

                        // всегда синхронизируем состояние движка (CC21/22/23)
                        rigControl->syncLooperStateToMidi();
                    }


                    // 🔹 Тюнер: только MIDI, без UI
                    rigControl->sendTunerMidi(tabName == "TUNER");
                }
                // 🔹 Автосохранение при уходе с вкладки OUTPUT
                if (lastTabIndex >= 0)
                {
                    auto prevTabName = tabs.getTabNames()[lastTabIndex];
                    if (prevTabName == "OUTPUT" && outControlComponent)
                    {
                        DBG("Leaving OUTPUT tab → saving settings");
                        outControlComponent->saveSettings();
                    }
                }

                // обновляем lastTabIndex
                lastTabIndex = newIndex;
            };

        // 11) CPU 
        cpuIndicator = std::make_unique<CpuLoadIndicator>(globalCpuLoad);
        addAndMakeVisible(cpuIndicator.get());

        // 🔹 BPM кнопка
        tapTempoDisplay.setLookAndFeel(&bpmLAF); // кастомный LAF с прямыми углами
        tapTempoDisplay.setButtonText(
            juce::String((int)vstHostComponent->getCurrentBPM()) + " BPM"
        );
        tapTempoDisplay.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        tapTempoDisplay.setColour(juce::TextButton::textColourOffId, juce::Colours::white);

        tapTempoDisplay.onClick = [this]()
            {
                if (metronomeWindow)
                    metronomeWindow->setVisible(true); // открыть окно настроек метронома
            };

        addAndMakeVisible(tapTempoDisplay);

        // 🔹 Создание окна настроек метронома
        metronomeWindow = std::make_unique<MetronomeWindow>(metronomeComponent.get());

        // 13) метроном
        juce::MemoryInputStream mis(BinaryData::Metronome_png, BinaryData::Metronome_pngSize, false);
        juce::Image img = juce::PNGImageFormat().decodeImage(mis);
        metronomeIcon = std::make_unique<juce::DrawableImage>();
        metronomeIcon->setImage(img);
        // Назначаем LookAndFeel
        metronomeLAF.icon = metronomeIcon.get();
        metronomeButton.setLookAndFeel(&metronomeLAF);
        // Включаем toggle режим
        metronomeButton.setClickingTogglesState(true);
        // Логика клика
        metronomeButton.onClick = [this]()
            {
                if (metronomeComponent)
                {
                    if (metronomeComponent->isRunningState())
                    {
                        metronomeComponent->stop();
                        metronomeButton.setToggleState(false, juce::dontSendNotification);
                    }
                    else
                    {
                        metronomeComponent->start();
                        metronomeButton.setToggleState(true, juce::dontSendNotification);
                    }
                }
            };

        addAndMakeVisible(metronomeButton);

        juce::Desktop::getInstance().addFocusChangeListener(&globalWatcher);
        setSize(900, 600);
    }

    // Гарантирует, что OUT CONTROL всегда последняя
    void ensureOutControlLast()
    {
        auto names = tabs.getTabNames();
        int outIndex = names.indexOf("OUTPUT");

        if (outIndex >= 0 && outIndex != tabs.getNumTabs() - 1)
            tabs.moveTab(outIndex, tabs.getNumTabs() - 1);
    }
    void ensureRigControlActive()
    {
        auto names = tabs.getTabNames();
        int rigIndex = names.indexOf("RIG CONTROL");

        if (rigIndex >= 0 && tabs.getCurrentTabIndex() != rigIndex)
            tabs.setCurrentTabIndex(rigIndex, true);
    }

    // При первом показе окна — активируем RIG CONTROL
    void parentHierarchyChanged()
    {
        if (isShowing())
        {
            ensureOutControlLast();
            ensureRigControlActive(); // если нужно, чтобы RIG CONTROL была активной
        }
    }
    MainContentComponent::~MainContentComponent()
    {
        if (auto* f = Component::getCurrentlyFocusedComponent())
            f->giveAwayKeyboardFocus();

        if (vstHostComponent)
            vstHostComponent->onBpmChanged = nullptr;

        if (looperEngine)
            looperEngine->onPlayerStateChanged = nullptr;

        if (vstHostComponent)
        {
            if (rigTuner)       vstHostComponent->removeTuner(rigTuner.get());
            if (tunerComponent) vstHostComponent->removeTuner(tunerComponent.get());
        }

        juce::Desktop::getInstance().removeFocusChangeListener(&globalWatcher);
        tabs.setLookAndFeel(nullptr);
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);

        deviceManager.removeAudioCallback(&audioSourcePlayer);
        audioSourcePlayer.setSource(nullptr);

        if (metronomeComponent)
            metronomeComponent->stop();

        metronomeButton.setLookAndFeel(nullptr);
        metronomeIcon.reset();
            
    }

    VSTHostComponent& getVstHostComponent() noexcept { return *vstHostComponent; }

    void resized() override
    {
        auto area = getLocalBounds();           // Вся доступная область компонента

        // === Настройки размеров и отступов ===
        const int buttonWidth = 100;       // ширина обычных кнопок
        const int buttonHeight = 28;       // высота обычных кнопок
        const int margin = 2;              // отступ между элементами

        // === Гибкие сдвиги ===
        const int shiftIN_X = 3;
        const int shiftIN_Y = 5;
        const int shiftOUT_X = -3;
        const int shiftOUT_Y = 5;
        const int shiftBlock_X = 90;
        const int shiftBlock_Y = 7;
        const int cpuWidth = 100;          // ширина CPU индикатора
        const int tempoWidth = 100;        // ширина TEMPO дисплея
        const int blockHeight = 30;        // высота блока CPU/TEMPO

        // === Блок CPU + TEMPO ===
        auto blockArea = area;
        blockArea.reduce(buttonWidth + margin, 0); // Освобождаем боковые зоны под кнопки

        // CPU индикатор
        auto cpuBounds = blockArea.removeFromRight(cpuWidth + margin).removeFromTop(blockHeight);
        cpuIndicator->setBounds(cpuBounds.translated(shiftBlock_X, shiftBlock_Y));

        // TEMPO дисплей
        auto tempoBounds = blockArea.removeFromRight(tempoWidth + margin).removeFromTop(blockHeight);
        tapTempoDisplay.setBounds(tempoBounds.translated(shiftBlock_X, shiftBlock_Y));

        // 🔹 Кнопка метронома — делаем короче и выше
        const int metronomeWidth = 50;
        const int metronomeHeight = 30;

        auto toggleBounds = blockArea.removeFromRight(metronomeWidth + margin).removeFromTop(blockHeight);

        // Центрируем по вертикали
        int yOffset = (blockHeight - metronomeHeight) / 2;
        metronomeButton.setBounds(toggleBounds.withHeight(metronomeHeight).translated(shiftBlock_X, shiftBlock_Y + yOffset));

        // === Вкладки ===
        tabs.setBounds(area); // ← занимают всю оставшуюся область
    }

    void MainContentComponent::setLooperTabVisibleFromButton(bool shouldShow)
    {
        looperSyncInternal = true;
        setLooperTabVisible(shouldShow);
        looperSyncInternal = false;
    }
    void MainContentComponent::setLooperTabVisible(bool shouldShow)
    {
        if (shouldShow && !looperTabVisible)
        {
            // ищем индекс вкладки OUTPUT
            int outIndex = getTabIndexByName("OUTPUT");
            if (outIndex < 0) outIndex = tabs.getNumTabs(); // если OUTPUT нет, добавляем в конец

            // добавляем LOOPER перед OUTPUT
            tabs.addTab("LOOPER",
                juce::Colour::fromRGBA(50, 62, 68, 255),
                looperComponent.get(),
                false,
                outIndex);

            looperTabVisible = true;
            ensureRigControlActive();

            // синхронизация LooperEngine
            if (!looperSyncInternal && rigControl)
            {
                rigControl->setLooperState(true);
                rigControl->syncLooperStateToMidi();
            }
        }
        else if (!shouldShow && looperTabVisible)
        {
            if (auto i = findTabIndexFor(looperComponent.get()); i >= 0)
                tabs.removeTab(i);

            looperTabVisible = false;
            selectBestTabAfterChange();

            if (!looperSyncInternal && rigControl)
                rigControl->setLooperState(false);
        }
    }
    void setTunerTabVisible(bool shouldShow)
    {
        if (shouldShow && !tunerTabVisible)
        {
            tabs.addTab("TUNER", Colour::fromRGBA(50, 62, 68, 255), tunerComponent.get(), false);
            vstHostComponent->addTuner(tunerComponent.get());
            tunerTabVisible = true;
            ensureRigControlActive();
        }
        else if (!shouldShow && tunerTabVisible)
        {
            vstHostComponent->removeTuner(tunerComponent.get());
            if (auto i = findTabIndexFor(tunerComponent.get()); i >= 0)
            tabs.removeTab(i);
            tunerTabVisible = false;
            selectBestTabAfterChange(); // вместо tabs.setCurrentTabIndex(0, ...)
        }
        ensureOutControlLast();
        updateTunerRouting();
    }
    void setTunerStyleClassic(bool isClassic)
    {
        if (tunerComponent)
            tunerComponent->setVisualStyle(isClassic
                ? TunerComponent::TunerVisualStyle::Classic
                : TunerComponent::TunerVisualStyle::Triangles);

        if (rigTuner)
            rigTuner->setVisualStyle(isClassic
                ? TunerComponent::TunerVisualStyle::Classic
                : TunerComponent::TunerVisualStyle::Triangles);
    }
    void activateTunerTabIfVisible()
    {
        int tunerIndex = getTabIndexByName("TUNER");
        if (tunerIndex >= 0 && tabs.getCurrentTabIndex() != tunerIndex)
            tabs.setCurrentTabIndex(tunerIndex, true);
    }

    void closeTunerTabIfVisible()
    {
        int tunerIndex = getTabIndexByName("TUNER");
        if (tunerIndex >= 0)
        {
            tabs.removeTab(tunerIndex);
            tunerTabVisible = false;
            ensureRigControlActive();
        }
    }
    juce::AudioDeviceManager& getAudioDeviceManager() noexcept
    {
        return deviceManager;
    }
    InputControlComponent* getInputControlComponent() noexcept
    {
        return inputControlComponent.get();
    }
    BankEditor* getBankEditor() noexcept
    {
        return bankEditor.get();
    }
    Metronome* getMetronomeComponent() 
    {
        return metronomeComponent.get();
    }
    //
    bool autoSwitchToRig = false;
    void setAutoSwitchToRig(bool enabled) noexcept { autoSwitchToRig = enabled; }
    bool isAutoSwitchToRigEnabled() const noexcept { return autoSwitchToRig; }

    juce::TabbedComponent& getTabs()
    {
        return tabs; // tabs — твой основной TabbedComponent
    }
    void applySavedTabColours()
    {
        juce::PropertiesFile props = TabColourSettingsWindow::getPropsOptions();

        for (int i = 0; i < tabs.getNumTabs(); ++i)
        {
            auto name = tabs.getTabNames()[i];
            auto val = props.getValue("Tab_" + name + "_Colour");
            if (val.isNotEmpty())
            {
                auto parts = juce::StringArray::fromTokens(val, ",", "");
                if (parts.size() == 4) // 🔹 теперь читаем RGBA
                {
                    auto colour = juce::Colour::fromRGBA(
                        (uint8_t)parts[0].getIntValue(),
                        (uint8_t)parts[1].getIntValue(),
                        (uint8_t)parts[2].getIntValue(),
                        (uint8_t)parts[3].getIntValue()
                    );
                    tabs.setTabBackgroundColour(i, colour);
                }
            }
        }
    }
    Rig_control* getRigControl() const { return rigControl.get(); }

private:

    void updateTunerRouting()
    {
        vstHostComponent->removeTuner(tunerComponent.get());
        vstHostComponent->removeTuner(rigTuner.get());

        auto* current = tabs.getTabContentComponent(tabs.getCurrentTabIndex());
        if (current == tunerComponent.get())
            vstHostComponent->addTuner(tunerComponent.get());
        else if (rigControl->isTunerVisible())
            vstHostComponent->addTuner(rigTuner.get());
    }
    int findTabIndexFor(Component* comp) const
    {
        for (int i = 0; i < tabs.getNumTabs(); ++i)
            if (tabs.getTabContentComponent(i) == comp)
                return i;
        return -1;
    }
    int getTabIndexByName(const juce::String& name) const
    {
        auto names = tabs.getTabNames();
        return names.indexOf(name);
    }

    // Если удаляем/скрываем вкладку, держим/возвращаем RIG CONTROL
    void selectBestTabAfterChange()
    {
        // Если RIG CONTROL есть — активируем её
        int rigIndex = getTabIndexByName("RIG CONTROL");
        if (rigIndex >= 0)
        {
            tabs.setCurrentTabIndex(rigIndex, true);
            return;
        }

        // Иначе — если остались вкладки, остаёмся на текущей, либо на 0
        if (tabs.getNumTabs() > 0)
        {
            int cur = tabs.getCurrentTabIndex();
            if (cur < 0 || cur >= tabs.getNumTabs())
                tabs.setCurrentTabIndex(juce::jmin(0, tabs.getNumTabs() - 1), true);
        }
    }

    AudioDeviceManager                       deviceManager;
    PluginManager&                           pluginManager;
    std::unique_ptr<AudioMidiSettingsDialog> audioSettingsDialog;
    std::unique_ptr<VSTHostComponent>        vstHostComponent;
    std::unique_ptr<BankEditor>              bankEditor;
    std::unique_ptr<Rig_control>             rigControl;
    std::unique_ptr<LooperEngine>            looperEngine;
    std::unique_ptr<LooperComponent>         looperComponent;
    std::unique_ptr<TunerComponent>          tunerComponent, rigTuner;
    std::unique_ptr<OutControlComponent>     outControlComponent;
    std::unique_ptr<CpuLoadIndicator>        cpuIndicator;
    std::unique_ptr<InputControlComponent>   inputControlComponent;
    std::unique_ptr<StartupShutdown>         Start;
   
    // 🔹 Метроном
    std::unique_ptr<Metronome> metronomeComponent;
    juce::AudioSourcePlayer audioSourcePlayer;
    juce::TextButton metronomeButton;
    std::unique_ptr<juce::DrawableImage> metronomeIcon;
    std::unique_ptr<MetronomeWindow> metronomeWindow;
    MetronomeLookAndFeel metronomeLAF;
    BpmButtonLookAndFeel bpmLAF;
    GlobalTextInputWatcher globalWatcher;
    IconTextTabLookAndFeel inputTabLF;
    juce::TextButton tapTempoDisplay;


    CallbackTabbedComponent                  tabs;
    int    currentBlockSize = 0;
    double currentSampleRate = 0.0;
    bool                                      hostIsDriving = false;
    bool                                      looperTabVisible = false;
    bool                                      tunerTabVisible = false;
    bool                                      outControlTabVisible = false;
    bool                                      inControlTabVisible = false;
    bool looperSyncInternal = false;
    juce::Value rmsValue;
    juce::Value confValue;
    int lastTabIndex = -1;

   JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};