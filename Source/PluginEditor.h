#pragma once
#include "PluginProcessor.h"

class LoFiAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit LoFiAudioProcessorEditor (LoFiAudioProcessor&);
    ~LoFiAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override {}

private:
    LoFiAudioProcessor& processorRef;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoFiAudioProcessorEditor)
};
