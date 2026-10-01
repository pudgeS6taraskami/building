#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

// Режим Bass.
// Вход -> Squash (компрессор) -> разделение на НИЗ (чистый, моно) и ВЕРХ
//  НИЗ:  Sub (громкость), всегда моно
//  ВЕРХ: + Harmonics (октава вверх из низа) -> Drive (сатурация) -> тёмный Tone
// Сумма -> Mix -> Output
class BassEffect
{
public:
    struct Params
    {
        float drive = 0.0f, harmonics = 0.0f, tone = 1.0f, squash = 0.0f;
        float subDb = 0.0f, mix = 1.0f, outputDb = 0.0f;
    };

    void prepare (double sampleRate);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, int numCh, int numSamples, const Params& p);

private:
    // биквад 12 дБ/окт (Баттерворт)
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

        void setLowPass (double sr, float fc)
        {
            fc = juce::jlimit (10.0f, (float) (sr * 0.45), fc);
            const float w0 = juce::MathConstants<float>::twoPi * fc / (float) sr;
            const float cw = std::cos (w0), sw = std::sin (w0);
            const float alpha = sw / (2.0f * 0.7071f);
            const float a0 = 1.0f + alpha;

            b0 = (1.0f - cw) * 0.5f;
            b1 = 1.0f - cw;
            b2 = b0;

            b0 /= a0; b1 /= a0; b2 /= a0;
            a1 = -2.0f * cw / a0;
            a2 = (1.0f - alpha) / a0;
        }
    };

    static constexpr float kCrossoverHz = 120.0f;   // граница "чистого низа"

    double sr = 44100.0;
    bool needsInit = true;

    juce::SmoothedValue<float> driveS, harmS, squashS, subS, mixS, outS;

    Biquad lowA[2], lowB[2];     // два каскада = фильтр 24 дБ/окт (Linkwitz-Riley)
    Biquad harmLp;               // сглаживание гармоник
    Biquad toneLp[2];

    float env = 0.0f;            // огибающая компрессора
    float harmDcX = 0.0f, harmDcY = 0.0f;
    float dcX1[2] = { 0.0f, 0.0f }, dcY1[2] = { 0.0f, 0.0f };
};
