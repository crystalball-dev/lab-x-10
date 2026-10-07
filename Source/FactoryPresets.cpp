#include "FactoryPresets.h"
#include "Params.h"

namespace labx10
{
    namespace
    {
        // Choice indices, spelled out for readability.
        constexpr float saw = 0, pulse = 1, triangle = 2, sine = 3, table = 4;      // GENERATOR A
        constexpr float bSine = 0, bSaw = 2, bSquare = 3;                             // GENERATOR B
        constexpr float subSine = 0, subHum = 3;                                      // SUB
        constexpr float lockAll = 2;
        constexpr float specTable = 0, specGrain = 1;
        constexpr float diode = 0, ladder = 1, svfLow = 2;                            // FILTER
        constexpr float tube = 0, clip = 1, fold = 2, crush = 4;                      // EMITTER
        constexpr float lfoSine = 0, lfoTriangle = 1, lfoSampleHold = 4, lfoDrift = 5;
        constexpr float sync1 = 1, sync2 = 2, sync8 = 4, sync16 = 5;                // LFO sync: 1/1, 1/2, 1/8, 1/16
        constexpr float toCutoff = 0, toPitch = 1, toShape = 2, toResonator = 5;
        constexpr float coolQuarter = 2;
        constexpr float mono = 0, legato = 1, poly = 2;

        float spec (const char* name) { return (float) specimenChoiceForName (name); }

