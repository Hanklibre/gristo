#!/usr/bin/env python3
"""Build Gristo's 6-voice drum map from a pattern corpus.

The map is a W x H grid of nodes. Each node stores, for 6 voices x 32 steps,
a "level" from 0..255. At runtime the plugin bilinearly interpolates the four
nodes around the (X, Y) position and plays a step when its level exceeds the
voice's Fill threshold. High levels are the backbone of a groove; low levels
are the fills that appear as density increases.

Pipeline:
  1. Read the corpus (.txt patterns and .mid loops) into level vectors.
  2. Train a self-organizing map (batch SOM, PCA-initialised, deterministic)
     so that similar grooves land on neighbouring nodes.
  3. Sharpen nodes that "own" corpus patterns towards those patterns, so the
     map keeps the character of the source grooves instead of averaging them
     into mush.
  4. Orient the map: X runs from straight to syncopated, Y from
     four-on-the-floor to broken / halftime beats.
  5. Emit C++ (core/src/MapData.cpp) and a human-readable overview
     (docs/map.md).

Requires numpy. Output is deterministic for a given corpus and settings.
"""

import argparse
import glob
import os
import struct
import sys

import numpy as np

VOICES = ["kick", "snare", "hh1", "hh2", "perc1", "perc2"]
NUM_VOICES = len(VOICES)
STEPS = 32
STEPS_PER_BAR = 16

# Hit velocity factors for the text format.
CHAR_VELOCITY = {"X": 1.0, "x": 0.6, "o": 0.25}
REST_CHARS = set("-.")

# Level model. A hit's level grows with its velocity and its metric weight;
# rests get a small "fill potential" so higher Fill settings add plausible
# notes in metrically sensible places.
LEVEL_BASE = 0.40
LEVEL_VELOCITY = 0.40
LEVEL_METRIC = 0.20
FILL_POTENTIAL = 0.38
VOICE_FILL_FACTOR = {"kick": 0.35, "snare": 0.5, "hh1": 0.9, "hh2": 0.4, "perc1": 0.6, "perc2": 0.5}

# General MIDI drum notes -> Gristo voices.
GM_NOTE_TO_VOICE = {
    35: "kick", 36: "kick",
    38: "snare", 40: "snare",
    42: "hh1", 44: "hh1", 51: "hh1", 59: "hh1",
    46: "hh2", 49: "hh2", 52: "hh2", 55: "hh2", 57: "hh2",
    37: "perc1", 39: "perc1", 54: "perc1", 69: "perc1", 70: "perc1", 82: "perc1",
    41: "perc2", 43: "perc2", 45: "perc2", 47: "perc2", 48: "perc2", 50: "perc2",
    56: "perc2", 60: "perc2", 61: "perc2", 62: "perc2", 63: "perc2", 64: "perc2",
    65: "perc2", 66: "perc2", 67: "perc2", 68: "perc2", 75: "perc2", 76: "perc2", 77: "perc2",
}


def metric_weight(step):
    s = step % STEPS_PER_BAR
    if s == 0:
        return 1.0
    if s % 8 == 0:
        return 0.8
    if s % 4 == 0:
        return 0.6
    if s % 2 == 0:
        return 0.35
    return 0.15


def hit_level(velocity, step):
    return min(1.0, LEVEL_BASE + LEVEL_VELOCITY * velocity + LEVEL_METRIC * metric_weight(step))


def rest_level(voice, step):
    return FILL_POTENTIAL * VOICE_FILL_FACTOR[voice] * (0.3 + 0.7 * metric_weight(step))


def levels_from_hits(hits):
    """hits: dict voice -> list of 32 velocities (None for rest). Returns (6, 32) array."""
    out = np.zeros((NUM_VOICES, STEPS))
    for vi, voice in enumerate(VOICES):
        row = hits.get(voice, [None] * STEPS)
        for s in range(STEPS):
            out[vi, s] = hit_level(row[s], s) if row[s] is not None else rest_level(voice, s)
    return out


# --------------------------------------------------------------------------- text corpus

