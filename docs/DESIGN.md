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

## Random modulation (Beads-style)

Three parameters have a small teal **Random** knob, like the attenurandomizers
on Mutable Instruments Beads. It sets how far that parameter is randomly moved
around its knob value. Each is sampled and held at a rate that suits it:

| Parameter | Random range at 100 % | Sampled |
| --- | --- | --- |
| Flam ms (Flam / Slop) | 0 … 2 × Flam ms | per hit. Kick and snare roll independently; in Slop the hats follow the snare's roll |
| Shuffle | ± 3 shuffle rows | per groove cell (one beat; two for Clave), so cells still start on time and order is kept |
| Map X / Y | ± 0.5 of the map on each axis | per bar. The map pad shows the range as a box and the current bar's position as a dot |
| Fill (per voice) | ± 0.35 | per bar, per voice: some bars busy, some sparse |
| Probability (per voice) | ± 0.5 | per bar, per voice |
| Rolls (Feel Random) | 2 … 5 hits per roll (default 3) | per roll; the roll crescendos into its last hit |

As with everything else, the values are hashed from (seed, position). Playback
and bounces match, and *Variation* re-rolls them.

## Modulation lanes (Massive-style performers)

Four lanes, each a drawn curve with one value (0–1) per 16th, looping over 1, 2
or 4 bars. Each lane has four routing slots: a target and a bipolar amount
(−100 … +100 %). At 100 % and a lane value of 1, the target moves by its full
knob range.

* **Drawing:** paint the curve with the mouse; fast strokes are interpolated so
  there are no gaps. Right-click (or *Shapes*) for ready-made curves: ramps,
  triangle, sine, square, saw per beat, offbeat 8ths, random steps.
* **Routing, Massive-style:** drag the lane handle onto a knob. Fill,
  Probability, Chaos, Shuffle, Flam ms and Humanize knobs, the Velocity ranges
  and the map (pick X or Y) all accept a lane. The first free slot gets the
  target, with ±50 % pointing into the knob's free travel. Knobs show a ring in
  the lane colour for the modulation range, and a white dot for the live value
  while playing.
* **Targets:** per voice Fill, Probability and Velocity; Map X, Map Y, Chaos,
  Shuffle, Flam ms, Humanize.
* **Engine:** `Engine::effectiveParams(step)` applies all lanes to the base
  parameters for that step before anything else is evaluated. Pattern, groove,
  humanizer and velocity all see the modulated values. Shuffle is sampled at
  the start of its groove cell, so timing stays monotonic. Lanes are tied to
  song position, like everything else, so renders are reproducible.
* **Example:** to make Hihat 1 thin out towards the end of every bar, leave its
  Probability at 100 %, draw a rising ramp and route it to *Hihat 1
  Probability* at −100 %.

Lane curves are saved with the project (not as automatable parameters). The
routing targets, amounts and lane lengths are automatable parameters.

## Velocity range

Each voice has a Min–Max MIDI velocity range (1–127). Gristo works out a
normalized velocity first: accents = 1, full-strength notes = 0.65, ghost notes
lower, then Humanize and Velocity modulation. That value is mapped into the
range, so every note of the voice, including roll hits, stays inside it.
Accents play at Max unless Humanize shaves a little off. Defaults: 1–127 for
kick and snare, 1–108 for Hihat 1, 1–115 for the rest.

## Per-voice Humanize and Dynamics

The Humanize and Dynamics knobs are masters. Each voice has its own Humanize
and Dynamics (0–200 %, default 100 %) that scale the master; the result is
capped at 100 %. Kick at 0 % Humanize stays exactly on the grid while the hats
breathe. Hats at 0 % Dynamics play every non-accented note at the same
velocity.

## Help box

Hovering any control shows a one-line explanation in the box at the top of the
window.

## MIDI out notes and channels

Each voice has its own output note and channel (default 10, set next to Mute),
so voices can go to separate instruments. The default notes are the GM drum
notes (36 / 38 / 42 / 46 / 39 / 45) three octaves down (0 / 2 / 6 / 10 / 3 / 9).
That keeps them below anything the arps play, because Ableton Live merges a
plugin's MIDI out to channel 1 and can only split the parts by note (Instrument
Rack key zones). A Pitch +36 in the drum chain restores GM. The Voices panel shows each note as `C1 · 36`, using middle C = C3
(Ableton / Cubase). Click a note to pick from a menu organised by octave, with
General MIDI drum names, or scroll to step one note at a time.

## Arps (drum-clocked arpeggiator / sequencer)

Two independent arps live on the tabs next to Modulation (`core/src/Arp.cpp`,
scheduling in `Engine::processArp`).

* **Clock:** an arp advances one step each time one of its trigger voices plays
  (any mix of the six). Simultaneous triggers count once, the loudest hit wins;
  each roll sub-hit advances it. The count restarts every *Reset* bars
  (1/2/4/8/16), so a kick-driven arp lines up with the bar. The trigger voices
  keep playing their drum notes.
* **Deterministic position:** the step is the number of trigger hits since the
  last reset bar. After a jump or loop the engine recounts from the start of the
  cycle, so the arp is in the same place every time you play a section.
* **Arp mode:** held notes (sorted, or as played) spread over 1–4 octaves, in
  Up / Down / Up-Down / Random / As Played order. Latch keeps the last chord
  after release; a new chord replaces it when you play with no keys held.
* **Generate mode (Sting-style):** a seeded pattern of 1–16 steps. Each step
  has a pitch (a random walk, mostly small moves with the odd leap), a rest
  value, an accent value and a slide value. Density, Accent and Slide are
  thresholds over those values, so turning a knob adds or removes steps without
  reshuffling the rest. *New* picks a new seed. *Mutate* changes some steps
  each reset cycle, and the changes accumulate. Pitches come from the held
  notes if any, else the chosen scale from Root over the octave range.
* **Notes:** monophonic. Gate is in 16ths. Slides overlap the next note
  slightly (legato, for synths with glide) and hold until it arrives. Accents play
  at the top of the velocity range; otherwise velocity follows the drum hit that clocked the step, mapped
  into the arp's velocity range.
* **Out Shift:** semitones added to the arp's MIDI out (Arp 1: 0, Arp 2: +36 by
  default), folded back by octaves at the ends of the MIDI range, so each arp
  gets its own note range in hosts that can't split by channel. The DAW undoes
  it with a Pitch device; the preview synth plays the unshifted pitch. The arp
  page shows the resulting range, the key zone and Pitch value to use in Live,
  and turns red when the range overlaps a drum note or the other arp.

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
* **Arps:** per-step pitch editing, a note-length pattern, and CC output for
  accents / slides.
* **MIDI in:** pattern snapshots (X/Y/Fill sets) switchable by note, like the
  Stolperbeats MIDI pattern select, and mutes.
* **ENV/LFO modulators:** tempo-synced shapes that move X/Y (the beat walks
  across the map), and CC outputs.
* **Per-voice step lengths** (e.g. 64-step kick/snare, 16-step hats) and a
  pattern "capture to editable grid".
* **Drag-and-drop MIDI export** from the editor, reusing `tools/render`.
* **A bigger corpus.** The map gets better with more (and your own) loops.
