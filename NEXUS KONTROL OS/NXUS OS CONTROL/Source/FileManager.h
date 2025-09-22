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
        {
            flags |= juce::FileBrowserComponent::saveMode
                | juce::FileBrowserComponent::canSelectFiles;
        }
        else
        {
            flags |= juce::FileBrowserComponent::openMode
                | juce::FileBrowserComponent::canSelectFiles;
        }


        fileBrowser = std::make_unique<juce::FileBrowserComponent>(
            flags, root, nullptr, nullptr);

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
                    // Ñîõðàíÿåì êàê ðàíüøå — èìÿ + .xml
                    file = getSelectedFile();
                    auto name = file.getFileName().trim();

                    if (!name.endsWithIgnoreCase(".xml"))
                        name += ".xml";

                    file = file.getParentDirectory().getChildFile(name);
                }
                else // Mode::Load èëè General
                {
                    // Çàãðóæàåì âûáðàííûé ôàéë èëè ââåä¸ííûé âðó÷íóþ
                    file = getSelectedFile();

                    if (!file.existsAsFile())
                        file = fileBrowser->getHighlightedFile();

                    // Â ðåæèìå Load — íóæåí ðåàëüíî ñóùåñòâóþùèé ôàéë
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

    void setConfirmCallback(std::function<void(const juce::File&)> cb)
    {
        onFileConfirmed = std::move(cb);
    }

    void setDialogWindow(juce::DialogWindow* dw)
    {
        dialogWindow = dw;
    }

    void refresh()
    {
        fileBrowser->refresh();
    }

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

    // Internal actions
    void copySelected()
    {
        clipboard.clear();
        for (int i = 0; i < fileBrowser->getNumSelectedFiles(); ++i)
            clipboard.add(fileBrowser->getSelectedFile(i));
    }
    void pasteClipboardTo(const juce::File& targetDirectory)
    {
        if (!targetDirectory.isDirectory() || clipboard.isEmpty())
            return;

        for (auto& sourceFile : clipboard)
        {
            auto targetFile = targetDirectory.getChildFile(sourceFile.getFileName());

            // Гарантированно видимый родитель — активное окно
            auto* parentWindow = juce::TopLevelWindow::getActiveTopLevelWindow();

            auto performCopy = [sourceFile, targetFile, parentWindow]()
                {
                    bool success = sourceFile.copyFileTo(targetFile);

                    juce::NativeMessageBox::showMessageBoxAsync(
                        juce::AlertWindow::InfoIcon,
                        "Copy Status",
                        success
                        ? "✅ File copied successfully:\n" + targetFile.getFullPathName()
                        : "❌ Failed to copy file:\n" + sourceFile.getFullPathName(),
                        parentWindow
                    );
                };

            if (targetFile.exists())
            {
                juce::AlertWindow::showOkCancelBox(
                    juce::AlertWindow::WarningIcon,
                    "File Exists",
                    "A file named \"" + targetFile.getFileName() + "\" already exists.\n\nDo you want to overwrite it?",
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

    void pasteFiles()
    {
        auto destDir = fileBrowser->getRoot();
        for (auto& f : clipboard)
        {
            auto dest = destDir.getChildFile(f.getFileName());
            f.copyFileTo(dest);
        }
        fileBrowser->refresh();
    }
    void deleteSelected()
    {
        auto selectedFiles = getSelectedFiles();
        if (selectedFiles.isEmpty())
            return;

        juce::String message = "Are you sure you want to delete the selected file(s)?\n\n";
        for (auto& f : selectedFiles)
            message += f.getFileName() + "\n";

        juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::WarningIcon,
            "Delete Confirmation",
            message.trim(),
            "Delete", "Cancel",
            this,
            juce::ModalCallbackFunction::create([this, selectedFiles](int result)
                {
                    if (result != 1)
                        return;

                    for (auto& f : selectedFiles)
                    {
                        auto path = f.getFullPathName();

                        try
                        {
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
            if (f.hasFileExtension("exe"))
                f.startAsProcess();
        }
    }
};
