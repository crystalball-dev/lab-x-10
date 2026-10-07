#pragma once

namespace labx10
{
    inline constexpr const char* pluginName    = "LAB X-10";
    inline constexpr const char* vendorName    = "OPERATION FAIRWAY, LLC";
    inline constexpr const char* versionString = "0.1.0";
    inline constexpr const char* projectUrl    = "github.com/crystalball-dev/lab-x-10";

    inline constexpr int maxVoices    = 6;    // voice slots; mono modes use spare slots to hand notes over cleanly
    inline constexpr int maxUnison    = 7;    // MONOLITH copies of GENERATOR A
    inline constexpr int controlBlock = 16;   // samples per control-rate step
}
