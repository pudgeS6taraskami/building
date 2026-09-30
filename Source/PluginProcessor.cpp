#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // длительность в долях четверти (beat)
    const float divBeats[9] = { 0.25f, 1.0f / 3.0f, 0.5f, 2.0f / 3.0f, 0.75f, 1.0f, 1.5f, 2.0f, 4.0f };

    // мягкое ограничение выше порога, чтобы драм-режим не давал жёстких перегрузов
    inline float softClip (float x)
    {
        const float t = 0.8f;
        const float a = std::abs (x);
        if (a <= t)
            return x;

        const float s = t + (1.0f - t) * std::tanh ((a - t) / (1.0f - t));
        return x < 0.0f ? -s : s;
    }
}

LoFiAudioProcessor::LoFiAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    pMode          = apvts.getRawParameterValue ("mode");
    pBits          = apvts.getRawParameterValue ("bits");
    pDownsample    = apvts.getRawParameterValue ("downsample");
    pEchoTime      = apvts.getRawParameterValue ("echoTime");
    pEchoSync      = apvts.getRawParameterValue ("echoSync");
    pEchoDivision  = apvts.getRawParameterValue ("echoDivision");
    pManualBpm     = apvts.getRawParameterValue ("manualBpm");
    pEchoFeedback  = apvts.getRawParameterValue ("echoFeedback");
    pEchoPingPong  = apvts.getRawParameterValue ("echoPingPong");
    pEchoLevel     = apvts.getRawParameterValue ("echoLevel");
    pReverbType    = apvts.getRawParameterValue ("reverbType");
    pReverbLength  = apvts.getRawParameterValue ("reverbLength");
    pReverbLevel   = apvts.getRawParameterValue ("reverbLevel");
    pHp            = apvts.getRawParameterValue ("hp");
    pLp            = apvts.getRawParameterValue ("lp");
    pMono          = apvts.getRawParameterValue ("mono");
    pMix           = apvts.getRawParameterValue ("mix");
    pOutput        = apvts.getRawParameterValue ("output");
    pDrumsCrush    = apvts.getRawParameterValue ("drumsCrush");
    pDrumsCrunch   = apvts.getRawParameterValue ("drumsCrunch");
    pDrumsMix      = apvts.getRawParameterValue ("drumsMix");
    pDrumsOutput   = apvts.getRawParameterValue ("drumsOutput");
    pKeysWow       = apvts.getRawParameterValue ("keysWow");
    pKeysFlutter   = apvts.getRawParameterValue ("keysFlutter");
    pKeysTape      = apvts.getRawParameterValue ("keysTape");
    pKeysDust      = apvts.getRawParameterValue ("keysDust");
    pKeysChorus    = apvts.getRawParameterValue ("keysChorus");
    pKeysMix       = apvts.getRawParameterValue ("keysMix");
    pKeysOutput    = apvts.getRawParameterValue ("keysOutput");
}

