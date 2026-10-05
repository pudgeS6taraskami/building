#include "ViolinEffect.h"

void ViolinEffect::prepare (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;

    const int size = (int) (sampleRate * 0.05) + 4;   // 50 мс
    for (int c = 0; c < 2; ++c)
    {
        wobbleDelay[c].prepare (size);
        ensDelay[c].prepare (size);
        dryDelay[c].prepare (size);
        inHp[c].set (sr, 120.0f, true);   // ниже самой низкой ноты скрипки (~196 Гц)
    }

    wobbleS.reset (sampleRate, 0.03);
    ensembleS.reset (sampleRate, 0.03);
    tapeS.reset (sampleRate, 0.03);
    dustS.reset (sampleRate, 0.03);
    hallS.reset (sampleRate, 0.03);
    mixS.reset (sampleRate, 0.02);
    outS.reset (sampleRate, 0.02);

    reverb.setSampleRate (sampleRate);

    const int n = juce::jmax (1, samplesPerBlock);
    dryBuf.setSize (2, n);
    procBuf.setSize (2, n);
    revBuf.setSize (2, n);

    needsInit = true;
}

void ViolinEffect::reset()
{
    for (int c = 0; c < 2; ++c)
    {
        wobbleDelay[c].clear();
        ensDelay[c].clear();
        dryDelay[c].clear();
        inHp[c].reset();
        softPeak[c].reset();
        softLp[c].reset();
        dcX1[c] = dcY1[c] = 0.0f;
        tapeLp[c] = 0.0f;
        hiss[c] = click[c] = 0.0f;
    }
    reverb.reset();
    needsInit = true;
}

