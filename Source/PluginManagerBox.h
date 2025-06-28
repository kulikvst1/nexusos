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

        // — создаём кнопки —
        initBtn(scanButton, "Scan");
        initBtn(addFolderButton, "Add Folder");
        initBtn(removeFolderButton, "Remove Selected");
        initBtn(selectAllButton, "Select All");
        initBtn(deselectAllButton, "Deselect All");

        // — список путей —
        addAndMakeVisible(pathsList);
        pathsList.setModel(this);
        pathsList.setColour(juce::ListBox::backgroundColourId, juce::Colours::darkgrey);
        pathsList.updateContent();

        // — таблица плагинов —
        addAndMakeVisible(pluginsTable);
        {
            auto& h = pluginsTable.getHeader();
            h.addColumn("Enabled", 1, 60);
            h.addColumn("Name", 2, 200);
            h.addColumn("Path", 3, 400);
        }
        pluginsTable.setModel(&pluginsModel);
        pluginsTable.setColour(juce::TableListBox::backgroundColourId, juce::Colours::darkgrey);

        // — статусный лейбл —
        addAndMakeVisible(statusLabel);
        statusLabel.setText("Scanning...", juce::dontSendNotification);
        statusLabel.setJustificationType(juce::Justification::centred);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        statusLabel.setVisible(false);

        // — слушаем sendChangeMessage() от менеджера —
        manager.addChangeListener(this);
    }

    ~PluginManagerBox() override
    {
        manager.removeChangeListener(this);
        scanWindow.reset();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::darkgrey);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);

        // — строка из 5 кнопок равной ширины —
        auto bar = area.removeFromTop(28);
        int w = bar.getWidth() / 5;
        for (auto* b : { &scanButton, &addFolderButton, &removeFolderButton,
                         &selectAllButton, &deselectAllButton })
            b->setBounds(bar.removeFromLeft(w).reduced(2));

        area.removeFromTop(6);
        pathsList.setBounds(area.removeFromTop(80));
        area.removeFromTop(6);
        pluginsTable.setBounds(area);
    }

    // — список путей (ListBoxModel) —
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
        g.drawText(f.getFullPathName(), 4, 0, w - 4, h, juce::Justification::centredLeft);
    }

private:
    // универсальный инициализатор TextButton
    void initBtn(juce::TextButton& b, const juce::String& txt)
    {
        addAndMakeVisible(b);
        b.setButtonText(txt);
        b.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        b.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        b.addListener(this);
    }

    // модель для TableListBox
    struct PluginsModel : juce::TableListBoxModel
    {
        PluginsModel(PluginManager& pm) : manager(pm) {}

        int getNumRows() override
        {
            return manager.getPlugins().size();
        }

        void paintRowBackground(juce::Graphics& g,
            int row, int w, int h, bool isSelected) override
        {
            g.fillAll(isSelected
                ? juce::Colours::darkslategrey
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
            if (colId == 3)
                g.drawText(e.desc.fileOrIdentifier, 4, 0, w - 4, h, juce::Justification::centredLeft);
        }

        Component* refreshComponentForCell(int row, int colId,
            bool, Component* existing) override
        {
            if (colId != 1) return nullptr;
            auto* btn = static_cast<juce::ToggleButton*>(existing);
            if (!btn) btn = new juce::ToggleButton();

            btn->setToggleState(
                manager.getPlugins().getReference(row).enabled,
                juce::dontSendNotification);
            btn->setColour(juce::ToggleButton::tickColourId, juce::Colours::white);

            btn->onClick = [this, row, btn]
                {
                    manager.setPluginEnabled(row, btn->getToggleState());
                    auto file = juce::File::getSpecialLocation(
                        juce::File::userApplicationDataDirectory)
                        .getChildFile("PluginSettings.xml");
                    manager.saveToFile(file);
                };
            return btn;
        }

        PluginManager& manager;
    };

    // нажатия по кнопкам
    void PluginManagerBox::buttonClicked(juce::Button* b) override
    {
        auto settingsFile = juce::File::getSpecialLocation(
            juce::File::userApplicationDataDirectory)
            .getChildFile("PluginSettings.xml");

        if (b == &scanButton)
        {
            statusLabel.setVisible(true);
            scanButton.setEnabled(false);
            addFolderButton.setEnabled(false);
            removeFolderButton.setEnabled(false);

            scanWindow = std::make_unique<juce::DialogWindow>(
                "Scanning Plugins...",         // ASCII-only title
                juce::Colours::darkgrey,0
                
            );
            scanWindow->setUsingNativeTitleBar(false);
            scanWindow->setAlwaysOnTop(true);
            scanWindow->setResizable(false, false);

            auto* lbl = new juce::Label("scanLbl",
                "Please wait, scanning plugins...");  // ASCII-only text
            lbl->setJustificationType(juce::Justification::centred);
            scanWindow->setContentOwned(lbl, false);
            scanWindow->centreWithSize(300, 80);
            scanWindow->setVisible(true);

            manager.startScan();
        }
        else if (b == &addFolderButton)
        {
            auto chooser = std::make_shared<juce::FileChooser>(
                "Select plugin folder...",      // ASCII-only
                juce::File(),
                "*.vst3;*.dll"                  // ASCII-only filter
            );

            chooser->launchAsync(
                juce::FileBrowserComponent::canSelectDirectories,
                [this, chooser](const juce::FileChooser& fc)
                {
                    auto folder = fc.getResult();
                    if (folder.isDirectory())
                    {
                        manager.addSearchPath(folder);
                        pathsList.updateContent();

                        manager.saveToFile(
                            juce::File::getSpecialLocation(
                                juce::File::userApplicationDataDirectory)
                            .getChildFile("PluginSettings.xml"));
                    }
                });
        }
        else if (b == &removeFolderButton)
        {
            int idx = pathsList.getSelectedRow();
            if (idx >= 0)
            {
                manager.removeSearchPath(
                    manager.getSearchPaths().getReference(idx));
                pathsList.updateContent();
            }
        }
        else // Select All / Deselect All
        {
            bool enable = (b == &selectAllButton);
            for (int i = 0; i < manager.getPlugins().size(); ++i)
                manager.setPluginEnabled(i, enable);

            pluginsTable.updateContent();
            manager.saveToFile(settingsFile);
        }
    }


    // вызывается после того, как scan окончен или loadFromFile()
    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        // 1) закрываем окно прогресса
        if (scanWindow)
        {
            scanWindow->setVisible(false);
            scanWindow.reset();
        }

        // 2) возвращаем кнопки в рабочее состояние
        statusLabel.setVisible(false);
        scanButton.setEnabled(true);
        addFolderButton.setEnabled(true);
        removeFolderButton.setEnabled(true);

        // 3) обновляем UI
        pluginsTable.updateContent();
        pathsList.updateContent();
    }


    PluginManager& manager;
    std::unique_ptr<juce::DialogWindow> scanWindow;
    juce::ListBox                     pathsList{ "Scan Paths", this };
    PluginsModel                      pluginsModel;
    juce::TableListBox                pluginsTable{ "Plugins", &pluginsModel };
    juce::TextButton                  scanButton,
        addFolderButton,
        removeFolderButton,
        selectAllButton,
        deselectAllButton;
    juce::Label                       statusLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginManagerBox)
};
