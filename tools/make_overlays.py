#!/usr/bin/env python3
"""Printable controller sheets for Gristo's built-in layouts (docs/CONTROLLERS.md).

For each controller: a labelled diagram (one page, landscape A4) and cut-out label
strips, one per row of controls, to lay between the rows on the hardware.

The strip column spacing is a guess, not a measurement: measure the distance between
two neighbouring knobs on your unit and pass it, e.g.
    python3 tools/make_overlays.py --lcxl-pitch 27.5 --apc-pitch 31
Print at 100 % ("actual size"), not "fit to page".
"""
import argparse
from reportlab.lib.pagesizes import A4, landscape
from reportlab.lib.units import mm
from reportlab.pdfgen import canvas

DRUMS = ["Kick", "Snare", "Hat 1", "Hat 2", "Perc 1", "Perc 2"]

LCXL_RHYTHM = {
    "title": "Launch Control XL 1 - Rhythm (channel 15)",
    "columns": DRUMS + ["7", "8"],
    "rows": [
        ("Knobs top (Send A)", ["Fill"] * 6 + ["Map X", "Map Y"]),
        ("Knobs middle (Send B)", ["Probability"] * 6 + ["BUSY", "LOOSE"]),
        ("Knobs bottom (Pan/Device)", ["Ratchet"] * 6 + ["WILD", "Kit Decay"]),
        ("Faders", ["Level"] * 6 + ["Kit Tone", "Master"]),
        ("Track Focus", ["Mute"] * 6 + ["Linear", "Play"]),
        ("Track Control", ["Scene %d" % (i + 1) for i in range(8)]),
    ],
}
LCXL_MELODY = {
    "title": "Launch Control XL 2 - Melody, harmony and sound (channel 14)",
    "columns": ["Arp 1", "Arp 2", "Arp 3", "Chords", "5", "6", "7", "8"],
    "rows": [
        ("Knobs top (Send A)", ["Density"] * 3 + ["WANDER", "Harmony X", "Harmony Y", "DIRT", "SPACE"]),
        ("Knobs middle (Send B)", ["Cutoff"] * 4 + ["Reverb Size", "Shimmer", "Alive", "Strum"]),
        ("Knobs bottom (Pan/Device)", ["Resonance"] * 4 + ["Macro 1", "Macro 2", "Macro 3", "Macro 4"]),
        ("Faders", ["Level"] * 4 + ["Rev Arp 1", "Rev Arp 2", "Rev Arp 3", "Rev Chords"]),
        ("Track Focus", ["Mute"] * 4 + ["Catch", "Mutate", "Undo", "Redo"]),
        ("Track Control", ["Arp On"] * 3 + ["Chords On", "Wander/Keys", "Grit Tape", "Grit Crunch", "Grit Drive"]),
    ],
}
APC_PARTS = ["Kick", "Snare", "Hat 1", "Hat 2", "Arp 1", "Arp 2", "Arp 3", "Chords"]
HARMONY_ROWS = ["11th", "9th", "7th", "Add9", "Triad"]
APC40 = {
    "title": "APC40 (original) - Harmony grid",
    "columns": APC_PARTS,
    "rows": [("Clip row %d: %s" % (i + 1, name), ["-4", "-3", "-2", "-1", "TONIC", "+1", "+2", "+3"]) for i, name in enumerate(HARMONY_ROWS)]
    + [
        ("Clip Stop", ["Scene %d" % (i + 1) for i in range(8)]),
        ("Activator", ["Mute"] * 8),
        ("Solo", ["Solo"] * 8),
        ("Track faders", ["Level"] * 8),
        ("Track Control knobs", ["BUSY", "LOOSE", "WILD", "DIRT", "SPACE", "Alive", "Wander", "Shimmer"]),
        ("Device Control knobs", ["Macro %d" % (i + 1) for i in range(8)]),
    ],
    "extras": ["Scene Launch 1-5: Mutate, Catch, Undo, Redo, Chords On", "Master fader: Master level", "Crossfader: Map X", "Play: Play",
               "Harmony grid: columns are steps along the chain of thirds from the tonic (5th column); a pad moves home there."],
}


