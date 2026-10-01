#include "PluginEditor.h"
#include <BinaryData.h>

// Размер окна плагина (пропорции 7:5 как у фоновой картинки 2100x1500)
static constexpr int kWidth  = 1050;
static constexpr int kHeight = 750;

LoFiAudioProcessorEditor::LoFiAudioProcessorEditor (LoFiAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    background = juce::ImageCache::getFromMemory (BinaryData::background_png,
                                                  BinaryData::background_pngSize);

    // --- выпадающие списки ---
    initCombo (modeBox,       "mode",         "MODE");
    initCombo (syncBox,       "echoSync",     "ECHO SYNC");
    initCombo (divisionBox,   "echoDivision", "DIVISION");
    initCombo (reverbTypeBox, "reverbType",   "REVERB TYPE");

    // переключение режима (и из автоматизации тоже) обновляет видимость ручек
    modeBox.box.onChange = [this] { updateModeVisibility(); };

    pingPongButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    pingPongButton.setColour (juce::ToggleButton::tickColourId, juce::Colours::white);
    addAndMakeVisible (pingPongButton);
    pingPongAttachment = std::make_unique<ButtonAttachment> (processorRef.apvts, "echoPingPong", pingPongButton);

    // --- ручки Lo-Fi ---
    initKnob (bits,          "bits",         "BITS");
    initKnob (downsample,    "downsample",   "DOWNSAMPLE");
    initKnob (echoTime,      "echoTime",     "ECHO TIME");
    initKnob (manualBpm,     "manualBpm",    "BPM");
    initKnob (echoFeedback,  "echoFeedback", "FEEDBACK");
    initKnob (echoLevel,     "echoLevel",    "ECHO LEVEL");
    initKnob (reverbLength,  "reverbLength", "REVERB LENGTH");
    initKnob (reverbLevel,   "reverbLevel",  "REVERB LEVEL");
    initKnob (hp,            "hp",           "HP");
    initKnob (lp,            "lp",           "LP");
    initKnob (mono,          "mono",         "MONO");
    initKnob (mix,           "mix",          "MIX");
    initKnob (output,        "output",       "OUTPUT");

    // --- ручки Drums ---
    initKnob (crush,       "drumsCrush",  "CRUSH");
    initKnob (crunch,      "drumsCrunch", "CRUNCH");
    initKnob (drumsMix,    "drumsMix",    "MIX");
    initKnob (drumsOutput, "drumsOutput", "OUTPUT");

    // --- ручки Keys ---
    initKnob (keysWow,     "keysWow",     "WOW");
    initKnob (keysFlutter, "keysFlutter", "FLUTTER");
    initKnob (keysTape,    "keysTape",    "TAPE");
    initKnob (keysDust,    "keysDust",    "DUST");
    initKnob (keysChorus,  "keysChorus",  "CHORUS");
    initKnob (keysMix,     "keysMix",     "MIX");
    initKnob (keysOutput,  "keysOutput",  "OUTPUT");

    // --- ручки Bass ---
    initKnob (bassDrive,     "bassDrive",     "DRIVE");
    initKnob (bassHarmonics, "bassHarmonics", "HARMONICS");
    initKnob (bassTone,      "bassTone",      "TONE");
    initKnob (bassSquash,    "bassSquash",    "SQUASH");
    initKnob (bassSub,       "bassSub",       "SUB");
    initKnob (bassMix,       "bassMix",       "MIX");
    initKnob (bassOutput,    "bassOutput",    "OUTPUT");

    setSize (kWidth, kHeight);
    updateModeVisibility();
}

void LoFiAudioProcessorEditor::initKnob (Knob& k, const juce::String& paramId, const juce::String& name)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    k.slider.setColour (juce::Slider::rotarySliderFillColourId,    juce::Colours::white);
    k.slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff4a4a4a));
    k.slider.setColour (juce::Slider::thumbColourId,               juce::Colours::white);
    k.slider.setColour (juce::Slider::textBoxTextColourId,         juce::Colours::white);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<SliderAttachment> (processorRef.apvts, paramId, k.slider);
}

void LoFiAudioProcessorEditor::initCombo (Combo& c, const juce::String& paramId, const juce::String& name)
{
    // список пунктов берём из параметра (порядок должен совпадать)
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processorRef.apvts.getParameter (paramId)))
        c.box.addItemList (choice->choices, 1);

    c.box.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff111111));
    c.box.setColour (juce::ComboBox::textColourId,       juce::Colours::white);
    c.box.setColour (juce::ComboBox::outlineColourId,    juce::Colour (0xff666666));
    c.box.setColour (juce::ComboBox::arrowColourId,      juce::Colours::white);
    addAndMakeVisible (c.box);

    c.label.setText (name, juce::dontSendNotification);
    c.label.setJustificationType (juce::Justification::centredLeft);
    c.label.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (c.label);

    c.attachment = std::make_unique<ComboAttachment> (processorRef.apvts, paramId, c.box);
}