juce::AudioProcessorValueTreeState::ParameterLayout LoFiAudioProcessor::createParameterLayout()
{
    using juce::AudioParameterFloat;
    using juce::AudioParameterChoice;
    using juce::AudioParameterBool;
    using juce::NormalisableRange;
    using juce::ParameterID;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // --- Режим ---
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode",
        juce::StringArray { "Space", "Drums", "Keys" }, 0));

    // --- Lo-fi ---
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "bits", 1 }, "Bits",
        NormalisableRange<float> (4.0f, 16.0f, 0.1f), 12.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "downsample", 1 }, "Downsample",
        NormalisableRange<float> (1.0f, 32.0f, 1.0f), 2.0f));

    // --- Echo ---
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "echoTime", 1 }, "Echo Time (ms)",
        NormalisableRange<float> (10.0f, 2000.0f, 1.0f, 0.5f), 350.0f));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "echoSync", 1 }, "Echo Sync",
        juce::StringArray { "MS", "BPM", "HOST" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "echoDivision", 1 }, "Echo Division",
        juce::StringArray { "1/16", "1/8T", "1/8", "1/4T", "1/8.", "1/4", "1/4.", "1/2", "1/1" }, 4));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "manualBpm", 1 }, "Manual BPM",
        NormalisableRange<float> (40.0f, 240.0f, 0.1f), 120.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "echoFeedback", 1 }, "Echo Feedback",
        NormalisableRange<float> (0.0f, 0.95f, 0.01f), 0.4f));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "echoPingPong", 1 }, "Ping Pong", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "echoLevel", 1 }, "Echo Level",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f));

    // --- Reverb ---
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "reverbType", 1 }, "Reverb Type",
        juce::StringArray { "Spring", "Plate" }, 1));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "reverbLength", 1 }, "Reverb Length",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "reverbLevel", 1 }, "Reverb Level",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f));

    // --- Filters / Mono ---
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "hp", 1 }, "HP (Hz)",
        NormalisableRange<float> (20.0f, 2000.0f, 1.0f, 0.3f), 20.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "lp", 1 }, "LP (Hz)",
        NormalisableRange<float> (500.0f, 20000.0f, 1.0f, 0.3f), 20000.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "mono", 1 }, "Mono",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    // --- Master (Lo-Fi) ---
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.8f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output (dB)",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));

    // --- Drums ---
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "drumsCrush", 1 }, "Drums Crush",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.55f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "drumsCrunch", 1 }, "Drums Crunch",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.45f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "drumsMix", 1 }, "Drums Mix",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "drumsOutput", 1 }, "Drums Output (dB)",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));

    // --- Keys ---
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "keysWow", 1 }, "Keys Wow",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.35f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "keysFlutter", 1 }, "Keys Flutter",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.25f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "keysTape", 1 }, "Keys Tape",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.4f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "keysDust", 1 }, "Keys Dust",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.15f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "keysChorus", 1 }, "Keys Chorus",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "keysMix", 1 }, "Keys Mix",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "keysOutput", 1 }, "Keys Output (dB)",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));

    return layout;
}

double LoFiAudioProcessor::getDelayTimeSeconds()
{
    const int mode = (int) std::round (pEchoSync->load());
    double seconds = (double) pEchoTime->load() * 0.001;

    if (mode != 0)
    {
        double bpm = (double) pManualBpm->load();

        if (mode == 2)
        {
            if (auto* ph = getPlayHead())
            {
                if (auto pos = ph->getPosition())
                {
                    if (auto hostBpm = pos->getBpm())
                        bpm = *hostBpm;
                }
            }
        }

        bpm = juce::jlimit (20.0, 400.0, bpm);
        const int idx = juce::jlimit (0, 8, (int) std::round (pEchoDivision->load()));
        seconds = (60.0 / bpm) * (double) divBeats[idx];
    }

    return juce::jlimit (0.001, 4.5, seconds);
}

void LoFiAudioProcessor::resetLoFiState()
{
    held[0] = held[1] = 0.0f;
    counter[0] = counter[1] = 0;
    fbLp[0] = fbLp[1] = 0.0f;

    delays[0].clear();
    delays[1].clear();

    reverb.reset();
    for (int c = 0; c < 2; ++c) { hp[c].reset(); lp[c].reset(); }
}

void LoFiAudioProcessor::resetDrumsState()
{
    drumsEnv = 0.0f;
    for (int c = 0; c < 2; ++c) { dcX1[c] = 0.0f; dcY1[c] = 0.0f; toneLp[c] = 0.0f; }
}

void LoFiAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    const int maxDelay = (int) (sampleRate * 5.0) + 4;
    delays[0].prepare (maxDelay);
    delays[1].prepare (maxDelay);

    mixSmooth.reset (sampleRate, 0.02);
    outSmooth.reset (sampleRate, 0.02);
    echoLevelSmooth.reset (sampleRate, 0.02);
    reverbLevelSmooth.reset (sampleRate, 0.02);
    delaySmooth.reset (sampleRate, 0.3);   // плавный "плёночный" сдвиг при смене времени

    mixSmooth.setCurrentAndTargetValue (pMix->load());
    outSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    echoLevelSmooth.setCurrentAndTargetValue (pEchoLevel->load());
    reverbLevelSmooth.setCurrentAndTargetValue (pReverbLevel->load());
    delaySmooth.setCurrentAndTargetValue ((float) (getDelayTimeSeconds() * sampleRate));

    drumsCrushSmooth.reset (sampleRate, 0.03);
    drumsCrunchSmooth.reset (sampleRate, 0.03);
    drumsMixSmooth.reset (sampleRate, 0.02);
    drumsOutSmooth.reset (sampleRate, 0.02);

    drumsCrushSmooth.setCurrentAndTargetValue (pDrumsCrush->load());
    drumsCrunchSmooth.setCurrentAndTargetValue (pDrumsCrunch->load());
    drumsMixSmooth.setCurrentAndTargetValue (pDrumsMix->load());
    drumsOutSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pDrumsOutput->load()));

    fbCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 3500.0f / (float) sampleRate);
    reverb.setSampleRate (sampleRate);

    resetLoFiState();
    resetDrumsState();
    keys.prepare (sampleRate);
    keys.reset();

    wetBuf.setSize (2, juce::jmax (1, samplesPerBlock));
    revBuf.setSize (2, juce::jmax (1, samplesPerBlock));

    lastMode = (int) std::round (pMode->load());
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

    const int numCh      = juce::jmin (getTotalNumInputChannels(), 2);
    const int numSamples = buffer.getNumSamples();
    if (numCh == 0 || numSamples == 0)
        return;

    const int mode = juce::jlimit (0, 2, (int) std::round (pMode->load()));

    // при смене режима чистим состояние того режима, в который переходим,
    // чтобы старые "хвосты" не всплывали
    if (mode != lastMode)
    {
        if (mode == 1)      resetDrumsState();
        else if (mode == 2) keys.reset();
        else                resetLoFiState();
        lastMode = mode;
    }

    if (mode == 1)
        processDrums (buffer, numCh, numSamples);
    else if (mode == 2)
        processKeys (buffer, numCh, numSamples);
    else
        processLoFi (buffer, numCh, numSamples);
}

