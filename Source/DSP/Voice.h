#pragma once

#include "Config.h"
#include "Common.h"
#include "Envelope.h"
#include "Filters.h"
#include "Granular.h"
#include "Oscillator.h"
#include "Specimen.h"

namespace labx10::dsp
{
    // Control-rate snapshot of everything a voice needs. Built by the processor every control block.
    struct VoiceParams
    {
        int   genAWave = 0;
        float genAPitch = 0.0f;   // octave, semitone and fine offset in semitones
        float genAShape = 0.0f, genALevel = 0.8f;

        int   uniVoices = 1;
        float uniDetune = 20.0f, uniSpread = 0.5f;

        int   genBWave = 0;
        float genBRatio = 1.0f, genBFm = 0.0f, genBRing = 0.0f, genBLevel = 0.0f;

        int   subWave = 0;
        float subOctave = -1.0f, subLevel = 0.7f;
        bool  subDirect = true;

        float sparkLevel = 0.0f, sparkColour = 0.4f, sparkDecay = 0.012f, noiseLevel = 0.0f;

        int   specMode = 0;
        float specLevel = 0.0f, specPosition = 0.3f, specSize = 0.12f, specDensity = 24.0f, specTrack = 1.0f;
        const SpecimenData* specimen = nullptr;

        int   filterModel = 1;
        float cutoff = 1800.0f, resonance = 0.2f, driveDb = 0.0f, filterEnv = 0.3f, keytrack = 0.5f, accentAmount = 0.5f;

        float a1 = 0.002f, d1 = 0.4f, s1 = 0.85f, r1 = 0.12f;
        float a2 = 0.002f, d2 = 0.35f, s2 = 0.2f, r2 = 0.2f;

        float glide = 0.06f, pitchBend = 0.0f, dropTime = 0.08f;

        // Global LFO contributions for this control block
        float lfoCutoffOct = 0.0f, lfoPitch = 0.0f, lfoShape = 0.0f, lfoPosition = 0.0f;
    };

    // Per-note settings decided by the processor when the note starts.
    struct NoteStart
    {
        int note = 60;
        float velocity = 0.8f;
        bool accent = false;
        float accentSweep = 0.0f;   // 0..1 charge of the accent sweep circuit
        float startPitch = 60.0f;
        bool glideFromStart = false;
        float dropAmount = 0.0f;    // semitones
        int lock = 1;               // 0 off, 1 sub, 2 all
        uint64_t stamp = 0;
    };

    class Voice
    {
    public:
        static constexpr int maxUnison = labx10::maxUnison;

        void prepare (float newSampleRate, uint32_t seedValue) noexcept
        {
            sampleRate = newSampleRate;
            voiceSeed = seedValue;
            rng.seed (seedValue * 2654435761u + 12345u);
            grains.seed (seedValue * 69069u + 1u);
            grains.prepare (sampleRate);
            env1.setSampleRate (sampleRate);
            env2.setSampleRate (sampleRate);
            fadeStep = 1.0f / (0.005f * sampleRate);
            kill();
        }

        void noteOn (const NoteStart& s) noexcept
        {
            const bool wasActive = isActive();
            const bool silent = ! wasActive || fading || env1.getValue() < 0.02f;

            noteNumber = s.note;
            vel = std::clamp (s.velocity, 0.0f, 1.0f);
            accent = s.accent;
            accentSweep = s.accentSweep;
            keyDown = true;
            sustained = false;
            fading = false;
            fadeGain = 1.0f;
            targetPitch = (float) s.note;
            pitch = s.glideFromStart ? s.startPitch : (float) s.note;
            dropOffset = s.dropAmount;
            sparkEnv = 1.0f;

            if (! wasActive || (s.lock == 2 && silent))
                resetState();

            if (s.lock == 2 && silent)
            {
                // Every hit starts from the same place: fixed oscillator phases, fixed noise sequence.
                for (int i = 0; i < maxUnison; ++i)
                    phaseA[(size_t) i] = wrap01 (0.1366f * (float) i);
                phaseB = phaseSub = phaseSpec = 0.0f;
                rng.seed (0x51ED2701u);
                grains.seed (0x2545F491u);
                grains.clear();
            }
            else if (s.lock == 1 && silent)
            {
                phaseSub = 0.0f;
                if (! wasActive)
                    randomisePhases (false);
            }
            else if (! wasActive)
            {
                randomisePhases (true);
            }

            age = s.stamp;
            grains.noteStarted();
            env1.noteOn();
            env2.noteOn();
        }

