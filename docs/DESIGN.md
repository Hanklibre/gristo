# Gristo design

Gristo combines two ideas:

* **What plays** comes from a *topographic drum map*, in the spirit of Mutable
  Instruments Grids: a 2D field of patterns you morph through with X/Y, sculpted
  per voice with Fill and perturbed with Chaos.
* **When it plays** comes from a *groove engine*, in the spirit of Making Sound
  Machines' Stolperbeats: Timing models × Shuffle depth × Feelings, plus a
  Linear mode.

Gristo adds a **groove-aware humanizer** on top. It contains no code or data
from either product. The map is trained from Gristo's own corpus
(`data/corpus`), and the groove tables are original.

```
DrumMap (X, Y) ─► PatternGenerator (Chaos, Fill, Mutes, Probability, Linear)
               ─► Groove (Timing × Shuffle, Feeling)
               ─► Humanizer (structural weight, drift, groove-follow, gap cap)
               ─► Engine scheduler (sample-accurate, lookahead for early notes)
               ─► MIDI out  +  PreviewKit audio
```

## Code layout

| Path | What |
| --- | --- |
| `core/` | Host-independent C++17 library. No JUCE. All randomness is hashed from (seed, stream, position), so output is deterministic. |
| `tests/` | Dependency-free unit tests for the core. |
| `plugin/` | Thin JUCE wrapper: parameters, processor, preview kit, editor. |
| `tools/mapbuilder/` | Python: corpus → self-organizing map → `core/src/MapData.cpp`. |
| `tools/render/` | `gristo_render`: renders the engine to a `.mid` file. |
| `data/corpus/` | Original training patterns (text format, MIDI also accepted). |

## The 6-voice map

* Voices: Kick, Snare, Hihat 1 (closed), Hihat 2 (open), Perc 1 (clap / rim /
  shaker), Perc 2 (toms / bells). Patterns are 32 steps (2 bars of 16ths).
* A node stores a *level* (0–255) per voice and step. A step plays when its level
  is greater than `1 - fill`. Levels ≥ 0.75 are accents.
* **Level model** (`build_map.py`): a written hit gets
  `0.40 + 0.40·velocity + 0.20·metric`, so accents rank above normal hits,
  normal hits above ghost notes, and stronger metric positions win ties. Rests
  get a small *fill potential* scaled per voice (hats high, kick low). Turning
  Fill up therefore reveals the written groove first, then ghost notes, then
  plausible extra notes.
* **Map training:**
  1. A batch self-organizing map, PCA-initialised and deterministic, places
     similar grooves next to each other.
  2. A balanced assignment step gives each node at most ⌈patterns/nodes⌉
     patterns, so outliers don't pile into the corners.
  3. Nodes are sharpened toward their own patterns (80 %) to keep the source
     character.
  4. Finally the map is oriented: X runs straight → syncopated, Y runs
     four-on-the-floor → broken/halftime.
* `docs/map.md` shows which pattern sits where.
* To grow the map, add patterns or `.mid` loops to `data/corpus` and run
  `python3 tools/mapbuilder/build_map.py`. Loops longer than 2 bars are folded,
  and each step's level becomes the average over the windows. This is how
  "sometimes" notes become fills.

## Probability

Each voice has a Probability (0–100 %). A hit that passes Fill plays with that
chance. The roll is hashed from (seed, voice, absolute step), so:

* every bar is different, but a re-render or bounce is identical;
* the *Variation* control re-rolls it, along with Chaos and the humanizer.

Probability is applied **before** Linear mode. A dropped snare frees its slot
for the kick or hats, the way a drummer who skips a note plays something else
there. Probability 0 % in Linear mode behaves exactly like Mute. The pattern
view draws dropped hits as hollow outlines for the cycle that is playing.

Fill and Probability do different jobs. Fill decides *which* notes belong to
the groove, in order of importance. Probability decides *how often* those notes
actually play.

## Groove

