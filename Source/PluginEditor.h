#pragma once
#include "PluginProcessor.h"

// Временный интерфейс: автоматическое окно со всеми параметрами.
// Позже заменяется на свой дизайн с картинками.
class LoFiAudioProcessorEditor : public juce::GenericAudioProcessorEditor
{
public:
    explicit LoFiAudioProcessorEditor (LoFiAudioProcessor& p)
        : juce::GenericAudioProcessorEditor (p) {}
};