def diagram(c, sheet):
    width, height = landscape(A4)
    margin = 14 * mm
    c.setFont("Helvetica-Bold", 15)
    c.drawString(margin, height - margin, "Gristo - " + sheet["title"])
    rows, columns = sheet["rows"], sheet["columns"]
    label_w = 52 * mm
    cell_w = (width - 2 * margin - label_w) / len(columns)
    extras = sheet.get("extras", [])
    top = height - margin - 12 * mm
    cell_h = min(16 * mm, (top - margin - 6 * mm * len(extras) - 8 * mm) / (len(rows) + 1))
    c.setFont("Helvetica-Bold", 10)
    for i, name in enumerate(columns):
        c.drawCentredString(margin + label_w + (i + 0.5) * cell_w, top - cell_h * 0.6, name)
    for r, (row_name, cells) in enumerate(rows):
        y = top - (r + 2) * cell_h
        c.setFont("Helvetica", 9)
        c.drawString(margin, y + cell_h * 0.38, row_name)
        for i, text in enumerate(cells):
            x = margin + label_w + i * cell_w
            c.roundRect(x + 1.2 * mm, y + 1.2 * mm, cell_w - 2.4 * mm, cell_h - 2.4 * mm, 2 * mm)
            c.setFont("Helvetica-Bold" if text.isupper() else "Helvetica", 9.5)
            c.drawCentredString(x + cell_w / 2, y + cell_h * 0.38, text)
    y = top - (len(rows) + 2) * cell_h - 2 * mm
    c.setFont("Helvetica", 9)
    for line in extras:
        c.drawString(margin, y, line)
        y -= 6 * mm
    c.setFont("Helvetica-Oblique", 8)
    c.drawString(margin, margin - 5 * mm, "Layout as built into Gristo (MIDI button). Not checked against the hardware: re-learn any control that differs.")
    c.showPage()


def strips(c, sheet, pitch_mm):
    width, height = landscape(A4)
    margin = 12 * mm
    pitch = pitch_mm * mm
    strip_h = 9 * mm
    c.setFont("Helvetica-Bold", 12)
    c.drawString(margin, height - margin, "Cut-out strips - " + sheet["title"])
    c.setFont("Helvetica", 8)
    c.drawString(margin, height - margin - 5 * mm,
                 "Column spacing %.1f mm (a guess: measure yours and re-run tools/make_overlays.py). Print at 100 %%. Cut along the outer lines." % pitch_mm)
    x0 = (width - 8 * pitch) / 2
    y = height - margin - 12 * mm
    for row_name, cells in sheet["rows"]:
        y -= strip_h + 5 * mm
        if y < margin:
            c.showPage()
            y = height - margin - strip_h
        c.setFont("Helvetica", 6.5)
        c.drawString(x0, y + strip_h + 1 * mm, row_name)
        c.setLineWidth(0.6)
        c.rect(x0, y, 8 * pitch, strip_h)
        c.setLineWidth(0.2)
        for i, text in enumerate(cells):
            if i:
                c.line(x0 + i * pitch, y, x0 + i * pitch, y + strip_h)
            c.setFont("Helvetica-Bold" if text.isupper() else "Helvetica", 8.5)
            c.drawCentredString(x0 + (i + 0.5) * pitch, y + strip_h * 0.33, text)
    # A ruler to check the print scale.
    y -= 14 * mm
    if y > margin:
        c.setLineWidth(0.4)
        c.line(x0, y, x0 + 100 * mm, y)
        for t in range(0, 101, 10):
            c.line(x0 + t * mm, y, x0 + t * mm, y + 2 * mm)
        c.setFont("Helvetica", 7)
        c.drawString(x0 + 102 * mm, y - 1 * mm, "100 mm: check with a ruler")
    c.showPage()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", default="docs/Gristo-controller-overlays.pdf")
    parser.add_argument("--lcxl-pitch", type=float, default=27.0, help="mm between neighbouring Launch Control XL knobs")
    parser.add_argument("--apc-pitch", type=float, default=30.0, help="mm between neighbouring APC40 track columns")
    args = parser.parse_args()
    c = canvas.Canvas(args.out, pagesize=landscape(A4))
    c.setTitle("Gristo controller overlays")
    for sheet, pitch in ((LCXL_RHYTHM, args.lcxl_pitch), (LCXL_MELODY, args.lcxl_pitch), (APC40, args.apc_pitch)):
        diagram(c, sheet)
        strips(c, sheet, pitch)
    c.save()
    print("wrote", args.out)


if __name__ == "__main__":
    main()
