#pragma once
#include <JuceHeader.h>
#include <filesystem>

namespace fs = std::filesystem;

class FileManager : public juce::Component,
    public juce::FileBrowserListener
{
public:
    enum class Mode { General, Load, Save };

    FileManager(const juce::File& root, Mode mode)
        : currentMode(mode)
    {
        int flags = juce::FileBrowserComponent::canSelectDirectories;

        if (mode == Mode::Save)
            flags |= juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;
        else
            flags |= juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

        fileBrowser = std::make_unique<juce::FileBrowserComponent>(flags, root, nullptr, nullptr);
        fileBrowser->addListener(this);
        addAndMakeVisible(*fileBrowser);

        addAndMakeVisible(copyButton);
        addAndMakeVisible(pasteButton);
        addAndMakeVisible(deleteButton);
        addAndMakeVisible(runButton);
        addAndMakeVisible(okButton);

        copyButton.setButtonText("Copy");
        pasteButton.setButtonText("Paste");
        deleteButton.setButtonText("Delete");
        runButton.setButtonText("Run");
        okButton.setButtonText("OK");

        copyButton.onClick = [this] { copySelected(); };
        pasteButton.onClick = [this] { pasteFiles(); };
        deleteButton.onClick = [this] { deleteSelected(); };
        runButton.onClick = [this] { runSelected(); };

        okButton.onClick = [this]
            {
                juce::File file;

                if (currentMode == Mode::Save)
                {
                    file = getSelectedFile();
                    auto name = file.getFileName().trim();
                    if (!name.endsWithIgnoreCase(".xml"))
                        name += ".xml";
                    file = file.getParentDirectory().getChildFile(name);
                }
                else
                {
                    file = getSelectedFile();
                    if (!file.existsAsFile())
                        file = fileBrowser->getHighlightedFile();
                    if (currentMode == Mode::Load && !file.existsAsFile())
                        return;
                }

                if (onFileConfirmed)
                    onFileConfirmed(file);

                if (dialogWindow)
                    dialogWindow->exitModalState(0);
            };
    }

    void resized() override
    {
        auto area = getLocalBounds();
        auto buttonHeight = 28;
        auto buttonArea = area.removeFromBottom(buttonHeight);

        fileBrowser->setBounds(area);

        okButton.setVisible(currentMode != Mode::General);
        copyButton.setVisible(!minimalUI);
        pasteButton.setVisible(!minimalUI);
        deleteButton.setVisible(!minimalUI);
        runButton.setVisible(!minimalUI);

        juce::FlexBox flex;
        flex.flexDirection = juce::FlexBox::Direction::row;
        flex.justifyContent = juce::FlexBox::JustifyContent::center;
        flex.alignItems = juce::FlexBox::AlignItems::center;

        if (!minimalUI)
        {
            flex.items.add(juce::FlexItem(copyButton).withWidth(100).withHeight(buttonHeight));
            flex.items.add(juce::FlexItem(pasteButton).withWidth(100).withHeight(buttonHeight));
            flex.items.add(juce::FlexItem(deleteButton).withWidth(100).withHeight(buttonHeight));
            flex.items.add(juce::FlexItem(runButton).withWidth(100).withHeight(buttonHeight));
        }

        if (currentMode != Mode::General)
            flex.items.add(juce::FlexItem(okButton).withWidth(100).withHeight(buttonHeight));

        flex.performLayout(buttonArea);
    }

    // Public API
    void setMinimalUI(bool b) { minimalUI = b; }

    void setWildcardFilter(const juce::String& pattern)
    {
        wildcardFilter = std::make_unique<juce::WildcardFileFilter>(pattern, "*", "Filtered files");
        fileBrowser->setFileFilter(wildcardFilter.get());
    }

    void setConfirmCallback(std::function<void(const juce::File&)> cb) { onFileConfirmed = std::move(cb); }
    void setDialogWindow(juce::DialogWindow* dw) { dialogWindow = dw; }
    void refresh() { fileBrowser->refresh(); }

    juce::File getSelectedFile() const
    {
        if (fileBrowser->getNumSelectedFiles() > 0)
            return fileBrowser->getSelectedFile(0);
        return {};
    }

    juce::Array<juce::File> getSelectedFiles() const
    {
        juce::Array<juce::File> result;
        for (int i = 0; i < fileBrowser->getNumSelectedFiles(); ++i)
            result.add(fileBrowser->getSelectedFile(i));
        return result;
    }

private:
    Mode currentMode;
    bool minimalUI = false;

    std::unique_ptr<juce::WildcardFileFilter> wildcardFilter;
    std::unique_ptr<juce::FileBrowserComponent> fileBrowser;

    juce::TextButton copyButton, pasteButton, deleteButton, runButton, okButton;
    juce::Array<juce::File> clipboard;
    std::function<void(const juce::File&)> onFileConfirmed;
    juce::DialogWindow* dialogWindow = nullptr;

    // FileBrowserListener
    void selectionChanged() override {}
    void fileClicked(const juce::File&, const juce::MouseEvent&) override {}
    void fileDoubleClicked(const juce::File& file) override
    {
        if (currentMode == Mode::Load && onFileConfirmed && file.exists())
        {
            onFileConfirmed(file);
            if (dialogWindow)
                dialogWindow->exitModalState(0);
        }
    }
    void browserRootChanged(const juce::File&) override {}

    // Helpers
    static bool copyAny(const juce::File& source, const juce::File& target)
    {
        if (!source.exists())
            return false;

        if (source.isDirectory())
        {
            if (!target.createDirectory())
                return false;

            juce::DirectoryIterator it(source, false, "*", juce::File::findFilesAndDirectories);
            bool ok = true;
            while (it.next() && ok)
            {
                auto childSrc = it.getFile();
                auto childDst = target.getChildFile(childSrc.getFileName());
                ok = copyAny(childSrc, childDst);
            }
            return ok;
        }
        else
        {
            return source.copyFileTo(target);
        }
    }

    static juce::File resolveDestinationDir(juce::FileBrowserComponent* browser)
    {
        juce::File dest = browser->getHighlightedFile();
        if (dest.existsAsFile())
            dest = dest.getParentDirectory();
        if (!dest.isDirectory())
            dest = browser->getRoot();
        return dest;
    }

    // Actions
    void copySelected()
    {
        clipboard.clear();
        for (int i = 0; i < fileBrowser->getNumSelectedFiles(); ++i)
            clipboard.add(fileBrowser->getSelectedFile(i));
    }

    void pasteFiles()
    {
        if (clipboard.isEmpty())
            return;

        auto destDir = resolveDestinationDir(fileBrowser.get());
        auto* parentWindow = juce::TopLevelWindow::getActiveTopLevelWindow();

        for (auto& src : clipboard)
        {
            auto dst = destDir.getChildFile(src.getFileName());

            auto performCopy = [src, dst, parentWindow]()
                {
                    bool success = copyAny(src, dst);
                    if (!success)
                    {
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::AlertWindow::WarningIcon,
                            "Copy Failed",
                            "Could not copy:\n" + src.getFullPathName() +
                            "\n→ " + dst.getFullPathName(),
                            "OK",              // текст кнопки
                            parentWindow       // компонент-владелец
                        );

                    }
                };

            if (dst.exists())
            {
                juce::AlertWindow::showOkCancelBox(
                    juce::AlertWindow::WarningIcon,
                    "File Exists",
                    "An item named \"" + dst.getFileName() + "\" already exists.\n\nOverwrite?",
                    "Overwrite", "Cancel",
                    parentWindow,
                    juce::ModalCallbackFunction::create([performCopy](int result)
                        {
                            if (result == 1)
                                performCopy();
                        })
                );
            }
            else
            {
                performCopy();
            }
        }

        fileBrowser->refresh();
    }

    void deleteSelected()
    {
        auto selectedFiles = getSelectedFiles();
        if (selectedFiles.isEmpty())
            return;

        auto* parentWindow = juce::TopLevelWindow::getActiveTopLevelWindow();

        juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::WarningIcon,
            "Delete Confirmation",
            "Delete selected item(s)?",
            "Delete", "Cancel",
            parentWindow,
            juce::ModalCallbackFunction::create([this, selectedFiles](int result)
                {
                    if (result != 1)
                        return;

                    for (auto& f : selectedFiles)
                    {
                        const auto path = f.getFullPathName();
                        try
                        {
                            // Без ограничений: удаляем всё, что попросили
                            fs::remove_all(fs::u8path(path.toStdString()));
                        }
                        catch (const std::exception& e)
                        {
                            juce::AlertWindow::showMessageBoxAsync(
                                juce::AlertWindow::WarningIcon,
                                "Delete Failed",
                                "Could not delete:\n" + path + "\n\nReason:\n" + juce::String(e.what())
                            );
                        }
                    }

                    fileBrowser->refresh();
                })
        );
    }
    void runSelected()
    {
        for (int i = 0; i < fileBrowser->getNumSelectedFiles(); ++i)
        {
            auto f = fileBrowser->getSelectedFile(i);
            if (!f.exists()) continue;

            if (f.isDirectory())
            {
                f.revealToUser();
                continue;
            }

#if JUCE_WINDOWS
            if (f.hasFileExtension("exe") || f.hasFileExtension("msi") ||
                f.hasFileExtension("bat") || f.hasFileExtension("cmd"))
            {
                f.startAsProcess();
            }
            else
            {
                juce::URL(f).launchInDefaultBrowser();
            }

#elif JUCE_LINUX
            if (f.hasFileExtension("exe") || f.hasFileExtension("msi"))
            {
                juce::ChildProcess proc;
                juce::StringArray args{ "wine", f.getFullPathName() };

                if (!proc.start(args))
                {
                    DBG("Error: Failed to start Wine process!");
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::WarningIcon,
                        "Wine Error",
                        "Could not launch Wine. Please make sure Wine is installed and available in PATH."
                    );
                }
                else
                {
                    DBG("Launched with Wine: " + f.getFullPathName());
                    // Do not wait here, let the installer run independently
                    proc.waitForProcessToFinish(0);
                }
            }
            else
            {
                juce::URL(f).launchInDefaultBrowser();
            }

#elif JUCE_MAC
            juce::URL(f).launchInDefaultBrowser();
#endif
        }
    }




};
