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
* **Three drum-clocked arps:** each arp steps forward when the drum voices you
  pick (kick, snare, any mix) play, and restarts every 1/2/4/8/16 bars. *Arp*
  mode plays the chord you hold on MIDI in (Up / Down / Up-Down / Random / As
  Played, 1–4 octaves, Latch). *Generate* mode is a Sting-style sequencer:
  pattern length, density (rests), accents, slides and Mutate, over the held
  notes or a scale. Velocity follows the drum hit, mapped into each arp's range,
  and each arp has its own MIDI channel.
* **Chords:** a harmony map instead of a chord list. Columns are a chain of
  thirds around the tonic, so neighbours always share two notes (in C major:
  ... Dm F Am **C** Em G Bm ...); the flat side on the left is darker, the sharp
  side brighter. Rows add colour: triad, add9, 7th, 9th, 11th. Drag to set
  home, **Wander** lets the harmony walk around it by itself and come home
  every phrase, **Catch** loops a phrase you liked. The chords can play their
  own part (held, or stabbing on drum hits, with their own sound, MIDI channel
  and Shift), and any arp switched to **Chords** plays the current chord's notes.
* **Arp Probability:** each arp has a Probability knob (plus Random, and it's
  a lane target). Skipped steps still advance the arp. It's independent of the
  drums' Probability: the arp steps on the drum pattern as written.
* **Built-in sounds (Sounds tab):** twelve synthesized drum kits (Classic,
  808, Trap, Lo-fi, Minimal, Reggaeton, House 909, Techno, Breakbeat, Jungle,
  Amapiano with a log drum on P1, Dancehall) with Tune / Decay / Tone / Drive
  and a level per drum, and a synth per arp and for the chords with 58 sounds, among them:
  bass (Sub, Acid, Reese, FM, Upright), keys (E-piano, Suitcase Keys, Tape
  Keys, Felt Piano, Bell, Celesta, Vibraphone, Pluck, Organ, Jazz Organ, Poly
  Synth), pads (Warm, Glass, Dark, Breath, Slow Strings, Poly Strings), leads
  (Square, Sync, Soft) and Synth Brass, each with Cutoff, Resonance, Env,
  Attack, Release, Glide and Level. Pads, strings, keys and brass run
  through a stereo ensemble chorus, the analog-style patches drift a few
  cents per note, FM keys and mallets get brighter the harder you hit, and
  drums get darker on soft hits, with a little variation on every hit, a
  punchier kick and no clicks on fast re-hits. Acid, brass and the analog pads
  go through a saturating ladder filter; basses and leads behave like a
  monosynth; keys and mallets have a hammer / mallet attack, the suitcase
  e-piano a stereo tremolo, the organs a rotary-style swirl and the vibraphone
  its motor tremolo. Lanes glide the levels, filters and sends smoothly. The
  reverb is the one from Mutable Instruments Beads (see below), the delay has a bit
  of tape wow and warmth, and a gentle glue compressor sits before a clean
  output stage.
  A shared reverb and tempo-synced ping-pong delay with sends. Lanes can drive
  the sound knobs too: the kit's Tune, Decay, Tone, Drive and sends; each
  synth's Cutoff, Resonance, Env, Attack, Release, Level and sends; Reverb
  Size and Delay Feedback.
* **Presets:** a preset bar at the top. There are 14 factory presets (Trap
  Acid, Boom Bap Keys, Reggaeton Night, 808 House Pads, Minimal Bells, Lazy
  Organ, Electro Sync, 2-Step Glass, Pavilion Haze, Jazz Blues Walk, Jungle
  Reese, Amapiano Logs, Techno Brass, House Organ), plus your own as `.gristo` files in
  `Documents/Gristo/Presets`. Use Save / Save As, step through presets with
  < >, and use Import / Export to move them between computers (or from the
  tablet). A preset stores everything musical, including the sounds, but not
  your MIDI notes, channels, Out Shift or Sound on/level, so loading one never
  breaks your DAW routing. `*` after the name means you've changed it.
