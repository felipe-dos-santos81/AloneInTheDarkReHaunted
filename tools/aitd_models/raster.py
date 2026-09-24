# SPDX-License-Identifier: GPL-2.0-only
"""A small numpy z-buffer rasteriser for orthographic reference renders.

Views are yaw angles about the engine's vertical axis, in the engine's own
camera convention (screen right, screen down, forward = +x, +y, +z at yaw 0),
so a render is never mirrored relative to the game. Shading is a headlight:
palette colour times (AMBIENT + (1 - AMBIENT) * |n . forward|), which keeps
the palette readable and shows the shape."""
from __future__ import annotations

import math
from dataclasses import dataclass

import numpy as np

from .mesh import Mesh, face_normals

AMBIENT = 0.55
MARGIN = 0.05


@dataclass(frozen=True)
class View:
    name: str
    yaw_deg: float

    def basis(self) -> np.ndarray:
        """Rows: right, down, forward, in engine space. Right-handed:
        right x down == forward."""
        t = math.radians(self.yaw_deg)
        c, s = math.cos(t), math.sin(t)
        return np.array([[c, 0.0, -s], [0.0, 1.0, 0.0], [s, 0.0, c]])


@dataclass(frozen=True)
class Framing:
    """Shared by every view of one body, so proportions match across views."""
    centre: tuple[float, float, float]  # engine space, projected to the image centre
    units_per_pixel: float
    size: int

    def to_json(self) -> dict:
        return {"centre": list(self.centre), "units_per_pixel": self.units_per_pixel, "size": self.size}


def framing_for(positions: np.ndarray, size: int) -> Framing:
    lo, hi = positions.min(axis=0), positions.max(axis=0)
    centre = (lo + hi) / 2
    # Any yaw sees at most the horizontal radius and the full height.
    radius = float(np.max(np.linalg.norm((positions - centre)[:, [0, 2]], axis=1)))
    half = max(radius, float(hi[1] - lo[1]) / 2, 1.0)
    return Framing(tuple(float(c) for c in centre), 2 * half * (1 + 2 * MARGIN) / size, size)


def render(mesh: Mesh, view: View, framing: Framing, ssaa: int = 4) -> np.ndarray:
    """(size, size, 4) uint8 RGBA; background fully transparent."""
    n = framing.size * ssaa
    colour = np.zeros((n, n, 3), np.float32)
    cover = np.zeros((n, n), np.float32)
    depth = np.full((n, n), np.inf)
    if mesh.triangle_count:
        basis = view.basis()
        cam = (mesh.positions - np.array(framing.centre)) @ basis.T  # right, down, forward
        scale = 1.0 / (framing.units_per_pixel / ssaa)
        sx = cam[:, 0] * scale + n / 2
        sy = cam[:, 1] * scale + n / 2
        sz = cam[:, 2]
        shade = AMBIENT + (1 - AMBIENT) * np.abs(face_normals(mesh.positions) @ basis[2])
        for t in range(mesh.triangle_count):
            i = 3 * t
            xs, ys, zs = sx[i:i + 3], sy[i:i + 3], sz[i:i + 3]
            x0, x1 = max(int(np.floor(xs.min())), 0), min(int(np.ceil(xs.max())), n - 1)
            y0, y1 = max(int(np.floor(ys.min())), 0), min(int(np.ceil(ys.max())), n - 1)
            if x0 > x1 or y0 > y1:
                continue
            area = (xs[1] - xs[0]) * (ys[2] - ys[0]) - (xs[2] - xs[0]) * (ys[1] - ys[0])
            if abs(area) < 1e-9:
                continue
            px, py = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
            w0 = ((xs[1] - px) * (ys[2] - py) - (xs[2] - px) * (ys[1] - py)) / area
            w1 = ((xs[2] - px) * (ys[0] - py) - (xs[0] - px) * (ys[2] - py)) / area
            w2 = 1 - w0 - w1
            inside = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
            if not inside.any():
                continue
            z = w0 * zs[0] + w1 * zs[1] + w2 * zs[2]
            region = depth[y0:y1 + 1, x0:x1 + 1]
            win = inside & (z < region)
            region[win] = z[win]
            colour[y0:y1 + 1, x0:x1 + 1][win] = mesh.colors[i] * shade[t]
            cover[y0:y1 + 1, x0:x1 + 1][win] = 1.0
    rgba = np.concatenate([colour, cover[..., None]], axis=2)
    rgba = rgba.reshape(framing.size, ssaa, framing.size, ssaa, 4).mean(axis=(1, 3))
    alpha = rgba[..., 3:4]
    rgb = np.divide(rgba[..., :3], alpha, out=np.zeros_like(rgba[..., :3]), where=alpha > 0)
    return (np.concatenate([rgb, alpha], axis=2) * 255 + 0.5).clip(0, 255).astype(np.uint8)
