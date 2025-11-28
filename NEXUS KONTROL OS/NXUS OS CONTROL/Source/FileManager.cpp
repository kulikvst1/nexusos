#include "FileManager.h"
using namespace std::filesystem;
#if JUCE_WINDOWS
#include <windows.h>
#include <winioctl.h>
#include <vector>
#include <string>
#endif
#include <thread>
static bool isPhysicalDriveRemovable(char driveLetter)
{
    if (driveLetter < 'A' || driveLetter > 'Z')
        return false;
    char devicePath[] = "\\\\.\\X:";
    devicePath[4] = driveLetter;
    HANDLE h = CreateFileA(devicePath,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    STORAGE_PROPERTY_QUERY query;
    ZeroMemory(&query, sizeof(query));
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;
    std::vector<BYTE> buffer(4096);
    DWORD returned = 0;
    bool removable = false;
    if (DeviceIoControl(h,
        IOCTL_STORAGE_QUERY_PROPERTY,
        &query, sizeof(query),
        buffer.data(), static_cast<DWORD>(buffer.size()),
        &returned,
        nullptr))
    {
        if (returned >= sizeof(STORAGE_DEVICE_DESCRIPTOR))
        {
            auto* desc = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buffer.data());
            if (desc->RemovableMedia)
            {
                removable = true;
            }
            else
            {
                   if (desc->BusType == BusTypeUsb)
                    removable = true;
            }
        }
    }
    CloseHandle(h);
    return removable;
}
static juce::Array<juce::File> detectRemovableDrives()
{
    juce::Array<juce::File> result;

#if JUCE_WINDOWS
    DWORD mask = GetLogicalDrives();
    juce::File exeFolder = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    juce::String sysDrive = exeFolder.getFullPathName().substring(0, 2).toUpperCase(); // "C:"

    for (char letter = 'A'; letter <= 'Z'; ++letter)
    {
        int bit = 1 << (letter - 'A');
        if (!(mask & bit)) continue;

        juce::String rootPath = juce::String::formatted("%c:\\\\", letter); // "D:\\"
        juce::File drive(rootPath);
        if (!drive.exists() || !drive.isDirectory()) continue;

        UINT type = GetDriveTypeW((LPCWSTR)juce::String(rootPath).toWideCharPointer());

        if (type == DRIVE_REMOVABLE)
        {
            result.addIfNotAlreadyThere(drive);
            continue;
        }

        if (type == DRIVE_FIXED)
        {
            juce::String driveRoot = rootPath.substring(0, 2).toUpperCase(); // "D:"
            if (driveRoot == sysDrive) continue;

            if (isPhysicalDriveRemovable(static_cast<char>(letter)))
                result.addIfNotAlreadyThere(drive);
        }
    }
#endif

    juce::StringArray names;
    for (auto& f : result) names.add(f.getFullPathName());
    juce::Logger::writeToLog("detectRemovableDrives -> " + names.joinIntoString("; "));
    return result;
}

