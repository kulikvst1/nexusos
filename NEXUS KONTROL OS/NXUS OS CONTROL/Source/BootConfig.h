// BootConfig.h
#pragma once
#include <JuceHeader.h>

void ensureNexusFolders();
void ensureBootFile();
juce::File getBootConfigFile();
juce::File readBootConfig();
void writeBootConfig(const juce::File& targetFile);

