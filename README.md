# Gristo

Gristo is a topographic drum generator that trips over itself. It's a VST3 / AU /
standalone plugin that combines a Grids-style morphable 6-voice drum map with a
Stolperbeats-style groove engine and adds a groove-aware humanizer.

* **Map:** drag X/Y through 36 grooves (boom bap, breaks, house, afrobeat,
  trap and more). Morph between them, sculpt each voice with Fill, and add
  Chaos. A per-voice Probability makes hits play only some of the time.
* **Groove:** Trip / Shake / Push / Clave timing with a continuous Shuffle
  control and Tight / Rolls / Flam / Slop feelings. Linear mode gives
  one-limb-at-a-time drumming.
* **Humanize:** anchors stay tight while ghost notes breathe. Drift is
  correlated like a player's, weak notes lean into the swing, and the groove's
  shape is never broken.
* **Random:** Beads-style random amounts on Flam / Rolls (per hit), Shuffle
  (per beat), the map position and each voice's Fill and Probability (per bar).
* **Modulation:** four Massive-style performer lanes. Draw a curve (1, 2 or 4
  bars) and drag it onto a knob to modulate Fill, Probability, Velocity, the map,
  Chaos, Shuffle, Flam or Humanize, or each arp's Density, Accent, Slide, Gate,
  Octaves, Length, Velocity and Transpose. Rings on the knobs show what is
  routed where.
* **Per voice:** Fill, Fill Random, Probability, Probability Random, Humanize,
  Dynamics, Velocity range (1–127), MIDI note, MIDI channel and Mute, laid out
  as a mixer-style grid. Hover any control for a one-line explanation.
* **Two drum-clocked arps:** each arp steps forward when the drum voices you
  pick (kick, snare, any mix) play, and restarts every 1/2/4/8/16 bars. *Arp*
  mode plays the chord you hold on MIDI in (Up / Down / Up-Down / Random / As
  Played, 1–4 octaves, Latch). *Generate* mode is a Sting-style sequencer:
  pattern length, density (rests), accents, slides and Mutate, over the held
  notes or a scale. Velocity follows the drum hit, mapped into each arp's range,
  and each arp has its own MIDI channel.
* **Built-in sounds (Sounds tab):** six synthesized drum kits (Classic, 808,
  Trap, Lo-fi, Minimal, Reggaeton) with Tune / Decay / Tone / Drive and a level
  per drum, and a synth per arp with 14 sounds (Sub, Acid, Reese and FM bass;
  E-piano, Bell, Pluck, Organ; Warm, Glass and Dark pads; Square, Sync and Soft
  leads), each with Cutoff, Resonance, Env, Attack, Release, Glide and Level.
  A shared reverb and tempo-synced ping-pong delay with sends. Lanes can sweep
  each synth's Cutoff.
* **Presets:** a preset bar at the top. There are 8 factory presets (Trap
  Acid, Boom Bap Keys, Reggaeton Night, 808 House Pads, Minimal Bells, Lazy
  Organ, Electro Sync, 2-Step Glass), plus your own as `.gristo` files in
  `Documents/Gristo/Presets`. Use Save / Save As, step through presets with
  < >, and use Import / Export to move them between computers (or from the
  tablet). A preset stores everything musical, including the sounds, but not
  your MIDI notes, channels, Out Shift or Sound on/level, so loading one never
  breaks your DAW routing. `*` after the name means you've changed it.
* **UI size:** 70–125 %, and it picks a size that fits your screen the first time.
* **Output:** MIDI notes with a selectable note and channel per voice (GM drum
  notes three octaves down on channel 10 by default, so the drums sit below the
  arps), the arps on channels 1 and 2 with an Out Shift each, plus the built-in
  sounds, so it makes sound straight away.

See [docs/DESIGN.md](docs/DESIGN.md) for how it works and
[docs/map.md](docs/map.md) for the current map.

## Download

Every push builds the plugin on Windows, macOS (universal: Apple Silicon and
Intel) and Linux with GitHub Actions. Open the repository's **Actions** tab,
pick the latest *Build plugin* run, and download the artifact for your system.

Install the VST3 by copying `Gristo.vst3` to:

