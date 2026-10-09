#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Score the frames the game writes in HD-model compare mode
(debug.hdModelsCompare = true): hdcompare_lit.png, hdcompare_unlit.png,
hdcompare_hidden.png, hdcompare_classic.png and hdcompare.txt, next to the
saves.

    tools/hd_compare.py DIR [--limit IOU]

The hidden frame is the scene with every replaced body left out, so a body's
silhouette is where a frame differs from it. For every replacement listed in
hdcompare.txt, inside its screen box (grown by a game pixel):

- iou: intersection over union of the unlit HD silhouette and the classic
  one. With identity models (make identity-models) this is the identity
  oracle; colours do not enter it (the identity texture is the palette
  colour, the classic body may be painted or shaded).
- brightness: the lit HD frame's mean over the classic frame's, on the
  classic silhouette (1.0 = as bright as the classic body), for the lighting
  sign-off.

A body fails when its iou is below --limit (default 0.85), and so does a
run that drew no replacement at all. Exit codes: 0 every body passes, 1 a
body fails or none was drawn, 2 missing or unreadable files."""
from __future__ import annotations

import argparse
import pathlib
import sys
from dataclasses import dataclass

import numpy as np
from PIL import Image

# Largest channel difference from the hidden frame that is still background. The
# frames are bit-identical outside the bodies (clock held still); inside a box,
# pixels differ by up to 2 levels from frame to frame. A higher value loses a
# dark body on a dark wall.
SILHOUETTE_THRESHOLD = 3
DEFAULT_LIMIT = 0.85
SCREEN_W, SCREEN_H = 320, 200
FRAMES = ("lit", "unlit", "hidden", "classic")


@dataclass
class BodyScore:
    key: str
    box: tuple[int, int, int, int]  # 320x200, as hdcompare.txt gives it
    iou: float
    brightness: float


def read_boxes(path) -> list[tuple[str, tuple[int, int, int, int]]]:
    out = []
    for line in pathlib.Path(path).read_text().splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        key, *coords = line.split()
        out.append((key, tuple(int(c) for c in coords)))
    return out


def pixel_box(box, width: int, height: int) -> tuple[slice, slice]:
    """A 320x200 box (inclusive), grown by one game pixel, as row and column
    slices of a width x height frame."""
    x0, y0, x1, y1 = box
    x0, y0, x1, y1 = max(x0 - 1, 0), max(y0 - 1, 0), min(x1 + 1, SCREEN_W - 1), min(y1 + 1, SCREEN_H - 1)
    return (slice(y0 * height // SCREEN_H, (y1 + 1) * height // SCREEN_H),
            slice(x0 * width // SCREEN_W, (x1 + 1) * width // SCREEN_W))


def silhouette(frame: np.ndarray, hidden: np.ndarray) -> np.ndarray:
    return np.abs(frame.astype(int) - hidden.astype(int)).max(axis=2) > SILHOUETTE_THRESHOLD


def score(frames: dict[str, np.ndarray], boxes) -> list[BodyScore]:
    height, width = frames["classic"].shape[:2]
    out = []
    for key, box in boxes:
        rows, cols = pixel_box(box, width, height)
        hidden = frames["hidden"][rows, cols]
        hd = silhouette(frames["unlit"][rows, cols], hidden)
        classic = silhouette(frames["classic"][rows, cols], hidden)
        union = (hd | classic).sum()
        iou = float((hd & classic).sum() / union) if union else 1.0
        base = frames["classic"][rows, cols][classic].astype(float).mean() if classic.any() else 0.0
        lit = frames["lit"][rows, cols][classic].astype(float).mean() if classic.any() else 0.0
        out.append(BodyScore(key, box, iou, float(lit / base) if base > 0 else 1.0))
    return out


def main(argv=None, log=print) -> int:
    parser = argparse.ArgumentParser(prog="hd_compare.py", description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("dir", type=pathlib.Path, help="folder holding the hdcompare_* files (the game's Resources)")
    parser.add_argument("--limit", type=float, default=DEFAULT_LIMIT, help="smallest silhouette IoU (default 0.85)")
    args = parser.parse_args(argv)
    try:
        frames = {n: np.asarray(Image.open(args.dir / f"hdcompare_{n}.png").convert("RGB")) for n in FRAMES}
        boxes = read_boxes(args.dir / "hdcompare.txt")
    except (OSError, ValueError) as exc:
        log(f"error: {exc}")
        return 2
    if len({f.shape for f in frames.values()}) != 1:
        log("error: the frames differ in size")
        return 2
    scores = score(frames, boxes)
    if not scores:
        log("no replacement was drawn: nothing to compare (are the .hdm files in models_hd/?)")
        return 1
    failed = [s for s in scores if s.iou < args.limit]
    for s in scores:
        log(f"{s.key}: box {s.box}, iou {s.iou:.3f}, brightness {s.brightness:.2f}, "
            + ("ok" if s not in failed else "BELOW LIMIT"))
    log(f"{len(scores)} bodies, {len(failed)} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