def parse_text_corpus(path):
    patterns = []
    name, hits = None, {}

    def flush():
        if name is not None:
            patterns.append((name, levels_from_hits(hits), os.path.basename(path)))

    with open(path, encoding="utf-8") as f:
        for lineno, raw in enumerate(f, 1):
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            if line.startswith("=="):
                flush()
                name, hits = line.strip("= ").strip(), {}
                continue
            if name is None:
                raise SystemExit(f"{path}:{lineno}: voice line before any '== name ==' header")
            voice, _, steps = line.partition(" ")
            if voice not in VOICES:
                raise SystemExit(f"{path}:{lineno}: unknown voice '{voice}'")
            cells = [c for c in steps if c not in "| \t"]
            if len(cells) == STEPS_PER_BAR:
                cells = cells * 2
            if len(cells) != STEPS:
                raise SystemExit(f"{path}:{lineno}: expected 16 or 32 steps, got {len(cells)}")
            row = []
            for c in cells:
                if c in REST_CHARS:
                    row.append(None)
                elif c in CHAR_VELOCITY:
                    row.append(CHAR_VELOCITY[c])
                else:
                    raise SystemExit(f"{path}:{lineno}: unknown step character '{c}'")
            hits[voice] = row
    flush()
    return patterns


# --------------------------------------------------------------------------- MIDI corpus

def _read_varlen(data, pos):
    value = 0
    while True:
        b = data[pos]
        pos += 1
        value = (value << 7) | (b & 0x7F)
        if not b & 0x80:
            return value, pos


