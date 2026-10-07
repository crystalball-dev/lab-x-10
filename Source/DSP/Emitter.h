#pragma once

#include <memory>
#include <vector>
#include <juce_dsp/juce_dsp.h>
#include "Common.h"

namespace labx10::dsp
{
    // EMITTER: the Rainbow emitter at full power.
    //
    // The voice sum is oversampled (off, 2x or 4x, polyphase IIR half-band filters) and split by a
    // 4th-order Linkwitz-Riley crossover. Below the split the signal is summed to mono and passed clean,
    // so the sub never distorts and never spreads. Above the split one of five shapers runs:
    //   - the shaper input is pre-emphasised above 400 Hz in proportion to drive (up to +12 dB), so heavy
    //     settings distort the mids hardest, the way amp and pedal circuits are voiced;
    //   - the shaper fades in over the first 6 dB of drive, so drive 0 is transparent;
    //   - only the change the shaper makes is kept, high-passed at the split frequency, so distortion
    //     products never land in the sub band;
    // followed by a DC blocker, a tilt EQ and a dry/wet mix. The two bands leave separately so later
    // stages (resonator, cooldown, width) can work on the high band only.
    //
    // Latency is constant whatever the oversampling setting: lower settings are padded with a delay to
    // match the 4x path, so the host never sees the latency change.
    class Emitter
    {
    public:
        enum Mode { tube = 0, clip, fold, rectify, crush };

        struct Settings
        {
            int mode = tube;
            float driveDb = 0.0f;   // already includes SCORCH and LFO
            float bias = 0.0f;      // -1..1
            float tone = 0.0f;      // -1..1, negative is darker
            float splitHz = 120.0f;
            float mix = 1.0f;
        };

        static constexpr int maxBlock = 64;
        static constexpr float referenceLevel = 0.7f;   // output level the shapers settle at as drive rises
        static constexpr float emphasisMax = 2.98f;     // +12 dB above 400 Hz at full emphasis

        void prepare (double newSampleRate)
        {
            sampleRate = newSampleRate;

            for (int m = 0; m < 3; ++m)
            {
                const double rate = sampleRate * (double) (1 << m);
                const juce::dsp::ProcessSpec spec { rate, (juce::uint32) (maxBlock << m), 2 };
                auto& xo = crossover[(size_t) m];
                xo.prepare (spec);
                xo.setCutoffFrequency (120.0f);
                auto& hp = productFilter[(size_t) m];
                hp.prepare (spec);
                hp.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
                hp.setCutoffFrequency (120.0f);
            }

            using OS = juce::dsp::Oversampling<float>;
            os2 = std::make_unique<OS> (3, 1, OS::filterHalfBandPolyphaseIIR, true, true);
            os4 = std::make_unique<OS> (3, 2, OS::filterHalfBandPolyphaseIIR, true, true);
            os2->initProcessing ((size_t) maxBlock);
            os4->initProcessing ((size_t) maxBlock);

            latency[0] = 0;
            latency[1] = (int) std::lround (os2->getLatencyInSamples());
            latency[2] = (int) std::lround (os4->getLatencyInSamples());
            maxLatency = std::max ({ latency[0], latency[1], latency[2] });

            work.setSize (3, maxBlock, false, true, false);
            for (auto& d : delay)
                d.assign ((size_t) maxLatency + 1, 0.0f);

            reset();
        }

        int getLatency() const noexcept { return maxLatency; }
        int getOversampling() const noexcept { return osIndex; }

        // Call only while the output is faded out: the new path starts from silence.
        void setOversampling (int index) noexcept
        {
            osIndex = std::clamp (index, 0, 2);
            reset();
        }

        void reset() noexcept
        {
            for (auto& xo : crossover)
                xo.reset();
            for (auto& hp : productFilter)
                hp.reset();
            if (os2 != nullptr) os2->reset();
            if (os4 != nullptr) os4->reset();
            for (auto& d : delay)
                std::fill (d.begin(), d.end(), 0.0f);
            delayWrite = 0;
            for (auto& c : chan)
                c = {};
            firstBlock = true;
        }

