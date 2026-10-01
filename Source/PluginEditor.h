#pragma once
#include "PluginProcessor.h"
#include <memory>

class LoFiAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit LoFiAudioProcessorEditor (LoFiAudioProcessor&);
    ~LoFiAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // атачмент объявлен последним -> уничтожается первым
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    struct Combo
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<ComboAttachment> attachment;
    };

    void initKnob  (Knob& k, const juce::String& paramId, const juce::String& name);
    void initCombo (Combo& c, const juce::String& paramId, const juce::String& name);
    void setKnobVisible  (Knob& k, bool visible);
    void setComboVisible (Combo& c, bool visible);
    void updateModeVisibility();

    void placeKnob  (Knob& k, int centreX, int y, int size);
    void placeCombo (Combo& c, int x, int y, int w);

    LoFiAudioProcessor& processorRef;
    juce::Image background;

    Combo modeBox, syncBox, divisionBox, reverbTypeBox;
    juce::ToggleButton pingPongButton { "Ping Pong" };
    std::unique_ptr<ButtonAttachment> pingPongAttachment;

    // Lo-Fi режим
    Knob bits, downsample, echoTime, manualBpm, echoFeedback, echoLevel,
         reverbLength, reverbLevel, hp, lp, mono, mix, output;

    // Drums режим
    Knob crush, crunch, drumsMix, drumsOutput;

    // Keys режим
    Knob keysWow, keysFlutter, keysTape, keysDust, keysChorus, keysMix, keysOutput;

    // Bass режим
    Knob bassDrive, bassHarmonics, bassTone, bassSquash, bassSub, bassMix, bassOutput;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoFiAudioProcessorEditor)
};
