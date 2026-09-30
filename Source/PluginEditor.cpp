#include "PluginEditor.h"

LoFiAudioProcessorEditor::LoFiAudioProcessorEditor (LoFiAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    const char* ids[]   = { "bits", "downsample", "mix", "output" };
    const char* names[] = { "Bits", "Downsample", "Mix", "Output" };

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];

        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
        addAndMakeVisible (k.slider);

        k.label.setText (names[i], juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (k.label);

        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processorRef.apvts, ids[i], k.slider);
    }

    setSize (480, 260);
}

void LoFiAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff2b2233));
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (24.0f));
    g.drawText ("LoFi Machine", 0, 10, getWidth(), 30, juce::Justification::centred);
}

void LoFiAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    area.removeFromTop (40);

    const int colWidth = area.getWidth() / (int) knobs.size();

    for (auto& k : knobs)
    {
        auto col = area.removeFromLeft (colWidth);
        k.label.setBounds (col.removeFromTop (24));
        k.slider.setBounds (col);
    }
}
