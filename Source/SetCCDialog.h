#pragma once
#include <JuceHeader.h>        // всё из JUCE
#include "vst_host.h"          // реальный хост-класс с getPluginInstance()
#include <functional>          // для std::function
#include <array>               // для CCMapArray, если он тут нужен

// Форвардное объявление
class VSTHostComponent;

// Простая структура–модель одного CC-слота
struct CCMapping
{
    int     paramIndex = -1;    // индекс параметра в плагине (-1 = none)
    bool    enabled = false; // включён или нет
    uint8_t ccValue = 64;    // 0…127
    bool    invert = false; // НОВОЕ поле: инвертировать значение
};

// Массив из 10 маппингов, по одному на каждую CC-кнопку
using CCMapArray = std::array<CCMapping, 10>;

//==============================================================================
// SetCCDialog — редактирование одного слота CC: выбор параметра, уровень, ON/OFF и Invert.
// Теперь окно отображает в заголовке (и внутри) имя редактируемой кнопки.
//==============================================================================
class SetCCDialog : public juce::DialogWindow
{
public:
    /**
       @param host       — указатель на VSTHostComponent (плагин-хост)
       @param initial    — текущее состояние CCMapping (paramIndex, enabled, ccValue, invert)
       @param slotName   — название редактируемой кнопки, например "Set CC1"
       @param onFinished — колбэк (mapping, ок/отмена)
    */
    SetCCDialog(VSTHostComponent* host,
        CCMapping initial,
        const juce::String& slotName,
        std::function<void(CCMapping, bool)> onFinished)
        : DialogWindow(slotName, juce::Colours::darkgrey, true),
        host(host),
        mapping(initial),
        callback(std::move(onFinished))
    {
        // Устанавливаем заголовок для окна (отображается также внутри окна)
        titleLabel.setText(slotName, juce::dontSendNotification);
        titleLabel.setJustificationType(juce::Justification::centred);
        titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        addAndMakeVisible(&titleLabel);

        // 1) ComboBox со списком параметров плагина
        if (auto* plugin = host->getPluginInstance())
        {
            paramBox.addItem("<none>", 1);
            for (int i = 0; i < plugin->getNumParameters(); ++i)
                paramBox.addItem(plugin->getParameterName(i), i + 2);

            if (mapping.paramIndex >= 0 && mapping.paramIndex < plugin->getNumParameters())
                paramBox.setSelectedId(mapping.paramIndex + 2);
            else
                paramBox.setSelectedId(1);
        }
        else
        {
            paramBox.addItem("<none>", 1);
            paramBox.setSelectedId(1);
        }
        addAndMakeVisible(&paramBox);

        // 2) Чекбокс включения/отключения слота
        enableToggle.setButtonText("Enabled");
        enableToggle.setToggleState(mapping.enabled, juce::dontSendNotification);
        addAndMakeVisible(&enableToggle);

        // 2.1) Новый чекбокс Invert
        invertToggle.setButtonText("Invert");
        invertToggle.setToggleState(mapping.invert, juce::dontSendNotification);
        addAndMakeVisible(&invertToggle);

        // 3) Слайдер для уровня CC-value
        levelSlider.setRange(0, 127, 1);
        levelSlider.setValue(mapping.ccValue);
        levelSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 50, 20);
        addAndMakeVisible(&levelSlider);

        // 4) Кнопки OK / Cancel
        okButton.setButtonText("OK");
        cancelButton.setButtonText("Cancel");
        okButton.onClick = [this] { finish(true); };
        cancelButton.onClick = [this] { finish(false); };
        addAndMakeVisible(&okButton);
        addAndMakeVisible(&cancelButton);

        setResizable(false, false);
        centreWithSize(400, 280); // увеличенный размер для учёта дополнительного чекбокса

        // Делаем окно видимым и модальным
        addToDesktop();        // требуется для top-level окна
        setVisible(true);
        toFront(true);
        enterModalState(true, nullptr);
    }

    // Переопределяем closeButtonPressed, чтобы окно само удалялось
    void closeButtonPressed() override
    {
        DialogWindow::closeButtonPressed();
        delete this;
    }

    ~SetCCDialog() override = default;

    // Раскладка элементов окна
    void resized() override
    {
        // Получаем клиентскую область и задаём небольшой отступ (например, 5 пикселей)
        auto r = getLocalBounds().reduced(5);

        // Заголовок в самом верху: высота 40 пикселей
        titleLabel.setBounds(5, -8, r.getWidth() - 10, 40);
        r.removeFromTop(40);

        // Например, для ComboBox — 30 пикселей высоты:
        paramBox.setBounds(r.removeFromTop(30));

        // Toggle кнопка "Enabled" — 25 пикселей
        enableToggle.setBounds(r.removeFromTop(25));

        // Новый чекбокс "Invert" — также примерно 25 пикселей:
        invertToggle.setBounds(r.removeFromTop(25));

        // Слайдер — 50 пикселей:
        levelSlider.setBounds(r.removeFromTop(50));

        // Размещение кнопок OK/Cancel в нижней части окна (30 пикселей по высоте)
        auto btnArea = r.removeFromBottom(30).withTrimmedLeft(r.getWidth() / 4);
        okButton.setBounds(btnArea.removeFromLeft(btnArea.getWidth() / 2).reduced(5));
        cancelButton.setBounds(btnArea.reduced(5));
    }

private:
    VSTHostComponent* host;
    CCMapping mapping;
    std::function<void(CCMapping, bool)> callback;

    // Новый Label для отображения названия редактируемой кнопки
    juce::Label titleLabel;

    juce::ComboBox     paramBox;
    juce::ToggleButton enableToggle;
    juce::ToggleButton invertToggle;  // НОВЫЙ элемент для чекбокса "Invert"
    juce::Slider       levelSlider;
    juce::TextButton   okButton, cancelButton;

    void finish(bool isOk)
    {
        int sel = paramBox.getSelectedId();
        mapping.paramIndex = (sel >= 2 ? sel - 2 : -1);
        mapping.enabled = enableToggle.getToggleState();
        mapping.ccValue = static_cast<uint8_t>(levelSlider.getValue());
        mapping.invert = invertToggle.getToggleState();  // сохраняем состояние чекбокса-инверсии

        if (callback)
            callback(mapping, isOk);
        closeButtonPressed();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SetCCDialog)
};
