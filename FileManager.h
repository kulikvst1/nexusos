#pragma once
#include <JuceHeader.h>
#include <functional>
#include <memory>

class FileManager
{
public:
    /**
      * ���������� �������� ������ ���������� �����.
      *
      * @param dialogTitle ��������� ����������� ����.
      * @param fileFilter ������ ������ (��������, "*.ini").
      * @param callback �������, ������� ����� ������� ����� ������ �����.
      */
    static void chooseSaveFileAsync(const juce::String& dialogTitle,
        const juce::String& fileFilter,
        std::function<void(const juce::File&)> callback)
    {
        // ���������� shared_ptr ������ unique_ptr, ����� ������ ���� ����������.
        auto fileChooser = std::make_shared<juce::FileChooser>(
            dialogTitle,
            juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory),
            fileFilter);

        fileChooser->launchAsync(juce::FileBrowserComponent::saveMode,
            [callback, fileChooser](const juce::FileChooser& chooser)
            {
                // ��� ���������� ������� �������� ��������� ����
                juce::File selectedFile = chooser.getResult();
                callback(selectedFile);
            });
    }

    /**
      * ���������� �������� ������ �������� �����.
      *
      * @param dialogTitle ��������� ����������� ����.
      * @param fileFilter ������ ������ (��������, "*.ini").
      * @param callback �������, ������� ����� ������� ����� ������ �����.
      */
    static void chooseLoadFileAsync(const juce::String& dialogTitle,
        const juce::String& fileFilter,
        std::function<void(const juce::File&)> callback)
    {
        auto fileChooser = std::make_shared<juce::FileChooser>(
            dialogTitle,
            juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory),
            fileFilter);

        fileChooser->launchAsync(juce::FileBrowserComponent::openMode,
            [callback, fileChooser](const juce::FileChooser& chooser)
            {
                juce::File selectedFile = chooser.getResult();
                callback(selectedFile);
            });
    }
};