* **UI size:** 70–125 %, and it picks a size that fits your screen the first time.
* **Output:** MIDI notes with a selectable note and channel per voice (GM drum
  notes three octaves down on channel 10 by default, so the drums sit below the
  arps), the arps on channels 1, 2 and 4 (3 is the chords) with an Out Shift each, plus the built-in
  sounds, so it makes sound straight away.

### Styles on the chord chart, back to the 1, a clearer map and Circles

* **Styles on the chart:** pick a style (Jazz, Neo-soul, Lo-fi, Trap, Jungle, House,
  Melancholic, Ethereal: slow major sevenths and ninths after Harold Budd and Eno)
  and click one of its progressions: it goes into the progression, in your key (and in
  its own major or minor), and plays through once while stopped. The map then rings
  the chords that style tends to go to next, from the chord you last heard or placed.
* **Back to the 1:** the progression's Reset. *Hold to the bar* (the default) lets
  the last chord ring on to the next bar line, so a 9-beat progression comes back on
  the 1 (shown as "hold" in the strip); *Restart every 1 / 2 / 4 / 8 bars* starts it
  again from its first chord, like a reset into a sequencer; *Loop as it is* lets an
  odd length drift.
* **Drag chords** in the progression strip to move them.
* **The map, clearer:** the grooves it is made of are named on it (Boom bap, House,
  Jungle, Trap...), the four directions sit around it, and hovering shows a card with
  the groove there and its first bar, before you move. Switching between Map and
  Circles no longer loses the map's picture.
* **Circles, reworked:** the rings set and clear steps; the page beside them lists
  every ring unrolled (click one to edit it), with **Steps** as chips (5 6 8 12 16 24
  32, any other length from the box beside them), **Rate** as chips (/1 .. /8), a
  **step lane** to draw velocity, chance, nudge or repeats as bars, **Euclid hits**
  (+ / -), Shift, Reverse, Random, Clear and From map. The drums' Length menus list
  the common lengths first too.

### Circles, a chord timeline, a tidier chords page

* **Circles** (Pattern mode, at the top of the drums' map panel and on the Play page),
  after the Patterning app: the drums as rings of steps, the kick outermost. Each ring
  is as long as its drum's Length (so rings of 16, 12 and 5 steps drift against each
  other) and runs at its clock divider's rate. Click a step to set or clear it; drag a
  set step up or down to change, by the edit mode: its **velocity**, its **chance**,
  its **nudge** (up to half a step early or late) or its **repeats** (1 to 4 hits across
  the step). **Edit circles** opens the rings big, with each ring's Steps and Rate,
  **Euclid** (hits spread evenly) and **Rotate**, Shift left / right, Reverse, Random,
  Clear, and **From map** / **All from map** to start from the groove the map plays
  (switching to Circles the first time does this for you). Probability, mutes, Swing
  and Humanize still apply. The rings are saved with songs, presets, scenes and undo.
* **Chord progressions in quarter notes:** up to 16 slots, each lasting its own
  number of quarter notes (1/4 to 4 bars). The strip under the map is a little
  timeline: drag a slot's right edge to make it longer or shorter, right-click it for
  its length, Duplicate or Remove. A chord you add lasts the chord length.