def read_midi_notes(path):
    """Minimal Standard MIDI File reader. Returns (ticks_per_quarter, [(tick, note, velocity)])."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] != b"MThd":
        raise ValueError("not a MIDI file")
    header_len = struct.unpack(">I", data[4:8])[0]
    _, ntracks, division = struct.unpack(">HHH", data[8:14])
    if division & 0x8000:
        raise ValueError("SMPTE time division is not supported")
    pos = 8 + header_len
    notes = []
    for _ in range(ntracks):
        if data[pos:pos + 4] != b"MTrk":
            raise ValueError("bad track chunk")
        length = struct.unpack(">I", data[pos + 4:pos + 8])[0]
        end = pos + 8 + length
        pos += 8
        tick, status = 0, 0
        while pos < end:
            delta, pos = _read_varlen(data, pos)
            tick += delta
            b = data[pos]
            if b & 0x80:
                status = b
                pos += 1
            if status == 0xFF:
                pos += 1  # meta type
                length, pos = _read_varlen(data, pos)
                pos += length
            elif status in (0xF0, 0xF7):
                length, pos = _read_varlen(data, pos)
                pos += length
            else:
                kind = status & 0xF0
                if kind in (0xC0, 0xD0):
                    pos += 1
                else:
                    d1, d2 = data[pos], data[pos + 1]
                    pos += 2
                    if kind == 0x90 and d2 > 0:
                        notes.append((tick, d1, d2))
        pos = end
    return division, notes


def parse_midi_corpus(path):
    tpq, notes = read_midi_notes(path)
    if not notes:
        return []
    ticks_per_step = tpq / 4.0
    total_steps = max(int(round(t / ticks_per_step)) for t, _, _ in notes) + 1
    windows = max(1, -(-total_steps // STEPS))
    grids = [{} for _ in range(windows)]
    for tick, note, vel in notes:
        voice = GM_NOTE_TO_VOICE.get(note)
        if voice is None:
            continue
        step = int(round(tick / ticks_per_step))
        w, s = divmod(step, STEPS)
        if w >= windows:
            continue
        row = grids[w].setdefault(voice, [None] * STEPS)
        velocity = min(1.0, max(0.0, (vel - 30) / 97.0))
        row[s] = velocity if row[s] is None else max(row[s], velocity)
    levels = np.mean([levels_from_hits(g) for g in grids], axis=0)
    name = os.path.splitext(os.path.basename(path))[0]
    return [(name, levels, os.path.basename(path))]


# --------------------------------------------------------------------------- SOM

def train_som(data, width, height, epochs, sigma_start, sigma_end):
    """Deterministic batch SOM with PCA initialisation. data: (P, D). Returns (W*H, D)."""
    mean = data.mean(axis=0)
    centered = data - mean
    _, s, vt = np.linalg.svd(centered, full_matrices=False)
    spread = s / np.sqrt(max(1, len(data) - 1))
    gx, gy = np.meshgrid(np.linspace(-1, 1, width), np.linspace(-1, 1, height))
    coords = np.stack([gx.ravel(), gy.ravel()], axis=1)  # node index = y * width + x
    codebook = mean + coords[:, :1] * spread[0] * vt[0] + coords[:, 1:2] * spread[1] * vt[1]
    codebook = np.clip(codebook, 0.0, 1.0)

    grid = np.stack(np.meshgrid(np.arange(width), np.arange(height)), axis=-1).reshape(-1, 2)
    grid_d2 = ((grid[:, None, :] - grid[None, :, :]) ** 2).sum(-1)
    for e in range(epochs):
        sigma = sigma_start * (sigma_end / sigma_start) ** (e / max(1, epochs - 1))
        bmu = best_matching_units(data, codebook)
        h = np.exp(-grid_d2[:, bmu] / (2.0 * sigma * sigma))  # (nodes, P)
        codebook = (h @ data) / h.sum(axis=1, keepdims=True)
    return codebook


def best_matching_units(data, codebook):
    d2 = ((data[:, None, :] - codebook[None, :, :]) ** 2).sum(-1)
    return d2.argmin(axis=1)


def balanced_assignment(data, codebook):
    """Assign patterns to nodes, closest pairs first, with at most ceil(P / N) patterns
    per node. A plain best-matching-unit lookup lets a trained SOM pile outliers into
    its corners; spreading them keeps every corpus groove reachable on the map."""
    capacity = -(-len(data) // len(codebook))
    d2 = ((data[:, None, :] - codebook[None, :, :]) ** 2).sum(-1)
    order = np.argsort(d2, axis=None, kind="stable")
    assignment = np.full(len(data), -1)
    load = np.zeros(len(codebook), dtype=int)
    for flat in order:
        p, n = divmod(int(flat), len(codebook))
        if assignment[p] < 0 and load[n] < capacity:
            assignment[p] = n
            load[n] += 1
    return assignment


def sharpen(codebook, data, assignment, amount):
    out = codebook.copy()
    for node in range(len(codebook)):
        owned = data[assignment == node]
        if len(owned):
            out[node] = amount * owned.mean(axis=0) + (1.0 - amount) * codebook[node]
    return out


def orient(codebook, width, height):
    """Flip axes so X increases with syncopation and Y with broken-beat-ness."""
    nodes = codebook.reshape(height, width, NUM_VOICES, STEPS)
    node_ids = np.arange(width * height).reshape(height, width)
    offbeats = [s for s in range(STEPS) if s % 2 == 1]
    syncopation = nodes[:, :, :, offbeats].mean(axis=(2, 3)) / (nodes.mean(axis=(2, 3)) + 1e-9)
    four_floor = nodes[:, :, 0, [0, 4, 8, 12, 16, 20, 24, 28]].mean(axis=2)
    xs = np.arange(width)
    ys = np.arange(height)
    flip_x = np.corrcoef(np.tile(xs, height), syncopation.ravel())[0, 1] < 0
    flip_y = np.corrcoef(np.repeat(ys, width), four_floor.ravel())[0, 1] > 0
    if flip_x:
        nodes, node_ids = nodes[:, ::-1], node_ids[:, ::-1]
    if flip_y:
        nodes, node_ids = nodes[::-1, :], node_ids[::-1, :]
    # new node index -> old node index
    return nodes.reshape(width * height, -1), node_ids.ravel()


# --------------------------------------------------------------------------- output

def node_labels(patterns, assignment, old_index_of_new):
    new_index_of_old = np.argsort(old_index_of_new)
    labels = [[] for _ in range(len(old_index_of_new))]
    for (name, _, _), node in zip(patterns, assignment):
        labels[new_index_of_old[node]].append(name)
    return labels


def write_cpp(path, codebook_u8, labels, width, height, corpus_desc):
    lines = [
        "// GENERATED by tools/mapbuilder/build_map.py - do not edit by hand.",
        f"// Corpus: {corpus_desc}",
        "",
        '#include "gristo/DrumMap.h"',
        "",
        "namespace gristo {",
        "namespace {",
        "",
        f"constexpr int kWidth = {width};",
        f"constexpr int kHeight = {height};",
        "",
        "// [node = y * width + x][voice][step]",
        f"const std::uint8_t kLevels[kWidth * kHeight * kNumVoices * kPatternSteps] = {{",
    ]
    for node in range(width * height):
        y, x = divmod(node, width)
        name = ", ".join(labels[node]) if labels[node] else "(blend)"
        lines.append(f"    // node ({x}, {y}): {name}")
        values = codebook_u8[node].reshape(NUM_VOICES, STEPS)
        for vi, voice in enumerate(VOICES):
            row = ",".join(f"{v:3d}" for v in values[vi])
            lines.append(f"    {row}, // {voice}")
    lines.append("};")
    lines.append("")
    lines.append("const char* const kNodeNames[kWidth * kHeight] = {")
    for node in range(width * height):
        name = " / ".join(labels[node]) if labels[node] else ""
        lines.append(f'    "{name}",')
    lines.append("};")
    lines += [
        "",
        "} // namespace",
        "",
        "const DrumMapData& defaultMapData()",
        "{",
        "    static const DrumMapData data { kWidth, kHeight, kLevels, kNodeNames };",
        "    return data;",
        "}",
        "",
        "} // namespace gristo",
        "",
    ]
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))


def write_overview(path, codebook_u8, labels, width, height, corpus_desc):
    out = [
        "# Drum map overview",
        "",
        "Generated by `tools/mapbuilder/build_map.py`. Corpus: " + corpus_desc + ".",
        "",
        "X runs from straight (left) to syncopated (right). Y runs from",
        "four-on-the-floor (top, Y = 0) to broken / halftime beats (bottom, Y = 1).",
        "Each cell lists the corpus patterns assigned to that node; empty cells",
        "are blends of their neighbours. Between nodes the plugin interpolates.",
        "",
        "| Y \\ X | " + " | ".join(str(x) for x in range(width)) + " |",
        "|---" * (width + 1) + "|",
    ]
    for y in range(height):
        cells = []
        for x in range(width):
            node = y * width + x
            cells.append("<br>".join(labels[node]) if labels[node] else "·")
        out.append(f"| **{y}** | " + " | ".join(cells) + " |")
    out.append("")
    out.append("## Mean density per node")
    out.append("")
    out.append("```")
    for y in range(height):
        row = []
        for x in range(width):
            row.append(f"{codebook_u8[y * width + x].mean() / 255:.2f}")
        out.append("  ".join(row))
    out.append("```")
    out.append("")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(out))


def main():
    root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--corpus", default=os.path.join(root, "data", "corpus"))
    ap.add_argument("--out", default=os.path.join(root, "core", "src", "MapData.cpp"))
    ap.add_argument("--overview", default=os.path.join(root, "docs", "map.md"))
    ap.add_argument("--width", type=int, default=6)
    ap.add_argument("--height", type=int, default=6)
    ap.add_argument("--epochs", type=int, default=80)
    ap.add_argument("--sigma-start", type=float, default=2.5)
    ap.add_argument("--sigma-end", type=float, default=0.5)
    ap.add_argument("--sharpen", type=float, default=0.8,
                    help="0 = pure SOM average, 1 = nodes copy their own corpus patterns")
    args = ap.parse_args()

    patterns = []
    txt_files = sorted(glob.glob(os.path.join(args.corpus, "**", "*.txt"), recursive=True))
    mid_files = sorted(glob.glob(os.path.join(args.corpus, "**", "*.mid"), recursive=True))
    for path in txt_files:
        patterns += parse_text_corpus(path)
    for path in mid_files:
        try:
            patterns += parse_midi_corpus(path)
        except (ValueError, IndexError) as exc:
            print(f"warning: skipping {path}: {exc}", file=sys.stderr)
    if len(patterns) < 4:
        raise SystemExit("need at least 4 patterns in the corpus")
    names = [p[0] for p in patterns]
    dupes = {n for n in names if names.count(n) > 1}
    if dupes:
        raise SystemExit("duplicate pattern names: " + ", ".join(sorted(dupes)))

    data = np.array([p[1].ravel() for p in patterns])
    codebook = train_som(data, args.width, args.height, args.epochs, args.sigma_start, args.sigma_end)
    assignment = balanced_assignment(data, codebook)
    codebook = sharpen(codebook, data, assignment, args.sharpen)
    codebook, old_index_of_new = orient(codebook, args.width, args.height)
    codebook_u8 = np.clip(np.round(codebook * 255.0), 0, 255).astype(np.uint8)
    labels = node_labels(patterns, assignment, old_index_of_new)
    corpus_desc = f"{len(patterns)} patterns from {len(txt_files)} text and {len(mid_files)} MIDI files"
    write_cpp(args.out, codebook_u8, labels, args.width, args.height, corpus_desc)
    if args.overview:
        write_overview(args.overview, codebook_u8, labels, args.width, args.height, corpus_desc)

    used = sum(1 for l in labels if l)
    print(f"{corpus_desc}; {args.width}x{args.height} map, {used} nodes hold corpus patterns")
    print(f"wrote {os.path.relpath(args.out, root)}")


if __name__ == "__main__":
    main()
