#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

class LoFiAudioProcessor : public juce::AudioProcessor
{
public:
    LoFiAudioProcessor();
    ~LoFiAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    double getDelayTimeSeconds();

    // ---- простая линия задержки с интерполяцией ----
    struct Delay
    {
        std::vector<float> buf;
        int writePos = 0;

        void prepare (int size) { buf.assign ((size_t) size, 0.0f); writePos = 0; }

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

        void write (float x)
        {
            buf[(size_t) writePos] = x;
            if (++writePos >= (int) buf.size()) writePos = 0;
        }
    };

    // ---- биквадный фильтр (HP / LP, 12 дБ/окт) ----
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

    // указатели на параметры
    std::atomic<float> *pBits = nullptr, *pDownsample = nullptr,
                       *pEchoTime = nullptr, *pEchoSync = nullptr, *pEchoDivision = nullptr,
                       *pManualBpm = nullptr, *pEchoFeedback = nullptr, *pEchoPingPong = nullptr,
                       *pEchoLevel = nullptr, *pReverbType = nullptr, *pReverbLength = nullptr,
                       *pReverbLevel = nullptr, *pHp = nullptr, *pLp = nullptr,
                       *pMono = nullptr, *pMix = nullptr, *pOutput = nullptr;

    double currentSampleRate = 44100.0;

    juce::SmoothedValue<float> mixSmooth, outSmooth, delaySmooth, echoLevelSmooth, reverbLevelSmooth;

    // lo-fi
    float held[2] = { 0.0f, 0.0f };
    int counter[2] = { 0, 0 };

    // эхо
    Delay delays[2];
    float fbLp[2] = { 0.0f, 0.0f };
    float fbCoef = 0.3f;

    // реверб
    juce::Reverb reverb;

    // фильтры
    Biquad hp[2], lp[2];

    // рабочие буферы
    juce::AudioBuffer<float> wetBuf, revBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoFiAudioProcessor)
};