* **The chords page, regrouped:** the top row shows only what the mode uses (Wander,
  chord length, phrase, Catch for Wander; the chord length for Progression); the
  map and the progression on the left; **Rhythm** (Held's Push and Re-strike, or
  Stabs' drums, Every, Stab % and gate, as Play is set) and **Voicing** on the right;
  the MIDI out along the bottom.
* **No lost notes when you click chords quickly:** a chord started while the last one
  still rang could steal its own just-started notes. The synths now steal released
  notes first, and have 16 voices.

### A new synth engine

Every synth sound is rebuilt on a new engine. The voices run at twice the host's
sample rate and are decimated once per synth, so filter drive, sync, FM and phase
distortion stay clean instead of folding back as grit.

* **Analog:** two oscillators that morph sine, triangle, saw and square (the
  variable-shape oscillator of Mutable Instruments Plaits, band-limited, with a
  band-limited hard sync), up to three stacked and detuned, a sub, noise and PWM,
  into one of three filter models: a transistor **Ladder** (fat, squelchy, rings in
  tune at full resonance), an **OTA** 4-pole (the rounder filter of the classic
  polysynths) and a 12 dB **SEM** state-variable filter (low-, band-, high-pass or
  notch; open and vocal). All three saturate inside, like the circuits. The
  envelopes charge and fall like analog ones. Each voice has its own small
  tolerances (cutoff, tuning, envelope times), and the oscillators drift slowly.
* **FM**, after the Digitone's FM Tone engine: four operators (C, A, B1, B2), eight
  algorithms, each with two outputs (X and Y) and a Mix between them, frequency
  ratios in the Digitone's steps, Harmonics, Detune, Feedback on the top operator,
  and an attack / decay / end envelope each for A and the B pair. Harder notes
  modulate harder, high notes more gently; Cutoff is the brightness, Resonance
  adds feedback, Env scales the modulators' decay.
* **TRIO, LOFI, PHASE** (from Felucca), the organs, the formant voices, the plucked
  strings and the singing bowl now run oversampled too; TRIO goes through the SEM
  filter.
* **Every sound re-voiced** on these engines: the acid bass on the ladder, the pads
  and brass on the OTA, the e-pianos as an FM bark with a 14:1 tine, and so on.
  Every sound is measured to the same loudness.
* **Six new sounds:** PWM Pad, Mono Bass (three oscillators through the ladder), SEM
  Pad (band-pass, singing), FM Pluck, FM Pad, FM Choir.
* **Tone:** eight more knobs per synth, under the others on the Sound panel. What
  they do follows the sound's engine: on the analog sounds Wave, Osc 2, Detune,
  Sub, Noise, PWM, Drive and Analog (drift); on FM Ratio A, Ratio B, Harm, Detune,
  Feedback, Mix, Level A and Level B; the organs get drawbars, TRIO its oscillator
  set, intervals and pulse width, LOFI its chip, wave, crush, sweep and arpeggio,
  PHASE its waves, depth, fold and ring. At 50% a sound is as designed
  (double-click a knob to go back). **Filter** swaps the sound's filter for any of
  the models, and **FM algorithm** picks one of the eight. All of them are
  automatable from the host.

### The Beads reverb

* Beads' **Reverb** knob is now the module's own reverb on the grains (ported
  from Mutable Instruments' code): more is both wetter and longer, up to about
  12 seconds, modulated and wide. Darker on the tape media.
* The same reverb is the main reverb (the plate is gone). Size sets the
  length; Beads' **Quality** sets its colour, as on the module.
* Beads' **Pitch** knob steps through musical intervals: unison, fourths,
  fifths, octaves, and octave + fourth / fifth, up to two octaves either way.

### A new drum engine

Every drum is now one of 15 engines, and all kits are rebuilt on them:

* **Kicks:** Analog (a pinged resonator with attack FM and self-FM, after the
  808), Synth (a dirty sine with a click and FM, after the 909), Acoustic.
* **Snares:** Analog (five resonant modes plus snappy noise), Synth (two
  coupled oscillators and filtered noise), Acoustic (heads and wires), Crack
  (a noise burst into a short string, Karplus-Strong, as in the Perkons), Clap.
* **Metal:** 808 Hat (six square oscillators), 909 Hat (ring-modulated pairs),
  Cymbal (four feedback combs).
* **Percussion:** Tom, Cowbell, Shaker, Bell.

On a drum's **Sound** tab, **Engine** swaps the kit's drum for any of them, and
each drum has its own **Filter** (low-, band- or high-pass) and **Cutoff**, as on
the Perkons. Tune, Decay, Tone, Punch and Noise drive the engine's controls;
soft hits are darker on every engine.

The Analog and Synth kicks and snares and the hats are ported from Mutable
Instruments Plaits (MIT, Copyright 2016 Emilie Gillet). 7DeadlySnares (closed
source) and the Perkons inspired the choice of engines, not code.

### Also new

* A new instance starts in a random key on the Chords page; an opened song or a
  preset keeps its own.
* **Octave** on each synth's sound panel: moves the built-in sound up or down
  (-3 .. +3 octaves); the MIDI it sends is unchanged.
* **Beads** has its own plugin output (with Separate outputs on; the eighth bus).

### Building a chord progression

* **Played by: Progression.** The chords you keep, in order, in eight slots (the strip
  under the map), one per Chord length, looping. The map shows each chord's slot numbers.
* **Listen first:** click and hold chords on the map to hear them, as long as you hold
  (at least about a second), as many as you like. Nothing is kept until you say so:
  the last one you heard is the candidate (ringed in amber).
* **Keep it:** double-click the chord, drag it onto a slot, or press **Add** (on the
  chart). Click a slot to select it (you hear it): Add or a double-click then replaces
  it. Right-click a slot to remove it.
* **Chart:** the Chart button (Play and Chords pages) opens the chord chart: the map
  and the progression big, with Key, mode, Played by, Chord length, Held / Stabs and
  Voicing beside them. Opening it sets Played by to Progression.
* **Use walk:** in Wander, the strip shows the walk you are hearing. When you like it
  (Catch it first to keep it looping), Use walk turns it into a progression you can
  change chord by chord.
* In Wander, a click on the map while stopped also lets you hear the chord.
* Push, Re-strike, Stabs, Chance, voicings and arps following the chords all work on
  a progression too; it is saved with the song, presets and scenes. (Trying chords
  plays the built-in Chords sound, not the MIDI out.)

### A new look

* A calmer, quieter design: Inter as the typeface, a warm near-black palette with
  panels and controls a step above it, hairline edges, thin knob arcs with soft caps,
  and active buttons tinted in their own colour instead of solid blocks.
* The rhythm map is a topographic map now: a smoothed density field from deep slate
  (sparse) to warm umber (dense), with contour lines.
* Small fixes: values show on every big knob of the FX page, the mixer's sends are
  labelled once per row, routing amounts read clearly.

### Clock dividers, stab and held rhythm, more Body

* **Clock divider per drum** (Pattern tab, next to each Length: /1 .. /8): the drum
  plays only on every Nth 16th and walks through its pattern, Length and hand edits
  that many times slower. /2 is a half-time hat, /3 a hat in threes against the
  fours.
* **Chord stabs:** **Every** (every hit, or every 2nd, 3rd, 4th, 6th, 8th hit of the
  stab drums, counted from each bar) and **Stab %** (the chance of each stab).
* **Held chords:** **Push**: the chords change on the 1, or early (a 16th, an 8th
  or a beat ahead, anticipating the bar) or late (laid back: a 16th, an 8th or a beat). **Re-strike**: the
  chord plays again on a pulse (every 8th, dotted 8th, beat, dotted beat, half bar or
  bar, from its own pushed start), each strike rolling Chance anew.
* **Body on the synths:** a material (Wood, the default: an instrument's body;
  Box; Shell; Metal; Glass; Tube), **B.Tune** and **B.Decay**. Drums get Wood and
  Glass too.

### Mixer, a delay of its own, pre-fader Beads sends

* **Mixer** (the button next to Rings): a strip per channel, Kit, Arp 1-3,
  Chords, Rings and Beads, then the Delay and Reverb returns. Each has its level,
  Mute and Solo (the returns: Mute; they stay on when you solo, so a soloed part
  keeps its echoes and reverb) and its sends to the Reverb, the Delay and Beads.
  Rings, Beads, Delay and Reverb now each have their own level and mute.
* **Pre** on a Beads send: Beads hears the channel before its fader and mute. Mute
  an arp and keep only its grains. A muted arp or chord part keeps playing
  inside (for Rings and pre-fader sends); its sound is silent and it sends no MIDI.
* **The delay is its own effect** (FX / Beads page): **Tape** (a worn tape echo:
  wow, flutter, darker and softly saturated repeats, a little hiss), **BBD** (an
  analog bucket-brigade chip: longer times run its clock slower, so long echoes
  get darker and grainier; a slow modulation; it can be pushed close to runaway)
  or **Clean**; **Age** (how worn), **Ping-pong**, and on the Mixer its return
  level, mute, and sends **into the Reverb** and **into Beads**.
* The order is fixed so nothing feeds back on itself: the parts and Rings feed
  the Delay, the Delay feeds Beads, and all of them feed the Reverb.
* **Separate outputs:** the old FX output is now **Reverb**, and the Delay has its
  own output (the tenth bus).
* The synths' level is now applied after their inserts (ensemble, grit), as on a
  mixing desk.

### Rings, chord Chance and voicings, one reverb, a new Body, gentler Dirt

* **Rings** (the **Rings** button under the parts): Mutable Instruments' resonator,
  the module's own code. Every note of the part it is **Played by** (an arp or
  the chords) strums it at that note's pitch, so it always plays in the song's
  key; a chord rolls over a few milliseconds. Model (Modal, Sympathetic strings,
  String, FM voice, Sympathetic (chords) on the song's key, String + reverb),
  Polyphony 1 / 2 / 4, Octave, Structure, Brightness, Damping, Position, and
  **Exciter**: its own pluck, the drums (tuned percussion), or both. Level, sends
  to the reverb and the delay, and **Into Beads** (grains of the resonance). With
  Separate outputs on, it has its own plugin output (the ninth bus). Like the
  module it runs at 48 kHz, resampled to your project's rate.
* **Chords: Chance** gives each note of a chord its own chance to sound, rolled
  every time a chord starts (if every note misses, the lowest plays). Five more
  **voicings**: Drop 2, Open (wide, root underneath), Rootless (neo-soul keys),
  Shell (root, third, seventh and the top colour) and Upper + bass (root and
  fifth under the colour notes).
* **One reverb:** the plate is gone; the reverb is Beads'. As on the module,
  Beads' **Quality** colours it (and Beads' own reverb): Digital is the
  cleanest, the other media darker and longer; on Clean tape it runs as the
  module's 32 kHz does, longer still; its 16-bit memory clips when driven hard.
* **Body, rebuilt:** eight resonant modes instead of a comb, with a **Body type**
  per drum: **Shell** (a drum's membrane and wooden shell), **Box** (a cajon-like
  box: a low air thump under dull panels), **Metal** (a struck plate) and **Tube**
  (a tuned pipe, close to the old Body). Higher modes fade first; every hit moves
  the modes a little (Alive); and the bodies ring a little with the rest of the
  kit, like a snare buzzing to the kick.
* **Dirt, gentler:** Tape and Drive blend in with the amount instead of being
  fully on from a third of the knob, Tape keeps the lows clean, and both
  saturators are anti-aliased (far less harsh fizz on hats and bright synths).
  Crunch comes in more gradually.
* **MIDI out vs. sound:** each part's MIDI out row now reads *MIDI ch* and
  *Shift* (moves only the MIDI you send), and the synth's *Sound ... oct* only
  moves the built-in sound. The chords' Octave moves both.
* Scenes and lane / macro routings are now also saved by parameter name, so an
  update that adds parameters keeps them.

### Drawing steps by hand

Click a drum step in the timeline (at the bottom of the Play page) to change it: a
hit becomes a rest (shown with a cross) and an empty step becomes a hit (shown with
a ring). Click again to flip it back, drag along a row to draw several steps, and
right-click a step to give it back to the generator. Edits sit in the drum's own
loop (its Length), are saved with the song, presets and scenes, and win over the
map, Fill, Busy, Wild, Linear and the trig condition. A hit you draw still goes
through the drum's Probability and Mute; a rest you draw is always a rest.

### Felucca engines and Beads

* **TRIO** (three oscillators, ring modulation, hard sync, combined waves, a
  multimode filter): Trio Bass, Trio Sync, Ring Bell, Chip Choir, Trio Arp.
* **LOFI** (a chip voice: pulse, triangle, saw, noise and a 16-table wave RAM,
  bit depth, sample hold, pitch sweep, arpeggio): Chip Lead, Chip Bass, Chip
  Arp, Wave Vox.
* **FM** sounds: FM Rhodes, DX Bass, FM Bells, FM Marimba, FM Brass, FM Glass (now
  on the four-operator engine, see "A new synth engine").
* **Beads** (FX / Beads page): a granular processor that records the synths
  (an Input amount per synth) and plays grains of the last four seconds.
  The drums are its gates: pick which drums start grains, and Density divides
  their hits (left of noon) or lets each through by chance (right of noon).
  With no drum picked it runs on Beads' own clock. Time, Size (backwards below
  a third), Pitch and Shape each have a Random amount; Musical snaps random
  pitch to octaves, fifths and fourths. Feedback, Freeze, four media (digital,
  vintage digital, clean tape, dirty tape), Level and Reverb.
