# SPDX-License-Identifier: GPL-2.0-only
"""Atomic file writes: nothing under data/ or Assets/ is ever half written."""
from __future__ import annotations

import io
import os
import pathlib

import numpy as np
from PIL import Image


def atomic_write_bytes(path, data: bytes) -> None:
    path = pathlib.Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(path.name + ".tmp")
    tmp.write_bytes(data)
    os.replace(tmp, path)


def png_bytes(pixels: np.ndarray) -> bytes:
    """Encode an (H, W, 3) or (H, W, 4) uint8 array as an RGB or RGBA PNG."""
    buf = io.BytesIO()
    Image.fromarray(np.ascontiguousarray(pixels, dtype=np.uint8)).save(buf, format="PNG")
    return buf.getvalue()


def save_png(path, pixels: np.ndarray) -> None:
    """Write an (H, W, 3) or (H, W, 4) uint8 array as an RGB or RGBA PNG, atomically."""
    atomic_write_bytes(path, png_bytes(pixels))