// ============================ LO-FI РЕЖИМ ============================
void LoFiAudioProcessor::processLoFi (juce::AudioBuffer<float>& buffer, int numCh, int numSamples)
{
    if (wetBuf.getNumSamples() < numSamples)
    {
        wetBuf.setSize (2, numSamples, false, false, true);
        revBuf.setSize (2, numSamples, false, false, true);
    }

    // ---- параметры на этот блок ----
    const float bits   = pBits->load();
    const int   hold   = juce::jmax (1, (int) std::round (pDownsample->load()));
    const float levels = std::pow (2.0f, bits - 1.0f);
    const float fb     = pEchoFeedback->load();
    const bool  ping   = (pEchoPingPong->load() > 0.5f) && numCh == 2;
    const float monoAmt = pMono->load();

    delaySmooth.setTargetValue ((float) (getDelayTimeSeconds() * currentSampleRate));
    echoLevelSmooth.setTargetValue (pEchoLevel->load());
    reverbLevelSmooth.setTargetValue (pReverbLevel->load());
    mixSmooth.setTargetValue (pMix->load());
    outSmooth.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));

    // реверб: Spring короче и "уже", Plate длиннее и шире
    {
        const bool spring = pReverbType->load() < 0.5f;
        const float len = pReverbLength->load();
        juce::Reverb::Parameters rp;
        rp.roomSize   = spring ? juce::jmap (len, 0.1f, 0.7f) : juce::jmap (len, 0.2f, 0.95f);
        rp.damping    = spring ? 0.6f : 0.25f;
        rp.width      = spring ? 0.3f : 1.0f;
        rp.wetLevel   = 0.33f;
        rp.dryLevel   = 0.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);
    }

    // фильтры
    const float hpFreq = pHp->load();
    const float lpFreq = pLp->load();
    for (int c = 0; c < 2; ++c)
    {
        hp[c].set (currentSampleRate, hpFreq, true);
        lp[c].set (currentSampleRate, lpFreq, false);
    }

    const float* in[2] = { buffer.getReadPointer (0), numCh > 1 ? buffer.getReadPointer (1) : nullptr };
    float* wet[2]      = { wetBuf.getWritePointer (0), wetBuf.getWritePointer (1) };

    // ---- проход 1: lo-fi (sample-rate reduction + bitcrush) -> эхо ----
    for (int n = 0; n < numSamples; ++n)
    {
        const float delaySamples = delaySmooth.getNextValue();
        const float echoLvl      = echoLevelSmooth.getNextValue();

        float cr[2]  = { 0.0f, 0.0f };
        float del[2] = { 0.0f, 0.0f };
        float f[2]   = { 0.0f, 0.0f };

        for (int c = 0; c < numCh; ++c)
        {
            const float x = in[c][n];

            if (counter[c] <= 0)
            {
                held[c] = x;
                counter[c] = hold;
            }
            counter[c]--;

            cr[c]  = std::round (held[c] * levels) / levels;
            del[c] = delays[c].read (delaySamples);

            // тёмный тон в цепи обратной связи + мягкое ограничение
            fbLp[c] += fbCoef * (del[c] - fbLp[c]);
            f[c] = std::tanh (fbLp[c]);
        }

        if (ping)
        {
            const float m = 0.5f * (cr[0] + cr[1]);
            delays[0].write (m + fb * f[1]);
            delays[1].write (fb * f[0]);
        }
        else
        {
            for (int c = 0; c < numCh; ++c)
                delays[c].write (cr[c] + fb * f[c]);
        }

        for (int c = 0; c < numCh; ++c)
            wet[c][n] = cr[c] + echoLvl * del[c];
    }

    // ---- реверб (вход = lo-fi + эхо) ----
    for (int c = 0; c < numCh; ++c)
        revBuf.copyFrom (c, 0, wetBuf, c, 0, numSamples);

    if (numCh == 2)
        reverb.processStereo (revBuf.getWritePointer (0), revBuf.getWritePointer (1), numSamples);
    else
        reverb.processMono (revBuf.getWritePointer (0), numSamples);

    // ---- проход 2: сумма, HP/LP, Mono, Mix, Output ----
    float* out[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };
    const float* rev[2] = { revBuf.getReadPointer (0), revBuf.getReadPointer (1) };

    for (int n = 0; n < numSamples; ++n)
    {
        const float mix  = mixSmooth.getNextValue();
        const float gain = outSmooth.getNextValue();
        const float rLvl = reverbLevelSmooth.getNextValue();

        float p[2] = { 0.0f, 0.0f };

        for (int c = 0; c < numCh; ++c)
        {
            p[c] = wet[c][n] + rLvl * rev[c][n];
            p[c] = hp[c].process (p[c]);
            p[c] = lp[c].process (p[c]);
        }

        if (numCh == 2 && monoAmt > 0.0f)
        {
            const float m = 0.5f * (p[0] + p[1]);
            p[0] += monoAmt * (m - p[0]);
            p[1] += monoAmt * (m - p[1]);
        }

        for (int c = 0; c < numCh; ++c)
        {
            const float dry = out[c][n];
            out[c][n] = (dry * (1.0f - mix) + p[c] * mix) * gain;
        }
    }
}