* Presets: Bead Rhodes, Chip Ritual.

Credits: TRIO and LOFI are ported from Felucca-Engines (MIT, Copyright (C) 2026
Leo Kuroshita, Hügelton Instruments); Beads is ported from Mutable Instruments'
eurorack code (MIT, Copyright 2017 Emilie Gillet).

### Modelled acoustic drums

* **Acoustic kit:** kick, snare and toms built as struck drum heads: a handful
  of modes (the upper ones quieter and shorter), a pitch that sags as the hit
  dies away, a resonant head tuned against the batter head, a shell ring, and
  snare wires that buzz for as long as the bottom head moves. Soft hits are
  duller, not just quieter.
* The Breakbeat, Jungle and Dusty Soul kits get the same heads and wires under
  their kicks and snares.
* On a drum's Sound tab, Punch sets how far the pitch sags, Noise the wires,
  and Body rings at the head's pitch.

* **Modelled cymbals** on the Acoustic kit's hats: strike noise into four long
  feedback combs at low, unrelated pitches, behind filters that open with the
  hit and close as it fades. The closed hat chokes the open one.
* **Phase distortion synth** (Casio CZ-style): five sounds, Phase Bass, Reso
  Lead, Phase Pad, Phase Keys and Fold Drone. Their Cutoff knob and filter
  envelope bend the wave instead of only filtering it.