        // Legato slide: change the pitch target without retriggering anything.
        void legatoTo (int note, float velocity, bool isAccent, float sweep) noexcept
        {
            noteNumber = note;
            targetPitch = (float) note;
            vel = std::clamp (velocity, 0.0f, 1.0f);
            accent = isAccent;
            accentSweep = sweep;
            keyDown = true;
            sustained = false;
        }

        // Return to a held note after the newest one was released.
        void returnTo (int note) noexcept
        {
            noteNumber = note;
            targetPitch = (float) note;
        }

        void noteOff() noexcept              { keyDown = false; env1.noteOff(); env2.noteOff(); }
        void setSustained (bool s) noexcept  { sustained = s; }

        // Fades out over 5 ms and stops. Used when a mono note hands over to a fresh voice.
        void fastRelease() noexcept
        {
            if (! isActive())
                return;
            fading = true;
            keyDown = false;
            sustained = false;
        }

        void kill() noexcept
        {
            env1.kill();
            env2.kill();
            keyDown = false;
            sustained = false;
            fading = false;
            fadeGain = 1.0f;
        }

        bool isActive() const noexcept       { return env1.isActive(); }
        bool isFading() const noexcept       { return fading; }
        bool isKeyDown() const noexcept      { return keyDown; }
        bool isSustained() const noexcept    { return sustained; }
        bool isReleasing() const noexcept    { return env1.isReleasing() || fading; }
        int getNote() const noexcept         { return noteNumber; }
        float getPitch() const noexcept      { return pitch; }
        float getLevel() const noexcept      { return env1.getValue() * (fading ? fadeGain : 1.0f); }
        uint64_t getAge() const noexcept     { return age; }

