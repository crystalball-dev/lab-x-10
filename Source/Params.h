#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "SpecimenCatalog.h"

namespace labx10
{
    enum class Kind { real, integer, choice };
    enum class Smooth { none, linear, log };

    struct ParamSpec
    {
        const char* id;
        const char* name;
        Kind kind;
        float minValue, maxValue, defaultValue, skewCentre;   // skewCentre 0 = linear range
        Smooth smooth;
        const char* unit;      // "%", "Hz", "s", "ms", "dB", "ct", "st", "x", "/s", "oct", ""
        const char* choices;   // '|' separated; "@specimen" builds the specimen list
    };

    // Every parameter is a float or a choice. A bool parameter failed pluginval's state restore in LAB X-3.
    namespace P
    {
        enum Index : int
        {
            genAWave, genAOctave, genASemi, genAFine, genAShape, genALevel,
            uniVoices, uniDetune, uniSpread,
            genBWave, genBRatio, genBFm, genBRing, genBLevel,
            subWave, subOctave, subLevel, subRoute, lock,
            dropAmount, dropTime,
            sparkLevel, sparkColour, sparkDecay, noiseLevel,
            specSource, specMode, specLevel, specPosition, specSize, specDensity, specTrack,
            filterModel, cutoff, resonance, filterDrive, filterEnv, keytrack, accentAmount, accentVel,
            env1A, env1D, env1S, env1R,
            env2A, env2D, env2S, env2R,
            emitMode, emitDrive, emitBias, emitTone, emitSplit, emitMix, emitOs,
            resLevel, resRatio, resFeedback, resVowel,
            lfoShape, lfoRate, lfoSync, lfoTarget, lfoDepth, lfoRetrig,
            coolRate, coolDepth, coolShape, coolHold,
            regAmount, regCeiling, weight,
            scorch, volume, voiceMode, voices, glide, bendRange, width,
            count
        };
    }

    // Choice indices that the DSP code relies on.
    namespace Choice
    {
        enum GenAWave   { saw = 0, pulse, triangle, sine, table };
        enum GenBWave   { bSine = 0, bTriangle, bSaw, bSquare };
        enum SubWave    { subSine = 0, subTriangle, subSquare, subHum };
        enum SubRoute   { routeFilter = 0, routeDirect };
        enum Lock       { lockOff = 0, lockSub, lockAll };
        enum SpecMode   { specTable = 0, specGrain };
        enum Filter     { diode = 0, ladder, svfLow, svfBand, svfHigh };
        enum EmitMode   { tube = 0, clip, fold, rectify, crush };
        enum EmitOs     { osOff = 0, os2x, os4x };
        enum LfoShape   { lfoSine = 0, lfoTriangle, lfoSaw, lfoSquare, lfoSampleHold, lfoDrift };
        enum LfoTarget  { toCutoff = 0, toPitch, toShape, toPosition, toDrive, toResonator, toLevel };
        enum CoolShape  { pump = 0, gate, coolSaw, coolSine };
        enum VoiceMode  { mono = 0, legato, poly };
    }

    // Beats (quarter notes) per LFO cycle for each lfo_sync choice; index 0 is free-running.
    inline constexpr float lfoSyncBeats[] = { 0.0f, 4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f,
                                              2.0f / 3.0f, 1.0f / 3.0f, 1.0f / 6.0f, 1.5f, 0.75f };

    // Beats per COOLDOWN cycle for each cool_rate choice; index 0 is off.
    inline constexpr float coolRateBeats[] = { 0.0f, 2.0f, 1.0f, 0.5f, 0.25f };

