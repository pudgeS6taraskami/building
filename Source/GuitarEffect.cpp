#include "GuitarEffect.h"

void GuitarEffect::prepare (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;

    const int size = (int) (sampleRate * 0.05) + 4;   // 50 мс
    for (int c = 0; c < 2; ++c)
    {
        wobbleDelay[c].prepare (size);
        dryDelay[c].prepare (size);
        inHp[c].set (sr, 80.0f, true);
        cabHp[c].set (sr, 60.0f, true);
    }

    driveS.reset (sampleRate, 0.03);
    tremS.reset (sampleRate, 0.03);
    wobbleS.reset (sampleRate, 0.03);
    roomS.reset (sampleRate, 0.03);
    mixS.reset (sampleRate, 0.02);
    outS.reset (sampleRate, 0.02);

    reverb.setSampleRate (sampleRate);

    const int n = juce::jmax (1, samplesPerBlock);
    dryBuf.setSize (2, n);
    procBuf.setSize (2, n);
    revBuf.setSize (2, n);

    needsInit = true;
}

void GuitarEffect::reset()
{
    for (int c = 0; c < 2; ++c)
    {
        wobbleDelay[c].clear();
        dryDelay[c].clear();
        inHp[c].reset();
        cabHp[c].reset();
        toneA[c].reset();
        toneB[c].reset();
        dcX1[c] = dcY1[c] = 0.0f;
    }
    reverb.reset();
    needsInit = true;
}

void GuitarEffect::process (juce::AudioBuffer<float>& buffer, int numCh, int numSamples, const Params& p)
{
    const float outGain = juce::Decibels::decibelsToGain (p.outputDb);

    if (needsInit)
    {
        driveS.setCurrentAndTargetValue (p.drive);
        tremS.setCurrentAndTargetValue (p.tremolo);
        wobbleS.setCurrentAndTargetValue (p.wobble);
        roomS.setCurrentAndTargetValue (p.room);
        mixS.setCurrentAndTargetValue (p.mix);
        outS.setCurrentAndTargetValue (outGain);
        needsInit = false;
    }
    else
    {
        driveS.setTargetValue (p.drive);
        tremS.setTargetValue (p.tremolo);
        wobbleS.setTargetValue (p.wobble);
        roomS.setTargetValue (p.room);
        mixS.setTargetValue (p.mix);
        outS.setTargetValue (outGain);
    }

    if (dryBuf.getNumSamples() < numSamples)
    {
        dryBuf.setSize (2, numSamples, false, false, true);
        procBuf.setSize (2, numSamples, false, false, true);
        revBuf.setSize (2, numSamples, false, false, true);
    }

    const float fs = (float) sr;
    const float twoPi = juce::MathConstants<float>::twoPi;

    // Tone: 1.5 кГц (тёмный) ... 12 кГц (яркий); 24 дБ/окт как у колонки
    const float toneFc = juce::jlimit (800.0f, fs * 0.45f, 1500.0f * std::pow (8.0f, p.tone));
    for (int c = 0; c < 2; ++c)
    {
        toneA[c].set (sr, toneFc, false);
        toneB[c].set (sr, toneFc, false);
    }

    // маленькая комната
    {
        juce::Reverb::Parameters rp;
        rp.roomSize   = 0.35f;
        rp.damping    = 0.5f;
        rp.width      = 0.8f;
        rp.wetLevel   = 0.33f;
        rp.dryLevel   = 0.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);
    }

    const float baseSec = 0.004f;      // база вибрато
    const float wobASec = 0.0010f;     // медленное плавание (~0.9 Гц)
    const float wobBSec = 0.00012f;    // быстрая дрожь (~4.7 Гц)
    const float baseSamples = baseSec * fs;
    const float tremInc = p.rate / fs;
    const float dcR = std::exp (-twoPi * 25.0f / fs);

    float* data[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };
    float* dryW[2] = { dryBuf.getWritePointer (0), dryBuf.getWritePointer (1) };
    float* proc[2] = { procBuf.getWritePointer (0), procBuf.getWritePointer (1) };

    // ---------- проход 1: усилитель, колонка, тремоло, вибрато ----------
    for (int n = 0; n < numSamples; ++n)
    {
        const float drive  = driveS.getNextValue();
        const float tremD  = tremS.getNextValue();
        const float wobble = wobbleS.getNextValue();

        const float d    = 1.0f + 20.0f * drive * drive;
        const float bias = 0.15f * drive;
        const float tb   = std::tanh (bias);
        const float comp = 1.0f / (1.0f + 0.08f * (d - 1.0f));

        const float tremGain = 1.0f - tremD * (0.5f + 0.5f * std::sin (twoPi * tremPhase));

        const float wobLfo = wobASec * std::sin (twoPi * wobPhase1) + wobBSec * std::sin (twoPi * wobPhase2);
        const float wobDelay = (baseSec + wobble * wobLfo) * fs;

        for (int c = 0; c < numCh; ++c)
        {
            const float x = data[c][n];

            // овердрайв: сначала срезаем самый низ, чтобы не "бубнило"
            float y = inHp[c].process (x);
            y = (std::tanh (d * y + bias) - tb) * comp;

            const float dc = y - dcX1[c] + dcR * dcY1[c];
            dcX1[c] = y;
            dcY1[c] = dc;
            y = dc;

            // колонка: HP + 24 дБ/окт LP
            y = cabHp[c].process (y);
            y = toneB[c].process (toneA[c].process (y));

            y *= tremGain;

            // вибрато (модулируемая задержка)
            wobbleDelay[c].write (y);
            proc[c][n] = wobbleDelay[c].read (wobDelay);

            // сухой сигнал с той же базовой задержкой
            dryDelay[c].write (x);
            dryW[c][n] = dryDelay[c].read (baseSamples);
        }

        tremPhase += tremInc;  if (tremPhase >= 1.0f) tremPhase -= 1.0f;
        wobPhase1 += 0.9f / fs; if (wobPhase1 >= 1.0f) wobPhase1 -= 1.0f;
        wobPhase2 += 4.7f / fs; if (wobPhase2 >= 1.0f) wobPhase2 -= 1.0f;
    }

    // ---------- комната ----------
    for (int c = 0; c < numCh; ++c)
        revBuf.copyFrom (c, 0, procBuf, c, 0, numSamples);

    if (numCh == 2)
        reverb.processStereo (revBuf.getWritePointer (0), revBuf.getWritePointer (1), numSamples);
    else
        reverb.processMono (revBuf.getWritePointer (0), numSamples);

    const float* rev[2] = { revBuf.getReadPointer (0), revBuf.getReadPointer (1) };

    // ---------- проход 2: Room + Mix + Output ----------
    for (int n = 0; n < numSamples; ++n)
    {
        const float room = roomS.getNextValue();
        const float mix  = mixS.getNextValue();
        const float gain = outS.getNextValue();

        for (int c = 0; c < numCh; ++c)
        {
            const float wet = proc[c][n] + room * rev[c][n];
            data[c][n] = (dryW[c][n] * (1.0f - mix) + wet * mix) * gain;
        }
    }
}
