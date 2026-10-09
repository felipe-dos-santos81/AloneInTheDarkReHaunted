# SPDX-License-Identifier: GPL-2.0-only
"""Decode AITD1 palettes; locate the game data."""
from __future__ import annotations

import pathlib

import numpy as np

PALETTE_BYTES = 768
PALETTE_PAK = "ITD_RESS"  # the game palette: this PAK's entry 3
PALETTE_ENTRY = 3
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

