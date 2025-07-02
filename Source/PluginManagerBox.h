#pragma once

#include <JuceHeader.h>
#include "PluginManager.h"

class PluginManagerBox
    : public juce::Component,
    private juce::Button::Listener,
    private juce::ChangeListener,
    private juce::ListBoxModel
{
public:
    explicit PluginManagerBox(PluginManager& sharedManager)
        : manager(sharedManager),
        pluginsModel(manager)
    {
        setOpaque(true);
        setSize(500, 600);

        // Кнопки
        initBtn(scanButton, "Scan All");
        initBtn(addFolderButton, "Add Folder");
        initBtn(removeFolderButton, "Remove Selected");
        initBtn(selectAllButton, "Select All");
        initBtn(deselectAllButton, "Deselect All");
        initBtn(scanNewButton, "Scan New");

        

        // Список путей
        addAndMakeVisible(pathsList);
        pathsList.setModel(this);
        pathsList.setColour(juce::ListBox::backgroundColourId, juce::Colours::darkgrey);
        pathsList.updateContent(); // Обязательно: актуализация UI после loadFromFile()

        // Таблица плагинов
        addAndMakeVisible(pluginsTable);
        auto& h = pluginsTable.getHeader();
        h.addColumn("Enabled", 1, 60);
        h.addColumn("Name", 2, 200);
        h.addColumn("Path", 3, 400);
        pluginsTable.setModel(&pluginsModel);
        pluginsTable.setColour(juce::TableListBox::backgroundColourId, juce::Colours::darkgrey);

        // Статус
        addAndMakeVisible(statusLabel);
        statusLabel.setText("Scanning...", juce::dontSendNotification);
        statusLabel.setJustificationType(juce::Justification::centred);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        statusLabel.setVisible(false);

        // Подписка на обновления
        manager.addChangeListener(this);
    }

    ~PluginManagerBox() override
    {
        manager.removeChangeListener(this);
        scanWindow.reset();
        manager.clearNewPluginFlags();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::darkgrey);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        auto bar = area.removeFromTop(28);
        int w = bar.getWidth() / 6;
        for (auto* b : { &scanButton, &scanNewButton, &addFolderButton,
                         &removeFolderButton, &selectAllButton, &deselectAllButton })
            b->setBounds(bar.removeFromLeft(w).reduced(2));


        area.removeFromTop(6);
        pathsList.setBounds(area.removeFromTop(80));
        area.removeFromTop(6);
        pluginsTable.setBounds(area);
    }

    // Путь списка
    int getNumRows() override
    {
        return manager.getSearchPaths().size();
    }

    void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool isSelected) override
    {
        g.fillAll(isSelected ? juce::Colours::darkslategrey : juce::Colours::transparentBlack);
        g.setColour(juce::Colours::white);
        const auto& f = manager.getSearchPaths().getReference(row);
        g.drawText(f.getFullPathName(), 4, 0, w - 4, h, juce::Justification::centredLeft);
    }