The drum and cymbal models follow Punk Labs' **OneTrick URCHIN** (GPL-3.0-or-later,
Copyright (c) 2023 Punk Labs LLC); the cymbal's comb pitches and filter ranges are
taken from it. The phase-distortion waves and fold are ported from the PHASE engine
of **Felucca-Engines** (MIT, Copyright (C) 2026 Leo Kuroshita, Hügelton
Instruments). Because of the URCHIN-derived parts, Gristo as a whole is under
GPL-3.0-or-later.

### Drum sound pages, Body, any-parameter modulation

* **A sound page per drum:** in Edit, pick a drum on the left and open the
  **Sound** tab: Tune, Decay, Tone, Punch, Noise, Drive, Pan, and **Body**, a
  resonant body the drum excites (with Body Tune, Body Decay and a Body type)
  for the ring and air of a real drum.
* **Body on the synths:** a small resonant wooden body around any sound.
* **Plays** (trig condition, per drum): every bar, 1st or 2nd of 2 bars, 1st
  or 4th of 4 bars.
* **Lanes and macros reach every parameter:** right-click any knob, or pick it
  in a routing slot. (MIDI notes, channels and Out Shift are left out.) Knobs
  the engine has its own target for still show rings; the others don't.
* **Printable controller sheets:** docs/Gristo-controller-overlays.pdf.

