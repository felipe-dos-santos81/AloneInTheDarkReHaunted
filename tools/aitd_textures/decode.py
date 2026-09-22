# SPDX-License-Identifier: GPL-2.0-only
"""Decode AITD1 palettes and 320x200 indexed images; locate the game data."""
from __future__ import annotations

import pathlib

import numpy as np

SCREEN_WIDTH = 320
SCREEN_HEIGHT = 200
SCREEN_PIXELS = SCREEN_WIDTH * SCREEN_HEIGHT  # 64000, an engine invariant
PALETTE_BYTES = 768
SENTINEL_PAK = "ITD_RESS.PAK"


class DataNotFound(Exception):
    pass


def find_data_dir(root) -> pathlib.Path:
    """Return the folder holding the .PAK files: `root` itself, or the first
    folder below it (sorted) that contains ITD_RESS.PAK."""
    root = pathlib.Path(root)
    if (root / SENTINEL_PAK).is_file():
        return root
    if root.is_dir():
        for hit in sorted(root.rglob(SENTINEL_PAK)):
            return hit.parent
    raise DataNotFound(f"no {SENTINEL_PAK} found under {root}")


def decode_palette(raw: bytes) -> np.ndarray:
    """768 palette bytes -> (256, 3) uint8. A 6-bit VGA palette (every value
    <= 63) is scaled x4, as the engine's convertPaletteIfRequired does."""
    if len(raw) != PALETTE_BYTES:
        raise ValueError(f"palette must be {PALETTE_BYTES} bytes, got {len(raw)}")
    pal = np.frombuffer(raw, dtype=np.uint8).reshape(256, 3).copy()
    if pal.max() <= 63:
        pal = (pal.astype(np.uint16) * 4).astype(np.uint8)
    return pal


def decode_image(raw: bytes, palette: np.ndarray, offset: int = 0) -> np.ndarray:
    """64000 palette indices starting at `offset` -> (200, 320, 3) uint8 RGB."""
    needed = offset + SCREEN_PIXELS
    if len(raw) < needed:
        raise ValueError(f"image needs {needed} bytes ({SCREEN_PIXELS} pixels at offset {offset}), got {len(raw)}")
    indices = np.frombuffer(raw, dtype=np.uint8, count=SCREEN_PIXELS, offset=offset)
    return palette[indices.reshape(SCREEN_HEIGHT, SCREEN_WIDTH)]
