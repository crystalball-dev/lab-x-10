#pragma once

#include "Common.h"
#include "Programmer.h"

namespace labx10::dsp
{
    // Shared transport view for one control block.
    struct Transport
    {
        double bpm = 128.0;        // host tempo, or the last one seen
        bool synced = false;       // host is playing and reports a musical position
        double ppq = 0.0;          // quarter notes at the start of this control block, valid when synced
    };

    // One global LFO. Free-running in Hz, or locked to the host's beat position when the host plays;
    // when it does not, synced rates free-run at the last known tempo. Drift is LAB X-3's PROGRAMMER walk.
    class Lfo
    {
    public:
        enum Shape { sine = 0, triangle, saw, square, sampleHold, drift };

        void prepare (float sr, uint32_t seed) noexcept
        {
            sampleRate = sr;
            rng.seed (seed * 747796405u + 2891336453u);
            walk.seed (seed * 1103515245u + 12345u);
            walk.reset (sr);
            reset();
        }

        void reset() noexcept
        {
            phase = 0.0;
            lastCycle = -1;
            held = rng.bi();
            value = 0.0f;
        }

        void retrigger() noexcept
        {
            phase = 0.0;
            held = rng.bi();
        }

        // Advances by n samples; returns the bipolar value for this block.
        float advance (int n, int shape, float rateHz, float syncBeats, const Transport& t, bool retrigMode) noexcept
        {
            const bool synced = syncBeats > 0.0f;
            const double cyclesPerSecond = synced ? (t.bpm / 60.0) / (double) syncBeats : (double) rateHz;
            bool wrapped = false;

            if (synced && t.synced && ! retrigMode)
            {
                const double cycles = t.ppq / (double) syncBeats;
                const auto index = (long long) std::floor (cycles);
                wrapped = index != lastCycle;
                lastCycle = index;
                phase = cycles - std::floor (cycles);
            }
            else
            {
                phase += cyclesPerSecond * (double) n / (double) sampleRate;
                if (phase >= 1.0)
                {
                    phase -= std::floor (phase);
                    wrapped = true;
                }
                lastCycle = -1;
            }

            if (wrapped)
                held = rng.bi();

            const float p = (float) phase;
            switch (shape)
            {
                case sine:       value = sin2pi (p); break;
                case triangle:   value = p < 0.25f ? 4.0f * p : (p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f); break;
                case saw:        value = 1.0f - 2.0f * p; break;
                case square:     value = p < 0.5f ? 1.0f : -1.0f; break;
                case sampleHold: value = held; break;
                default:         value = walk.advance (n, (float) cyclesPerSecond); break;
            }
            return value;
        }

        float getValue() const noexcept { return value; }
        double getPhase() const noexcept { return phase; }

    private:
        Rng rng;
        Programmer walk;
        float sampleRate = 48000.0f, value = 0.0f, held = 0.0f;
        double phase = 0.0;
        long long lastCycle = -1;
    };

    // COOLDOWN: the emitter's duty cycle. A beat-locked gain envelope for the high band.
    class Cooldown
    {
    public:
        enum Shape { pump = 0, gate, saw, sine };

        void prepare (float sr) noexcept
        {
            sampleRate = sr;
            smoothCoef = onePoleCoef (0.001f, sr);
            reset();
        }

        void reset() noexcept
        {
            phase = 0.0;
            gain = 1.0f;
        }

        // Fills gains[0..n) for this block. beats: cycle length in quarter notes, 0 = off.
        void process (float* gains, int n, float beats, int shape, float depth, float hold, const Transport& t) noexcept
        {
            if (beats <= 0.0f || depth <= 1.0e-4f)
            {
                for (int i = 0; i < n; ++i)
                {
                    gain += (1.0f - gain) * smoothCoef;
                    gains[i] = gain;
                }
                return;
            }

            const double inc = (t.bpm / 60.0) / (double) beats / (double) sampleRate;
            if (t.synced)
            {
                const double cycles = t.ppq / (double) beats;
                phase = cycles - std::floor (cycles);
            }

            depth = std::clamp (depth, 0.0f, 1.0f);
            const float tau = 0.05f + 0.6f * std::clamp (hold, 0.0f, 1.0f);
            const float norm = 1.0f / (1.0f - std::exp (-1.0f / tau));
            const float duty = 0.15f + 0.8f * std::clamp (hold, 0.0f, 1.0f);

            for (int i = 0; i < n; ++i)
            {
                const float p = (float) phase;
                float target;
                switch (shape)
                {
                    case pump:  target = 1.0f - depth * (1.0f - (1.0f - std::exp (-p / tau)) * norm); break;
                    case gate:  target = p < duty ? 1.0f : 1.0f - depth; break;
                    case saw:   target = 1.0f - depth * (1.0f - p); break;
                    default:    target = 1.0f - depth * 0.5f * (1.0f + sin2pi (p + 0.25f)); break;
                }
                gain += (target - gain) * smoothCoef;
                gains[i] = gain;

                phase += inc;
                if (phase >= 1.0)
                    phase -= 1.0;
            }
        }

        double getPhase() const noexcept { return phase; }

    private:
        float sampleRate = 48000.0f, smoothCoef = 0.02f, gain = 1.0f;
        double phase = 0.0;
    };
}