### Controllers, and sounds for hybrid styles

* **MIDI controllers:** right-click any control for **MIDI learn**, or load a
  layout from the **MIDI** button: two Launch Control XLs (one for the drums,
  one for arps, harmony and sound) and the APC40 (its grid plays the harmony
  map). See [docs/CONTROLLERS.md](docs/CONTROLLERS.md).
* **New sounds:** 808 Bass (drops onto its note, slides when notes overlap),
  Monk Choir and Vox Pad (formant voices whose vowel slowly moves), Singing
  Bowl, Nylon Guitar and Koto (plucked strings). Kits **Temple** (frame drum,
  wood fish, rattle, bell, bowl, gong) and **Dusty Soul**.
* **Shimmer** (Master / FX): the reverb feeds itself an octave up.
* **Arp scales:** Insen, Hirajoshi, Phrygian dominant.
* **Presets** that cross styles: Jungle Monastery, Liquid Soul, Trap Sutra,
  Dust Hymn, Bowl Drift, Indie Soul Tape, Slow Jam 808, Chant Break.

### Gristo 3: play it, don't program it

The plugin opens on the **Play** screen:

* **Two pads.** The rhythm map on the left, the harmony map on the right.
  Drag either and it plays.
* **Five big knobs**, each moving a family of settings that belong together:
  **Busy** (how much the drums play, with ratchets at the top), **Loose**
  (shuffle, humanize and flam), **Wild** (how much the pattern changes by
  itself), **Dirt** (grit, see below) and **Space** (reverb and delay for
  everything). They act on top of the detailed settings, which stay as the
  balance between the parts.
