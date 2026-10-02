# SPDX-License-Identifier: GPL-2.0-only
"""Silhouette agreement between an aligned delivery and the exported
reference views (bodies/<KEY>/reference/*.png + views.json): each view is
re-rendered at SIZE pixels with the export's own framing and compared, by
intersection over union, with the reference's alpha (box-filtered down when
the export was larger)."""
from __future__ import annotations

import json
import pathlib

import numpy as np
from PIL import Image

from .mesh import Mesh, Surface
from .raster import Framing, View, render

SIZE = 256


def _mask(rgba: np.ndarray) -> np.ndarray:
    return rgba[..., 3] >= 128


def silhouette_iou(surface: Surface, reference_dir) -> dict[str, float]:
    """IoU per view name; `surface` is engine space, already aligned."""
    reference_dir = pathlib.Path(reference_dir)
    doc = json.loads((reference_dir / "views.json").read_text())
    f = doc["framing"]
    size = min(SIZE, f["size"])
    framing = Framing(tuple(f["centre"]), f["units_per_pixel"] * f["size"] / size, size)
    soup = surface.positions[surface.triangles].reshape(-1, 3)
    n = len(surface.triangles)
    mesh = Mesh(soup, np.zeros((3 * n, 3), np.float32), np.zeros(3 * n, int), np.arange(n))
    out = {}
    for v in doc["views"]:
        ref = Image.open(reference_dir / v["file"]).convert("RGBA").resize((size, size), Image.BOX)
        want = _mask(np.asarray(ref))
        got = _mask(render(mesh, View(v["name"], v["yaw_deg"]), framing, ssaa=1))
        union = (want | got).sum()
        out[v["name"]] = float((want & got).sum() / union) if union else 1.0
    return out