    inline const std::array<ParamSpec, P::count>& paramSpecs()
    {
        static const std::array<ParamSpec, P::count> specs {{
            { "gen_a_wave",        "Generator A Wave",       Kind::choice,  0.0f,   4.0f,     0.0f,    0.0f,   Smooth::none,   "",    "Saw|Pulse|Triangle|Sine|Table" },
            { "gen_a_octave",      "Generator A Octave",     Kind::integer, -2.0f,  1.0f,     0.0f,    0.0f,   Smooth::none,   "oct", "" },
            { "gen_a_semi",        "Generator A Semitone",   Kind::integer, -12.0f, 12.0f,    0.0f,    0.0f,   Smooth::none,   "st",  "" },
            { "gen_a_fine",        "Generator A Fine",       Kind::real,    -100.0f, 100.0f,  0.0f,    0.0f,   Smooth::linear, "ct",  "" },
            { "gen_a_shape",       "Generator A Shape",      Kind::real,    0.0f,   1.0f,     0.0f,    0.0f,   Smooth::linear, "%",   "" },
            { "gen_a_level",       "Generator A Level",      Kind::real,    0.0f,   1.0f,     0.8f,    0.0f,   Smooth::linear, "%",   "" },

            { "uni_voices",        "Monolith Voices",        Kind::integer, 1.0f,   7.0f,     1.0f,    0.0f,   Smooth::none,   "",    "" },
            { "uni_detune",        "Monolith Detune",        Kind::real,    0.0f,   100.0f,   20.0f,   0.0f,   Smooth::linear, "ct",  "" },
            { "uni_spread",        "Monolith Spread",        Kind::real,    0.0f,   1.0f,     0.5f,    0.0f,   Smooth::linear, "%",   "" },

            { "gen_b_wave",        "Generator B Wave",       Kind::choice,  0.0f,   3.0f,     0.0f,    0.0f,   Smooth::none,   "",    "Sine|Triangle|Saw|Square" },
            { "gen_b_ratio",       "Generator B Ratio",      Kind::real,    0.25f,  16.0f,    1.0f,    2.0f,   Smooth::log,    "x",   "" },
            { "gen_b_fm",          "Generator B FM",         Kind::real,    0.0f,   1.0f,     0.0f,    0.0f,   Smooth::linear, "%",   "" },
            { "gen_b_ring",        "Generator B Ring",       Kind::real,    0.0f,   1.0f,     0.0f,    0.0f,   Smooth::linear, "%",   "" },
            { "gen_b_level",       "Generator B Level",      Kind::real,    0.0f,   1.0f,     0.0f,    0.0f,   Smooth::linear, "%",   "" },

            { "sub_wave",          "Sub Wave",               Kind::choice,  0.0f,   3.0f,     0.0f,    0.0f,   Smooth::none,   "",    "Sine|Triangle|Square|Hum" },
            { "sub_octave",        "Sub Octave",             Kind::integer, -2.0f,  0.0f,     -1.0f,   0.0f,   Smooth::none,   "oct", "" },
            { "sub_level",         "Sub Level",              Kind::real,    0.0f,   1.0f,     0.7f,    0.0f,   Smooth::linear, "%",   "" },
            { "sub_route",         "Sub Route",              Kind::choice,  0.0f,   1.0f,     1.0f,    0.0f,   Smooth::none,   "",    "Filter|Direct" },
            { "lock",              "Phase Lock",             Kind::choice,  0.0f,   2.0f,     1.0f,    0.0f,   Smooth::none,   "",    "Off|Sub|All" },

            { "drop_amount",       "Drop Amount",            Kind::real,    0.0f,   36.0f,    0.0f,    0.0f,   Smooth::none,   "st",  "" },
            { "drop_time",         "Drop Time",              Kind::real,    5.0f,   800.0f,   80.0f,   100.0f, Smooth::none,   "ms",  "" },

            { "spark_level",       "Spark Level",            Kind::real,    0.0f,   1.0f,     0.0f,    0.0f,   Smooth::linear, "%",   "" },
            { "spark_colour",      "Spark Colour",           Kind::real,    0.0f,   1.0f,     0.4f,    0.0f,   Smooth::linear, "%",   "" },
            { "spark_decay",       "Spark Decay",            Kind::real,    1.0f,   200.0f,   12.0f,   20.0f,  Smooth::none,   "ms",  "" },
            { "noise_level",       "Noise Level",            Kind::real,    0.0f,   1.0f,     0.0f,    0.0f,   Smooth::linear, "%",   "" },

            { "specimen_source",   "Specimen Source",        Kind::choice,  0.0f,   (float) specimenCatalogSize, 1.0f, 0.0f, Smooth::none, "", "@specimen" },
            { "specimen_mode",     "Specimen Mode",          Kind::choice,  0.0f,   1.0f,     0.0f,    0.0f,   Smooth::none,   "",    "Table|Grain" },
            { "specimen_level",    "Specimen Level",         Kind::real,    0.0f,   1.0f,     0.0f,    0.0f,   Smooth::linear, "%",   "" },
            { "specimen_position", "Specimen Position",      Kind::real,    0.0f,   1.0f,     0.3f,    0.0f,   Smooth::linear, "%",   "" },
            { "specimen_size",     "Specimen Grain Size",    Kind::real,    10.0f,  800.0f,   120.0f,  100.0f, Smooth::none,   "ms",  "" },
            { "specimen_density",  "Specimen Density",       Kind::real,    1.0f,   120.0f,   24.0f,   20.0f,  Smooth::none,   "/s",  "" },
            { "specimen_track",    "Specimen Keytrack",      Kind::real,    0.0f,   1.0f,     1.0f,    0.0f,   Smooth::linear, "%",   "" },

            { "filter_model",      "Filter Model",           Kind::choice,  0.0f,   4.0f,     1.0f,    0.0f,   Smooth::none,   "",    "Diode|Ladder|SVF LP|SVF BP|SVF HP" },
            { "filter_cutoff",     "Filter Cutoff",          Kind::real,    20.0f,  20000.0f, 1800.0f, 1000.0f, Smooth::log,   "Hz",  "" },
            { "filter_res",        "Filter Resonance",       Kind::real,    0.0f,   1.0f,     0.2f,    0.0f,   Smooth::linear, "%",   "" },
            { "filter_drive",      "Filter Drive",           Kind::real,    0.0f,   24.0f,    0.0f,    0.0f,   Smooth::linear, "dB",  "" },
            { "filter_env",        "Filter Env Amount",      Kind::real,    -1.0f,  1.0f,     0.3f,    0.0f,   Smooth::linear, "%",   "" },
            { "filter_keytrack",   "Filter Keytrack",        Kind::real,    0.0f,   1.0f,     0.5f,    0.0f,   Smooth::linear, "%",   "" },
            { "accent_amount",     "Accent Amount",          Kind::real,    0.0f,   1.0f,     0.5f,    0.0f,   Smooth::linear, "%",   "" },
            { "accent_vel",        "Accent Velocity",        Kind::real,    0.0f,   1.0f,     0.87f,   0.0f,   Smooth::none,   "%",   "" },

            { "env1_attack",       "Amp Attack",             Kind::real,    0.001f, 10.0f,    0.002f,  0.5f,   Smooth::none,   "s",   "" },
            { "env1_decay",        "Amp Decay",              Kind::real,    0.001f, 10.0f,    0.4f,    0.8f,   Smooth::none,   "s",   "" },
            { "env1_sustain",      "Amp Sustain",            Kind::real,    0.0f,   1.0f,     0.85f,   0.0f,   Smooth::none,   "%",   "" },
            { "env1_release",      "Amp Release",            Kind::real,    0.001f, 20.0f,    0.12f,   1.5f,   Smooth::none,   "s",   "" },

            { "env2_attack",       "Filter Attack",          Kind::real,    0.001f, 10.0f,    0.002f,  0.5f,   Smooth::none,   "s",   "" },
            { "env2_decay",        "Filter Decay",           Kind::real,    0.001f, 10.0f,    0.35f,   0.8f,   Smooth::none,   "s",   "" },
            { "env2_sustain",      "Filter Sustain",         Kind::real,    0.0f,   1.0f,     0.2f,    0.0f,   Smooth::none,   "%",   "" },
            { "env2_release",      "Filter Release",         Kind::real,    0.001f, 20.0f,    0.2f,    1.5f,   Smooth::none,   "s",   "" },

            { "emit_mode",         "Emitter Mode",           Kind::choice,  0.0f,   4.0f,     0.0f,    0.0f,   Smooth::none,   "",    "Tube|Clip|Fold|Rectify|Crush" },
            { "emit_drive",        "Emitter Drive",          Kind::real,    0.0f,   40.0f,    0.0f,    0.0f,   Smooth::linear, "dB",  "" },
            { "emit_bias",         "Emitter Bias",           Kind::real,    -1.0f,  1.0f,     0.0f,    0.0f,   Smooth::linear, "%",   "" },
            { "emit_tone",         "Emitter Tone",           Kind::real,    -1.0f,  1.0f,     0.0f,    0.0f,   Smooth::linear, "%",   "" },
            { "emit_split",        "Emitter Split",          Kind::real,    30.0f,  400.0f,   120.0f,  110.0f, Smooth::log,    "Hz",  "" },
            { "emit_mix",          "Emitter Mix",            Kind::real,    0.0f,   1.0f,     1.0f,    0.0f,   Smooth::linear, "%",   "" },
            { "emit_os",           "Emitter Oversampling",   Kind::choice,  0.0f,   2.0f,     1.0f,    0.0f,   Smooth::none,   "",    "Off|2x|4x" },

            { "res_level",         "Resonator Level",        Kind::real,    0.0f,   1.0f,     0.0f,    0.0f,   Smooth::linear, "%",   "" },
            { "res_ratio",         "Resonator Ratio",        Kind::real,    0.5f,   8.0f,     1.0f,    2.0f,   Smooth::log,    "x",   "" },
            { "res_feedback",      "Resonator Feedback",     Kind::real,    0.0f,   1.0f,     0.5f,    0.0f,   Smooth::linear, "%",   "" },
            { "res_vowel",         "Resonator Vowel",        Kind::real,    0.0f,   1.0f,     0.0f,    0.0f,   Smooth::linear, "%",   "" },

            { "lfo_shape",         "LFO Shape",              Kind::choice,  0.0f,   5.0f,     0.0f,    0.0f,   Smooth::none,   "",    "Sine|Triangle|Saw|Square|S&H|Drift" },
            { "lfo_rate",          "LFO Rate",               Kind::real,    0.02f,  30.0f,    2.0f,    2.0f,   Smooth::none,   "Hz",  "" },
            { "lfo_sync",          "LFO Sync",               Kind::choice,  0.0f,   11.0f,    0.0f,    0.0f,   Smooth::none,   "",    "Free|1/1|1/2|1/4|1/8|1/16|1/32|1/4 T|1/8 T|1/16 T|1/4 D|1/8 D" },
            { "lfo_target",        "LFO Target",             Kind::choice,  0.0f,   6.0f,     0.0f,    0.0f,   Smooth::none,   "",    "Cutoff|Pitch|Shape|Position|Drive|Resonator|Level" },
            { "lfo_depth",         "LFO Depth",              Kind::real,    -1.0f,  1.0f,     0.0f,    0.0f,   Smooth::linear, "%",   "" },
            { "lfo_retrig",        "LFO Retrigger",          Kind::choice,  0.0f,   1.0f,     0.0f,    0.0f,   Smooth::none,   "",    "Free|Note" },

            { "cool_rate",         "Cooldown Rate",          Kind::choice,  0.0f,   4.0f,     0.0f,    0.0f,   Smooth::none,   "",    "Off|1/2|1/4|1/8|1/16" },
            { "cool_depth",        "Cooldown Depth",         Kind::real,    0.0f,   1.0f,     0.6f,    0.0f,   Smooth::linear, "%",   "" },
            { "cool_shape",        "Cooldown Shape",         Kind::choice,  0.0f,   3.0f,     0.0f,    0.0f,   Smooth::none,   "",    "Pump|Gate|Saw|Sine" },
            { "cool_hold",         "Cooldown Hold",          Kind::real,    0.0f,   1.0f,     0.4f,    0.0f,   Smooth::linear, "%",   "" },

            { "reg_amount",        "Regulator Amount",       Kind::real,    0.0f,   1.0f,     0.2f,    0.0f,   Smooth::linear, "%",   "" },
            { "reg_ceiling",       "Regulator Ceiling",      Kind::real,    -12.0f, 0.0f,     -0.3f,   0.0f,   Smooth::linear, "dB",  "" },
            { "weight",            "Weight",                 Kind::real,    -12.0f, 12.0f,    0.0f,    0.0f,   Smooth::linear, "dB",  "" },

            { "scorch",            "SCORCH",                 Kind::real,    0.0f,   1.0f,     0.0f,    0.0f,   Smooth::linear, "%",   "" },
            { "master_volume",     "Master Volume",          Kind::real,    -48.0f, 6.0f,     -6.0f,   0.0f,   Smooth::linear, "dB",  "" },
            { "voice_mode",        "Voice Mode",             Kind::choice,  0.0f,   2.0f,     1.0f,    0.0f,   Smooth::none,   "",    "Mono|Legato|Poly" },
            { "voices",            "Voices",                 Kind::integer, 1.0f,   6.0f,     6.0f,    0.0f,   Smooth::none,   "",    "" },
            { "glide",             "Glide",                  Kind::real,    0.0f,   2.0f,     0.06f,   0.3f,   Smooth::none,   "s",   "" },
            { "bend_range",        "Bend Range",             Kind::integer, 1.0f,   24.0f,    2.0f,    0.0f,   Smooth::none,   "st",  "" },
            { "width",             "Width",                  Kind::real,    0.0f,   1.0f,     1.0f,    0.0f,   Smooth::linear, "%",   "" },
        }};
        return specs;
    }

