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
        setSize(800, 900); // ?? увеличили окно

        initBtn(scanButton, "Scan All");
        initBtn(scanNewButton, "Scan New");
        initBtn(addFolderButton, "Add Folder");
        initBtn(removeFolderButton, "Remove Selected");
        initBtn(selectAllButton, "Select All");
        initBtn(deselectAllButton, "Deselect All");

        // Paths list
        addAndMakeVisible(pathsList);
        pathsList.setModel(this);
      //  pathsList.setColour(juce::ListBox::backgroundColourId, juce::Colours::darkgrey);
        pathsList.setRowHeight(40); // ?? увеличили высоту строки
        pathsList.updateContent();

        // Plugins table
        addAndMakeVisible(pluginsTable);
        auto& h = pluginsTable.getHeader();
        h.addColumn("Enabled", 1, 120); // ?? шире колонка
        h.addColumn("Name", 2, 300);
        h.addColumn("Path", 3, 500);
        pluginsTable.setModel(&pluginsModel);
       // pluginsTable.setColour(juce::TableListBox::backgroundColourId, juce::Colours::darkgrey);
        pluginsTable.setRowHeight(48); // ?? увеличили высоту строки

        // Status label
        addAndMakeVisible(statusLabel);
        statusLabel.setText("Scanning...", juce::dontSendNotification);
        statusLabel.setJustificationType(juce::Justification::centred);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        statusLabel.setFont(juce::Font(24.0f)); // ?? увеличили шрифт
        statusLabel.setVisible(false);

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
        // Берём цвет, который назначен для backgroundColourId
        auto bg = findColour(juce::ResizableWindow::backgroundColourId);

        // Заливаем им весь компонент
        g.fillAll(bg);
    }


    void resized() override
    {
        auto area = getLocalBounds().reduced(12);
        auto bar = area.removeFromTop(80); // ?? увеличили панель кнопок
        int w = bar.getWidth() / 3;        // ?? по 3 кнопки в ряд

        for (auto* b : { &scanButton, &scanNewButton, &addFolderButton })
            b->setBounds(bar.removeFromLeft(w).reduced(6));

        auto bar2 = area.removeFromTop(80);
        int w2 = bar2.getWidth() / 3;
        for (auto* b : { &removeFolderButton, &selectAllButton, &deselectAllButton })
            b->setBounds(bar2.removeFromLeft(w2).reduced(6));

        area.removeFromTop(12);
        pathsList.setBounds(area.removeFromTop(200)); // ?? увеличили список путей
        area.removeFromTop(12);
        pluginsTable.setBounds(area); // ?? таблица занимает оставшееся место
    }

    // ListBoxModel for paths
    int getNumRows() override
    {
        return manager.getSearchPaths().size();
    }

    void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool isSelected) override
    {
        g.fillAll(isSelected ? juce::Colours::darkslategrey : juce::Colours::transparentBlack);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(20.0f)); // ?? увеличили шрифт
        const auto& f = manager.getSearchPaths().getReference(row);
        g.drawText(f.getFullPathName(), 10, 0, w - 20, h, juce::Justification::centredLeft);
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

        int getNumRows() override { return manager.getNumPlugins(); }

        void paintRowBackground(juce::Graphics& g, int row, int w, int h, bool isSelected) override
        {
            const auto& entry = manager.getPlugin(row);
            const bool isNew = manager.isPluginNew(entry.getUID());

            if (isSelected)
                g.fillAll(juce::Colours::darkslategrey);
            else if (isNew)
                g.fillAll(juce::Colour::fromRGB(0, 150, 100));
            else
                g.fillAll(juce::Colours::transparentBlack);
        }

        void paintCell(juce::Graphics& g, int row, int colId, int w, int h, bool) override
        {
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(18.0f));

            const auto& e = manager.getPlugin(row);

            if (colId == 2)
                g.drawText(e.getName(), 6, 0, w - 12, h, juce::Justification::centredLeft);
            else if (colId == 3)
                g.drawText(e.getPath(), 6, 0, w - 12, h, juce::Justification::centredLeft);
        }

        juce::Component* refreshComponentForCell(int row, int colId, bool, juce::Component* existing) override
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

                    juce::File settingsDir = juce::File::getSpecialLocation(
                        juce::File::userApplicationDataDirectory
                    ).getChildFile("NEXUS_KONTROL_OS");

                    if (!settingsDir.exists())
                        settingsDir.createDirectory();

                    juce::File settingsFile = settingsDir.getChildFile("PluginSettings.xml");
                    manager.saveToFile(settingsFile);
                };

            return btn;
        }

        PluginManager& manager;
    };

    void buttonClicked(juce::Button* b) override
    {
        juce::File settingsDir = juce::File::getSpecialLocation(
            juce::File::userApplicationDataDirectory
        ).getChildFile("NEXUS_KONTROL_OS");

        if (!settingsDir.exists())
            settingsDir.createDirectory();

        juce::File settingsFile = settingsDir.getChildFile("PluginSettings.xml");

        // Scan All
        if (b == &scanButton)
        {
            scanButton.setEnabled(false);
            addFolderButton.setEnabled(false);
            removeFolderButton.setEnabled(false);

            scanWindow = std::make_unique<juce::AlertWindow>(
                "Scanning All Plugins...",
                "Scanning all plugins, please wait...",
                juce::AlertWindow::NoIcon);

            // подключаем прогрессбар к менеджеру
            scanWindow->addProgressBarComponent(manager.getScanProgressRef());
            scanWindow->centreAroundComponent(this, 420, 140);
            scanWindow->setVisible(true);

            manager.startScanAsync(PluginManager::ScanMode::All, this);
        }
        // Scan New
        else if (b == &scanNewButton)
        {
            scanButton.setEnabled(false);
            scanNewButton.setEnabled(false);
            addFolderButton.setEnabled(false);
            removeFolderButton.setEnabled(false);

            scanWindow = std::make_unique<juce::AlertWindow>(
                "Scanning New Plugins...",
                "Scanning only new plugins, please wait...",
                juce::AlertWindow::NoIcon);

            // подключаем прогрессбар к менеджеру
            scanWindow->addProgressBarComponent(manager.getScanProgressRef());
            scanWindow->centreAroundComponent(this, 420, 140);
            scanWindow->setVisible(true);

            manager.startScanAsync(PluginManager::ScanMode::New, this);
        }
        // Add Folder
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
        // Remove Folder
        else if (b == &removeFolderButton)
        {
            const int pathIdx = pathsList.getSelectedRow();
            const int pluginIdx = pluginsTable.getSelectedRow();

            if (pathIdx >= 0 || pluginIdx >= 0)
            {
                juce::AlertWindow::showOkCancelBox(
                    juce::AlertWindow::WarningIcon,
                    "Delete Confirmation",
                    (pathIdx >= 0
                        ? "Are you sure you want to delete the selected path?"
                        : "Are you sure you want to delete the selected plugin?"),
                    "Yes",
                    "Cancel",
                    nullptr,
                    juce::ModalCallbackFunction::create([this, pathIdx, pluginIdx](int result)
                        {
                            if (result == 1)
                            {
                                if (pathIdx >= 0)
                                {
                                    auto folder = manager.getSearchPaths().getReference(pathIdx);
                                    manager.removeSearchPath(folder);
                                    pathsList.updateContent();
                                }
                                else if (pluginIdx >= 0)
                                {
                                    manager.removePlugin(pluginIdx);
                                    pluginsTable.updateContent();
                                }

                                manager.saveToFile(manager.getSettingsFile());
                            }
                        })
                );
            }
        }


        // Select/Deselect All
        else if (b == &selectAllButton || b == &deselectAllButton)
        {
            const bool enable = (b == &selectAllButton);
            for (int i = 0; i < manager.getNumPlugins(); ++i)
                manager.setPluginEnabled(i, enable);

            pluginsTable.updateContent();
            manager.saveToFile(settingsFile);
        }
    }

    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        pluginsTable.updateContent();
        pathsList.updateContent();

        if (manager.isScanFinished())
        {
            if (scanWindow)
            {
                scanWindow->setVisible(false);
                scanWindow.reset();
            }

            statusLabel.setVisible(false);
            scanButton.setEnabled(true);
            scanNewButton.setEnabled(true);
            addFolderButton.setEnabled(true);
            removeFolderButton.setEnabled(true);
        }
    }

    // Fields
    PluginManager& manager;
    PluginsModel pluginsModel;

    juce::ListBox pathsList{ "Scan Paths", this };
    juce::TableListBox pluginsTable{ "Plugins", &pluginsModel };

    juce::TextButton scanButton, addFolderButton, removeFolderButton,
        selectAllButton, deselectAllButton, scanNewButton;
    juce::Label statusLabel;

    std::unique_ptr<juce::AlertWindow> scanWindow;
    std::unique_ptr<juce::ProgressBar> progressBar;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginManagerBox)
};

