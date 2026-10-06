# Pattern corpus

The drum map is trained from the patterns in this folder. Every pattern here is
original material written for Gristo. Add your own loops as `.txt` (format
below) or as `.mid` files and re-run the map builder:

    python3 tools/mapbuilder/build_map.py

## Text format

```
== pattern_name ==
kick   X-----x---x-----|X-----x-x-----o-
snare  ----X-------X---
hh1    x-x-x-x-x-x-x-x-
```

* A pattern is two bars of sixteenths (32 steps). A line with only 16 steps is
  repeated for the second bar. `|` and spaces are ignored.
* Voices: `kick`, `snare`, `hh1` (closed hat), `hh2` (open hat), `perc1`
  (clap / rim / shaker), `perc2` (toms / bells / congas). Missing voices are
  empty.
* `X` = accent, `x` = normal hit, `o` = ghost note, `-` or `.` = rest.
* Lines starting with `#` are comments.

## MIDI files

`.mid` files are read with a General MIDI drum note mapping (see
`build_map.py`). Loops longer than two bars are folded: each step's level
becomes the average over all two-bar windows, so hits that only occur
sometimes end up with lower levels and appear later as Fill increases.