        std::vector<Preset> build()
        {
            std::vector<Preset> p;

            // 1. The init patch: a clean sine sub and nothing else.
            p.push_back ({ "Emitter Disabled", {
                { "gen_a_level", 0.0f }, { "sub_wave", subSine }, { "sub_level", 0.8f }, { "sub_octave", -1.0f },
                { "emit_mix", 0.0f }, { "reg_amount", 0.0f }, { "master_volume", -4.0f } } });

            // 2. The generators below the Radar: the 81 Hz hall hum as a wavetable over a clean sub.
            p.push_back ({ "Generator Hall", {
                { "gen_a_wave", table }, { "gen_a_shape", 0.4f }, { "gen_a_level", 0.6f },
                { "specimen_source", spec ("Generator Hall") },
                { "sub_wave", subSine }, { "sub_level", 0.75f },
                { "filter_model", ladder }, { "filter_cutoff", 900.0f }, { "filter_res", 0.15f }, { "filter_env", 0.15f },
                { "env1_attack", 0.003f }, { "env1_decay", 0.6f }, { "env1_sustain", 0.9f }, { "env1_release", 0.25f },
                { "emit_mode", tube }, { "emit_drive", 6.0f }, { "emit_split", 90.0f },
                { "lfo_shape", lfoDrift }, { "lfo_rate", 0.2f }, { "lfo_target", toShape }, { "lfo_depth", 0.15f },
                { "reg_amount", 0.3f }, { "weight", 2.0f }, { "master_volume", -6.0f } } });

            // 3. A deep 808-style sub that drops into place, with a relay click on top.
            p.push_back ({ "Bunker X-10", {
                { "voice_mode", mono }, { "glide", 0.0f }, { "lock", lockAll },
                { "gen_a_wave", triangle }, { "gen_a_shape", 0.3f }, { "gen_a_level", 0.35f },
                { "sub_wave", subHum }, { "sub_level", 0.85f },
                { "drop_amount", 12.0f }, { "drop_time", 120.0f },
                { "spark_level", 0.25f }, { "spark_colour", 0.2f }, { "spark_decay", 6.0f },
                { "filter_model", ladder }, { "filter_cutoff", 600.0f }, { "filter_res", 0.1f }, { "filter_env", 0.4f },
                { "env2_decay", 0.18f }, { "env2_sustain", 0.0f },
                { "env1_attack", 0.001f }, { "env1_decay", 0.9f }, { "env1_sustain", 0.6f }, { "env1_release", 0.3f },
                { "emit_mode", tube }, { "emit_drive", 9.0f }, { "emit_split", 80.0f },
                { "reg_amount", 0.35f }, { "weight", 3.0f }, { "master_volume", -6.0f } } });

            // 4. The emitter at full power: saw and sub, hard-clipped above 100 Hz.
            p.push_back ({ "Rainbow Emitter", {
                { "gen_a_wave", saw }, { "gen_a_level", 0.8f },
                { "uni_voices", 3.0f }, { "uni_detune", 12.0f }, { "uni_spread", 0.6f },
                { "sub_level", 0.7f },
                { "filter_model", ladder }, { "filter_cutoff", 2500.0f }, { "filter_res", 0.25f }, { "filter_env", 0.2f },
                { "emit_mode", clip }, { "emit_drive", 24.0f }, { "emit_tone", 0.2f }, { "emit_split", 100.0f }, { "emit_mix", 0.9f },
                { "reg_amount", 0.4f }, { "master_volume", -7.0f } } });

            // 5. Everything pushed into the red: folded, resonating, SCORCH fully open.
            p.push_back ({ "Brain Scorcher", {
                { "gen_a_wave", saw }, { "gen_a_level", 0.8f },
                { "uni_voices", 5.0f }, { "uni_detune", 25.0f }, { "uni_spread", 0.7f },
                { "gen_b_wave", bSquare }, { "gen_b_ratio", 2.0f }, { "gen_b_fm", 0.25f },
                { "sub_level", 0.6f },
                { "filter_model", ladder }, { "filter_cutoff", 1500.0f }, { "filter_res", 0.35f },
                { "emit_mode", fold }, { "emit_drive", 12.0f }, { "emit_bias", 0.2f },
                { "res_level", 0.5f }, { "res_ratio", 1.0f }, { "res_feedback", 0.7f }, { "res_vowel", 0.5f },
                { "lfo_shape", lfoSampleHold }, { "lfo_sync", sync16 }, { "lfo_target", toResonator }, { "lfo_depth", 0.4f },
                { "scorch", 1.0f }, { "reg_amount", 0.5f }, { "master_volume", -8.0f } } });

            // 6. Power lowered every beat to cool the apparatus: a pumping, driven saw.
            p.push_back ({ "Cooldown Period", {
                { "gen_a_wave", saw }, { "gen_a_level", 0.85f },
                { "uni_voices", 3.0f }, { "uni_detune", 15.0f },
                { "sub_level", 0.65f },
                { "filter_model", ladder }, { "filter_cutoff", 3000.0f }, { "filter_res", 0.2f },
                { "emit_mode", tube }, { "emit_drive", 18.0f }, { "emit_split", 110.0f },
                { "cool_rate", coolQuarter }, { "cool_depth", 0.85f }, { "cool_hold", 0.25f },
                { "reg_amount", 0.35f }, { "master_volume", -7.0f } } });

            // 7. Seven detuned saws in a slow-moving wall: the Reese.
            p.push_back ({ "Monolith Wall", {
                { "gen_a_wave", saw }, { "gen_a_level", 0.8f },
                { "uni_voices", 7.0f }, { "uni_detune", 35.0f }, { "uni_spread", 0.9f },
                { "sub_level", 0.7f },
                { "filter_model", ladder }, { "filter_cutoff", 700.0f }, { "filter_res", 0.25f }, { "filter_env", 0.1f },
                { "lfo_shape", lfoSine }, { "lfo_rate", 0.25f }, { "lfo_target", toCutoff }, { "lfo_depth", 0.25f },
                { "emit_mode", tube }, { "emit_drive", 8.0f }, { "emit_split", 130.0f },
                { "glide", 0.08f }, { "reg_amount", 0.35f }, { "master_volume", -7.0f } } });

            // 8. The 303 line on the dead-end sidetrack: diode filter, overlapping notes slide.
            p.push_back ({ "Dead-End Sidetrack", {
                { "voice_mode", legato }, { "glide", 0.06f },
                { "gen_a_wave", saw }, { "gen_a_level", 0.9f }, { "sub_level", 0.3f },
                { "filter_model", diode }, { "filter_cutoff", 350.0f }, { "filter_res", 0.7f }, { "filter_drive", 6.0f },
                { "filter_env", 0.55f }, { "filter_keytrack", 0.3f }, { "accent_amount", 0.8f },
                { "env2_attack", 0.001f }, { "env2_decay", 0.25f }, { "env2_sustain", 0.0f }, { "env2_release", 0.1f },
                { "env1_attack", 0.002f }, { "env1_decay", 0.3f }, { "env1_sustain", 0.85f }, { "env1_release", 0.05f },
                { "emit_mode", tube }, { "emit_drive", 6.0f },
                { "reg_amount", 0.3f }, { "master_volume", -5.0f } } });

            // 9. Square-wave acid with hard accents and a snappy envelope. The code that opens the door.
            p.push_back ({ "Keypad 342089", {
                { "voice_mode", legato }, { "glide", 0.05f },
                { "gen_a_wave", pulse }, { "gen_a_level", 0.85f }, { "sub_level", 0.25f },
                { "filter_model", diode }, { "filter_cutoff", 280.0f }, { "filter_res", 0.8f }, { "filter_drive", 9.0f },
                { "filter_env", 0.7f }, { "filter_keytrack", 0.3f }, { "accent_amount", 1.0f }, { "accent_vel", 0.8f },
                { "env2_attack", 0.001f }, { "env2_decay", 0.12f }, { "env2_sustain", 0.0f }, { "env2_release", 0.08f },
                { "env1_attack", 0.001f }, { "env1_decay", 0.25f }, { "env1_sustain", 0.7f }, { "env1_release", 0.04f },
                { "emit_mode", clip }, { "emit_drive", 12.0f }, { "emit_split", 120.0f }, { "emit_mix", 0.6f },
                { "reg_amount", 0.3f }, { "master_volume", -5.0f } } });

            // 10. A growl cut from the bloodsucker recordings, wobbling on the eighth notes.
            p.push_back ({ "Bloodsucker", {
                { "gen_a_wave", table }, { "gen_a_shape", 0.5f }, { "gen_a_level", 0.8f },
                { "specimen_source", spec ("Bloodsucker Growl") },
                { "uni_voices", 3.0f }, { "uni_detune", 12.0f },
                { "sub_level", 0.7f },
                { "filter_model", ladder }, { "filter_cutoff", 600.0f }, { "filter_res", 0.45f }, { "filter_env", 0.1f },
                { "lfo_shape", lfoSine }, { "lfo_sync", sync8 }, { "lfo_target", toCutoff }, { "lfo_depth", 0.5f },
                { "emit_mode", tube }, { "emit_drive", 15.0f }, { "emit_split", 110.0f },
                { "res_level", 0.2f }, { "res_vowel", 0.3f },
                { "reg_amount", 0.4f }, { "master_volume", -7.0f } } });

            // 11. Hard-clipped square, short and aggressive, every note hits the same.
            p.push_back ({ "Spetsnaz", {
                { "voice_mode", mono }, { "glide", 0.0f }, { "lock", lockAll },
                { "gen_a_wave", pulse }, { "gen_a_shape", 0.1f }, { "gen_a_level", 0.8f },
                { "sub_level", 0.6f },
                { "spark_level", 0.3f }, { "spark_decay", 4.0f },
                { "filter_model", ladder }, { "filter_cutoff", 1800.0f }, { "filter_res", 0.3f }, { "filter_env", 0.5f },
                { "env2_decay", 0.12f }, { "env2_sustain", 0.1f },
                { "env1_attack", 0.001f }, { "env1_decay", 0.18f }, { "env1_sustain", 0.5f }, { "env1_release", 0.06f },
                { "emit_mode", clip }, { "emit_drive", 28.0f }, { "emit_tone", 0.3f }, { "emit_split", 100.0f },
                { "reg_amount", 0.4f }, { "master_volume", -8.0f } } });

            // 12. Metallic FM bass: GENERATOR B at an inharmonic ratio bends the sine into bells and bars.
            p.push_back ({ "Lab X19", {
                { "gen_a_wave", sine }, { "gen_a_level", 0.8f },
                { "gen_b_wave", bSine }, { "gen_b_ratio", 3.5f }, { "gen_b_fm", 0.55f }, { "gen_b_ring", 0.2f },
                { "sub_level", 0.6f },
                { "filter_model", svfLow }, { "filter_cutoff", 3000.0f }, { "filter_env", 0.3f },
                { "env2_decay", 0.3f },
                { "emit_mode", tube }, { "emit_drive", 8.0f },
                { "lfo_shape", lfoDrift }, { "lfo_rate", 0.5f }, { "lfo_target", toPitch }, { "lfo_depth", 0.02f },
                { "reg_amount", 0.3f }, { "master_volume", -6.0f } } });

            // 13. Bit-starved saw and static: the Red Forest's radio noise.
            p.push_back ({ "Red Forest Static", {
                { "gen_a_wave", saw }, { "gen_a_level", 0.7f },
                { "noise_level", 0.15f }, { "spark_level", 0.4f }, { "spark_colour", 0.3f },
                { "sub_level", 0.6f },
                { "filter_model", ladder }, { "filter_cutoff", 2200.0f }, { "filter_res", 0.3f },
                { "emit_mode", crush }, { "emit_drive", 26.0f }, { "emit_bias", 0.3f }, { "emit_mix", 0.8f },
                { "reg_amount", 0.4f }, { "master_volume", -7.0f } } });

            // 14. The psy field: a resonator sweeping vowels over a steady sub.
            p.push_back ({ "Psi Field", {
                { "gen_a_wave", saw }, { "gen_a_level", 0.6f },
                { "uni_voices", 3.0f }, { "uni_detune", 10.0f },
                { "sub_level", 0.75f },
                { "filter_model", svfLow }, { "filter_cutoff", 4000.0f }, { "filter_env", 0.0f },
                { "res_level", 0.7f }, { "res_ratio", 1.0f }, { "res_feedback", 0.75f }, { "res_vowel", 0.5f },
                { "lfo_shape", lfoTriangle }, { "lfo_sync", sync2 }, { "lfo_target", toResonator }, { "lfo_depth", 0.9f },
                { "emit_mode", tube }, { "emit_drive", 6.0f }, { "emit_split", 140.0f },
                { "reg_amount", 0.3f }, { "master_volume", -7.0f } } });

            // 15. A wavetable cut from the turbine recording, morphing frame to frame over a bar.
            p.push_back ({ "Components From X-16", {
                { "gen_a_wave", table }, { "gen_a_shape", 0.0f }, { "gen_a_level", 0.75f },
                { "specimen_source", spec ("Turbine") },
                { "specimen_mode", specGrain }, { "specimen_level", 0.2f }, { "specimen_density", 30.0f },
                { "specimen_size", 80.0f }, { "specimen_track", 1.0f },
                { "sub_level", 0.7f },
                { "filter_model", ladder }, { "filter_cutoff", 1400.0f }, { "filter_res", 0.2f },
                { "lfo_shape", lfoTriangle }, { "lfo_sync", sync1 }, { "lfo_target", toShape }, { "lfo_depth", 0.9f },
                { "emit_mode", tube }, { "emit_drive", 10.0f },
                { "reg_amount", 0.35f }, { "master_volume", -7.0f } } });

            // 16. Wide, mid-heavy bass for chords; the sub stays mono underneath.
            p.push_back ({ "Antenna Grounds", {
                { "voice_mode", poly }, { "voices", 6.0f },
                { "gen_a_wave", saw }, { "gen_a_level", 0.7f },
                { "uni_voices", 5.0f }, { "uni_detune", 22.0f }, { "uni_spread", 1.0f },
                { "gen_b_wave", bSaw }, { "gen_b_ratio", 2.0f }, { "gen_b_level", 0.25f },
                { "sub_level", 0.75f },
                { "filter_model", ladder }, { "filter_cutoff", 2500.0f }, { "filter_res", 0.15f },
                { "emit_mode", tube }, { "emit_drive", 12.0f }, { "emit_tone", 0.2f }, { "emit_split", 160.0f },
                { "reg_amount", 0.35f }, { "master_volume", -7.0f } } });

            juce::ignoreUnused (specTable, lfoSine);
            return p;
        }
    }

    const std::vector<Preset>& factoryPresets()
    {
        static const std::vector<Preset> presets = build();
        return presets;
    }
}
