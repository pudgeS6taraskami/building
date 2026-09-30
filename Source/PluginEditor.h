#pragma once
#include "PluginProcessor.h"
#include <array>

class LoFiAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit LoFiAudioProcessorEditor (LoFiAudioProcessor&);
    ~LoFiAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    LoFiAudioProcessor& processorRef;
    std::array<Knob, 4> knobs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoFiAudioProcessorEditor)
};
