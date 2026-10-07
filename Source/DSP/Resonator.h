#pragma once

#include <vector>
#include "Common.h"
#include "Filters.h"

namespace labx10::dsp
{
    // RESONATOR: a feedback comb tuned to the played note, followed by three vowel band-passes.
    // It listens to the high band only, so the sub never feeds it; the output is mono and is added
    // back to the high band. The comb loop is damped and soft-limited so full feedback rings without
    // running away.
    class Resonator
    {
    public:
        void prepare (float sr)
        {
            sampleRate = sr;
            size = (int) std::ceil (sr / 15.0f) + 8;
            buffer.assign ((size_t) size, 0.0f);
            dampCoef = onePoleCoefHz (5000.0f, sr);
            reset();
        }

        void reset() noexcept
        {
            std::fill (buffer.begin(), buffer.end(), 0.0f);
            write = 0;
            damp = 0.0f;
            delay = -1.0f;
            for (auto& f : formants)
                f.reset();
        }

        // Adds the resonator into highL/highR. freqHz: comb tuning; vowel 0..1 sweeps U-O-A-E-I.
        void process (float* highL, float* highR, int n, float freqHz, float feedback, float vowel, float level) noexcept
        {
            if (level < 1.0e-4f)
            {
                if (! idle)
                {
                    reset();
                    idle = true;
                }
                return;
            }
            idle = false;

            const float targetDelay = std::clamp (sampleRate / std::max (freqHz, 15.0f), 2.0f, (float) size - 4.0f);
            if (delay < 0.0f)
                delay = targetDelay;
            const float dDelay = (targetDelay - delay) / (float) n;

            const float fb = std::clamp (feedback, 0.0f, 1.0f) * 0.97f;
            const float outGain = level * 0.9f * (1.0f - 0.6f * fb);

            // Formant frequencies for U, O, A, E, I (male voice averages).
            static constexpr float table[5][3] = {
                { 300.0f, 870.0f, 2240.0f }, { 570.0f, 840.0f, 2410.0f }, { 730.0f, 1090.0f, 2440.0f },
                { 530.0f, 1840.0f, 2480.0f }, { 270.0f, 2290.0f, 3010.0f } };
            const float x = std::clamp (vowel, 0.0f, 1.0f) * 4.0f;
            const int v0 = std::min ((int) x, 3);
            const float vf = x - (float) v0;
            constexpr float q = 7.0f;
            for (int b = 0; b < 3; ++b)
            {
                const float hz = table[v0][b] + (table[v0 + 1][b] - table[v0][b]) * vf;
                formants[(size_t) b].set (SVF::gFor (hz, sampleRate), 1.0f / q);
            }
            constexpr float bandGain[3] = { 1.0f / q, 0.6f / q, 0.35f / q };   // band-pass peak gain is q

            for (int i = 0; i < n; ++i)
            {
                delay += dDelay;
                const float in = 0.5f * (highL[i] + highR[i]);

                float readPos = (float) write - delay;
                if (readPos < 0.0f)
                    readPos += (float) size;
                const int r0 = (int) readPos;
                const float fr = readPos - (float) r0;
                const int r1 = (r0 + 1) % size;
                const float delayed = buffer[(size_t) r0] + fr * (buffer[(size_t) r1] - buffer[(size_t) r0]);

                damp += (delayed - damp) * dampCoef;
                const float y = 2.0f * fastTanh (0.5f * (in + fb * damp));
                buffer[(size_t) write] = y;
                write = (write + 1) % size;

                float v = 0.0f;
                for (int b = 0; b < 3; ++b)
                    v += formants[(size_t) b].bandpass (y) * bandGain[b];

                const float out = v * outGain * 2.5f;
                highL[i] += out;
                highR[i] += out;
            }
        }

    private:
        float sampleRate = 48000.0f, dampCoef = 0.5f, damp = 0.0f, delay = -1.0f;
        int size = 0, write = 0;
        bool idle = true;
        std::vector<float> buffer;
        std::array<SVF, 3> formants {};
    };
}