juce::String FileManager::makeDriveLabel(const juce::File& f)
{
    auto p = f.getFullPathName();
    if (p.length() >= 2 && p[1] == ':')
        return p.substring(0, 2); // "D:"
    return f.getFileName();
}
void FileManager::updateUsbButtons(const juce::Array<juce::File>& removable)
{
    if (!juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        juce::MessageManager::callAsync([this, removable]() { updateUsbButtons(removable); });
        return;
    }

    if (!usbButtonsContainer)
        return;

    // очистка старых кнопок
    for (auto* b : usbPartitionButtons)
        usbButtonsContainer->removeChildComponent(b);
    usbPartitionButtons.clear(true);

    // Параметры кнопок и отступов
    const int gapX = 6;
    const int gapY = 4;
    const int btnW = 64;
    const int btnH = 30; // компактная высота кнопки
    const int minContainerW = 80;
    const int maxContainerW = 600; // разумный максимум ширины контейнера

    if (removable.isEmpty())
    {
        usbButtonsContainer->setSize(minContainerW, btnH + 2 * gapY);
        usbButtonsContainer->setVisible(false);
        return;
    }

    int x = gapX;
    int y = gapY;

    // создаём кнопки и считаем ширину; прекращаем добавление при достижении maxContainerW
    for (int i = 0; i < removable.size(); ++i)
    {
        if (x + btnW + gapX > maxContainerW)
            break;

        juce::File drive = removable.getReference(i);

        auto* btn = new juce::TextButton(makeDriveLabel(drive));
        btn->setTooltip(drive.getFullPathName());
        btn->setComponentID(drive.getFullPathName());
        btn->setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        btn->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        btn->setClickingTogglesState(false);

        btn->onClick = [this, drive, removable]() { switchToRemovable(drive, removable); };

        usbButtonsContainer->addAndMakeVisible(btn);
        usbPartitionButtons.add(btn);

        btn->setBounds(x, y, btnW, btnH);
        x += btnW + gapX;
    }

    // итоговая ширина и высота контейнера
    int totalW = std::max(minContainerW, x + gapX);
    totalW = std::min(totalW, maxContainerW);
    int totalH = btnH + 2 * gapY;

    // центрируем кнопки по вертикали внутри контейнера (корректируем уже созданные кнопки)
    int centeredY = (totalH - btnH) / 2;
    for (auto* btn : usbPartitionButtons)
    {
        auto r = btn->getBounds();
        r.setY(centeredY);
        r.setHeight(btnH);
        btn->setBounds(r);
    }

    // скрываем кнопки, которые потенциально выходят за правую границу контейнера
    for (auto* btn : usbPartitionButtons)
    {
        if (btn->getRight() > totalW)
            btn->setVisible(false);
        else
            btn->setVisible(true);
    }

    usbButtonsContainer->setSize(totalW, totalH);
    usbButtonsContainer->setVisible(true);

    // отладочный лог (временно, убрать после проверки)
    juce::Logger::writeToLog("updateUsbButtons -> container = " + juce::String(totalW) + "x" + juce::String(totalH)
        + " buttons=" + juce::String(usbPartitionButtons.size()));
}


