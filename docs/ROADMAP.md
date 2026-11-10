# Gristo 2 roadmap

Preset and project compatibility with 1.x is not required. Each phase ends with a
green CI build (Windows VST3 + Android APK) for testing before the next starts.

## Phase 1: Engine foundations (done)
- One "part" model: 6 drums, 3 arps, chords = 10 parts, each with mute, solo, level,
  colour, MIDI channel / note range and its own sound. Replaces the arp/synth index
  mapping and the appended modulation-target blocks with one clean numbering.
- Parameter access through cached pointers (no lookups on the audio thread).
- State as one snapshot object (needed for scenes, undo and dice). Preset format v2.
- Sample-accurate (per-step, smoothed) modulation of sound parameters.
- Chords: 16 slots.

## Phase 2: Outputs (done; controls arrive with the new UI)
- Separate audio outputs: kit, each synth, FX returns (plus the stereo mix).
- MIDI export core: render any part (or all) for N bars to a MIDI file, tested
  against the live output. The drag handle arrives with the new UI.

## Phase 3: New UI (done)
- One resizable landscape layout (about 1280 x 760) for desktop and tablet.
- Top bar: presets, play, tempo, undo/redo, master level.
- Left: part strip (activity, mute, solo, level; tap to select).
- Centre: the selected part complete (pattern / notes above, sound below).
- Bottom: timeline of all parts with playhead, lanes underneath.
- Modulate from the knob (right-click / long-press), drag-and-drop kept.
- Drag MIDI to the DAW from each part and from the whole pattern.
- Checkpoint: screenshots for approval after the shell and first part page.

## Phase 4: Performance features (done)
- Undo / redo.
- Scenes: 8 snapshots, switch on the bar, MIDI-note trigger, chain into a song.
- Dice and locks per section.
- 8 macro knobs with MIDI learn.

## Phase 5: Sound (done)
- User one-shot samples per drum voice.
- Kick ducking (sidechain amount per synth).
- Lanes as tempo-synced LFOs (shape, rate, phase) besides drawn curves.

## Phase 6: Chords (done)
- Voice-leading, strum, one-key chord play from MIDI in.

## Phase 7: Engine extras
- Per-voice pattern length (polymeter), ratchets, per-arp clock division,
  swing amount per part.