void LoFiAudioProcessorEditor::setKnobVisible (Knob& k, bool visible)
{
    k.slider.setVisible (visible);
    k.label.setVisible (visible);
}

void LoFiAudioProcessorEditor::setComboVisible (Combo& c, bool visible)
{
    c.box.setVisible (visible);
    c.label.setVisible (visible);
}

void LoFiAudioProcessorEditor::updateModeVisibility()
{
    const int  modeIndex = modeBox.box.getSelectedItemIndex();
    const bool drums = modeIndex == 1;
    const bool keys  = modeIndex == 2;
    const bool bassMode = modeIndex == 3;
    const bool lofi  = ! drums && ! keys && ! bassMode;    // режим Space

    for (auto* k : { &bits, &downsample, &echoTime, &manualBpm, &echoFeedback, &echoLevel,
                     &reverbLength, &reverbLevel, &hp, &lp, &mono, &mix, &output })
        setKnobVisible (*k, lofi);

    for (auto* k : { &crush, &crunch, &drumsMix, &drumsOutput })
        setKnobVisible (*k, drums);

    for (auto* k : { &keysWow, &keysFlutter, &keysTape, &keysDust, &keysChorus, &keysMix, &keysOutput })
        setKnobVisible (*k, keys);

    for (auto* k : { &bassDrive, &bassHarmonics, &bassTone, &bassSquash, &bassSub, &bassMix, &bassOutput })
        setKnobVisible (*k, bassMode);

    setComboVisible (syncBox, lofi);
    setComboVisible (divisionBox, lofi);
    setComboVisible (reverbTypeBox, lofi);
    pingPongButton.setVisible (lofi);
}

void LoFiAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    if (background.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (background, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    }
}

void LoFiAudioProcessorEditor::placeKnob (Knob& k, int centreX, int y, int size)
{
    k.label.setBounds (centreX - size / 2 - 10, y, size + 20, 18);
    k.slider.setBounds (centreX - size / 2 - 10, y + 18, size + 20, size + 20);
}

void LoFiAudioProcessorEditor::placeCombo (Combo& c, int x, int y, int w)
{
    c.label.setBounds (x, y, w, 16);
    c.box.setBounds (x, y + 18, w, 26);
}

void LoFiAudioProcessorEditor::resized()
{
    // ---------- верхняя полоса: режим и списки ----------
    const int topY = 24;
    placeCombo (modeBox,       30,  topY, 120);
    placeCombo (syncBox,       200, topY, 100);
    placeCombo (divisionBox,   320, topY, 100);
    placeCombo (reverbTypeBox, 440, topY, 110);
    pingPongButton.setBounds (580, topY + 18, 130, 26);

    // ---------- Lo-Fi режим: два ряда ручек под логотипом ----------
    {
        const int size = 80;
        const int row1Y = 400;
        const int row2Y = 570;

        // ряд 1: Bits, Downsample | Echo
        Knob* row1[] = { &bits, &downsample, &echoTime, &manualBpm, &echoFeedback, &echoLevel };
        for (int i = 0; i < 6; ++i)
            placeKnob (*row1[i], 75 + 150 * i + 75, row1Y, size);

        // ряд 2: Reverb | Filters | Master
        Knob* row2[] = { &reverbLength, &reverbLevel, &hp, &lp, &mono, &mix, &output };
        for (int i = 0; i < 7; ++i)
            placeKnob (*row2[i], 35 + 140 * i + 70, row2Y, size);
    }

    // ---------- Drums режим: 4 крупные ручки ----------
    {
        const int size = 130;
        const int y = 440;
        Knob* ks[] = { &crush, &crunch, &drumsMix, &drumsOutput };
        for (int i = 0; i < 4; ++i)
            placeKnob (*ks[i], kWidth * (i + 1) / 5, y, size);
    }

    // ---------- Keys режим: 7 ручек в ряд ----------
    {
        const int size = 100;
        const int y = 430;
        Knob* ks[] = { &keysWow, &keysFlutter, &keysTape, &keysDust, &keysChorus, &keysMix, &keysOutput };
        for (int i = 0; i < 7; ++i)
            placeKnob (*ks[i], kWidth * (i + 1) / 8, y, size);
    }

    // ---------- Bass режим: 7 ручек в ряд ----------
    {
        const int size = 100;
        const int y = 430;
        Knob* ks[] = { &bassDrive, &bassHarmonics, &bassTone, &bassSquash, &bassSub, &bassMix, &bassOutput };
        for (int i = 0; i < 7; ++i)
            placeKnob (*ks[i], kWidth * (i + 1) / 8, y, size);
    }
}