static juce::File pickPreferredRoot(const juce::Array<juce::File>& removableList)
{
#if JUCE_WINDOWS
    juce::File dNexus("D:\\NEXUS");
    if (dNexus.exists() && dNexus.isDirectory()) return dNexus;
    if (!removableList.isEmpty()) return removableList[0];
    juce::File cNexus("C:\\NEXUS");
    if (cNexus.exists() && cNexus.isDirectory()) return cNexus;
    juce::File dRoot("D:\\");
    if (dRoot.exists() && dRoot.isDirectory()) return dRoot;
    juce::File cRoot("C:\\");
    if (cRoot.exists() && cRoot.isDirectory()) return cRoot;
    return juce::File();
#else
    if (!removableList.isEmpty()) return removableList[0];
    juce::File home = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    juce::File homeNexus = home.getChildFile("NEXUS");
    if (homeNexus.exists() && homeNexus.isDirectory()) return homeNexus;
    return home;
#endif
}
static juce::File findExistingMainNexus()
{
#if JUCE_WINDOWS
    juce::File dNexus("D:\\NEXUS");
    if (dNexus.exists() && dNexus.isDirectory()) return dNexus;
    juce::File cNexus("C:\\NEXUS");
    if (cNexus.exists() && cNexus.isDirectory()) return cNexus;
    juce::File dRoot("D:\\");
    if (dRoot.exists() && dRoot.isDirectory()) return dRoot;
    juce::File cRoot("C:\\");
    if (cRoot.exists() && cRoot.isDirectory()) return cRoot;
    return juce::File();
#else
    juce::File home = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    juce::File homeNexus = home.getChildFile("NEXUS");
    if (homeNexus.exists() && homeNexus.isDirectory()) return homeNexus;
    return home;
#endif
}
static juce::String normalizeForCompare(const juce::File& f)
{
    auto s = f.getFullPathName().replaceCharacter('\\', '/');
    if (s.endsWithChar('/')) s = s.dropLastCharacters(1);
    return s.toLowerCase();
}
NexusWildcardFilter::NexusWildcardFilter(const juce::Array<juce::File>& roots)
    : juce::FileFilter("NexusWildcardFilter"), allowedRoots(roots)
{
    wildcardFilter = std::make_unique<juce::WildcardFileFilter>("*", "*", "All files");
}
void NexusWildcardFilter::updateRoots(const juce::Array<juce::File>& newRoots)
{
    allowedRoots = newRoots;
}
void NexusWildcardFilter::setPattern(const juce::String& pattern)
{
    wildcardFilter = std::make_unique<juce::WildcardFileFilter>(
        pattern.isNotEmpty() ? pattern : "*", "*", "Filtered files");
}
bool NexusWildcardFilter::isInsideAllowedRoots(const juce::File& f) const
{
    if (allowedRoots.isEmpty()) return false;
    auto fp = normalizeForCompare(f);
    for (auto& r : allowedRoots)
    {
        auto rp = normalizeForCompare(r);
        if (fp == rp) return true;
        if (fp.startsWith(rp + "/")) return true;
    }
    return false;
}
bool NexusWildcardFilter::isFileSuitable(const juce::File& f) const
{
    if (!f.existsAsFile()) return false;
    if (!isInsideAllowedRoots(f)) return false;
    if (wildcardFilter) return wildcardFilter->isFileSuitable(f);
    return true;
}
bool NexusWildcardFilter::isDirectorySuitable(const juce::File& f) const
{
    if (!f.isDirectory()) return false;
    return isInsideAllowedRoots(f);
}
FileManager::FileManager(Mode mode)
    : currentMode(mode)
{
    auto removable = detectRemovableDrives();
    mainRoot = findExistingMainNexus();
    juce::Logger::writeToLog("findExistingMainNexus -> " + mainRoot.getFullPathName());
    juce::File chosen;
    if (!removable.isEmpty())
        chosen = removable[0];
    else
        chosen = pickPreferredRoot(removable);
    allowedRoots.clear();
    bool chosenIsRemovable = false;
    for (auto& r : removable)
        if (r.getFullPathName() == chosen.getFullPathName())
            chosenIsRemovable = true;
    if (chosenIsRemovable)
    {
        for (auto& r : removable)
            if (r.exists() && r.isDirectory())
                allowedRoots.add(r);
    }
    else
    {
        if (chosen.exists() && chosen.isDirectory())
            allowedRoots.add(chosen);
    }
    if (chosen == juce::File())
    {
        if (mainRoot.exists() && mainRoot.isDirectory())
            chosen = mainRoot;
        else
        {
            juce::File cNexus("C:\\NEXUS");
            if (cNexus.exists() && cNexus.isDirectory())
                chosen = cNexus;
            else
                chosen = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
        }
    }
    rootDir = chosen;
    if (allowedRoots.isEmpty() && rootDir.exists() && rootDir.isDirectory())
        allowedRoots.add(rootDir);
    if (filter)
        filter->updateRoots(allowedRoots);
    else
        filter = std::make_unique<NexusWildcardFilter>(allowedRoots);
    int flags = juce::FileBrowserComponent::canSelectDirectories;
    if (mode == Mode::Save)
        flags |= juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;
    else
        flags |= juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    fileBrowser = std::make_unique<juce::FileBrowserComponent>(flags, rootDir, filter.get(), nullptr);
    fileBrowser->addListener(this);
    addAndMakeVisible(*fileBrowser);
    fileBrowser->refresh();
    if (fileBrowser)
    {
        auto* fb = fileBrowser.get();
        for (int i = 0; i < fb->getNumChildComponents(); ++i)
        {
            auto* child = fb->getChildComponent(i);
            if (child && child->getName().containsIgnoreCase("path")) child->setVisible(false);
        }
    }

    // --- Создаём контейнер для динамических кнопок разделов USB ---
    usbButtonsContainer.reset(new juce::Component());
    usbButtonsContainer->setVisible(false);
    addAndMakeVisible(usbButtonsContainer.get());
    // Инициализируем динамические кнопки по текущему списку томов
    updateUsbButtons(detectRemovableDrives());
    // -----------------------------------------------------------------

    addAndMakeVisible(copyButton);
    addAndMakeVisible(pasteButton);
    addAndMakeVisible(deleteButton);
    addAndMakeVisible(okButton);
    // usbButton больше не добавляем в иерархию — кнопки разделов заменяют его
    addAndMakeVisible(homeButton);

    copyButton.setButtonText("Copy");
    pasteButton.setButtonText("Paste");
    deleteButton.setButtonText("Delete");
    okButton.setButtonText("OK");
    // usbButton.setButtonText("USB"); // больше не используем
    homeButton.setButtonText("Home");

    copyButton.onClick = [this] { copySelected(); };
    pasteButton.onClick = [this] { pasteFiles(); };
    deleteButton.onClick = [this] { deleteSelected(); };
    okButton.onClick = [this]()
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
                if (!file.existsAsFile() && fileBrowser)
                    file = fileBrowser->getHighlightedFile();
                if (currentMode == Mode::Load && !file.existsAsFile())
                    return;
            }

            if (onFileConfirmed)
                onFileConfirmed(file);

            // Запустить выбранный файл так же, как runSelected, только если режим не Save
            /*
            if (currentMode != Mode::Save)
            {
                juce::MessageManager::callAsync([this]()
                    {
                        runSelected();
                    });
            }
            */
            if (dialogWindow)
                dialogWindow->exitModalState(0);
        };

    // usbButton.onClick = [this] { chooseRemovableInteractive(); }; // больше не нужно
    homeButton.onClick = [this] { switchToMainRoot(); };
    setSize(900, 640);
    juce::Logger::writeToLog("FileManager constructed; mainRoot=" + mainRoot.getFullPathName() + " initialRoot=" + rootDir.getFullPathName());
    // запуск опроса USB при создании менеджера
    startTimer(usbPollIntervalMs);
    usbPollTimerRunning = true;

}
FileManager::~FileManager()
{
    stopTimer();
}