private:
    void initBtn(juce::TextButton& b, const juce::String& txt)
    {
        addAndMakeVisible(b);
        b.setButtonText(txt);
        b.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        b.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        b.addListener(this);
    }

    struct PluginsModel : juce::TableListBoxModel
    {
        PluginsModel(PluginManager& pm) : manager(pm) {}

        int getNumRows() override
        {
            return manager.getNumPlugins();
        }

        void paintRowBackground(juce::Graphics& g, int row, int w, int h, bool isSelected) override
        {
            const auto& entry = manager.getPlugin(row);
            bool isNew = manager.isPluginNew(entry.getUID());

            if (isSelected)
                g.fillAll(juce::Colours::darkslategrey);
            else if (isNew)
                g.fillAll(juce::Colour::fromRGB(000, 150, 100)); // светло-зелёный
            else
                g.fillAll(juce::Colours::transparentBlack);

        }

        void paintCell(juce::Graphics& g, int row, int colId, int w, int h, bool) override
        {
            g.setColour(juce::Colours::white);
            const auto& e = manager.getPlugin(row);

            if (colId == 2)
                g.drawText(e.getName(), 4, 0, w - 4, h, juce::Justification::centredLeft);
            else if (colId == 3)
                g.drawText(e.getPath(), 4, 0, w - 4, h, juce::Justification::centredLeft);
        }

        Component* refreshComponentForCell(int row, int colId, bool, Component* existing) override
        {
            if (colId != 1) return nullptr;

            auto* btn = static_cast<juce::ToggleButton*>(existing);
            if (!btn) btn = new juce::ToggleButton();

            const auto& entry = manager.getPlugin(row);
            btn->setToggleState(entry.enabled, juce::dontSendNotification);
            btn->setColour(juce::ToggleButton::tickColourId, juce::Colours::white);

            btn->onClick = [this, row, btn]
                {
                    manager.setPluginEnabled(row, btn->getToggleState());

                    auto file = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("PluginSettings.xml");

                    manager.saveToFile(file);
                };

            return btn;
        }

        PluginManager& manager;
    };

    void buttonClicked(juce::Button* b) override
    {
        auto settingsFile = juce::File::getSpecialLocation(
            juce::File::userApplicationDataDirectory).getChildFile("PluginSettings.xml");

        if (b == &scanButton)
        {
            statusLabel.setVisible(true);
            scanButton.setEnabled(false);
            addFolderButton.setEnabled(false);
            removeFolderButton.setEnabled(false);

            scanWindow = std::make_unique<juce::DialogWindow>("Scanning Plugins...",
                juce::Colours::darkgrey, false);
            scanWindow->setUsingNativeTitleBar(false);
            scanWindow->setAlwaysOnTop(true);
            scanWindow->setResizable(false, false);

            auto* lbl = new juce::Label("scanLbl", "Please wait, scanning plugins...");
            lbl->setJustificationType(juce::Justification::centred);
            scanWindow->setContentOwned(lbl, false);
            scanWindow->centreWithSize(300, 80);
            scanWindow->setVisible(true);

            manager.startScan();
        }
        else if (b == &scanNewButton)
        {
            statusLabel.setVisible(true);
            scanButton.setEnabled(false);
            scanNewButton.setEnabled(false);
            addFolderButton.setEnabled(false);
            removeFolderButton.setEnabled(false);

            scanWindow = std::make_unique<juce::DialogWindow>("Scanning New Plugins...",
                juce::Colours::darkgrey, false);
            scanWindow->setUsingNativeTitleBar(false);
            scanWindow->setAlwaysOnTop(true);
            scanWindow->setResizable(false, false);

            auto* lbl = new juce::Label("scanLbl", "Scanning only new plugins...");
            lbl->setJustificationType(juce::Justification::centred);
            scanWindow->setContentOwned(lbl, false);
            scanWindow->centreWithSize(300, 80);
            scanWindow->setVisible(true);
            manager.scanOnlyNew(); // 👈 добавим такой метод
        }

        else if (b == &addFolderButton)
        {
            auto chooser = std::make_shared<juce::FileChooser>(
                "Select plugin folder...", juce::File(), "*.vst3;*.dll");

            chooser->launchAsync(juce::FileBrowserComponent::canSelectDirectories,
                [this, chooser, settingsFile](const juce::FileChooser& fc)
                {
                    auto folder = fc.getResult();
                    if (folder.isDirectory())
                    {
                        manager.addSearchPath(folder);
                        pathsList.updateContent();
                        manager.saveToFile(settingsFile);
                    }
                });
        }
        else if (b == &removeFolderButton)
        {
            int idx = pathsList.getSelectedRow();
            if (idx >= 0)
            {
                manager.removeSearchPath(manager.getSearchPaths().getReference(idx));
                pathsList.updateContent();
            }
        }
        else
        {
            bool enable = (b == &selectAllButton);
            for (int i = 0; i < manager.getNumPlugins(); ++i)
                manager.setPluginEnabled(i, enable);

            pluginsTable.updateContent();

            manager.saveToFile(settingsFile);
        }

    }

    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        if (scanWindow)
        {
            scanWindow->setVisible(false);
            scanWindow.reset();
        }

        statusLabel.setVisible(false);
        scanButton.setEnabled(true);
        addFolderButton.setEnabled(true);
        removeFolderButton.setEnabled(true);

        pluginsTable.updateContent();
        pathsList.updateContent();
    }

    PluginManager& manager;
    PluginsModel pluginsModel;

    juce::ListBox pathsList{ "Scan Paths", this };
    juce::TableListBox pluginsTable{ "Plugins", &pluginsModel };

    juce::TextButton scanButton, addFolderButton, removeFolderButton, selectAllButton, deselectAllButton, scanNewButton;
    juce::Label statusLabel;
    std::unique_ptr<juce::DialogWindow> scanWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginManagerBox)
};