        // inL/inR: voice sum, n <= maxBlock samples. Writes the clean mono low band and the processed
        // high band, delayed to the constant latency.
        void process (const float* inL, const float* inR, int n, const Settings& s,
                      float* low, float* highL, float* highR) noexcept
        {
            jassert (n <= maxBlock);
            work.copyFrom (0, 0, inL, n);
            work.copyFrom (1, 0, inR, n);
            work.clear (2, 0, n);

            // Block-end targets; every stage ramps linearly from the previous block's values.
            const float g = dbToGain (std::clamp (s.driveDb, 0.0f, 60.0f));
            Ramp target;
            target.drive = g;
            target.comp = referenceLevel / fastTanh (g * referenceLevel);
            target.bias = std::clamp (s.bias, -1.0f, 1.0f);
            target.tone = std::clamp (s.tone, -1.0f, 1.0f) * 0.85f;
            target.mix = std::clamp (s.mix, 0.0f, 1.0f);
            target.crush = std::clamp (s.driveDb / 40.0f, 0.0f, 1.0f);
            target.emphasis = emphasisMax * std::clamp (s.driveDb / 24.0f, 0.0f, 1.0f);
            target.amount = s.mode == crush ? 1.0f : std::clamp (s.driveDb / 6.0f, 0.0f, 1.0f);
            if (firstBlock)
            {
                current = target;
                firstBlock = false;
            }

            const int factor = 1 << osIndex;
            const float split = std::clamp (s.splitHz, 20.0f, 1000.0f);
            crossover[(size_t) osIndex].setCutoffFrequency (split);
            productFilter[(size_t) osIndex].setCutoffFrequency (split);

            juce::dsp::AudioBlock<float> block (work.getArrayOfWritePointers(), 3, (size_t) n);
            if (osIndex == 0)
            {
                processBands (block, n, factor, s.mode, target);
            }
            else
            {
                auto& os = osIndex == 1 ? *os2 : *os4;
                auto up = os.processSamplesUp (block);
                processBands (up, (int) up.getNumSamples(), factor, s.mode, target);
                os.processSamplesDown (block);
            }
            current = target;

            // Pad the latency of this path up to the constant maximum.
            const int pad = maxLatency - latency[osIndex];
            const float* srcs[3] = { work.getReadPointer (2), work.getReadPointer (0), work.getReadPointer (1) };
            float* dsts[3] = { low, highL, highR };
            const int size = maxLatency + 1;
            for (int i = 0; i < n; ++i)
            {
                const int readPos = (delayWrite - pad + size) % size;
                for (int c = 0; c < 3; ++c)
                {
                    auto& d = delay[(size_t) c];
                    d[(size_t) delayWrite] = srcs[c][i];
                    dsts[c][i] = d[(size_t) readPos];
                }
                delayWrite = (delayWrite + 1) % size;
            }
        }

    private:
        struct Ramp
        {
            float drive = 1.0f, comp = 1.0f, bias = 0.0f, tone = 0.0f, mix = 1.0f, crush = 0.0f;
            float emphasis = 0.0f, amount = 0.0f;
        };

        struct ChannelState
        {
            float dcIn = 0.0f, dcOut = 0.0f, toneLp = 0.0f, emphasisLp = 0.0f, held = 0.0f, holdPhase = 1.0f;
        };

        static float shape (float x, int mode, float g, float bias, float comp) noexcept
        {
            switch (mode)
            {
                case tube:
                {
                    const float b = 0.6f * bias;
                    return (fastTanh (g * x + b) - fastTanh (b)) * comp;
                }
                case clip:
                {
                    const float b = 0.5f * bias;
                    return (std::clamp (g * x + b, -1.0f, 1.0f) - std::clamp (b, -1.0f, 1.0f)) * comp;
                }
                case fold:
                {
                    const float b = 0.5f * bias;
                    return (sin2pi (0.25f * (g * x + b)) - sin2pi (0.25f * b)) * comp;
                }
                case rectify:
                {
                    const float y = fastTanh (g * x);
                    const float w = 0.5f * (bias + 1.0f);   // -1 half-wave, +1 full-wave
                    return ((1.0f - w) * std::max (y, 0.0f) * 1.5f + w * std::abs (y)) * comp;
                }
                default:
                    return x;
            }
        }