| OS | Folder |
| --- | --- |
| Windows | `C:\Program Files\Common Files\VST3\` |
| macOS | `~/Library/Audio/Plug-Ins/VST3/` (AU: `~/Library/Audio/Plug-Ins/Components/`) |
| Linux | `~/.vst3/` |

**macOS:** the build is not notarized, so remove the download quarantine once
after copying:

```sh
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Gristo.vst3
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/Gristo.component
```

**Windows:** SmartScreen may warn about the unsigned standalone `.exe`. The
VST3 loads in your DAW without a prompt.

## Android (tablet)

Every push also builds an Android app (`Gristo-Android-apk` in the Actions run,
arm64, for your own use).

1. Download and unzip the artifact on the tablet (or copy `Gristo.apk` over USB).
2. Open it and allow "install unknown apps" for your browser or file manager
   when Android asks. Later builds install over the old one (same signing key),
   so your presets stay.
3. Hold the tablet sideways: the window fills the width and scrolls with the
   bar on the right. **Keys** toggles a keyboard strip at the bottom for
   playing chords into the arps. A USB MIDI keyboard also works (it's picked
   up automatically). **Settings** has the audio and MIDI device options.
4. Tap any control to see its help text at the top.

**Presets between tablet and PC:** on the tablet, presets are saved in
`Documents/Gristo/Presets`. You can copy them to the PC over USB, or use the
preset menu's **Share...** (Quick Share, email to yourself, ...) or
**Export...**. On the PC, use **Import...** or drop the `.gristo` files into
`Documents\Gristo\Presets`. To go the other way, use **Import...** on the
tablet. MIDI notes, channels and Out Shift aren't part of a preset, so each
device keeps its own routing.

How it's built: JUCE's CMake API doesn't target Android, so CI unpacks JUCE
8.0.4, runs Projucer on `android/Gristo.jucer` (the same sources as the
desktop build) and builds the generated project with Gradle and NDK 26.2.
`android/gristo-debug.keystore` is a throwaway signing key for personal
sideloading, not a secret.

## Build

Requirements: CMake ≥ 3.22, a C++17 compiler. JUCE 8 is fetched automatically,
or you can pass `-DGRISTO_JUCE_DIR=/path/to/JUCE`. On Linux you also need the
usual JUCE dependencies (`libasound2-dev libx11-dev libxrandr-dev
libxinerama-dev libxcursor-dev libfreetype-dev libfontconfig1-dev`).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build          # core unit tests
```

Artefacts end up in `build/plugin/Gristo_artefacts/Release/` (VST3, AU on
macOS, Standalone).

Core only (no JUCE, fast):

```sh
cmake -S . -B build -DGRISTO_BUILD_PLUGIN=OFF && cmake --build build && ./build/tests/gristo_tests
```

## Using it in a DAW

* **Bitwig, Reaper, Cubase, Studio One:** load it as an instrument and route its
  MIDI out to your drum instrument, or just use the preview kit.
* **Ableton Live:** load it on a MIDI track. On the drum rack's track, set
  *MIDI From* to the Gristo track and choose "Gristo" as the source.
  * **Splitting drums and arps in Live (by note):** Live merges a plugin's
    MIDI output to channel 1, so Gristo keeps each part in its own note range
    instead. Drums default to the GM notes three octaves down (C-2 to A#-2), and
    each arp has an Out **Shift** (Arp 1: 0, Arp 2: +36 semitones). The arp page
    shows the exact range and the setting to use.
    1. Hold or clip the arps' chord on the Gristo track itself (its MIDI in).
    2. Make one MIDI track with *MIDI From* = the Gristo track / "Gristo", and
       put an Instrument Rack on it. Show the Key zone editor and add one chain
       per part:
       * **Drums:** key zone C-2 to B-2, a Pitch device at +36, then your Drum
         Rack (standard GM pads).
       * **Arp 1:** key zone as shown on the Arp 1 page, then the synth.
       * **Arp 2:** key zone as shown on the Arp 2 page, a Pitch device at -36
         (the opposite of its Shift), then the synth.

       For separate mixer tracks, use one listening track per part instead,
       each with an Instrument Rack holding just that part's chain.
    3. Turn *Kit* off once your instruments play.

    Hosts that keep MIDI channels (Bitwig, Reaper, Cubase) can split the parts
    by channel as well.
* **Logic:** use the AU instrument for now. An AU MIDI-FX build is on the
  roadmap.
* **No transport running?** Enable *Free Run* to audition at the internal
  tempo.

## Tools

```sh
# Render a groove to MIDI to audition settings in a DAW
./build/gristo_render --x 0.7 --y 0.6 --timing trip --shuffle 4 --humanize 0.4 --bars 8 --out groove.mid

# Retrain the map after adding patterns or .mid loops to data/corpus (needs numpy)
python3 tools/mapbuilder/build_map.py
```

## Credits and licensing notes

Gristo is inspired by Mutable Instruments **Grids** and Making Sound Machines
**Stolperbeats**, and is not affiliated with either. It contains none of their
code, map data, patterns or timing tables. The drum map is trained from the
original corpus in `data/corpus`.

JUCE is licensed under AGPLv3 or a commercial licence. Pick the one that fits
how you distribute Gristo before releasing binaries.
