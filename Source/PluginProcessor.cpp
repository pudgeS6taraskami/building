#include "PluginProcessor.h"
#include "PluginEditor.h"

LoFiAudioProcessor::LoFiAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    bitsParam       = apvts.getRawParameterValue ("bits");
    downsampleParam = apvts.getRawParameterValue ("downsample");
    mixParam        = apvts.getRawParameterValue ("mix");
    outputParam     = apvts.getRawParameterValue ("output");
}

juce::AudioProcessorValueTreeState::ParameterLayout LoFiAudioProcessor::createParameterLayout()
{
    using juce::AudioParameterFloat;
    using juce::NormalisableRange;
    using juce::ParameterID;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "bits", 1 }, "Bits",
        NormalisableRange<float> (4.0f, 16.0f, 0.1f), 12.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "downsample", 1 }, "Downsample",
        NormalisableRange<float> (1.0f, 32.0f, 1.0f), 4.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "mix", 1 }, "Mix",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "output", 1 }, "Output",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));

    return layout;
}

void LoFiAudioProcessor::prepareToPlay (double sampleRate, int)
{
    mixSmooth.reset (sampleRate, 0.02);
    outSmooth.reset (sampleRate, 0.02);
    mixSmooth.setCurrentAndTargetValue (mixParam->load());
    outSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));

    held.fill (0.0f);
    counter.fill (0);
}

bool LoFiAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

void LoFiAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const int numChannels = juce::jmin (getTotalNumInputChannels(), 2);
    const int numSamples  = buffer.getNumSamples();

    const float bits   = bitsParam->load();
    const int   hold   = juce::jmax (1, (int) std::round (downsampleParam->load()));
    const float levels = std::pow (2.0f, bits - 1.0f);

    mixSmooth.setTargetValue (mixParam->load());
    outSmooth.setTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));

    for (int n = 0; n < numSamples; ++n)
    {
        const float mix  = mixSmooth.getNextValue();
        const float gain = outSmooth.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            const float dry = data[n];

            // 1) sample-rate reduction: держим сэмпл несколько тактов
            if (counter[(size_t) ch] <= 0)
            {
                held[(size_t) ch] = dry;
                counter[(size_t) ch] = hold;
            }
            counter[(size_t) ch]--;

            // 2) bitcrush: округляем до нужной разрядности
            const float crushed = std::round (held[(size_t) ch] * levels) / levels;

            // 3) dry/wet и выходная громкость
            data[n] = (dry * (1.0f - mix) + crushed * mix) * gain;
        }
    }
}

juce::AudioProcessorEditor* LoFiAudioProcessor::createEditor()
{
    return new LoFiAudioProcessorEditor (*this);
}

void LoFiAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void LoFiAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LoFiAudioProcessor();
}
