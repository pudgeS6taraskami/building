#include "PluginEditor.h"

LoFiAudioProcessorEditor::LoFiAudioProcessorEditor (LoFiAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setSize (400, 200);
}

void LoFiAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff2b2233));
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (28.0f));
    g.drawFittedText ("LoFi Machine", getLocalBounds(), juce::Justification::centred, 1);
}