void FileManager::visibilityChanged()
{
    if (isVisible())
    {
        startTimer(usbPollIntervalMs);
        timerCallback();
    }
    else
    {
        stopTimer();
    }
}
void FileManager::timerCallback()
{
    auto current = detectRemovableDrives();

    std::sort(current.begin(), current.end(),
        [](const juce::File& a, const juce::File& b)
        {
            return a.getFullPathName() < b.getFullPathName();
        });

    bool changed = false;
    if (current.size() != previousRemovableDrives.size())
        changed = true;
    else
    {
        for (int i = 0; i < current.size(); ++i)
            if (current[i].getFullPathName() != previousRemovableDrives[i].getFullPathName())
            {
                changed = true; break;
            }
    }

    if (changed)
    {
        previousRemovableDrives = current;
        juce::MessageManager::callAsync([this, current]() { updateUsbButtons(current); });
    }
}

FileManager::FileManager(const juce::File& root, Mode mode)
    : FileManager(mode)
{
    allowedRoots.clear();
    if (root.exists() && root.isDirectory())
        allowedRoots.add(root);
    if (filter) filter->updateRoots(allowedRoots);
    else filter = std::make_unique<NexusWildcardFilter>(allowedRoots);
    rootDir = root;
    if (fileBrowser)
    {
        auto* fb = fileBrowser.get();
        fb->setRoot(rootDir);
        fb->refresh();
    }
    juce::Logger::writeToLog("FileManager constructed with explicit root (no creation): " + rootDir.getFullPathName());
}
void FileManager::resized()
{
    auto full = getLocalBounds();

    const int margin = 8;
    const int gapBetween = 8;
    const int homeW = 100;
    const int homeH = 32;
    const int fallbackContainerW = 420;
    const int fallbackContainerH = 40;

    // Зарезервируем верхнюю высоту для Home/контейнера (используется для расположения, но не "поглощает" область fileBrowser)
    int actualContainerH = (usbButtonsContainer && usbButtonsContainer->isVisible()) ? usbButtonsContainer->getHeight() : fallbackContainerH;
    actualContainerH = std::min(actualContainerH, 200);
    const int topReserved = std::max(homeH, actualContainerH) + margin;

    // Резерв снизу под нижнюю панель
    const int buttonHeight = 28;
    const int bottomReserved = buttonHeight + 8;

    // Область для fileBrowser между верхом и низом
    auto contentArea = full;
    contentArea.removeFromTop(topReserved);
    auto buttonArea = contentArea.removeFromBottom(bottomReserved).reduced(8, 4);

    if (fileBrowser)
        fileBrowser->setBounds(contentArea);

    // Размещаем Home и контейнер поверх fileBrowser, но они не должны "съедать" контентArea
    homeButton.setVisible(!minimalUI);
    homeButton.setBounds(margin, margin, homeW, homeH);
    homeButton.toFront(true);

    if (usbButtonsContainer)
    {
        int containerX = margin + homeW + gapBetween;
        int actualContainerW = usbButtonsContainer->getWidth();
        if (actualContainerW <= 0) actualContainerW = fallbackContainerW;
        int containerY = margin + ((homeH - actualContainerH) / 2);

        int availableW = full.getWidth() - containerX - margin;
        int finalW = std::min(actualContainerW, std::max(0, availableW));

        usbButtonsContainer->setBounds(containerX, containerY, finalW, actualContainerH);
        usbButtonsContainer->setVisible(true);

        // Принудительный z‑порядок и клики
        usbButtonsContainer->toFront(true);
        usbButtonsContainer->setInterceptsMouseClicks(true, true);
        homeButton.toFront(true);
    }

    // Нижняя панель
    okButton.setVisible(currentMode != Mode::General);
    copyButton.setVisible(!minimalUI);
    pasteButton.setVisible(!minimalUI);
    deleteButton.setVisible(!minimalUI);
    
    juce::FlexBox flex;
    flex.flexDirection = juce::FlexBox::Direction::row;
    flex.justifyContent = juce::FlexBox::JustifyContent::center;
    flex.alignItems = juce::FlexBox::AlignItems::center;
    if (!minimalUI)
    {
        flex.items.add(juce::FlexItem(copyButton).withWidth(100).withHeight(buttonHeight));
        flex.items.add(juce::FlexItem(pasteButton).withWidth(100).withHeight(buttonHeight));
        flex.items.add(juce::FlexItem(deleteButton).withWidth(100).withHeight(buttonHeight));
       
    }
    if (currentMode != Mode::General)
        flex.items.add(juce::FlexItem(okButton).withWidth(100).withHeight(buttonHeight));

    flex.performLayout(buttonArea);

    // Асинхронно пересоздать/обновить usb-кнопки под новую ширину окна
    juce::MessageManager::callAsync([this]() { updateUsbButtons(detectRemovableDrives()); });

    // Отладочный лог (временно)
    juce::Logger::writeToLog("resized -> full=" + juce::String(getWidth()) + "x" + juce::String(getHeight())
        + " container=" + juce::String(usbButtonsContainer ? usbButtonsContainer->getWidth() : 0) + "x" + juce::String(usbButtonsContainer ? usbButtonsContainer->getHeight() : 0));
}

