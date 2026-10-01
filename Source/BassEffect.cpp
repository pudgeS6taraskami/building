#include "BassEffect.h"

void BassEffect::prepare (double sampleRate)
{
    sr = sampleRate;

    for (int c = 0; c < 2; ++c)
    {
        lowA[c].setLowPass (sr, kCrossoverHz);
        lowB[c].setLowPass (sr, kCrossoverHz);
    }
    harmLp.setLowPass (sr, 500.0f);

    driveS.reset (sampleRate, 0.03);
    harmS.reset (sampleRate, 0.03);
    squashS.reset (sampleRate, 0.03);
    subS.reset (sampleRate, 0.03);
    mixS.reset (sampleRate, 0.02);
    outS.reset (sampleRate, 0.02);

    needsInit = true;
}

void BassEffect::reset()
{
    for (int c = 0; c < 2; ++c)
    {
        lowA[c].reset();
        lowB[c].reset();
        toneLp[c].reset();
        dcX1[c] = dcY1[c] = 0.0f;
    }
    harmLp.reset();
    harmDcX = harmDcY = 0.0f;
    env = 0.0f;
    needsInit = true;
}

void BassEffect::process (juce::AudioBuffer<float>& buffer, int numCh, int numSamples, const Params& p)
{
    const float outGain = juce::Decibels::decibelsToGain (p.outputDb);
    const float subGain = juce::Decibels::decibelsToGain (p.subDb);

    if (needsInit)
    {
        driveS.setCurrentAndTargetValue (p.drive);
        harmS.setCurrentAndTargetValue (p.harmonics);
        squashS.setCurrentAndTargetValue (p.squash);
        subS.setCurrentAndTargetValue (subGain);
        mixS.setCurrentAndTargetValue (p.mix);
        outS.setCurrentAndTargetValue (outGain);
        needsInit = false;
    }
    else
    {
        driveS.setTargetValue (p.drive);
        harmS.setTargetValue (p.harmonics);
        squashS.setTargetValue (p.squash);
        subS.setTargetValue (subGain);
        mixS.setTargetValue (p.mix);
        outS.setTargetValue (outGain);
    }

    const float fs = (float) sr;

    // Tone: 800 Гц (тёмный) ... 16 кГц (открытый)
    const float toneFc = juce::jlimit (500.0f, fs * 0.45f, 800.0f * std::pow (20.0f, p.tone));
    for (int c = 0; c < 2; ++c)
        toneLp[c].setLowPass (sr, toneFc);

    const float attCoef = std::exp (-1.0f / (0.010f * fs));   // атака 10 мс: удар проходит
    const float relCoef = std::exp (-1.0f / (0.180f * fs));   // релиз 180 мс
    const float dcR     = std::exp (-juce::MathConstants<float>::twoPi * 25.0f / fs);

    float* data[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };

    for (int n = 0; n < numSamples; ++n)
    {
        const float drive   = driveS.getNextValue();
        const float harmAmt = harmS.getNextValue();
        const float squash  = squashS.getNextValue();
        const float sub     = subS.getNextValue();
        const float mix     = mixS.getNextValue();
        const float gain    = outS.getNextValue();

        float dry[2] = { 0.0f, 0.0f };
        float xc[2]  = { 0.0f, 0.0f };

        // ---------- Squash: мягкий компрессор-выравниватель ----------
        const float thresholdDb = juce::jmap (squash, -4.0f, -30.0f);
        const float ratio       = juce::jmap (squash, 1.5f, 8.0f);
        const float slope       = 1.0f - 1.0f / ratio;
        const float makeupDb    = 0.5f * (-thresholdDb) * slope;

        float peak = 0.0f;
        for (int c = 0; c < numCh; ++c)
        {
            dry[c] = data[c][n];
            peak = juce::jmax (peak, std::abs (dry[c]));
        }

        if (peak > env) env = attCoef * env + (1.0f - attCoef) * peak;
        else            env = relCoef * env + (1.0f - relCoef) * peak;

        const float levelDb = juce::Decibels::gainToDecibels (env, -100.0f);
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

        // ---------- разделение на низ и верх ----------
        float low[2]  = { 0.0f, 0.0f };
        float high[2] = { 0.0f, 0.0f };

        for (int c = 0; c < numCh; ++c)
        {
            xc[c]   = dry[c] * compGain;
            low[c]  = lowB[c].process (lowA[c].process (xc[c]));
            high[c] = xc[c] - low[c];
        }

        // низ всегда моно
        const float lowMono = (numCh == 2) ? 0.5f * (low[0] + low[1]) : low[0];

        // ---------- Harmonics: выпрямитель даёт октаву вверх ----------
        const float rect = std::abs (lowMono);
        const float hd   = rect - harmDcX + dcR * harmDcY;
        harmDcX = rect;
        harmDcY = hd;
        const float harm = harmLp.process (hd);

        // ---------- Drive: сатурация верха (вместе с гармониками) ----------
        const float d  = 1.0f + 12.0f * drive * drive;
        const float b  = 0.2f * drive;
        const float tb = std::tanh (b);

        for (int c = 0; c < numCh; ++c)
        {
            const float h = high[c] + harmAmt * 1.2f * harm;
            float y = (std::tanh (d * h + b) - tb) / d;

            const float dc = y - dcX1[c] + dcR * dcY1[c];
            dcX1[c] = y;
            dcY1[c] = dc;
            y = dc;

            float wet = lowMono * sub + y;
            wet = toneLp[c].process (wet);

            data[c][n] = (dry[c] * (1.0f - mix) + wet * mix) * gain;
        }
    }
}
