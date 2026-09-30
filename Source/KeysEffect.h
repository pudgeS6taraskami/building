#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cstdint>
#include <algorithm>

// Режим Keys: плёночные клавиши.
// Цепочка: Wow/Flutter (модуляция высоты) -> Chorus -> Tape (сатурация + тёмный тон) -> Dust (шум/треск) -> Mix/Output
class KeysEffect
{
public:
    struct Params
    {
        float wow = 0.0f, flutter = 0.0f, tape = 0.0f, dust = 0.0f, chorus = 0.0f;
        float mix = 1.0f, outputDb = 0.0f;
    };

    void prepare (double sampleRate);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, int numCh, int numSamples, const Params& p);

private:
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

        // delaySamples = 1 -> последний записанный сэмпл
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

    // быстрый генератор случайных чисел [0,1)
    struct Rng
    {
        uint32_t s = 22222u;
        float next()
        {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            return (float) (s & 0xFFFFFFu) / 16777216.0f;
        }
    };

    double sr = 44100.0;
    bool needsInit = true;

    juce::SmoothedValue<float> wowS, flutterS, tapeS, dustS, chorusS, mixS, outS;

    Delay wfDelay[2];   // линия для wow/flutter
    Delay chDelay[2];   // линия для chorus

    float wowPhase1 = 0.0f, wowPhase2 = 0.0f;
    float flPhase1 = 0.0f, flPhase2 = 0.0f;
    float chPhase = 0.0f;

    float dcX1[2] = { 0.0f, 0.0f }, dcY1[2] = { 0.0f, 0.0f };
    float tone1[2] = { 0.0f, 0.0f }, tone2[2] = { 0.0f, 0.0f };
    float hiss[2] = { 0.0f, 0.0f }, click[2] = { 0.0f, 0.0f };

    Rng rng;
};
