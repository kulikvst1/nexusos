#pragma once

#include <JuceHeader.h>
#include "PluginManager.h"

class PluginManagerBox : public juce::Component,
    private juce::Button::Listener,
    private juce::ChangeListener,
    private juce::ListBoxModel
{
public:
    // теперь принимаем ссылку на уже существующий менеджер
    PluginManagerBox(PluginManager& sharedManager)
        : manager(sharedManager),
        pluginsModel(manager)
    {
        // просто UI — без loadFromFile/saveToFile/stopScan
        setOpaque(true);
        setSize(500, 600);

        // pathsList
        addAndMakeVisible(pathsList);
        pathsList.setModel(this);
        pathsList.setColour(juce::ListBox::backgroundColourId, juce::Colours::darkgrey);

        // pluginsTable
        addAndMakeVisible(pluginsTable);
        {
            auto& h = pluginsTable.getHeader();
            h.addColumn("Enabled", 1, 60);
            h.addColumn("Name", 2, 200);
            h.addColumn("Path", 3, 400);
        }
        pluginsTable.setModel(&pluginsModel);
        pluginsTable.setColour(juce::ListBox::backgroundColourId, juce::Colours::darkgrey);

        // statusLabel
        addAndMakeVisible(statusLabel);
        statusLabel.setText("Scanning...", juce::dontSendNotification);
        statusLabel.setJustificationType(juce::Justification::centred);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        statusLabel.setVisible(false);

        // кнопки
        auto setupBtn = [&](juce::TextButton& b)
            {
                addAndMakeVisible(b);
                b.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
                b.addListener(this);
            };
        setupBtn(scanButton);         scanButton.setButtonText("Scan");
        setupBtn(addFolderButton);    addFolderButton.setButtonText("Add Folder");
        setupBtn(removeFolderButton); removeFolderButton.setButtonText("Remove Selected");

        // подписка на изменение плагинов
        manager.addChangeListener(this);
    }

    ~PluginManagerBox() override
    {
        manager.removeChangeListener(this);
        // больше ничего не трогаем — остановка скана и сохранение XML
        // происходит в VSTHostComponent
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::darkgrey);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        auto top = area.removeFromTop(24);

        scanButton.setBounds(top.removeFromLeft(80).reduced(2));
        statusLabel.setBounds(top.removeFromLeft(100).reduced(2));
        addFolderButton.setBounds(top.removeFromLeft(100).reduced(2));
        removeFolderButton.setBounds(top.removeFromLeft(120).reduced(2));

        area.removeFromTop(8);
        pathsList.setBounds(area.removeFromTop(80));
        area.removeFromTop(8);
        pluginsTable.setBounds(area);
    }

    // ListBoxModel — для списка путей
    int getNumRows() override
    {
        return manager.getSearchPaths().size();
    }

    void paintListBoxItem(int row, juce::Graphics& g,
        int w, int h, bool isSelected) override
    {
        g.fillAll(isSelected ? juce::Colours::darkslategrey
            : juce::Colours::transparentBlack);
        g.setColour(juce::Colours::white);
        auto& f = manager.getSearchPaths().getReference(row);
        g.drawText(f.getFullPathName(),
            4, 0, w - 4, h,
            juce::Justification::centredLeft);
    }

private:
    // модель для таблицы плагинов
    struct PluginsModel : public juce::TableListBoxModel
    {
        PluginsModel(PluginManager& pm) : manager(pm) {}

        int getNumRows() override
        {
            return manager.getPlugins().size();
        }

        void paintRowBackground(juce::Graphics& g,
            int row, int w, int h, bool isSelected) override
        {
            g.fillAll(isSelected ? juce::Colours::darkslategrey
                : juce::Colours::transparentBlack);
        }

        void paintCell(juce::Graphics& g,
            int row, int colId,
            int w, int h, bool) override
        {
            g.setColour(juce::Colours::white);
            auto& e = manager.getPlugins().getReference(row);

            if (colId == 2)
                g.drawText(e.desc.name, 4, 0, w - 4, h, juce::Justification::centredLeft);
            else if (colId == 3)
                g.drawText(e.desc.fileOrIdentifier, 4, 0, w - 4, h, juce::Justification::centredLeft);
        }

        Component* refreshComponentForCell(int row, int colId,
            bool, Component* existing) override
        {
            if (colId == 1)
            {
                auto* btn = static_cast<juce::ToggleButton*> (existing);
                if (!btn) btn = new juce::ToggleButton();

                btn->setToggleState(manager.getPlugins().getReference(row).enabled,
                    juce::dontSendNotification);
                btn->setColour(juce::ToggleButton::textColourId, juce::Colours::white);
                btn->onClick = [this, row, btn]
                    {
                        manager.setPluginEnabled(row, btn->getToggleState());
                    };
                return btn;
            }

            return nullptr;
        }

        PluginManager& manager;
    };

    // обработчик кнопок
    void buttonClicked(juce::Button* b) override
    {
        if (b == &scanButton)
        {
            statusLabel.setVisible(true);
            scanButton.setEnabled(false);
            addFolderButton.setEnabled(false);
            removeFolderButton.setEnabled(false);
            manager.startScan();
        }
        else if (b == &addFolderButton)
        {
            auto chooser = std::make_shared<juce::FileChooser>(
                "Select plugin folder...", juce::File(), "*.vst3");

            chooser->launchAsync(juce::FileBrowserComponent::canSelectDirectories,
                [this, chooser](const juce::FileChooser& c)
                {
                    auto d = c.getResult();
                    if (d.isDirectory())
                    {
                        manager.addSearchPath(d);
                        pathsList.updateContent();
                    }
                });
        }
        else if (b == &removeFolderButton)
        {
            int idx = pathsList.getSelectedRow();
            if (idx >= 0)
            {
                auto& f = manager.getSearchPaths().getReference(idx);
                manager.removeSearchPath(f);
                pathsList.updateContent();
            }
        }
    }

    // ChangeListener — реакция на sendChangeMessage()
    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        statusLabel.setVisible(false);
        scanButton.setEnabled(true);
        addFolderButton.setEnabled(true);
        removeFolderButton.setEnabled(true);
        pluginsTable.updateContent();
    }

    PluginManager& manager;
    juce::ListBox       pathsList{ "Scan Paths", this };
    PluginsModel        pluginsModel;
    juce::TableListBox  pluginsTable{ "Plugins", &pluginsModel };
    juce::TextButton    scanButton{ "Scan" };
    juce::TextButton    addFolderButton{ "Add Folder" };
    juce::TextButton    removeFolderButton{ "Remove Selected" };
    juce::Label         statusLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginManagerBox)
};