        template <typename Block>
        void processBands (Block& block, int numSamples, int factor, int mode, const Ramp& target) noexcept
        {
            float* L = block.getChannelPointer (0);
            float* R = block.getChannelPointer (1);
            float* X = block.getChannelPointer (2);
            auto& xo = crossover[(size_t) osIndex];
            auto& productHp = productFilter[(size_t) osIndex];

            const float rate = (float) sampleRate * (float) factor;
            const float dcR = 1.0f - twoPi * 10.0f / rate;
            const float toneCoef = onePoleCoefHz (900.0f, rate);
            const float emphasisCoef = onePoleCoefHz (400.0f, rate);
            const float inv = 1.0f / (float) std::max (1, numSamples);

            // Crush settings change slowly; one value per block is plenty.
            const float bitAmount = std::clamp (target.crush * (1.0f - 0.6f * target.bias), 0.0f, 1.0f);
            const float rateAmount = std::clamp (target.crush * (1.0f + 0.6f * target.bias), 0.0f, 1.0f);
            const float quant = std::exp2 (15.0f - 13.0f * std::pow (bitAmount, 0.8f));
            const float holdStep = 1.0f / ((1.0f + 23.0f * rateAmount * std::sqrt (rateAmount)) * (float) factor);

            float* io[2] = { L, R };
            for (int j = 0; j < numSamples; ++j)
            {
                const float t = (float) (j + 1) * inv;
                const float g = current.drive + (target.drive - current.drive) * t;
                const float comp = current.comp + (target.comp - current.comp) * t;
                const float bias = current.bias + (target.bias - current.bias) * t;
                const float tone = current.tone + (target.tone - current.tone) * t;
                const float mix = current.mix + (target.mix - current.mix) * t;
                const float emphasis = current.emphasis + (target.emphasis - current.emphasis) * t;
                const float amount = current.amount + (target.amount - current.amount) * t;

                float lowSum = 0.0f;
                for (int c = 0; c < 2; ++c)
                {
                    float lo, hi;
                    xo.processSample (c, io[c][j], lo, hi);
                    lowSum += lo;

                    auto& st = chan[(size_t) c];
                    float shaped;
                    if (mode == crush)
                    {
                        st.holdPhase += holdStep;
                        if (st.holdPhase >= 1.0f)
                        {
                            st.holdPhase -= std::floor (st.holdPhase);
                            st.held = hi;
                        }
                        shaped = std::round (st.held * quant) / quant;
                    }
                    else
                    {
                        // Pre-emphasis above 400 Hz: the mids reach the shaper hotter than the lows.
                        st.emphasisLp += (hi - st.emphasisLp) * emphasisCoef;
                        const float emphasised = hi + emphasis * (hi - st.emphasisLp);
                        shaped = shape (emphasised, mode, g, bias, comp);
                    }

                    // Keep only what the shaper changed, without DC and without anything below the split.
                    float delta = (shaped - hi) * amount;
                    const float dc = delta - st.dcIn + dcR * st.dcOut;
                    st.dcIn = delta;
                    st.dcOut = dc;
                    delta = productHp.processSample (c, dc);
                    const float wet = hi + delta;

                    // Tilt EQ around 900 Hz
                    st.toneLp += (wet - st.toneLp) * toneCoef;
                    const float tilted = st.toneLp * (1.0f - tone) + (wet - st.toneLp) * (1.0f + tone);

                    io[c][j] = hi + (tilted - hi) * mix;
                }
                X[j] = 0.5f * lowSum;
            }
        }

        double sampleRate = 48000.0;
        std::array<juce::dsp::LinkwitzRileyFilter<float>, 3> crossover, productFilter;
        std::unique_ptr<juce::dsp::Oversampling<float>> os2, os4;
        juce::AudioBuffer<float> work;
        std::array<std::vector<float>, 3> delay;
        std::array<int, 3> latency { 0, 0, 0 };
        std::array<ChannelState, 2> chan {};
        int maxLatency = 0, delayWrite = 0, osIndex = 1;
        Ramp current;
        bool firstBlock = true;
    };
}