void FileManager::setShowRunButton(bool shouldShow)
{
    runButtonAllowed = shouldShow;
    resized();
    repaint();
}
void FileManager::fileClicked(const juce::File&, const juce::MouseEvent&) {}
void FileManager::fileDoubleClicked(const juce::File& file)
{
    if (currentMode == Mode::Load && onFileConfirmed && file.exists() && isAllowed(file))
    {
        onFileConfirmed(file);
        if (dialogWindow) dialogWindow->exitModalState(0);
    }
}
void FileManager::browserRootChanged(const juce::File&) {}
void FileManager::selectionChanged() {}
void FileManager::setConfirmCallback(std::function<void(const juce::File&)> cb) { onFileConfirmed = std::move(cb); }
void FileManager::setDialogWindow(juce::DialogWindow* dw) { dialogWindow = dw; }
void FileManager::applyRootsAndRootDir(const juce::Array<juce::File>& newRoots, const juce::File& candidate)
{
    juce::MessageManager::callAsync([this, newRoots, candidate]()
        {
            if (filter) filter->updateRoots(newRoots);
            else filter = std::make_unique<NexusWildcardFilter>(newRoots);
            allowedRoots = newRoots;
            rootDir = candidate;
            if (fileBrowser)
            {
                auto* fb = fileBrowser.get();
                fb->setRoot(rootDir);
                fb->refresh();
            }
            juce::Logger::writeToLog("applyRootsAndRootDir -> " + rootDir.getFullPathName());
        });
}
void FileManager::refresh()
{
    juce::Logger::writeToLog("FileManager::refresh called");
    auto removable = detectRemovableDrives();
    juce::Array<juce::File> newAllowed;
    juce::File newRoot;
    if (!removable.isEmpty())
        newRoot = removable[0];
    else
        newRoot = pickPreferredRoot(removable);
    bool chosenIsRemovable = false;
    for (auto& r : removable) if (r.getFullPathName() == newRoot.getFullPathName()) chosenIsRemovable = true;
    if (chosenIsRemovable)
        for (auto& r : removable) if (r.exists() && r.isDirectory()) newAllowed.add(r);
        else
            if (newRoot.exists() && newRoot.isDirectory()) newAllowed.add(newRoot);
    applyRootsAndRootDir(newAllowed, newRoot);
}
void FileManager::onOpenUsbButtonClicked()
{
    auto removable = detectRemovableDrives();
    if (removable.isEmpty())
    {
        juce::Logger::writeToLog("onOpenUsbButtonClicked: no removable drives found");
        return;
    }
    auto chosen = removable[0];
    juce::Array<juce::File> newRoots;
    for (auto& r : removable) if (r.exists() && r.isDirectory()) newRoots.add(r);
    applyRootsAndRootDir(newRoots, chosen);
}
void FileManager::chooseRemovableInteractive()
{
    updateUsbButtons(detectRemovableDrives());
}

