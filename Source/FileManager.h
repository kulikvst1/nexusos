#pragma once
#include <JuceHeader.h>
#include <functional>
#include <memory>

class FileManager
{
public:
    /**
      * Àñèíõðîííî âûçûâàåò äèàëîã ñîõðàíåíèÿ ôàéëà.
      *
      * @param dialogTitle Çàãîëîâîê äèàëîãîâîãî îêíà.
      * @param fileFilter Ôèëüòð ôàéëîâ (íàïðèìåð, "*.ini").
      * @param callback Ôóíêöèÿ, êîòîðàÿ áóäåò âûçâàíà ïîñëå âûáîðà ôàéëà.
      */
    static void chooseSaveFileAsync(const juce::String& dialogTitle,
        const juce::String& fileFilter,
        std::function<void(const juce::File&)> callback)
    {
        // Èñïîëüçóåì shared_ptr âìåñòî unique_ptr, ÷òîáû ëÿìáäà áûëà êîïèðóåìîé.
        auto fileChooser = std::make_shared<juce::FileChooser>(
            dialogTitle,
            juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory),
            fileFilter);

        fileChooser->launchAsync(juce::FileBrowserComponent::saveMode,
            [callback, fileChooser](const juce::FileChooser& chooser)
            {
                // Ïðè çàâåðøåíèè äèàëîãà ïîëó÷àåì âûáðàííûé ôàéë
                juce::File selectedFile = chooser.getResult();
                callback(selectedFile);
            });
    }

    /**
      * Àñèíõðîííî âûçûâàåò äèàëîã îòêðûòèÿ ôàéëà.
      *
      * @param dialogTitle Çàãîëîâîê äèàëîãîâîãî îêíà.
      * @param fileFilter Ôèëüòð ôàéëîâ (íàïðèìåð, "*.ini").
      * @param callback Ôóíêöèÿ, êîòîðàÿ áóäåò âûçâàíà ïîñëå âûáîðà ôàéëà.
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