        void render (float* outL, float* outR, int n, const VoiceParams& p) noexcept
        {
            env1.setParameters (p.a1, p.d1, p.s1, p.r1);
            env2.setParameters (p.a2, p.d2, p.s2, p.r2);

            // Portamento: exponential, about 95 % of the way after one glide time.
            if (p.glide > 0.0005f)
                pitch += (targetPitch - pitch) * (1.0f - std::exp (-(float) n * 3.0f / (p.glide * sampleRate)));
            else
                pitch = targetPitch;

            // DROP: exponential fall from the drop amount, 95 % done after drop time.
            if (dropOffset != 0.0f)
            {
                const float tau = std::max (0.001f, p.dropTime / 3.0f);
                dropOffset *= std::exp (-(float) n / (tau * sampleRate));
                if (std::abs (dropOffset) < 1.0e-3f)
                    dropOffset = 0.0f;
            }

            const float base = pitch + p.pitchBend;
            const float pitchA = base + dropOffset + p.lfoPitch + p.genAPitch;
            const float pitchSub = base + dropOffset + 12.0f * p.subOctave;   // the LFO never moves the sub

            const float maxInc = 0.45f;
            const float incAEnd = std::min (midiToHz (pitchA) / sampleRate, maxInc);
            const float incBEnd = std::min (incAEnd * p.genBRatio, maxInc);
            const float incSubEnd = std::min (midiToHz (pitchSub) / sampleRate, maxInc);

            if (freshStart)
            {
                incA = incAEnd;
                incB = incBEnd;
                incSub = incSubEnd;
            }

            const float invN = 1.0f / (float) n;
            const float dIncA = (incAEnd - incA) * invN;
            const float dIncB = (incBEnd - incB) * invN;
            const float dIncSub = (incSubEnd - incSub) * invN;

            // MONOLITH unison: symmetric detune, neighbours panned to alternate sides.
            const int numUnison = std::clamp (p.uniVoices, 1, maxUnison);
            const bool stereo = numUnison > 1 && p.uniSpread > 0.001f;
            std::array<float, maxUnison> ratio {}, gainL {}, gainR {};
            float maxRatio = 1.0f;
            for (int i = 0; i < numUnison; ++i)
            {
                const float off = numUnison == 1 ? 0.0f : 2.0f * (float) i / (float) (numUnison - 1) - 1.0f;
                ratio[(size_t) i] = std::exp2 (off * p.uniDetune * (1.0f / 1200.0f));
                maxRatio = std::max (maxRatio, ratio[(size_t) i]);
                float pan = 0.0f;
                if (stereo && std::abs (off) > 1.0e-4f)
                    pan = ((i % 2) == 0 ? -1.0f : 1.0f) * p.uniSpread;
                panGains (pan, gainL[(size_t) i], gainR[(size_t) i]);
            }
            const float unisonNorm = 1.0f / std::sqrt ((float) numUnison);

            const float shape = std::clamp (p.genAShape + p.lfoShape, 0.0f, 1.0f);
            const WavetableData* table = (p.specimen != nullptr && p.specimen->table.valid) ? &p.specimen->table : nullptr;
            const int mipA = WavetableData::mipFor (incAEnd * sampleRate * maxRatio);
            const float fmDepth = p.genBFm * p.genBFm * 1.2f;   // phase deviation in cycles
            const float ring = p.genBRing;

            // SPECIMEN
            const bool useSpec = p.specLevel > 1.0e-4f && p.specimen != nullptr;
            const bool specTableMode = p.specMode == 0;
            float incSpec = 0.0f;
            int mipSpec = 0;
            const float specPos = std::clamp (p.specPosition + p.lfoPosition, 0.0f, 1.0f);
            GrainCloud::Settings gs;
            if (useSpec)
            {
                const float playPitch = base + dropOffset + p.lfoPitch;
                if (specTableMode)
                {
                    const float nativeHz = table != nullptr ? table->nativeHz : 0.0f;
                    const float nativePitch = nativeHz > 0.0f ? hzToMidi (nativeHz) : playPitch;
                    const float specPitch = p.specTrack * playPitch + (1.0f - p.specTrack) * nativePitch;
                    incSpec = std::min (midiToHz (specPitch) / sampleRate, maxInc);
                    mipSpec = WavetableData::mipFor (incSpec * sampleRate);
                }
                else
                {
                    gs.positionNorm = specPos;
                    gs.spray = 0.2f;
                    gs.sizeSeconds = p.specSize;
                    gs.density = p.specDensity;
                    gs.rate = std::exp2 (p.specTrack * (playPitch - 48.0f) * (1.0f / 12.0f));
                }
            }

            // Filter envelope runs for the whole block; the cutoff follows it at control rate.
            float e2 = env2.getValue();
            for (int i = 0; i < n; ++i)
                e2 = env2.next();

            const float velScale = 0.55f + 0.45f * vel;
            const float accentMul = accent ? 1.0f + p.accentAmount : 1.0f;
            const float cutoffOct = e2 * p.filterEnv * 6.0f * velScale * accentMul
                                  + e2 * accentSweep * p.accentAmount
                                  + p.keytrack * (base - 48.0f) * (1.0f / 12.0f)
                                  + p.lfoCutoffOct;
            const float cutoffHz = std::clamp (p.cutoff * std::exp2 (cutoffOct), 20.0f, sampleRate * 0.45f);

            const int model = p.filterModel;
            const float coefHz = model == 0 ? std::min (cutoffHz * DiodeLadder::peakScale, sampleRate * 0.45f) : cutoffHz;
            const float gEnd = std::tan (pi * coefHz / sampleRate);
            if (gPrev < 0.0f || model != lastModel)
                gPrev = gEnd;
            lastModel = model;
            const float dG = (gEnd - gPrev) * invN;
            float g = gPrev;

            const float res = std::clamp (p.resonance, 0.0f, 1.0f);
            float k = 0.0f, makeup = 1.0f;
            if (model == 0)      { k = DiodeLadder::kFor (res);      makeup = DiodeLadder::makeupFor (k); }
            else if (model == 1) { k = TransistorLadder::kFor (res); makeup = TransistorLadder::makeupFor (k); }
            else                 { k = 2.0f - 1.97f * res;           makeup = model == 3 ? k * 1.4f : 1.0f; }

            if (model == 0)      { diode[0].setG (g); diode[1].setG (g); }
            else if (model == 1) { ladder[0].setG (g); ladder[1].setG (g); }
            const bool sweeping = std::abs (dG) > 1.0e-9f;

            // Drive into the filter: linear at 0 dB, fully shaped from 3 dB up.
            const float driveGain = dbToGain (p.driveDb);
            const float driveAmount = std::clamp (p.driveDb / 3.0f, 0.0f, 1.0f);
            const float driveComp = 0.5f / fastTanh (0.5f * driveGain);

            // SPARK and NOISE share one coloured noise source.
            const float lpHz = std::min (1500.0f * std::exp2 (p.sparkColour * 3.7f), sampleRate * 0.45f);
            const float lpCoef = onePoleCoefHz (lpHz, sampleRate);
            const float hpCoef = onePoleCoefHz (300.0f, sampleRate);
            const float noiseGain = std::clamp (std::sqrt (19500.0f / lpHz), 1.0f, 4.0f);
            const float sparkCoef = std::exp (-1.0f / (std::max (p.sparkDecay, 0.0005f) * sampleRate));
            const bool useNoise = p.sparkLevel > 1.0e-4f || p.noiseLevel > 1.0e-4f;

            // Amplitude: velocity and accent
            const float velGainTarget = (0.55f + 0.45f * vel) * (accent ? dbToGain (3.0f * p.accentAmount) : 1.0f);
            if (freshStart)
                velGain = velGainTarget;

            constexpr float mixScale = 0.5f;
            const float subGain = p.subLevel * mixScale;

            for (int i = 0; i < n; ++i)
            {
                incA += dIncA;
                incB += dIncB;
                incSub += dIncSub;

                // GENERATOR B, also the phase modulator and ring modulator of A
                phaseB += incB;
                phaseB -= std::floor (phaseB);
                float b;
                switch (p.genBWave)
                {
                    case 0:  b = sin2pi (phaseB); break;
                    case 1:  b = osc::triangleFromZero (phaseB, incB); break;
                    case 2:  b = osc::saw (phaseB, incB); break;
                    default: b = osc::pulse (phaseB, incB, 0.5f); break;
                }

                const float pm = fmDepth * b;
                const float dtA = std::clamp (std::abs (incA + (pm - pmPrev)), 1.0e-6f, 0.5f);
                pmPrev = pm;

                // GENERATOR A through the MONOLITH unison
                float aL = 0.0f, aR = 0.0f;
                for (int u = 0; u < numUnison; ++u)
                {
                    float& ph = phaseA[(size_t) u];
                    ph += incA * ratio[(size_t) u];
                    ph -= std::floor (ph);
                    const float t = wrap01 (ph + pm);
                    const float dt = std::min (dtA * ratio[(size_t) u], 0.5f);

                    float v;
                    switch (p.genAWave)
                    {
                        case 0:  v = osc::sawMorph (t, dt, shape); break;
                        case 1:  v = osc::pulse (t, dt, 0.5f - 0.45f * shape); break;
                        case 2:  v = osc::fold (osc::triangle (t, dt), shape); break;
                        case 3:  v = osc::fold (sin2pi (t), shape); break;
                        default: v = table != nullptr ? readTable (*table, shape, t, mipA) : sin2pi (t); break;
                    }
                    aL += v * gainL[(size_t) u];
                    aR += v * gainR[(size_t) u];
                }
                const float ringMul = 1.0f - ring + ring * b;
                aL *= unisonNorm * ringMul * p.genALevel;
                aR *= unisonNorm * ringMul * p.genALevel;

                // SUB
                phaseSub += incSub;
                phaseSub -= std::floor (phaseSub);
                float sub;
                switch (p.subWave)
                {
                    case 0:  sub = sin2pi (phaseSub); break;
                    case 1:  sub = osc::triangleFromZero (phaseSub, incSub); break;
                    case 2:  sub = osc::pulse (phaseSub, incSub, 0.5f); break;
                    default: sub = osc::hum (phaseSub); break;
                }

                // SPARK burst and sustained NOISE
                float noisy = 0.0f;
                if (useNoise)
                {
                    const float w = rng.bi();
                    noiseHp += (w - noiseHp) * hpCoef;
                    noiseLp += ((w - noiseHp) - noiseLp) * lpCoef;
                    const float coloured = noiseLp * noiseGain;
                    noisy = coloured * (p.noiseLevel + sparkEnv * p.sparkLevel * 1.5f);
                }
                sparkEnv *= sparkCoef;

                // SPECIMEN
                float spec = 0.0f;
                if (useSpec)
                {
                    if (specTableMode)
                    {
                        if (table != nullptr)
                        {
                            phaseSpec += incSpec;
                            phaseSpec -= std::floor (phaseSpec);
                            spec = readTable (*table, specPos, phaseSpec, mipSpec);
                        }
                    }
                    else
                    {
                        spec = grains.process (p.specimen, gs) * 2.0f;
                    }
                }

                const float mono = b * p.genBLevel + noisy + spec * p.specLevel + (p.subDirect ? 0.0f : sub * p.subLevel);
                float xL = (aL + mono) * mixScale;
                float xR = (aR + mono) * mixScale;

                if (driveAmount > 0.0f)
                {
                    xL += (fastTanh (xL * driveGain) * driveComp - xL) * driveAmount;
                    if (stereo)
                        xR += (fastTanh (xR * driveGain) * driveComp - xR) * driveAmount;
                }

                // FILTER
                if (sweeping)
                {
                    g += dG;
                    if (model == 0)      { diode[0].setG (g); if (stereo) diode[1].setG (g); }
                    else if (model == 1) { ladder[0].setG (g); if (stereo) ladder[1].setG (g); }
                }

                float yL, yR;
                if (model == 0)
                {
                    yL = diode[0].process (xL, k) * makeup;
                    yR = stereo ? diode[1].process (xR, k) * makeup : yL;
                }
                else if (model == 1)
                {
                    yL = ladder[0].process (xL, k) * makeup;
                    yR = stereo ? ladder[1].process (xR, k) * makeup : yL;
                }
                else
                {
                    svf[0].set (g, k);
                    float lp, bp, hp;
                    svf[0].process (xL, lp, bp, hp);
                    yL = (model == 2 ? lp : (model == 3 ? bp : hp)) * makeup;
                    if (stereo)
                    {
                        svf[1].set (g, k);
                        svf[1].process (xR, lp, bp, hp);
                        yR = (model == 2 ? lp : (model == 3 ? bp : hp)) * makeup;
                    }
                    else
                    {
                        yR = yL;
                    }
                }

                if (p.subDirect)
                {
                    yL += sub * subGain;
                    yR += sub * subGain;
                }

                // Amplitude
                velGain += (velGainTarget - velGain) * 0.002f;
                float amp = env1.next() * velGain;
                if (fading)
                {
                    amp *= fadeGain;
                    fadeGain = std::max (0.0f, fadeGain - fadeStep);
                }

                outL[i] += yL * amp;
                outR[i] += yR * amp;
            }

            if (! stereo)
            {
                // Mono blocks only ran the left filter; keep the right one in step for a later stereo block.
                diode[1] = diode[0];
                ladder[1] = ladder[0];
                svf[1] = svf[0];
            }

            gPrev = gEnd;
            incA = incAEnd;
            incB = incBEnd;
            incSub = incSubEnd;
            freshStart = false;

            if (fading && fadeGain <= 0.0f)
                kill();
        }

