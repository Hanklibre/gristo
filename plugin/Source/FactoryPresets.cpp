#include "Presets.h"

// Starting points that pair a groove with sounds. Each lists only what differs
// from the defaults. Map positions follow docs/map.md (node i sits at i / 5).
// Choice parameters take their index; percent parameters 0..1.

namespace {

using V = std::vector<std::pair<juce::String, float>>;

enum Timing { Trip = 0, Shake, Push, Clave };
enum Feeling { Tight = 0, Rolls, Flam, Slop };
enum KitIndex { Classic = 0, K808, Trap, LoFi, Minimal, Reggaeton };
enum Sound { Sub = 0, Acid, Reese, FmBass, EPiano, Bell, Pluck, Organ, WarmPad, GlassPad, DarkPad, SquareLead, SyncLead, SoftLead };
enum Scale { Major = 0, Minor, Dorian, Phrygian, Lydian, Mixolydian, HarmonicMinor, MinorPenta, MajorPenta, Blues, Chromatic };

// Generate-mode arp: index, sound, root note, scale, the voices that step it.
V arp(int a, Sound sound, int root, Scale scale, std::initializer_list<int> triggers, float density, float gate = 2.0f)
{
    const juce::String n(a);
    V v { { "arpOn" + n, 1.0f }, { "arpMode" + n, 1.0f }, { "arpSnd" + n, static_cast<float>(sound) },
          { "arpRoot" + n, static_cast<float>(root) }, { "arpScale" + n, static_cast<float>(scale) },
          { "arpDens" + n, density }, { "arpGate" + n, gate } };
    for (int voice = 0; voice < 6; ++voice)
    {
        bool on = false;
        for (int t : triggers)
            on = on || t == voice;
        v.push_back({ "arpTrig" + n + "_" + juce::String(voice), on ? 1.0f : 0.0f });
    }
    return v;
}

V join(std::initializer_list<V> parts)
{
    V out;
    for (const auto& part : parts)
        out.insert(out.end(), part.begin(), part.end());
    return out;
}

constexpr int Kick = 0, Snare = 1, Hat = 2, OpenHat = 3, Perc1 = 4, Perc2 = 5;

} // namespace

const std::vector<PresetManager::Factory>& PresetManager::factoryPresets()
{
    static const std::vector<Factory> presets {
        { "Trap Acid",
          join({ { { "mapX", 0.4f }, { "mapY", 1.0f }, { "timing", Shake }, { "shuffle", 0.5f }, { "feeling", Rolls },
                   { "kitType", Trap }, { "kitDecay", 0.62f }, { "kitTune", -2.0f }, { "fill2", 0.75f } },
                 arp(0, Acid, 36, Minor, { Kick, Perc1 }, 0.75f),
                 { { "arpSlide0", 0.35f }, { "arpAcc0", 0.35f }, { "arpRes0", 0.65f } } }) },
        { "Boom Bap Keys",
          join({ { { "mapX", 0.6f }, { "mapY", 0.6f }, { "timing", Shake }, { "shuffle", 3.5f }, { "humanize", 0.5f },
                   { "kitType", LoFi }, { "kitRev", 0.12f } },
                 arp(0, Sub, 36, Dorian, { Kick }, 0.9f),
                 arp(1, EPiano, 60, Dorian, { Snare, Perc1 }, 0.7f, 4.0f), { { "arpRev1", 0.3f } } }) },
        { "Reggaeton Night",
          join({ { { "mapX", 0.2f }, { "mapY", 0.6f }, { "timing", Shake }, { "shuffle", 1.0f }, { "kitType", Reggaeton } },
                 arp(0, FmBass, 36, Phrygian, { Kick }, 0.9f),
                 arp(1, Pluck, 60, Phrygian, { Hat }, 0.6f), { { "arpDly1", 0.3f } } }) },
        { "808 House Pads",
          join({ { { "mapX", 0.2f }, { "mapY", 0.0f }, { "timing", Shake }, { "shuffle", 1.5f }, { "kitType", K808 } },
                 arp(0, Sub, 36, Minor, { OpenHat }, 0.95f), { { "arpLen0", 4.0f } },
                 arp(1, WarmPad, 60, Minor, { Kick }, 0.5f, 8.0f), { { "arpRev1", 0.4f } } }) },
        { "Minimal Bells",
          join({ { { "mapX", 0.2f }, { "mapY", 0.8f }, { "timing", Clave }, { "shuffle", 3.0f }, { "kitType", Minimal },
                   { "fxTime", 2.0f }, { "fxFb", 0.5f } },
                 arp(1, Bell, 72, MinorPenta, { Perc1, Perc2 }, 0.8f), { { "arpDly1", 0.45f }, { "arpRev1", 0.35f } } }) },
        { "Lazy Organ",
          join({ { { "mapX", 1.0f }, { "mapY", 0.4f }, { "timing", Push }, { "shuffle", 4.0f }, { "humanize", 0.6f },
                   { "kitType", LoFi } },
                 arp(0, Sub, 36, Dorian, { Kick }, 0.85f),
                 arp(1, Organ, 60, Dorian, { Snare }, 0.8f, 4.0f) }) },
        { "Electro Sync",
          join({ { { "mapX", 0.4f }, { "mapY", 0.2f }, { "timing", Shake }, { "shuffle", 0.5f }, { "kitType", K808 } },
                 arp(0, SyncLead, 60, Mixolydian, { Hat }, 0.8f), { { "arpMut0", 0.2f } },
                 arp(1, DarkPad, 48, Mixolydian, { Kick }, 0.4f, 8.0f) }) },
        { "2-Step Glass",
          join({ { { "mapX", 1.0f }, { "mapY", 0.8f }, { "timing", Shake }, { "shuffle", 4.0f }, { "kitType", K808 } },
                 arp(0, Reese, 36, Minor, { Kick }, 0.85f), { { "arpSlide0", 0.25f } },
                 arp(1, GlassPad, 60, Minor, { Snare }, 0.6f, 8.0f), { { "arpRev1", 0.45f } } }) },
    };
    return presets;
}
