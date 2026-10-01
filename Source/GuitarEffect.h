#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <algorithm>
#include <cmath>

// Режим Guitar: "усилитель + колонка + плёнка".
// Цепочка: Input HP -> Drive (овердрайв) -> Cabinet (HP + Tone LP) -> Tremolo -> Wobble (вибрато) -> Room -> Mix -> Output
class GuitarEffect
{
public:
    struct Params
    {
        float drive = 0.0f, tone = 0.5f, tremolo = 0.0f, rate = 4.0f;
        float wobble = 0.0f, room = 0.0f, mix = 1.0f, outputDb = 0.0f;
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

    double sr = 44100.0;
    bool needsInit = true;

    juce::SmoothedValue<float> driveS, tremS, wobbleS, roomS, mixS, outS;

    Biquad inHp[2], cabHp[2], toneA[2], toneB[2];
    float dcX1[2] = { 0.0f, 0.0f }, dcY1[2] = { 0.0f, 0.0f };

    Delay wobbleDelay[2];   // вибрато
    Delay dryDelay[2];      // выравнивание сухого сигнала

    float tremPhase = 0.0f, wobPhase1 = 0.0f, wobPhase2 = 0.0f;

    juce::Reverb reverb;
    juce::AudioBuffer<float> dryBuf, procBuf, revBuf;
};
