#pragma once

#include <JuceHeader.h>

class VSTHostComponent : public juce::Component,
    public juce::ListBoxModel,
    public juce::Button::Listener
{
public:
    VSTHostComponent()
    {
        // Инициализируем менеджер форматов плагинов (например, VST, AU, AAX и т.д.)
        formatManager.addDefaultFormats();

        // Задаём директорию для поиска плагинов.
#ifdef JUCE_WINDOWS
        juce::File pluginDirectory("C:\\Program Files\\Common Files\\VST3\\");
#else
        juce::File pluginDirectory("C:\\Program Files\\Common Files\\VST3\\");
#endif
        if (pluginDirectory.exists())
        {
            // На Windows ищем файлы с расширением .dll; на macOS - можно искать .vst или .component
            pluginDirectory.findChildFiles(pluginFiles, juce::File::findFiles, true, "*.vst3");
        }

        // Настройка списка плагинов (ListBox)
        addAndMakeVisible(pluginListBox);
        pluginListBox.setModel(this);

        // Кнопка «Load Plugin»
        loadButton.setButtonText("Load Plugin");
        loadButton.addListener(this);
        addAndMakeVisible(loadButton);

        // Кнопка «Close Plugin» – по умолчанию скрыта
        closeButton.setButtonText("Close Plugin");
        closeButton.addListener(this);
        addAndMakeVisible(closeButton);
        closeButton.setVisible(false);
    }

    ~VSTHostComponent() override
    {
        unloadPlugin();
    }

    // --- Реализация ListBoxModel (для отображения списка найденных плагинов) ---
    int getNumRows() override
    {
        return pluginFiles.size();
    }

    void paintListBoxItem(int rowNumber, juce::Graphics& g,
        int width, int height, bool rowIsSelected) override
    {
        if (rowNumber < pluginFiles.size())
        {
            if (rowIsSelected)
                g.fillAll(juce::Colours::lightblue);
            g.setColour(juce::Colours::black);
            g.drawText(pluginFiles[rowNumber].getFileName(),
                2, 0, width - 4, height, juce::Justification::centredLeft);
        }
    }

    void listBoxItemClicked(int row, const juce::MouseEvent&) override
    {
        selectedIndex = row;
    }

    // --- Обработка нажатий на кнопки ---
    void buttonClicked(juce::Button* button) override
    {
        if (button == &loadButton)
        {
            if (selectedIndex >= 0 && selectedIndex < pluginFiles.size())
                loadPlugin(pluginFiles[selectedIndex]);
        }
        else if (button == &closeButton)
        {
            unloadPlugin();
            repaint();
        }
    }

    void resized() override
    {
        auto r = getLocalBounds();

        if (pluginInstance != nullptr && pluginEditor != nullptr)
        {
            // Если плагин загружен – верхняя область отведена под кнопку закрытия,
            // а редактор занимает оставшееся пространство
            auto topBar = r.removeFromTop(30);
            closeButton.setBounds(topBar.reduced(4));
            pluginEditor->setBounds(r.reduced(4));
        }
        else
        {
            // В противном случае отображаем список с кнопкой загрузки снизу
            auto listArea = r.removeFromTop(r.getHeight() - 40);
            pluginListBox.setBounds(listArea.reduced(4));
            loadButton.setBounds(r.reduced(4));
        }
    }

private:
    // Загружает плагин из выбранного файла
    void loadPlugin(const juce::File& pluginFile)
    {
        unloadPlugin(); // Если уже что-то загружено – выгружаем

        juce::AudioPluginFormat* chosenFormat = nullptr;
        juce::OwnedArray<juce::PluginDescription> pluginDescs;

        // Перебираем все доступные форматы для поиска описания плагина в файле
        for (auto* format : formatManager.getFormats())
        {
            format->findAllTypesForFile(pluginDescs, pluginFile.getFullPathName());
            if (pluginDescs.size() > 0)
            {
                chosenFormat = format;
                break;
            }
        }

        if (chosenFormat == nullptr || pluginDescs.size() == 0)
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                "Error", "No plugin description found in the file.");
            return;
        }

        // Используем первое найденное описание
        juce::PluginDescription desc = *pluginDescs[0];
        juce::String error;

        // Вместо использования reset(), делаем простое присвоение.
        pluginInstance = chosenFormat->createInstanceFromDescription(desc, 44100.0, 512, error);

        if (pluginInstance == nullptr)
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                "Error", "Failed to create plugin instance:\n" + error);
            return;
        }

        // Если плагин имеет графический редактор, создаем его
        pluginEditor = pluginInstance->createEditor();
        if (pluginEditor != nullptr)
        {
            addAndMakeVisible(pluginEditor);
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                "Info", "Plugin does not provide an editor.");
        }

        // Меняем интерфейс: скрываем менеджер плагинов и кнопку загрузки, показываем кнопку закрытия
        pluginListBox.setVisible(false);
        loadButton.setVisible(false);
        closeButton.setVisible(true);

        resized();
    }


    // Выгружает плагин и возвращает менеджер плагинов
    void unloadPlugin()
    {
        if (pluginEditor != nullptr)
        {
            removeChildComponent(pluginEditor);
            delete pluginEditor;
            pluginEditor = nullptr;
        }
        pluginInstance.reset();

        pluginListBox.setVisible(true);
        loadButton.setVisible(true);
        closeButton.setVisible(false);
    }

    juce::AudioPluginFormatManager formatManager;
    juce::ListBox pluginListBox;
    juce::TextButton loadButton;
    juce::TextButton closeButton;
    juce::Array<juce::File> pluginFiles;
    int selectedIndex = -1;

    std::unique_ptr<juce::AudioPluginInstance> pluginInstance;
    juce::Component* pluginEditor = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VSTHostComponent)
};
