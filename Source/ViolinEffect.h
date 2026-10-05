#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdint>

// Режим Violin: "смягчённые плёночные струнные".
// Цепочка: Input HP -> Soften (срез резкости 3 кГц + LP) -> Wobble (wow/flutter) -> Ensemble (3 голоса)
//          -> Tape (мягкая сатурация) -> Hall (большая комната) + Dust (шум) -> Mix -> Output
class ViolinEffect
{
public:
    struct Params
    {
        float soften = 0.0f, wobble = 0.0f, ensemble = 0.0f, tape = 0.0f;
        float dust = 0.0f, hall = 0.0f, mix = 1.0f, outputDb = 0.0f;
    };

    void prepare (double sampleRate, int samplesPerBlock);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, int numCh, int numSamples, const Params& p);

private:
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

        void reset() { z1 = z2 = 0.0f; }

        float process (float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }

        void set (double sr, float fc, bool highPass)
        {
            fc = juce::jlimit (10.0f, (float) (sr * 0.45), fc);
            const float w0 = juce::MathConstants<float>::twoPi * fc / (float) sr;
            const float cw = std::cos (w0), sw = std::sin (w0);
            const float alpha = sw / (2.0f * 0.7071f);
            const float a0 = 1.0f + alpha;

            if (highPass) { b0 = (1.0f + cw) * 0.5f; b1 = -(1.0f + cw); }
            else          { b0 = (1.0f - cw) * 0.5f; b1 =  (1.0f - cw); }
            b2 = b0;

            b0 /= a0; b1 /= a0; b2 /= a0;
            a1 = -2.0f * cw / a0;
            a2 = (1.0f - alpha) / a0;
        }

        // колоколообразный срез/подъём (RBJ peaking EQ)
        void setPeak (double sr, float fc, float gainDb, float q)
        {
            fc = juce::jlimit (20.0f, (float) (sr * 0.45), fc);
            const float A = std::pow (10.0f, gainDb / 40.0f);
            const float w0 = juce::MathConstants<float>::twoPi * fc / (float) sr;
            const float cw = std::cos (w0), sw = std::sin (w0);
            const float alpha = sw / (2.0f * q);
            const float a0 = 1.0f + alpha / A;

            b0 = (1.0f + alpha * A) / a0;
            b1 = (-2.0f * cw) / a0;
            b2 = (1.0f - alpha * A) / a0;
            a1 = (-2.0f * cw) / a0;
            a2 = (1.0f - alpha / A) / a0;
        }
    };

    struct Delay
    {
        std::vector<float> buf;
        int writePos = 0;

        void prepare (int size) { buf.assign ((size_t) size, 0.0f); writePos = 0; }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); writePos = 0; }

        void write (float x)
        {
            buf[(size_t) writePos] = x;
            if (++writePos >= (int) buf.size()) writePos = 0;
        }

        float read (float delaySamples) const
        {
            const int size = (int) buf.size();
            const float* p = buf.data();
            float rp = (float) writePos - delaySamples;
            while (rp < 0.0f) rp += (float) size;
            int i0 = (int) rp;
            if (i0 >= size) i0 -= size;
            int i1 = i0 + 1;
            if (i1 >= size) i1 = 0;
            const float fr = rp - (float) i0;
            return p[i0] * (1.0f - fr) + p[i1] * fr;
        }
    };

    struct Rng
    {
        uint32_t s = 33333u;
        float next()
        {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            return (float) (s & 0xFFFFFFu) / 16777216.0f;
        }
    };

    double sr = 44100.0;
    bool needsInit = true;

    juce::SmoothedValue<float> wobbleS, ensembleS, tapeS, dustS, hallS, mixS, outS;

    Biquad inHp[2], softPeak[2], softLp[2];

    Delay wobbleDelay[2];   // wow/flutter
    Delay ensDelay[2];      // ensemble: одна линия, три считывания
    Delay dryDelay[2];      // выравнивание сухого сигнала

    float wowP1 = 0.0f, wowP2 = 0.0f, flutP = 0.0f, ensSlow = 0.0f, ensFast = 0.0f;

    float dcX1[2] = { 0.0f, 0.0f }, dcY1[2] = { 0.0f, 0.0f };
    float tapeLp[2] = { 0.0f, 0.0f };
    float hiss[2] = { 0.0f, 0.0f }, click[2] = { 0.0f, 0.0f };

    Rng rng;
    juce::Reverb reverb;
    juce::AudioBuffer<float> dryBuf, procBuf, revBuf;
};