    inline int paramIndexForId (const juce::String& id)
    {
        const auto& specs = paramSpecs();
        for (int i = 0; i < P::count; ++i)
            if (id == specs[(size_t) i].id)
                return i;
        return -1;
    }

    inline juce::StringArray choicesFor (const ParamSpec& s)
    {
        if (juce::String (s.choices) == "@specimen")
        {
            juce::StringArray names { "User File" };
            for (const auto& e : specimenCatalog)
                names.add (e.name);
            return names;
        }
        return juce::StringArray::fromTokens (s.choices, "|", "");
    }

    inline int specimenChoiceForName (const juce::String& name)
    {
        for (int i = 0; i < specimenCatalogSize; ++i)
            if (name == specimenCatalog[i].name)
                return i + 1;
        jassertfalse;
        return 1;
    }

    inline juce::NormalisableRange<float> rangeFor (const ParamSpec& s)
    {
        juce::NormalisableRange<float> range (s.minValue, s.maxValue);
        if (s.kind != Kind::real)
            range.interval = 1.0f;
        else if (s.skewCentre > 0.0f)
            range.setSkewForCentre (s.skewCentre);
        return range;
    }

    inline juce::String formatValue (const ParamSpec& s, float v)
    {
        const juce::String unit (s.unit);
        if (unit == "%")   return juce::String (juce::roundToInt (v * 100.0f)) + "%";
        if (unit == "Hz")  return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz"
                                               : juce::String (v, v < 10.0f ? 2 : (v < 100.0f ? 1 : 0)) + " Hz";
        if (unit == "s")   return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms" : juce::String (v, 2) + " s";
        if (unit == "ms")  return juce::String (v, v < 10.0f ? 1 : 0) + " ms";
        if (unit == "dB")  return juce::String (v, 1) + " dB";
        if (unit == "ct")  return juce::String (juce::roundToInt (v)) + " ct";
        if (unit == "st")  return s.kind == Kind::real ? juce::String (v, 1) + " st"
                                                       : (v > 0.0f ? "+" : "") + juce::String (juce::roundToInt (v)) + " st";
        if (unit == "x")   return juce::String (v, v < 10.0f ? 3 : 2) + "x";
        if (unit == "/s")  return juce::String (v, v < 10.0f ? 1 : 0) + "/s";
        if (unit == "oct") return (v > 0.0f ? "+" : "") + juce::String (juce::roundToInt (v));
        return juce::String (juce::roundToInt (v));
    }

