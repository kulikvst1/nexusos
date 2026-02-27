// IconManager.h
#pragma once
#include "BinaryData.h"
#include <juce_gui_basics/juce_gui_basics.h>

class IconManager
{
public:
    static std::unique_ptr<juce::Drawable> getIcon(const juce::String& name)
    {
        if (name == "fscreen")
            return loadPNG(BinaryData::fscreen_png, BinaryData::fscreen_pngSize);
        if (name == "config")
            return loadPNG(BinaryData::config_png, BinaryData::config_pngSize);
        if (name == "vstManager")
            return loadPNG(BinaryData::vstManager_png, BinaryData::vstManager_pngSize);
        if (name == "fileManeger")
            return loadPNG(BinaryData::fileManeger_png, BinaryData::fileManeger_pngSize);
        if (name == "looper")
            return loadPNG(BinaryData::looper_png, BinaryData::looper_pngSize);
        if (name == "Tuning")
            return loadPNG(BinaryData::Tuning_png, BinaryData::Tuning_pngSize);
        if (name == "TunMode")
            return loadPNG(BinaryData::TunMode_png, BinaryData::TunMode_pngSize);
        if (name == "Key")
            return loadPNG(BinaryData::Key_png, BinaryData::Key_pngSize);
        if (name == "Update")
            return loadPNG(BinaryData::Update_png, BinaryData::Update_pngSize);
        if (name == "close")
            return loadPNG(BinaryData::close_png, BinaryData::close_pngSize);
        if (name == "PowerOff")
            return loadPNG(BinaryData::PowerOff_png, BinaryData::PowerOff_pngSize);
        
        // fallback: пустая картинка
        return {};
    }
private:
    static std::unique_ptr<juce::Drawable> loadPNG(const void* data, size_t size)
    {
        juce::MemoryInputStream stream(data, size, false);
        auto image = juce::PNGImageFormat().decodeImage(stream);
        if (image.isValid())
            return std::make_unique<juce::DrawableImage>(image);
        return {};
    }
};
