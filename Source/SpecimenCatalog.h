#pragma once

#include <iterator>

namespace labx10
{
    // Sounds the SPECIMEN layer can read from a local copy of the S.T.A.L.K.E.R. sound pack.
    // Only relative paths live here. This repository contains no game audio: the plugin reads
    // the files from the user's own disk at runtime.
    //
    // Append new entries at the end only. Choice indices are saved inside host projects.
    // The pack's music folder is deliberately absent: it holds commercial tracks by other artists.
    //
    // Measured character (averaged 8192-point FFT of the mono decode) is noted per entry.
    struct SpecimenEntry
    {
        const char* name;
        const char* relativePath;
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
    };

    inline constexpr int specimenCatalogSize = (int) std::size (specimenCatalog);

    // Choice index 0 is the user's own file. Catalog entry i is choice i + 1.
    inline constexpr int specimenUserChoice = 0;

    // Location of the pack inside an FL Studio installation folder.
    inline constexpr const char* specimenPackSubPath = "Data/Patches/Packs/Legacy/stalker sounds";
}
