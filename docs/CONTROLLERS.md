# MIDI controllers

Gristo reads controllers directly from its MIDI input. There are two ways to use one:

* **MIDI learn.** Right-click (long-press on a tablet) any knob, switch, scene,
  Mutate, Catch, Undo or Redo, choose *MIDI learn*, then move a knob or press a
  button on the controller. *Forget MIDI* removes it. This works with anything
  that sends CCs or notes: Faderfox DJ2 / DX2, MS-20iC, a keyboard's knobs.
* **Layouts.** The **MIDI** button in the top bar loads a ready-made layout for
  the Launch Control XL (two of them) and the APC40. A layout is a set of
  ordinary bindings: you can re-learn or forget any single control afterwards.

Bindings are saved with the project, not with presets. A bound note or CC is
used for its control only; it does not reach the arps or the chord keys.

> The numbers below are the controllers' documented defaults as far as I know
> them. They have **not** been checked against the hardware. If a control does
> the wrong thing, re-learn it (right-click the control in Gristo).

## Printable sheets

[Gristo-controller-overlays.pdf](Gristo-controller-overlays.pdf) has, for each
layout, a labelled diagram and cut-out label strips (one per row of controls).
The strips' column spacing is a guess (27 mm for the Launch Control XL, 30 mm for
the APC40): measure yours and regenerate with
`python3 tools/make_overlays.py --lcxl-pitch <mm> --apc-pitch <mm>`. Print at 100 %.

## Getting the controllers to Gristo in Ableton Live

Live sends everything on a track to the plugin on channel 1, so two controllers
with the same CC numbers would collide. Give each controller its own MIDI track:

1. Create a MIDI track. *MIDI From*: the controller, *Monitor*: **In**.
2. *MIDI To*: the track Gristo is on. In the chooser below it, pick the channel
   the layout expects (for example **15-Gristo**).
3. In Gristo: **MIDI** > the layout > that channel.

If your Live does not offer numbered channels for Gristo there, route one
controller straight into Gristo's own track and load its layout "on channel 1".

Used this way the controllers are plain MIDI devices. Leave *Remote* off for
these ports in Live's MIDI preferences, and on the Launch Control XL pick a
**User** template (the Factory templates are for Live's own script).

## Launch Control XL 1: rhythm (channel 15)

| | 1 Kick | 2 Snare | 3 Hat 1 | 4 Hat 2 | 5 Perc 1 | 6 Perc 2 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| Knobs, top row | Fill | Fill | Fill | Fill | Fill | Fill | Map X | Map Y |
| Knobs, middle row | Probability | Probability | Probability | Probability | Probability | Probability | **Busy** | **Loose** |
| Knobs, bottom row | Ratchet | Ratchet | Ratchet | Ratchet | Ratchet | Ratchet | **Wild** | Kit Decay |
| Faders | Level | Level | Level | Level | Level | Level | Kit Tone | Master level |
| Track Focus buttons | Mute | Mute | Mute | Mute | Mute | Mute | Linear | Play |
| Track Control buttons | Scene 1 | Scene 2 | Scene 3 | Scene 4 | Scene 5 | Scene 6 | Scene 7 | Scene 8 |

A scene button stores into an empty scene and recalls a stored one.

## Launch Control XL 2: melody, harmony and sound (channel 14)

| | 1 Arp 1 | 2 Arp 2 | 3 Arp 3 | 4 Chords | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| Knobs, top row | Density | Density | Density | **Wander** | Harmony X | Harmony Y | **Dirt** | **Space** |
| Knobs, middle row | Cutoff | Cutoff | Cutoff | Cutoff | Reverb size | Shimmer | Alive | Strum |
| Knobs, bottom row | Resonance | Resonance | Resonance | Resonance | Macro 1 | Macro 2 | Macro 3 | Macro 4 |
| Faders | Level | Level | Level | Level | Reverb send Arp 1 | Reverb send Arp 2 | Reverb send Arp 3 | Reverb send Chords |
| Track Focus buttons | Mute | Mute | Mute | Mute | Catch | Mutate | Undo | Redo |
| Track Control buttons | Arp on | Arp on | Arp on | Chords on | Wander / Keys | Grit: Tape | Grit: Crunch | Grit: Drive |

Numbers used (factory User template): knob rows CC 13-20, 29-36, 49-56; faders
CC 77-84; Track Focus notes 41-44 and 57-60; Track Control notes 73-76 and 89-92.

## APC40 (original): harmony grid

The APC40 must not be selected as a Control Surface in Live, so that it stays in
its plain MIDI mode. It then no longer launches clips.

| Control | Does |
|---|---|
| Clip grid, 8 x 5 | The harmony map: eight columns around the tonic (the tonic is the 5th column), plain triads on the bottom row, richest chords on top. A pad moves home there. |
| Clip Stop row | Scenes 1-8 |
| Activator / Solo | Mute / solo of Kick, Snare, Hat 1, Hat 2, Arp 1, Arp 2, Arp 3, Chords |
| Track faders | Levels of the same eight parts |
| Track Control knobs | Busy, Loose, Wild, Dirt, Space, Alive, Wander, Shimmer |
| Device Control knobs | Macros 1-8 |
| Master fader | Master level |
| Crossfader | Map X |
| Scene Launch 1-5 | Mutate, Catch, Undo, Redo, Chords on |
| Play | Play |

**The catch in Live:** the APC40 tells its eight columns apart by MIDI channel
(column 1 on channel 1 ... column 8 on channel 8), and Live flattens channels.
The grid, Clip Stop, Activator, Solo and the track faders therefore need **eight
MIDI tracks**: track *n* with *MIDI From* APC40, channel *n*, Monitor In, *MIDI
To* Gristo, channel *n + 4*. Then load *APC40 > columns on channels 5-12*. With a
single track only the controls that are not per column work (both knob banks,
master fader, crossfader, Scene Launch, Play): load the layout with "columns on
channels 1-8" in that case. In the standalone app the APC40 works directly with
"columns on channels 1-8".

Numbers used: clip grid notes 53-57 (top to bottom), Clip Stop 52, Activator 50,
Solo 49, track fader CC 7, all on the column's channel; Scene Launch notes
82-86, Track Control knobs CC 48-55, Device Control knobs CC 16-23, master CC 14,
crossfader CC 15, Play note 91.

## Not there yet

* The controllers' lights do not follow Gristo.
* Knobs and faders jump to the hardware's position when first moved.
* Push 2: use it as usual in Live, or MIDI-learn its User mode controls.