* **Mutate** nudges everything that isn't set to Keep, by the amount next to
  the knobs: a variation at 20 %, all new at 100 %. Undo goes back.
* **Scene morph:** drag sideways on a stored scene to blend towards it.
* **Edit** (top right) shows the detailed page of the part selected on the
  left, with the lanes, macros and routing. Nothing was removed; it's one
  click away instead of on the main screen.

**Harmony:** Wander (how far it walks), chord length, phrase length, Catch,
and *Played by Keys*: the white keys from C1 play the seven chords around
home, the next octave one row richer.

**Dirt** is one amount with three kinds of grit: **Tape** (saturation that
leaves the lows below ~100 Hz clean, wow and flutter, soft highs, hiss that
comes and goes with the music), **Crunch**
(an old sampler's low sample rate and coarse converters) and **Drive**
(overdriven channels into a compressor that pumps). **Alive** (Master / FX
page) is how much nothing repeats exactly: oscillators and filters drift,
drum hits differ, parts move a little in the stereo field.

Presets and projects from before Gristo 3 don't load in it.

### Gristo 2

The window is now one landscape layout (1280 wide, the same on desktop and
tablet): a **part strip** on the left (six drums, three arps, chords; select,
mute, solo and level for each, plus Master / FX), the selected part's **page**
in the middle, and a **timeline** at the bottom that shows what every part
plays and holds the modulation lanes.

* **Scenes:** eight snapshots of the whole patch. Click an empty one to store, a
  stored one to recall (on the bar line while playing), right-click to store
  again or clear. **Chain** steps through them every 1-16 bars. MIDI channel 16, C1-G1, recalls them from a keyboard or clip.
* **Macros:** eight knobs that each move up to four parameters. Right-click
  (long-press on a tablet) any knob: *Control with macro* or *Modulate with
  lane*. Right-click a macro for MIDI learn.
* **Dice / Keep:** randomise a section (pattern, groove, kit, an arp, the
  chords, a sound) or everything; Keep locks a section. **Undo / Redo** cover
  every edit.
* **Lanes** can be drawn or run as tempo-synced LFOs (sine, triangle, ramps,
  square, random; 1/8 note to 8 bars).
* **Drums:** the grid has two tabs. *Pattern*: Fill, Probability, **Ratchet**
  (a hit becomes two or three quick ones) and **Length** (a drum loops after
  1-32 steps: polymeter). *Play*: Humanize, Dynamics, **Swing** (how much of
  the shuffle this drum follows), velocity, note, channel and **Sample** (your
  own WAV / AIFF / FLAC one-shot instead of the kit's sound; kept in
  `Documents/Gristo/Samples`).
* **Arps:** *Step on* every hit or every 2nd, 3rd, 4th, 6th, 8th trigger hit.
* **Chords:** **Voicing** (close; Smooth: every chord in one octave so
  notes move as little as possible; Smooth + bass; Drop 2; Open; Rootless;
  Shell; Upper + bass), **Strum** (ms between notes; up, down or alternating)
  and **Chance** (per note).
* **Sound:** each synth has **Duck** (pumps with the kick). **Separate
  outputs** (Master / FX page) gives Kit, Arp 1-3, Chords, Reverb, Beads, Rings
  and Delay their own plugin outputs.
* **MIDI to the DAW:** drag the handles in the timeline bar onto a track to
  drop 1-8 bars of the selected part or of everything as a MIDI clip.

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
3. Hold the tablet sideways. The top bar has the presets, the pages (**Beat**:
   map, pattern and voices; **Groove**: groove, output and the modulation
   lanes; **Arps**; **Chords**; **Sounds**), **Keys** (a keyboard strip, off by
   default: use the Chords page for chords on touch) and **Settings** (audio
   and MIDI devices; **Done** closes it). A USB MIDI keyboard is picked up
   automatically.
4. Modulation lanes: the **L1-L4** chips in the top bar reach every page. Drag
   one onto a knob, or tap the chip and then tap the knob (also works on the
   PC). Draw the curves on the Groove page.
5. Tap any control to see its help text at the top.

**Presets between tablet and PC:** on the tablet, presets are saved in
`Documents/Gristo/Presets`. You can copy them to the PC over USB, or use the
**Share** button next to Save (Quick Share, Gmail, Drive, ...) or the menu's
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
    each arp has an Out **Shift** (Arp 1: 0 from C1, Arp 2: +36 from C3, Arp 3:
    0 from C4, which lands between them; note names as Gristo shows them, with
    middle C = C3). The arp page shows the exact range,
    the setting to use and any overlap.
    1. Hold or clip the arps' chord on the Gristo track itself (its MIDI in).
    2. Make one MIDI track with *MIDI From* = the Gristo track / "Gristo", and
       put an Instrument Rack on it. Show the Key zone editor and add one chain
       per part:
       * **Drums:** key zone C-2 to B-2, a Pitch device at +36, then your Drum
         Rack (standard GM pads).
       * **Arp 1:** key zone as shown on the Arp 1 page, then the synth.
       * **Arp 2:** key zone as shown on the Arp 2 page, a Pitch device at -36
         (the opposite of its Shift), then the synth.
       * **Arp 3:** key zone as shown on the Arp 3 page, then the synth.

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

The analog synth oscillator is ported from Mutable Instruments **Plaits**'
variable-shape oscillator (MIT, Copyright 2016 Emilie Gillet). The FM engine follows
the layout of Elektron's Digitone FM Tone (operators, algorithms, ratios); it
contains none of Elektron's code.

**Rings** is Mutable Instruments' own code (rings/dsp and its tables, MIT,
Copyright 2015 Emilie Gillet) with the parts of **stmlib** it needs (MIT,
Copyright 2012 Emilie Gillet), in `third_party/eurorack`, unchanged but for
a few portability lines (see the README there).

The typeface is **Inter** (Copyright the Inter Project Authors, SIL Open Font
License 1.1; `plugin/Resources/Inter-LICENSE.txt`), a Latin subset embedded in the
plugin.

JUCE is licensed under AGPLv3 or a commercial licence. Pick the one that fits
how you distribute Gristo before releasing binaries.