void ViolinEffect::process (juce::AudioBuffer<float>& buffer, int numCh, int numSamples, const Params& p)
{
    const float outGain = juce::Decibels::decibelsToGain (p.outputDb);

    if (needsInit)
    {
        wobbleS.setCurrentAndTargetValue (p.wobble);
        ensembleS.setCurrentAndTargetValue (p.ensemble);
        tapeS.setCurrentAndTargetValue (p.tape);
        dustS.setCurrentAndTargetValue (p.dust);
        hallS.setCurrentAndTargetValue (p.hall);
        mixS.setCurrentAndTargetValue (p.mix);
        outS.setCurrentAndTargetValue (outGain);
        needsInit = false;
    }
    else
    {
        wobbleS.setTargetValue (p.wobble);
        ensembleS.setTargetValue (p.ensemble);
        tapeS.setTargetValue (p.tape);
        dustS.setTargetValue (p.dust);
        hallS.setTargetValue (p.hall);
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

    // Soften: срез резкости около 3 кГц (до -8 дБ) и LP от 16 кГц до 4 кГц
    const float softLpFc = juce::jlimit (1000.0f, fs * 0.45f, 16000.0f * std::pow (0.25f, p.soften));
    for (int c = 0; c < 2; ++c)
    {
        softPeak[c].setPeak (sr, 3000.0f, -8.0f * p.soften, 0.8f);
        softLp[c].set (sr, softLpFc, false);
    }

    // большая комната
    {
        juce::Reverb::Parameters rp;
        rp.roomSize   = 0.8f;
        rp.damping    = 0.35f;
        rp.width      = 1.0f;
        rp.wetLevel   = 0.33f;
        rp.dryLevel   = 0.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);
    }

    const float baseSec     = 0.0025f;    // база wobble
    const float wowAmpSec   = 0.0012f;    // ~ +-0.4% высоты при wobble = 1
    const float flutAmpSec  = 0.00005f;
    const float ensBaseSec  = 0.014f;
    const float ensDepthSec = 0.0035f;
    const float baseSamples = baseSec * fs;

    const float tapeFc    = juce::jlimit (1000.0f, fs * 0.45f, juce::jmap (p.tape, 20000.0f, 9000.0f));
    const float tapeCoef  = 1.0f - std::exp (-twoPi * tapeFc / fs);
    const float dcR       = std::exp (-twoPi * 25.0f / fs);
    const float hissCoef  = 1.0f - std::exp (-twoPi * juce::jmin (7000.0f, fs * 0.4f) / fs);
    const float clickDecay = std::exp (-1.0f / (0.0003f * fs));

    float* data[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };
    float* dryW[2] = { dryBuf.getWritePointer (0), dryBuf.getWritePointer (1) };
    float* proc[2] = { procBuf.getWritePointer (0), procBuf.getWritePointer (1) };

    // ---------- проход 1: Soften -> Wobble -> Ensemble -> Tape ----------
    for (int n = 0; n < numSamples; ++n)
    {
        const float wobble   = wobbleS.getNextValue();
        const float ensemble = ensembleS.getNextValue();
        const float tape     = tapeS.getNextValue();

        const float wowLfo = 0.6f * std::sin (twoPi * wowP1) + 0.4f * std::sin (twoPi * wowP2);

        const float drive = 0.3f + 1.7f * tape;
        const float bias  = 0.1f * tape;
        const float tb    = std::tanh (bias);
        const float norm  = 1.0f / (std::tanh (drive + bias) - tb);
        const float comp  = 1.0f / (1.0f + 0.4f * tape);

        for (int c = 0; c < numCh; ++c)
        {
            const float x = data[c][n];

            // --- Soften ---
            float y = inHp[c].process (x);
            y = softPeak[c].process (y);
            y = softLp[c].process (y);

            // --- Wobble ---
            const float flutLfo = std::sin (twoPi * (flutP + 0.15f * (float) c));
            const float wowDelaySamples = (baseSec
                                           + wobble * (wowAmpSec * wowLfo + flutAmpSec * flutLfo)) * fs;

            wobbleDelay[c].write (y);
            const float w = wobbleDelay[c].read (wowDelaySamples);

            // --- Ensemble: три голоса с фазами 0, 1/3, 2/3 ---
            ensDelay[c].write (w);
            float sum = 0.0f;
            for (int v = 0; v < 3; ++v)
            {
                const float ph1 = ensSlow + (float) v / 3.0f + 0.17f * (float) c;
                const float ph2 = ensFast + (float) v / 3.0f + 0.11f * (float) c;
                const float dsec = ensBaseSec + ensDepthSec * (std::sin (twoPi * ph1) + 0.15f * std::sin (twoPi * ph2));
                sum += ensDelay[c].read (dsec * fs);
            }
            const float voices = sum * (1.0f / 3.0f);

            y = w * (1.0f - 0.5f * ensemble) + voices * (0.5f * ensemble);

            // --- Tape: мягкая сатурация ---
            y = (std::tanh (drive * y + bias) - tb) * norm * comp;

            const float dc = y - dcX1[c] + dcR * dcY1[c];
            dcX1[c] = y;
            dcY1[c] = dc;
            y = dc;

            tapeLp[c] += tapeCoef * (y - tapeLp[c]);
            proc[c][n] = tapeLp[c];

            // сухой сигнал с той же базовой задержкой
            dryDelay[c].write (x);
            dryW[c][n] = dryDelay[c].read (baseSamples);
        }

        auto advance = [] (float& ph, float inc) { ph += inc; if (ph >= 1.0f) ph -= 1.0f; };
        advance (wowP1,    0.55f / fs);
        advance (wowP2,    0.93f / fs);
        advance (flutP,    5.7f  / fs);
        advance (ensSlow,  0.42f / fs);
        advance (ensFast,  4.6f  / fs);
    }

    // ---------- Hall ----------
    for (int c = 0; c < numCh; ++c)
        revBuf.copyFrom (c, 0, procBuf, c, 0, numSamples);

    if (numCh == 2)
        reverb.processStereo (revBuf.getWritePointer (0), revBuf.getWritePointer (1), numSamples);
    else
        reverb.processMono (revBuf.getWritePointer (0), numSamples);

    const float* rev[2] = { revBuf.getReadPointer (0), revBuf.getReadPointer (1) };

    // ---------- проход 2: Hall + Dust + Mix + Output ----------
    for (int n = 0; n < numSamples; ++n)
    {
        const float dust = dustS.getNextValue();
        const float hall = hallS.getNextValue();
        const float mix  = mixS.getNextValue();
        const float gain = outS.getNextValue();

        for (int c = 0; c < numCh; ++c)
        {
            float wet = proc[c][n] + hall * rev[c][n];

            const float noise = (rng.next() * 2.0f - 1.0f) * dust * 0.012f;
            hiss[c] += hissCoef * (noise - hiss[c]);

            if (rng.next() < dust * 25.0f / fs)
            {
                const float amp = (0.2f + 0.8f * rng.next()) * (0.01f + 0.06f * dust);
                click[c] = (rng.next() < 0.5f) ? -amp : amp;
            }

            wet += hiss[c] + click[c];
            click[c] *= clickDecay;

            data[c][n] = (dryW[c][n] * (1.0f - mix) + wet * mix) * gain;
        }
    }
}
