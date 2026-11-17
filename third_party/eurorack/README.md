# Mutable Instruments code (vendored)

Parts of Mutable Instruments' Rings and of stmlib, by Emilie Gillet, used by Gristo's
Rings resonator. MIT licence (see each file's header and `stmlib/LICENSE`).

- `rings/`: from https://github.com/pichenettes/eurorack (rings/dsp, rings/resources)
- `stmlib/`: from https://github.com/pichenettes/stmlib

Changes for Gristo: `stmlib/stmlib.h` always defines `TEST` (the portable code paths
instead of ARM assembly) and `stmlib/dsp/filter.h` defines `M_PI` when the compiler
doesn't. Nothing else is changed.