void FileManager::switchToRemovable(const juce::File& chosen, const juce::Array<juce::File>& removableList)
{
    juce::Array<juce::File> newRoots;
    for (auto& r : removableList) if (r.exists() && r.isDirectory()) newRoots.add(r);
    juce::MessageManager::callAsync([this, newRoots, chosen]()
        {
            if (filter) filter->updateRoots(newRoots);
            else filter = std::make_unique<NexusWildcardFilter>(newRoots);
            allowedRoots = newRoots;
            rootDir = chosen;
            if (fileBrowser)
            {
                auto* fb = fileBrowser.get();
                fb->setRoot(rootDir);
                fb->refresh();
            }
            juce::Logger::writeToLog("Switched to removable: " + rootDir.getFullPathName());
        });
}
void FileManager::switchToMainRoot()
{
    juce::File found = findExistingMainNexus();
    if (found.getFullPathName().isEmpty())
    {
        juce::Logger::writeToLog("switchToMainRoot: no existing main root found");
        return;
    }
    mainRoot = found;
    juce::File targetRoot = found;
    if (!homeSubfolder.isEmpty())
    {
        juce::File candidate = found.getChildFile(homeSubfolder);
        if (candidate.exists() && candidate.isDirectory())
            targetRoot = candidate;
        else
            juce::Logger::writeToLog("switchToMainRoot: homeSubfolder not found, falling back to main root: " + candidate.getFullPathName());
    }
    juce::Array<juce::File> newRoots;
    newRoots.add(mainRoot); // allowedRoots хранит физические корни (mainRoot / removable). mainRoot остаётся физическим корнем
    juce::MessageManager::callAsync([this, newRoots, targetRoot]()
        {
            if (filter) filter->updateRoots(newRoots);
            else filter = std::make_unique<NexusWildcardFilter>(newRoots);
            allowedRoots = newRoots;
            rootDir = targetRoot;
            if (fileBrowser)
            {
                fileBrowser->setRoot(rootDir);
                fileBrowser->refresh();
            }
            juce::Logger::writeToLog("Home -> switched to rootDir: " + rootDir.getFullPathName());
        });
}
void FileManager::setWildcardFilter(const juce::String& pattern)
{
    if (filter) filter->setPattern(pattern);
    if (fileBrowser) fileBrowser->refresh();
}
void FileManager::setMinimalUI(bool b) { minimalUI = b; }
juce::File FileManager::getSelectedFile() const
{
    if (fileBrowser && fileBrowser->getNumSelectedFiles() > 0) return fileBrowser->getSelectedFile(0);
    return {};
}
juce::Array<juce::File> FileManager::getSelectedFiles() const
{
    juce::Array<juce::File> res;
    if (!fileBrowser) return res;
    for (int i = 0; i < fileBrowser->getNumSelectedFiles(); ++i) res.add(fileBrowser->getSelectedFile(i));
    return res;
}
juce::File FileManager::resolveDestinationDir(juce::FileBrowserComponent* browser) const
{
    juce::File dest = browser->getHighlightedFile();
    if (dest.existsAsFile()) dest = dest.getParentDirectory();
    if (!dest.isDirectory()) dest = browser->getRoot();
    return dest;
}
bool FileManager::isAllowed(const juce::File& f) const
{
    if (!f.exists()) return false;
    auto target = normalizeForCompare(f);
    for (const auto& r : allowedRoots)
    {
        if (!r.exists() || !r.isDirectory()) continue;
        auto root = normalizeForCompare(r);
        if (target == root) return true;
        if (target.startsWith(root + "/")) return true;
    }
    return false;
}
void FileManager::copySelected()
{
    clipboard.clear();
    if (!fileBrowser) return;
    for (int i = 0; i < fileBrowser->getNumSelectedFiles(); ++i) clipboard.add(fileBrowser->getSelectedFile(i));
}
bool FileManager::copyAny(const juce::File& source, const juce::File& target)
{
    if (!source.exists())
        return false;
    if (source.isDirectory())
    {
        if (!target.exists())
        {
            if (!target.createDirectory())
                return false;
        }
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
void FileManager::pasteFiles()
{
    if (clipboard.isEmpty() || !fileBrowser) return;
    auto* fb = fileBrowser.get();
    if (!fb) return;
    auto destDir = resolveDestinationDir(fb);
    if (!destDir.exists() || !destDir.isDirectory()) return;
    for (auto& src : clipboard)
    {
        juce::File rootCandidate = src;
        while (rootCandidate.getParentDirectory().getFullPathName() != rootCandidate.getFullPathName())
            rootCandidate = rootCandidate.getParentDirectory();
        bool found = false;
        for (const auto& r : allowedRoots)
            if (normalizeForCompare(r) == normalizeForCompare(rootCandidate)) { found = true; break; }
        if (!found && rootCandidate.exists() && rootCandidate.isDirectory())
            allowedRoots.add(rootCandidate);
    }
    juce::File testFile = destDir.getChildFile(".nexus_write_test.tmp");
    bool canWrite = false;
    {
        bool created = testFile.create();
        if (created) { canWrite = true; testFile.deleteFile(); }
        else if (testFile.exists()) { canWrite = testFile.deleteFile(); }
    }
    if (!canWrite) return;
    bool anySrcAllowed = false;
    for (auto& src : clipboard)
    {
        bool ok = false;
        for (const auto& r : allowedRoots)
        {
            if (!r.exists() || !r.isDirectory()) continue;
            auto rr = normalizeForCompare(r);
            auto t = normalizeForCompare(src);
            if (t == rr || t.startsWith(rr + "/")) { ok = true; break; }
        }
        if (ok) { anySrcAllowed = true; break; }
    }
    if (!anySrcAllowed) return;
    std::thread([this, destDir, clipboard = clipboard]() mutable
        {
            for (auto& src : clipboard)
            {
                if (!src.exists()) continue;
                auto dst = destDir.getChildFile(src.getFileName());
                copyAny(src, dst);
            }
            juce::MessageManager::callAsync([this]() { if (fileBrowser) fileBrowser->refresh(); });
        }).detach();
}
static void deleteConfirmedCallback(int result, void* userData)
{
    if (result != 1) return; // 1 = OK/Yes
    auto selectedPtr = static_cast<std::vector<juce::File>*>(userData);
    if (!selectedPtr) return;
    std::vector<juce::File> selected = *selectedPtr;
    delete selectedPtr; // удаляем временный буфер
    std::thread([selected]() mutable
        {
            for (auto& f : selected)
            {
                if (!f.exists()) continue;
                try { std::filesystem::remove_all(std::filesystem::u8path(f.getFullPathName().toStdString())); }
                catch (...) {}
            }
            juce::MessageManager::callAsync([]() { /* refresh browser from instance */ });
        }).detach();
}
void FileManager::deleteSelected()
{
    auto selected = getSelectedFiles();
    if (selected.isEmpty() || !fileBrowser) return;
    juce::String message;
    if (selected.size() <= 5)
    {
        juce::StringArray names;
        for (auto& f : selected) names.add(f.getFileName());
        message = "Are you sure you want to delete:\n" + names.joinIntoString("\n");
    }
    else
    {
        message = "Are you sure you want to delete " + juce::String(selected.size()) + " items?";
    }
    juce::MessageBoxOptions opts;
    opts = opts.withIconType(juce::MessageBoxIconType::WarningIcon)
        .withTitle("Confirm deletion")
        .withMessage(message)
        .withButton("OK")
        .withButton("Cancel");
    juce::AlertWindow::showAsync(opts, [this, selected](int result)
        {
            if (result != 1) // Cancel or closed
                return;
            std::thread([this, selected]() mutable
                {
                    for (auto& f : selected)
                    {
                        if (!f.exists()) continue;
                        if (!isAllowed(f)) continue;
                        try
                        {
                            std::filesystem::remove_all(std::filesystem::u8path(f.getFullPathName().toStdString()));
                        }
                        catch (...)
                        {
                            // optional: logging failure
                        }
                    }
                    juce::MessageManager::callAsync([this]() {
                        if (fileBrowser) fileBrowser->refresh();
                        });
                }).detach();
        });
}
void FileManager::runSelected()
{
    if (!fileBrowser) return;
    for (int i = 0; i < fileBrowser->getNumSelectedFiles(); ++i)
    {
        auto f = fileBrowser->getSelectedFile(i);
        if (!f.exists() || !isAllowed(f)) continue;
        if (f.isDirectory()) { f.revealToUser(); continue; }
#if JUCE_WINDOWS
        f.startAsProcess();
#else
        juce::URL(f).launchInDefaultBrowser();
#endif
    }
}