    private:
        void resetState() noexcept
        {
            for (auto& f : diode)  f.reset();
            for (auto& f : ladder) f.reset();
            for (auto& f : svf)    f.reset();
            noiseLp = noiseHp = 0.0f;
            pmPrev = 0.0f;
            gPrev = -1.0f;
            freshStart = true;
            grains.clear();
        }

        void randomisePhases (bool includeSub) noexcept
        {
            for (auto& ph : phaseA)
                ph = rng.uni();
            phaseB = rng.uni();
            phaseSpec = rng.uni();
            if (includeSub)
                phaseSub = rng.uni();
        }

        Envelope env1, env2;
        GrainCloud grains;
        std::array<DiodeLadder, 2> diode {};
        std::array<TransistorLadder, 2> ladder {};
        std::array<SVF, 2> svf {};
        Rng rng;

        float sampleRate = 48000.0f;
        uint32_t voiceSeed = 1;
        int noteNumber = 60;
        float vel = 1.0f, pitch = 60.0f, targetPitch = 60.0f;
        bool accent = false;
        float accentSweep = 0.0f;
        bool keyDown = false, sustained = false, freshStart = true;
        bool fading = false;
        float fadeGain = 1.0f, fadeStep = 0.004f;
        uint64_t age = 0;

        std::array<float, maxUnison> phaseA {};
        float phaseB = 0.0f, phaseSub = 0.0f, phaseSpec = 0.0f;
        float incA = 0.0f, incB = 0.0f, incSub = 0.0f;
        float pmPrev = 0.0f;
        float dropOffset = 0.0f;
        float sparkEnv = 0.0f, noiseLp = 0.0f, noiseHp = 0.0f;
        float velGain = 1.0f;
        float gPrev = -1.0f;
        int lastModel = -1;
    };
}
