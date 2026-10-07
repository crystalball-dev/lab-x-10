#pragma once

#include <iterator>

namespace labx10
{
    // Sounds the SPECIMEN layer can read from the user's own disk: the 'stalker sounds' pack that ships
    // with older FL Studio installs, and the STALKER SPECIMENS library, a folder of sounds extracted from
    // the user's own copies of the games (<drive>:\_AUDIO\STALKER SPECIMENS\<game>\sounds\...).
    // Only relative paths live here. This repository contains no game audio.
    //
    // Append new entries at the end only. Choice indices are saved inside host projects.
    // The pack's music folder is deliberately absent: it holds commercial tracks by other artists.
    //
    // Measured character (averaged 8192-point FFT of the mono decode) is noted per entry.
    enum class SpecimenRoot { flPack, library };

    struct SpecimenEntry
    {
        const char* name;
        const char* relativePath;
        SpecimenRoot root = SpecimenRoot::flPack;
    };

    inline constexpr SpecimenEntry specimenCatalog[] =
    {
        { "Generator Hall",     "ambient/ugrnd/ugrnd_lab_2.ogg" },                // 81 Hz with a 43 Hz partner
        { "Machine Room",       "ambient/ugrnd/ugrnd_ambient_machine_2.ogg" },    // 108 Hz, 97 % in 60-250 Hz
        { "Turbine",            "ambient/ugrnd/ugrnd_ambient_machine_3.ogg" },    // 124 Hz, 99 % in 60-250 Hz
        { "Bunker Drone",       "ambient/ugrnd/ugrnd_drone_2.ogg" },              // 86 Hz
        { "Cooling Run",        "ambient/cooling_run.ogg" },                      // 43 Hz, very loud
        { "Cooling Stop",       "ambient/cooling_stop.ogg" },                     // 108 Hz winding down
        { "Fan",                "ambient/fan1.ogg" },                             // 43 Hz, 68 % below 60 Hz
        { "Radar Pulse",        "ambient/radar_2.ogg" },                          // 48 Hz pulse
        { "Transformer",        "ambient/transformer_hum1.ogg" },                 // 118 Hz with 59 Hz
        { "Servo",              "device/door_servomotor.ogg" },                   // 27 Hz growl
        { "Bloodsucker Growl",  "monsters/bloodsucker/sucker_growl_2.ogg" },      // 38 Hz
        { "Bloodsucker Hidden", "monsters/bloodsucker/forest_hidden_3.ogg" },     // 54 Hz
        { "Body Tear",          "anomaly/anomaly_body_tear_1.ogg" },              // 32 Hz impact
        { "Banging",            "ambient/ugrnd/ugrnd_ambient_banging_4.ogg" },    // 75 Hz impacts
        { "Relay",              "ambient/switch_1.ogg" },                         // switchgear transient
        { "Sparks",             "ambient/sparks1.ogg" },                          // 6.3 kHz, noise-like
        { "Alarm",              "ambient/siren2.ogg" },                           // 2.5 kHz
        { "Psi Whispers",       "ambient/ugrnd/ugrnd_whispers_1.ogg" },           // 495 Hz
        { "Vampire",            "monsters/bloodsucker/vampire_sucking.ogg" },     // 43 Hz
        { "Pipe Hum",           "ambient/ugrnd/ugrnd_metal_5.ogg" },              // 92 Hz, 98 % in 60-250 Hz

        // STALKER SPECIMENS library. Table pitch as measured by the wavetable cutter.
        { "Scorcher Radar",       "shadow_of_chornobyl/sounds/device/radar_idle.ogg", SpecimenRoot::library },                    // 22 Hz, the Radar's machinery
        { "Radar Wind-Down",      "shadow_of_chornobyl/sounds/device/radar_stop.ogg", SpecimenRoot::library },                    // 34 Hz, winding down
        { "X-16 Brain",           "shadow_of_chornobyl/sounds/ambient/x16/x16_brain_run.ogg", SpecimenRoot::library },            // 256 Hz
        { "X-16 Psy Field",       "shadow_of_chornobyl/sounds/ambient/x16/x16_psy_l.ogg", SpecimenRoot::library },                // 119 Hz, 91 % in 60-250 Hz
        { "Monolith",             "shadow_of_chornobyl/sounds/ambient/monolith_1.ogg", SpecimenRoot::library },                   // 58 Hz
        { "Psy Blackout",         "shadow_of_chornobyl/sounds/affects/psy_blackout_r.ogg", SpecimenRoot::library },               // 105 Hz; the _l file starts with a +71 dBFS decode burst
        { "Swamp Generator",      "clear_sky/sounds/ambient/special/marsh_generator.ogg", SpecimenRoot::library },                // 100 Hz
        { "Jupiter Generator",    "call_of_prypiat/sounds/ambient/jupiter/jup_b219_generator_looped.ogg", SpecimenRoot::library }, // 95 Hz
        { "Pripyat Generator",    "call_of_prypiat/sounds/ambient/pripyat/pri_b306_generator_work.ogg", SpecimenRoot::library },  // 275 Hz
        { "Underpass Transformer","call_of_prypiat/sounds/ambient/underpass/pas_b400_transformer.ogg", SpecimenRoot::library },   // noise-like
        { "X7 Generator",         "heart_of_chornobyl/sounds/Events/GSCAudioVolumes/AV_Generators/SFX_AV_Generators_X7.flac", SpecimenRoot::library },            // 25 Hz
        { "Generator Sub",        "heart_of_chornobyl/sounds/Events/GSCAudioVolumes/AV_Generators/SFX_AV_Generators_Large_Loop_16.flac", SpecimenRoot::library }, // 28 Hz, 76 % below 60 Hz
        { "Electro Anomaly",      "heart_of_chornobyl/sounds/Events/Anomalies/Electro/SFX_Anomaly_Electro_01.flac", SpecimenRoot::library },                     // 35 Hz, 85 % below 60 Hz
        { "Arena Generator",      "heart_of_chornobyl/sounds/Events/GSCAudioVolumes/AV_Generators/SFX_AV_Generators_Strelok_Arena_09.flac", SpecimenRoot::library }, // 45 Hz
        { "Mains Hum",            "heart_of_chornobyl/sounds/Events/GSCAudioVolumes/AV_Generators/SFX_AV_Generators_Large_Loop_06.flac", SpecimenRoot::library }, // 50 Hz, 78 % below 60 Hz
        { "Psy Noise",            "heart_of_chornobyl/sounds/Events/Effects/Psy/SFX_Psy_Noise_Play_02.flac", SpecimenRoot::library },                             // 53 Hz
        { "Hundred Hertz",        "heart_of_chornobyl/sounds/Events/GSCAudioVolumes/AV_Generators/SFX_AV_Generators_Large_Loop_22.flac", SpecimenRoot::library }, // 99 Hz, 98 % in 60-250 Hz
    };

    inline constexpr int specimenCatalogSize = (int) std::size (specimenCatalog);

    // Choice index 0 is the user's own file. Catalog entry i is choice i + 1.
    inline constexpr int specimenUserChoice = 0;

    // Location of the pack inside an FL Studio installation folder.
    inline constexpr const char* specimenPackSubPath = "Data/Patches/Packs/Legacy/stalker sounds";

    // Default location of the STALKER SPECIMENS library on any drive, and the game folders it may hold.
    inline constexpr const char* specimenLibrarySubPath = "_AUDIO/STALKER SPECIMENS";
    inline constexpr const char* specimenLibraryGames[] = { "shadow_of_chornobyl", "clear_sky", "call_of_prypiat",
                                                            "heart_of_chornobyl" };
}
