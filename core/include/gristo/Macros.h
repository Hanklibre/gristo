#pragma once

#include "gristo/Engine.h"

#include <algorithm>

namespace gristo {

// The big knobs: each moves a family of settings that usually go together. They act
// on top of the detailed settings (which stay as the balance between the parts), so
// Busy at 50 %, Loose at 0 and Wild at 0 leave a patch exactly as it was set up.
struct MacroParams
{
    float busy = 0.5f;  // 0 = nothing plays ... 0.5 = as set ... 1 = everything, with ratchets
    float loose = 0.0f; // shuffle, humanize and flam together: machine-tight ... drunk
    float wild = 0.0f;  // how much the pattern changes by itself from bar to bar
};

inline void applyMacros(EngineParams& p, const MacroParams& m)
{
    auto unit = [](float v) { return std::clamp(v, 0.0f, 1.0f); };
    const float busy = unit(m.busy), loose = unit(m.loose), wild = unit(m.wild);

    for (int v = 0; v < kNumVoices; ++v)
    {
        const auto i = static_cast<size_t>(v);
        float& fill = p.pattern.fill[i];
        fill = busy <= 0.5f ? fill * busy * 2.0f : fill + (1.0f - fill) * (busy - 0.5f) * 1.6f;
        // The top of Busy adds ratchets on the hats and percussion.
        if (v >= HiHat1 && busy > 0.7f)
            p.groove.ratchet[i] = unit(p.groove.ratchet[i] + (busy - 0.7f));
        p.pattern.fillRandom[i] = unit(p.pattern.fillRandom[i] + wild * 0.7f);
        p.pattern.probabilityRandom[i] = unit(p.pattern.probabilityRandom[i] + wild * 0.4f);
    }

    p.groove.shuffle = std::clamp(p.groove.shuffle + loose * 3.5f, 0.0f, 6.0f);
    p.groove.shuffleRandom = unit(p.groove.shuffleRandom + loose * loose * 0.5f);
    p.groove.feelMs *= 1.0f + loose;
    p.groove.feelRandom = unit(p.groove.feelRandom + loose * 0.4f);
    p.humanize.amount = unit(p.humanize.amount + loose * 0.6f);

    p.pattern.chaos = unit(p.pattern.chaos + wild * 0.5f);
    p.pattern.mapRandom = unit(p.pattern.mapRandom + wild * 0.7f);
    for (ArpParams& arp : p.arps)
    {
        arp.mutate = unit(arp.mutate + wild * 0.6f);
        arp.probabilityRandom = unit(arp.probabilityRandom + wild * 0.4f);
    }
}

} // namespace gristo
