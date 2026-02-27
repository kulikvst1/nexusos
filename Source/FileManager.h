#pragma once
#if defined(_WIN32) || defined(_WIN64)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif
#include <JuceHeader.h>
#include <filesystem>

namespace fs = std::filesystem;

class NexusWildcardFilter : public juce::FileFilter
{
public:
    NexusWildcardFilter(const juce::Array<juce::File>& roots);
    ~NexusWildcardFilter() override = default;
    void updateRoots(const juce::Array<juce::File>& newRoots);
    void setPattern(const juce::String& pattern);
    bool isFileSuitable(const juce::File& f) const override;
    bool isDirectorySuitable(const juce::File& f) const override;
private:
    bool isInsideAllowedRoots(const juce::File& f) const;
    juce::Array<juce::File> allowedRoots;
    std::unique_ptr<juce::WildcardFileFilter> wildcardFilter;
};

class FileManager : public juce::Component,
    public juce::FileBrowserListener,
    private juce::Timer
{
public:
    enum class Mode { General, Load, Save };
    explicit FileManager(Mode mode = Mode::General);
    explicit FileManager(const juce::File& rootDir, Mode mode = Mode::General);
    ~FileManager() override;

    void setConfirmCallback(std::function<void(const juce::File&)> cb);
    void setDialogWindow(juce::DialogWindow* dw);
    void refresh();
    void setWildcardFilter(const juce::String& pattern);
    void setMinimalUI(bool b);
    juce::File getSelectedFile() const;
    juce::Array<juce::File> getSelectedFiles() const;

    void fileClicked(const juce::File&, const juce::MouseEvent&) override;
    void fileDoubleClicked(const juce::File& file) override;
    void browserRootChanged(const juce::File&) override;
    void selectionChanged() override;

    void applyRootsAndRootDir(const juce::Array<juce::File>& newRoots, const juce::File& candidate);
    void onOpenUsbButtonClicked();
    void chooseRemovableInteractive();
    void switchToRemovable(const juce::File& chosen, const juce::Array<juce::File>& removableList);
    void switchToMainRoot();
    bool runButtonAllowed = true;
    void setShowRunButton(bool shouldShow);
    void setRootLocked(bool locked) noexcept { rootLocked = locked; }
    bool isRootLocked() const noexcept { return rootLocked; }
    void setHomeSubfolder(const juce::String& relativePath) noexcept { homeSubfolder = relativePath.trim(); }

protected:
    void resized() override;
    void paint(juce::Graphics&) override {}
    void visibilityChanged() override;

private:
    void timerCallback() override;
    bool rootLocked = false;

    void copySelected();
    void pasteFiles();
    void deleteSelected();
    void runSelected();
    bool copyAny(const juce::File& source, const juce::File& target);
    juce::File resolveDestinationDir(juce::FileBrowserComponent* browser) const;
    bool isAllowed(const juce::File& f) const;

    juce::TextButton copyButton, pasteButton, deleteButton,okButton, homeButton;
    std::unique_ptr<juce::Component> usbButtonsContainer;
    juce::OwnedArray<juce::TextButton> usbPartitionButtons;
    void updateUsbButtons(const juce::Array<juce::File>& removable);

    static juce::String makeDriveLabel(const juce::File& f);
    std::unique_ptr<juce::FileBrowserComponent> fileBrowser;
    Mode currentMode = Mode::General;
    bool minimalUI = false;
    juce::Array<juce::File> clipboard;
    juce::Array<juce::File> allowedRoots;
    std::unique_ptr<NexusWildcardFilter> filter;
    juce::File rootDir;
    juce::File mainRoot;
    std::function<void(const juce::File&)> onFileConfirmed;
    juce::DialogWindow* dialogWindow = nullptr;
    juce::String homeSubfolder;

    juce::Array<juce::File> previousRemovableDrives;
    bool usbPollTimerRunning = false;
    int usbPollIntervalMs = 1000;
};
