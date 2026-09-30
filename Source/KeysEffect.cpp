#include "KeysEffect.h"
#include <cmath>

void KeysEffect::prepare (double sampleRate)
{
    sr = sampleRate;

    const int size = (int) (sampleRate * 0.1) + 4;   // 100 мс
    for (int c = 0; c < 2; ++c)
    {
        wfDelay[c].prepare (size);
        chDelay[c].prepare (size);
    }

    wowS.reset (sampleRate, 0.03);
    flutterS.reset (sampleRate, 0.03);
    tapeS.reset (sampleRate, 0.03);
    dustS.reset (sampleRate, 0.03);
    chorusS.reset (sampleRate, 0.03);
    mixS.reset (sampleRate, 0.02);
    outS.reset (sampleRate, 0.02);

    needsInit = true;
}

void KeysEffect::reset()
{
    for (int c = 0; c < 2; ++c)
    {
        wfDelay[c].clear();
        chDelay[c].clear();
        dcX1[c] = dcY1[c] = 0.0f;
        tone1[c] = tone2[c] = 0.0f;
        hiss[c] = click[c] = 0.0f;
    }
    needsInit = true;   // при следующем блоке параметры встают сразу, без "разгона"
}

void KeysEffect::process (juce::AudioBuffer<float>& buffer, int numCh, int numSamples, const Params& p)
{
    const float outGain = juce::Decibels::decibelsToGain (p.outputDb);

    if (needsInit)
    {
        wowS.setCurrentAndTargetValue (p.wow);
        flutterS.setCurrentAndTargetValue (p.flutter);
        tapeS.setCurrentAndTargetValue (p.tape);
        dustS.setCurrentAndTargetValue (p.dust);
        chorusS.setCurrentAndTargetValue (p.chorus);
        mixS.setCurrentAndTargetValue (p.mix);
        outS.setCurrentAndTargetValue (outGain);
        needsInit = false;
    }
    else
    {
        wowS.setTargetValue (p.wow);
        flutterS.setTargetValue (p.flutter);
        tapeS.setTargetValue (p.tape);
        dustS.setTargetValue (p.dust);
        chorusS.setTargetValue (p.chorus);
        mixS.setTargetValue (p.mix);
        outS.setTargetValue (outGain);
    }

    const float fs = (float) sr;
    const float twoPi = juce::MathConstants<float>::twoPi;

    // базовая задержка и амплитуды модуляции (в секундах)
    const float baseSec      = 0.0025f;    // 2.5 мс: запас для модуляции
    const float wowAmpSec    = 0.0014f;    // ~ +-0.6% высоты на 0.7 Гц при wow = 1
    const float flutAmpSec   = 0.00007f;   // ~ +-0.25% высоты на 6 Гц при flutter = 1
    const float chBaseSec    = 0.012f;
    const float chDepthSec   = 0.0035f;
    const float baseSamples  = baseSec * fs;

    // коэффициенты, которые считаем раз на блок
    const float toneFc    = juce::jlimit (1000.0f, fs * 0.45f, juce::jmap (p.tape, 18000.0f, 5000.0f));
    const float toneCoef  = 1.0f - std::exp (-twoPi * toneFc / fs);
    const float hissCoef  = 1.0f - std::exp (-twoPi * juce::jmin (7000.0f, fs * 0.4f) / fs);
    const float clickDecay = std::exp (-1.0f / (0.0003f * fs));

    float* data[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };

    for (int n = 0; n < numSamples; ++n)
    {
        const float wow     = wowS.getNextValue();
        const float flutter = flutterS.getNextValue();
        const float tape    = tapeS.getNextValue();
        const float dust    = dustS.getNextValue();
        const float chorus  = chorusS.getNextValue();
        const float mix     = mixS.getNextValue();
        const float gain    = outS.getNextValue();

        // wow: два медленных несинхронных синуса (одинаковы для обоих каналов, как одна лента)
        const float wowLfo = 0.6f * std::sin (twoPi * wowPhase1) + 0.4f * std::sin (twoPi * wowPhase2);

        // параметры тейпа (от tape)
        const float drive = 0.3f + 2.2f * tape;
        const float bias  = 0.12f * tape;
        const float tb    = std::tanh (bias);
        const float norm  = 1.0f / (std::tanh (drive + bias) - tb);
        const float comp  = 1.0f / (1.0f + 0.5f * tape);

        for (int c = 0; c < numCh; ++c)
        {
            const float x = data[c][n];

            // --- Wow / Flutter: читаем из линии с переменной задержкой ---
            const float off = 0.15f * (float) c;
            const float flutLfo = 0.6f * std::sin (twoPi * (flPhase1 + off))
                                + 0.4f * std::sin (twoPi * (flPhase2 + 1.5f * off));

            const float delaySamples = (baseSec
                                        + wow * wowAmpSec * wowLfo
                                        + flutter * flutAmpSec * flutLfo) * fs;

            wfDelay[c].write (x);
            const float wet          = wfDelay[c].read (delaySamples);
            const float dryAligned   = wfDelay[c].read (baseSamples);   // сухой сигнал с той же базовой задержкой

            // --- Chorus: короткая модулируемая задержка, у каналов противоположные фазы ---
            chDelay[c].write (wet);
            const float chLfo = std::sin (twoPi * (chPhase + 0.5f * (float) c));
            const float chDelaySamples = (chBaseSec + chDepthSec * chLfo) * fs;
            const float chOut = chDelay[c].read (chDelaySamples);

            float y = wet * (1.0f - 0.5f * chorus) + chOut * (0.5f * chorus);

            // --- Tape: асимметричная сатурация ---
            y = (std::tanh (drive * y + bias) - tb) * norm * comp;

            const float dc = y - dcX1[c] + 0.995f * dcY1[c];
            dcX1[c] = y;
            dcY1[c] = dc;
            y = dc;

            // тёмный тон: два однополюсных фильтра (12 дБ/окт)
            tone1[c] += toneCoef * (y - tone1[c]);
            tone2[c] += toneCoef * (tone1[c] - tone2[c]);
            y = tone2[c];

            // --- Dust: шипение + редкие щелчки ---
            const float noise = (rng.next() * 2.0f - 1.0f) * dust * 0.012f;
            hiss[c] += hissCoef * (noise - hiss[c]);

            if (rng.next() < dust * 25.0f / fs)
            {
                const float amp = (0.2f + 0.8f * rng.next()) * (0.01f + 0.06f * dust);
                click[c] = (rng.next() < 0.5f) ? -amp : amp;
            }

            y += hiss[c] + click[c];
            click[c] *= clickDecay;

            // --- Mix / Output ---
            data[c][n] = (dryAligned * (1.0f - mix) + y * mix) * gain;
        }

        // двигаем фазы LFO
        auto advance = [] (float& ph, float inc) { ph += inc; if (ph >= 1.0f) ph -= 1.0f; };
        advance (wowPhase1, 0.63f / fs);
        advance (wowPhase2, 1.07f / fs);
        advance (flPhase1,  5.3f  / fs);
        advance (flPhase2,  8.1f  / fs);
        advance (chPhase,   0.65f / fs);
    }
}