    inline float parseValue (const ParamSpec& s, const juce::String& text)
    {
        const auto t = text.trim().toLowerCase();
        float v = t.retainCharacters ("0123456789.-+").getFloatValue();
        const juce::String unit (s.unit);
        if (unit == "%")                           v *= 0.01f;
        else if (unit == "Hz" && t.contains ("k")) v *= 1000.0f;
        else if (unit == "s" && t.contains ("ms")) v *= 0.001f;
        return juce::jlimit (s.minValue, s.maxValue, v);
    }

    inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        for (const auto& s : paramSpecs())
        {
            jassert (s.id != nullptr);
            const juce::ParameterID pid { s.id, 1 };
            const auto* spec = &s;

            switch (s.kind)
            {
                case Kind::choice:
                    layout.add (std::make_unique<juce::AudioParameterChoice> (pid, s.name, choicesFor (s), (int) s.defaultValue));
                    break;

                case Kind::integer:
                    layout.add (std::make_unique<juce::AudioParameterInt> (
                        pid, s.name, (int) s.minValue, (int) s.maxValue, (int) s.defaultValue,
                        juce::AudioParameterIntAttributes()
                            .withStringFromValueFunction ([spec] (int v, int) { return formatValue (*spec, (float) v); })));
                    break;

                case Kind::real:
                    layout.add (std::make_unique<juce::AudioParameterFloat> (
                        pid, s.name, rangeFor (s), s.defaultValue,
                        juce::AudioParameterFloatAttributes()
                            .withStringFromValueFunction ([spec] (float v, int) { return formatValue (*spec, v); })
                            .withValueFromStringFunction ([spec] (const juce::String& text) { return parseValue (*spec, text); })));
                    break;
            }
        }

        return layout;
    }
}
