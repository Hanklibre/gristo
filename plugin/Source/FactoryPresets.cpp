#include "Presets.h"

#include "gristo/Chords.h"

// Starting points that pair a groove with sounds. Each lists only what differs
// from the defaults. Map positions follow docs/map.md (node i sits at i / 5).
// Choice parameters take their index; percent parameters 0..1.

namespace {

using V = std::vector<std::pair<juce::String, float>>;

enum Timing { Trip = 0, Shake, Push, Clave };
enum Feeling { Tight = 0, Rolls, Flam, Slop };
enum KitIndex { Classic = 0, K808, Trap, LoFi, Minimal, Reggaeton, House, Techno, Breakbeat, Jungle, Amapiano, Dancehall, Temple, DustySoul };
enum Sound { Sub = 0, Acid, Reese, FmBass, EPiano, Bell, Pluck, Organ, WarmPad, GlassPad, DarkPad, SquareLead, SyncLead, SoftLead,
             FeltPiano, Celesta, BreathPad, SlowStrings, PolyStrings, SynthBrass, PolySynth, TapeKeys,
             SuitcaseKeys, Vibraphone, UprightBass, JazzOrgan, Bass808, MonkChoir, VoxPad, SingingBowl, NylonGuitar, Koto };
enum ChordPlay { Off = 0, Held, Stabs };
enum Scale { Major = 0, Minor, Dorian, Phrygian, Lydian, Mixolydian, HarmonicMinor, MinorPenta, MajorPenta, Blues, Chromatic, Insen, Hirajoshi, PhrygianDom };

// Generate-mode arp: index, sound, root note, scale, the voices that step it. (Synth
// settings are per sound-engine synth: arp 3 plays synth 3, the chords synth 2.)
V arp(int a, Sound sound, int root, Scale scale, std::initializer_list<int> triggers, float density, float gate = 2.0f)
{
    const juce::String n(a);
    V v { { "arpOn" + n, 1.0f }, { "arpMode" + n, 1.0f }, { "synSnd" + n, static_cast<float>(sound) },
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

// The harmony: key (and major / minor), home cell on the map, how far it wanders.
V chords(int key, bool minor, int column, int row, float wander, ChordPlay play, Sound sound, std::initializer_list<int> stabs = {})
{
    V v { { "chOn", 1.0f }, { "chKey", static_cast<float>(key) }, { "chMinor", minor ? 1.0f : 0.0f },
          { "chHomeX", gristo::Chords::columnToX(column) }, { "chHomeY", gristo::Chords::rowToY(row) }, { "chWander", wander },
          { "chPlay", static_cast<float>(play) }, { "synSnd" + juce::String(gristo::sound::kChordSynth), static_cast<float>(sound) } };
    for (int voice = 0; voice < 6; ++voice)
    {
        bool on = false;
        for (int t : stabs)
            on = on || t == voice;
        v.push_back({ "chStab" + juce::String(voice), on ? 1.0f : 0.0f });
    }
    return v;
}

enum GritIndex { Tape = 0, Crunch, Drive };
V dirt(GritIndex grit, float amount) { return { { "grit", static_cast<float>(grit) }, { "dirt", amount } }; }

// Presets give the groove as Shuffle and Humanize. Most of it goes onto the Loose knob
// (so the knob shows how loose the preset is); what is left stays on the detailed controls.
V withLoose(V v)
{
    float shuffle = 1.4f, humanize = 0.24f; // what Loose gives at its default
    for (const auto& [id, value] : v)
    {
        if (id == "shuffle") shuffle = value;
        if (id == "humanize") humanize = value;
    }
    const float loose = juce::jlimit(0.0f, 1.0f, shuffle / 6.0f);
    v.erase(std::remove_if(v.begin(), v.end(), [](const auto& e) { return e.first == "shuffle" || e.first == "humanize"; }), v.end());
    v.push_back({ "loose", loose });
    v.push_back({ "shuffle", std::max(0.0f, shuffle - loose * 3.5f) });
    v.push_back({ "humanize", std::max(0.0f, humanize - loose * 0.6f) });
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
    static const std::vector<Factory> presets = [] {
    std::vector<Factory> list {
        { "Trap Acid",
          join({ { { "mapX", 0.4f }, { "mapY", 1.0f }, { "timing", Shake }, { "shuffle", 0.5f }, { "feeling", Rolls },
                   { "kitType", Trap }, { "kitDecay", 0.62f }, { "kitTune", -2.0f }, { "fill2", 0.75f } },
                 arp(0, Acid, 36, Minor, { Kick, Perc1 }, 0.75f), dirt(Drive, 0.45f),
                 { { "arpSlide0", 0.35f }, { "arpAcc0", 0.35f }, { "synRes0", 0.65f } } }) },
        { "Boom Bap Keys",
          join({ { { "mapX", 0.6f }, { "mapY", 0.6f }, { "timing", Shake }, { "shuffle", 3.5f }, { "humanize", 0.5f },
                   { "kitType", LoFi }, { "kitRev", 0.12f } },
                 arp(0, Sub, 36, Dorian, { Kick }, 0.9f), dirt(Crunch, 0.5f),
                 arp(1, EPiano, 60, Dorian, { Snare, Perc1 }, 0.7f, 4.0f), { { "synRev1", 0.3f } } }) },
        { "Reggaeton Night",
          join({ { { "mapX", 0.2f }, { "mapY", 0.6f }, { "timing", Shake }, { "shuffle", 1.0f }, { "kitType", Reggaeton } },
                 arp(0, FmBass, 36, Phrygian, { Kick }, 0.9f),
                 arp(1, Pluck, 60, Phrygian, { Hat }, 0.6f), { { "synDly1", 0.3f } } }) },
        { "808 House Pads",
          join({ { { "mapX", 0.2f }, { "mapY", 0.0f }, { "timing", Shake }, { "shuffle", 1.5f }, { "kitType", K808 } },
                 arp(0, Sub, 36, Minor, { OpenHat }, 0.95f), { { "arpLen0", 4.0f } },
                 arp(1, WarmPad, 60, Minor, { Kick }, 0.5f, 8.0f), { { "synRev1", 0.4f } } }) },
        { "Minimal Bells",
          join({ { { "mapX", 0.2f }, { "mapY", 0.8f }, { "timing", Clave }, { "shuffle", 3.0f }, { "kitType", Minimal },
                   { "fxTime", 2.0f }, { "fxFb", 0.5f } },
                 arp(1, Bell, 72, MinorPenta, { Perc1, Perc2 }, 0.8f), { { "synDly1", 0.45f }, { "synRev1", 0.35f } } }) },
        { "Lazy Organ",
          join({ { { "mapX", 1.0f }, { "mapY", 0.4f }, { "timing", Push }, { "shuffle", 4.0f }, { "humanize", 0.6f },
                   { "kitType", LoFi } },
                 arp(0, Sub, 36, Dorian, { Kick }, 0.85f), dirt(Tape, 0.6f),
                 arp(1, Organ, 60, Dorian, { Snare }, 0.8f, 4.0f) }) },
        { "Electro Sync",
          join({ { { "mapX", 0.4f }, { "mapY", 0.2f }, { "timing", Shake }, { "shuffle", 0.5f }, { "kitType", K808 } },
                 arp(0, SyncLead, 60, Mixolydian, { Hat }, 0.8f), { { "arpMut0", 0.2f } },
                 arp(1, DarkPad, 48, Mixolydian, { Kick }, 0.4f, 8.0f) }) },
        { "2-Step Glass",
          join({ { { "mapX", 1.0f }, { "mapY", 0.8f }, { "timing", Shake }, { "shuffle", 4.0f }, { "kitType", K808 } },
                 arp(0, Reese, 36, Minor, { Kick }, 0.85f), { { "arpSlide0", 0.25f } },
                 arp(1, GlassPad, 60, Minor, { Snare }, 0.6f, 8.0f), { { "synRev1", 0.45f } } }) },
        { "Pavilion Haze",
          join({ { { "mapX", 0.2f }, { "mapY", 0.2f }, { "fill0", 0.2f }, { "fill1", 0.0f }, { "fill4", 0.15f },
                   { "kitType", Minimal }, { "kitTone", 0.3f }, { "kitRev", 0.35f }, { "fxSize", 0.9f }, { "fxTime", 2.0f },
                   { "fxFb", 0.45f } },
                 chords(2, false, 0, 3, 0.6f, Held, FeltPiano), dirt(Tape, 0.5f), { { "synRev3", 0.6f }, { "synRel3", 0.75f } },
                 arp(1, Celesta, 72, MajorPenta, { Hat, Perc2 }, 0.4f, 4.0f),
                 { { "arpFollow1", 1.0f }, { "synRev1", 0.6f }, { "synDly1", 0.35f } } }) },
        { "Jazz Blues Walk",
          join({ { { "mapX", 0.6f }, { "mapY", 0.4f }, { "timing", Shake }, { "shuffle", 5.0f }, { "humanize", 0.5f },
                   { "kitType", LoFi } },
                 chords(5, false, 0, 3, 0.7f, Stabs, SuitcaseKeys, { Snare, OpenHat }), dirt(Tape, 0.4f), { { "synRev3", 0.2f } },
                 arp(0, UprightBass, 36, Major, { Kick, Hat }, 0.85f, 3.0f), { { "arpFollow0", 1.0f } } }) },
        { "Jungle Reese",
          join({ { { "mapX", 0.8f }, { "mapY", 0.8f }, { "timing", Shake }, { "shuffle", 0.5f }, { "kitType", Jungle } },
                 chords(2, true, 0, 2, 0.25f, Held, SlowStrings), dirt(Crunch, 0.45f), { { "synRev3", 0.45f } },
                 arp(0, Reese, 36, Dorian, { Kick, Perc1 }, 0.8f), { { "arpFollow0", 1.0f } } }) },
        { "Amapiano Logs",
          join({ { { "mapX", 0.4f }, { "mapY", 0.4f }, { "timing", Shake }, { "shuffle", 2.0f }, { "kitType", Amapiano } },
                 chords(5, true, 0, 3, 0.5f, Held, SuitcaseKeys), { { "synRev3", 0.3f } },
                 arp(1, Vibraphone, 72, Dorian, { Perc2 }, 0.6f), { { "arpFollow1", 1.0f }, { "synDly1", 0.3f } } }) },
        { "Techno Brass",
          join({ { { "mapX", 0.0f }, { "mapY", 0.0f }, { "kitType", Techno } },
                 chords(9, true, 0, 2, 0.4f, Stabs, SynthBrass, { OpenHat }), dirt(Drive, 0.5f), { { "synDly3", 0.25f } },
                 arp(0, Sub, 36, Minor, { Kick }, 0.9f), { { "arpFollow0", 1.0f } } }) },
        { "House Organ",
          join({ { { "mapX", 0.2f }, { "mapY", 0.0f }, { "timing", Shake }, { "shuffle", 1.5f }, { "kitType", House } },
                 chords(0, true, 0, 2, 0.5f, Stabs, JazzOrgan, { OpenHat }),
                 arp(0, Sub, 36, Minor, { Kick }, 0.9f), { { "arpFollow0", 1.0f } } }) },
        // Hybrids: jungle, R&B, trap, hip-hop, ambient, chant and soul, crossed with each other.
        { "Jungle Monastery", // breaks under a monk choir, bowls on the off-beats
          join({ { { "mapX", 0.6f }, { "mapY", 0.4f }, { "timing", Shake }, { "shuffle", 0.5f }, { "kitType", Jungle },
                   { "freeTempo", 165.0f }, { "fxSize", 0.8f }, { "fxShim", 0.35f }, { "ratch2", 0.15f } },
                 chords(2, true, 0, 1, 0.3f, Held, MonkChoir), dirt(Crunch, 0.45f), { { "synRev3", 0.5f }, { "chRate", 2.0f } },
                 arp(0, Reese, 36, Phrygian, { Kick }, 0.8f), { { "arpFollow0", 1.0f }, { "synDuck0", 0.3f } },
                 arp(1, SingingBowl, 72, Insen, { Perc1 }, 0.5f, 4.0f), { { "synRev1", 0.6f }, { "arpDiv1", 1.0f } } }) },
        { "Liquid Soul", // rolling breaks, suitcase keys and a vox pad
          join({ { { "mapX", 1.0f }, { "mapY", 1.0f }, { "timing", Shake }, { "shuffle", 1.0f }, { "kitType", Breakbeat },
                   { "freeTempo", 168.0f }, { "fxSize", 0.7f } },
                 chords(5, true, 0, 3, 0.55f, Held, VoxPad), dirt(Tape, 0.35f), { { "synRev3", 0.5f }, { "chRate", 2.0f } },
                 arp(0, Sub, 36, Dorian, { Kick }, 0.85f), { { "arpFollow0", 1.0f } },
                 arp(1, SuitcaseKeys, 60, Dorian, { Snare, Perc1 }, 0.6f, 3.0f), { { "arpFollow1", 1.0f }, { "synDly1", 0.25f } } }) },
        { "Trap Sutra", // sliding 808, temple percussion, bowl melody
          join({ { { "mapX", 0.4f }, { "mapY", 1.0f }, { "timing", Shake }, { "shuffle", 0.5f }, { "feeling", Rolls },
                   { "kitType", Temple }, { "freeTempo", 140.0f }, { "fxShim", 0.25f }, { "ratch2", 0.25f } },
                 chords(7, true, 0, 2, 0.35f, Off, MonkChoir), dirt(Drive, 0.4f),
                 arp(0, Bass808, 36, Phrygian, { Kick }, 0.9f, 4.0f), { { "arpFollow0", 1.0f }, { "arpSlide0", 0.35f }, { "synGld0", 0.5f } },
                 arp(1, Koto, 72, Insen, { Hat, Perc2 }, 0.45f, 3.0f), { { "synRev1", 0.45f }, { "synDly1", 0.3f } } }) },
        { "Dust Hymn", // lazy hip-hop with a hummed choir and nylon guitar
          join({ { { "mapX", 0.6f }, { "mapY", 0.6f }, { "timing", Shake }, { "shuffle", 4.0f }, { "humanize", 0.5f },
                   { "kitType", DustySoul }, { "freeTempo", 84.0f } },
                 chords(0, true, 0, 3, 0.5f, Held, MonkChoir), dirt(Tape, 0.65f), { { "synRev3", 0.35f }, { "synLvl3", 0.5f } },
                 arp(0, UprightBass, 36, Minor, { Kick }, 0.85f, 3.0f), { { "arpFollow0", 1.0f } },
                 arp(1, NylonGuitar, 60, MinorPenta, { Snare, Hat }, 0.5f, 3.0f), { { "arpFollow1", 1.0f }, { "synRev1", 0.3f } } }) },
        { "Bowl Drift", // ethereal ambient: almost no drums, shimmer, wandering harmony
          join({ { { "mapX", 0.2f }, { "mapY", 0.8f }, { "busy", 0.25f }, { "kitType", Temple }, { "kitRev", 0.5f },
                   { "freeTempo", 72.0f }, { "fxSize", 0.95f }, { "fxShim", 0.6f }, { "fxTime", 3.0f }, { "fxFb", 0.5f }, { "space", 0.7f } },
                 chords(4, false, 0, 3, 0.7f, Held, VoxPad), dirt(Tape, 0.4f),
                 { { "synRev3", 0.7f }, { "synRel3", 0.8f }, { "chRate", 2.0f }, { "chStrum", 40.0f } },
                 arp(1, SingingBowl, 72, MajorPenta, { Perc1, OpenHat }, 0.4f, 6.0f), { { "arpFollow1", 1.0f }, { "synRev1", 0.7f } },
                 arp(2, Koto, 84, MajorPenta, { Perc2 }, 0.3f, 4.0f), { { "arpFollow2", 1.0f }, { "synDly2", 0.45f }, { "synRev2", 0.6f } } }) },
        { "Indie Soul Tape", // dry kit, strummed nylon chords, felt piano answers
          join({ { { "mapX", 0.8f }, { "mapY", 0.0f }, { "timing", Shake }, { "shuffle", 2.5f }, { "humanize", 0.4f },
                   { "kitType", DustySoul }, { "freeTempo", 96.0f } },
                 chords(7, false, 0, 2, 0.45f, Stabs, NylonGuitar, { Snare, OpenHat }), dirt(Tape, 0.55f),
                 { { "chStrum", 28.0f }, { "chStrumDir", 2.0f }, { "chGate", 4.0f } },
                 arp(0, UprightBass, 36, Major, { Kick }, 0.85f, 3.0f), { { "arpFollow0", 1.0f } },
                 arp(1, FeltPiano, 72, MajorPenta, { Perc1 }, 0.45f, 4.0f), { { "arpFollow1", 1.0f }, { "synRev1", 0.4f } } }) },
        { "Slow Jam 808", // R&B: half-time trap drums, e-piano ninths, vox pad
          join({ { { "mapX", 0.2f }, { "mapY", 1.0f }, { "timing", Shake }, { "shuffle", 1.5f }, { "kitType", Trap },
                   { "freeTempo", 68.0f }, { "ratch2", 0.2f } },
                 chords(10, true, 0, 3, 0.5f, Held, SuitcaseKeys), dirt(Tape, 0.3f), { { "synRev3", 0.4f }, { "chStrum", 18.0f } },
                 arp(0, Bass808, 36, Minor, { Kick }, 0.9f, 6.0f), { { "arpFollow0", 1.0f }, { "arpSlide0", 0.3f } },
                 arp(1, VoxPad, 60, Minor, { Snare }, 0.5f, 8.0f), { { "arpFollow1", 1.0f }, { "synRev1", 0.6f }, { "synLvl1", 0.45f } } }) },
        { "Chant Break", // boom bap break, wood fish, throat drone
          join({ { { "mapX", 0.6f }, { "mapY", 0.6f }, { "timing", Shake }, { "shuffle", 3.0f }, { "kitType", Temple },
                   { "freeTempo", 90.0f }, { "wild", 0.25f } },
                 chords(2, true, 0, 0, 0.15f, Held, MonkChoir), dirt(Crunch, 0.6f), { { "chOct", 0.0f }, { "synRev3", 0.45f }, { "chRate", 2.0f } },
                 arp(0, Sub, 36, Hirajoshi, { Kick }, 0.9f), { { "arpFollow0", 1.0f } },
                 arp(1, Bell, 72, Hirajoshi, { Perc1, Perc2 }, 0.4f, 4.0f), { { "synRev1", 0.55f }, { "synDly1", 0.3f } } }) },
    };
    for (auto& preset : list)
        preset.values = withLoose(preset.values);
    return list;
    }();
    return presets;
}
