#include "FactoryPresets.h"
#include "Params.h"

namespace labx10
{
    namespace
    {
        std::vector<Preset> build()
        {
            std::vector<Preset> p;

            // 1. The init patch: a clean sine sub and nothing else.
            p.push_back ({ "Emitter Disabled", {
                { "gen_a_level", 0.0f }, { "sub_wave", 0.0f }, { "sub_level", 0.8f }, { "sub_octave", -1.0f },
                { "reg_amount", 0.0f } } });

            return p;
        }
    }

    const std::vector<Preset>& factoryPresets()
    {
        static const std::vector<Preset> presets = build();
        return presets;
    }
}