// ============================ DRUMS РЕЖИМ ============================
// Crush  = жёсткая быстрая компрессия с автоподъёмом громкости
// Crunch = насыщение/дисторшн + сужение верха
void LoFiAudioProcessor::processDrums (juce::AudioBuffer<float>& buffer, int numCh, int numSamples)
{
    drumsCrushSmooth.setTargetValue (pDrumsCrush->load());
    drumsCrunchSmooth.setTargetValue (pDrumsCrunch->load());
    drumsMixSmooth.setTargetValue (pDrumsMix->load());
    drumsOutSmooth.setTargetValue (juce::Decibels::decibelsToGain (pDrumsOutput->load()));

    const float sr = (float) currentSampleRate;
    const float attCoef = std::exp (-1.0f / (0.0005f * sr));   // атака детектора 0.5 мс

    const float toneFc = juce::jlimit (1000.0f, sr * 0.45f,
                                       juce::jmap (pDrumsCrunch->load(), 20000.0f, 6000.0f));
    const float toneCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * toneFc / sr);

    float* data[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };

    for (int n = 0; n < numSamples; ++n)
    {
        const float crush  = drumsCrushSmooth.getNextValue();
        const float crunch = drumsCrunchSmooth.getNextValue();
        const float mix    = drumsMixSmooth.getNextValue();
        const float gain   = drumsOutSmooth.getNextValue();

        // --- параметры компрессора от Crush ---
        const float thresholdDb = juce::jmap (crush, -3.0f, -42.0f);
        const float ratio       = juce::jmap (crush, 2.0f, 30.0f);
        const float slope       = 1.0f - 1.0f / ratio;
        const float makeupDb    = 0.65f * (-thresholdDb) * slope;
        const float relCoef     = std::exp (-1.0f / (juce::jmap (crush, 0.15f, 0.05f) * sr));

        // --- детектор (стерео-связка, пиковый) ---
        float peak = 0.0f;
        for (int c = 0; c < numCh; ++c)
            peak = juce::jmax (peak, std::abs (data[c][n]));

        if (peak > drumsEnv) drumsEnv = attCoef * drumsEnv + (1.0f - attCoef) * peak;
        else                 drumsEnv = relCoef * drumsEnv + (1.0f - relCoef) * peak;

        const float levelDb = juce::Decibels::gainToDecibels (drumsEnv, -100.0f);
        const float over    = levelDb - thresholdDb;
        const float knee    = 6.0f;

        float grDb = 0.0f;
        if (2.0f * over > knee)
            grDb = slope * over;
        else if (2.0f * over > -knee)
        {
            const float t = over + knee * 0.5f;
            grDb = slope * t * t / (2.0f * knee);
        }

        const float compGain = juce::Decibels::decibelsToGain (makeupDb - grDb);

        // --- параметры сатурации от Crunch ---
        const float drive = 0.1f + 15.0f * crunch * crunch;
        const float bias  = 0.3f * crunch;
        const float tb    = std::tanh (bias);
        const float norm  = 1.0f / (std::tanh (drive + bias) - tb);

        for (int c = 0; c < numCh; ++c)
        {
            const float x = data[c][n];

            float y = x * compGain;

            // crunch: асимметричная сатурация, нормированная на полный уровень
            y = (std::tanh (drive * y + bias) - tb) * norm;

            // убираем постоянную составляющую после асимметрии
            const float dc = y - dcX1[c] + 0.995f * dcY1[c];
            dcX1[c] = y;
            dcY1[c] = dc;
            y = dc;

            // сужение верха: чем больше crunch, тем темнее
            toneLp[c] += toneCoef * (y - toneLp[c]);
            y = toneLp[c];

            y = softClip (y);

            data[c][n] = (x * (1.0f - mix) + y * mix) * gain;
        }
    }
}

// ============================ KEYS РЕЖИМ ============================
void LoFiAudioProcessor::processKeys (juce::AudioBuffer<float>& buffer, int numCh, int numSamples)
{
    KeysEffect::Params kp;
    kp.wow      = pKeysWow->load();
    kp.flutter  = pKeysFlutter->load();
    kp.tape     = pKeysTape->load();
    kp.dust     = pKeysDust->load();
    kp.chorus   = pKeysChorus->load();
    kp.mix      = pKeysMix->load();
    kp.outputDb = pKeysOutput->load();

    keys.process (buffer, numCh, numSamples, kp);
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
