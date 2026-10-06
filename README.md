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
* **Random:** Beads-style random amounts on Flam (per hit), Shuffle (per beat)
  and the map position (per bar).
* **Output:** MIDI notes with a selectable note per voice (GM drum map by
  default) on a chosen channel, plus a built-in preview kit, so it makes sound
  straight away.

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