Each timing model displaces the 16th grid inside a *cell* (one beat; two beats
for Clave). There are 7 shuffle rows per model. The Shuffle parameter is
continuous, and fractional values interpolate between rows. The hardware this
is inspired by can't do that.

| Timing | Model |
| --- | --- |
| Trip | Each beat's four 16ths snap onto n-tuplet slots (7, 9, 5, 6, 8, 9 per beat), ordered by how far they pull from straight. |
| Shake | MPC-style 16th swing (50 → 66 %), with the second 8th lagging slightly at deep settings. |
| Push | Positions inside a beat are warped by `p^γ`, so notes lean early and leave room before each downbeat. |
| Clave | 3:3:2 anchors (steps 0, 3, 6 of every 8) stay fixed, and the steps between them warp toward the next anchor. |

Feelings: **Tight** (all together), **Rolls** (Hihat 2 becomes a 3-hit roll
that fills the gap to the next groove slot), **Flam** (kick and snare delayed by
*Flam ms*), **Slop** (Flam, with the hats following at 75 %).

Invariants, all covered by tests: shuffle 0 is straight time, downbeats never
move, and positions are strictly monotonic for every timing and shuffle value.

## Humanizer

The goal is slight, tasteful randomness that respects the groove instead of
smearing it. For each hit:

1. **Structural weight** `w` (0 = ornament, 1 = anchor) comes from metric
   position (backbeats count as anchors for the snare) and accent/level.
2. **Timing** = drift + jitter, scaled by *Humanize*:
   * *Ensemble drift* (one lattice point per bar) and *limb drift* (hands vs.
     feet, per 2 beats). Both are smooth value noise, so neighbouring notes move
     together the way a player does.
   * *Per-hit jitter*, triangular-distributed.
   * Anchors take less of both. Other DAW tracks usually sit on the grid, so the
     backbone stays the time reference.
3. **Groove follow**: weak notes move up to 25 % further in the direction the
   groove already displaced them. A swung ghost note swings a little more.
4. **Gap cap**: the total offset is clamped to 35 % of the distance to the
   neighbouring groove slots. Dense Trip clusters never swap order or turn into
   flams.
5. **Velocity**: per-hit variation (more on ghosts), slow dynamic drift, and a
   "hand" hierarchy for hats and percussion, where on-beat strokes are slightly
   stronger than off-beat ones.
6. **Lock** keys all randomness to the position inside the 2-bar pattern, so
   the humanization repeats every cycle. Unlocked, every bar is different but
   still reproducible on re-render.

At Humanize = 1, hats deviate by a few ms on average and never more than about
20 ms (asserted in tests).

## Scheduler

* Everything is a pure function of (parameters, absolute 16th index), where
  ppq 0 is step 0. The scheduler keeps a per-voice "next step" cursor and
  emits a step once its final onset is before the end of the current block.
  Notes can be up to 0.5 beat early, so this works as lookahead without adding
  latency.
* If an onset is already in the past (a rushed note right after a jump or
  loop), it is emitted at sample 0 rather than lost.
* A jump (|ppq − expected| > 0.02 beat) resyncs the cursors and releases any
  sounding notes. Stopping the transport releases sounding notes.
* A new note-on cuts the previous note of the same voice, so voices never
  overlap.
* Output is block-size independent to ±1 sample (tested with 32 to 4096-sample
  blocks).

## Roadmap

* **Host coverage:** an AU MIDI-FX target for Logic (`IS_MIDI_EFFECT`) and CLAP
  via `clap-juce-extensions`.
* **MIDI in:** pattern snapshots (X/Y/Fill sets) switchable by note, like the
  Stolperbeats MIDI pattern select, and mutes.
* **ENV/LFO modulators:** tempo-synced shapes that move X/Y (the beat walks
  across the map), and CC outputs.
* **Per-voice step lengths** (e.g. 64-step kick/snare, 16-step hats) and a
  pattern "capture to editable grid".
* **Drag-and-drop MIDI export** from the editor, reusing `tools/render`.
* **A bigger corpus.** The map gets better with more (and your own) loops.
