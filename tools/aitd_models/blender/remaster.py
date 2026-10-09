# SPDX-License-Identifier: GPL-2.0-only
"""The plain-Python half of the Blender stage (docs/model-contract.md, "The
in-repo generator"). It runs outside Blender, before and after it:

- prepare: per original triangle, the engine's atlas UVs (front and back),
  its front weight and which atlas it takes, so Blender bakes exactly what
  the engine paints (TatouSource/FitdLib/modelAtlas.cpp, renderer.cpp);
- finish: the two bakes composited into one sRGB PNG, and the refined mesh
  written as model.glb.

Blender itself only refines geometry and bakes (stage.py)."""
from __future__ import annotations

import pathlib

import numpy as np

from ..body import PRIM_POLY, Body

KIND_PALETTE, KIND_BODY, KIND_RAMP, KIND_OTHER = 0, 1, 2, 3
KIND_NAMES = ("palette", "body", "ramp", "other")
TRIANGLE_TARGET = 30000  # import warns above it
MAX_LEVEL = 4


def engine_rest_vertices(body: Body) -> np.ndarray:
    """computeRestPoseVertices: each group's vertices offset by its pivot
    vertex, groups in index order, reading the pivot as already offset."""
    out = np.array(body.vertices, float).reshape(-1, 3)
    for g in body.groups:
        for v in range(g.start, g.start + g.count):
            out[v] += out[g.pivot]
    return out


def projection(vertices: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """computeProjectionParams: x/y bounds widened by 5 % per side; a range
    under 1 unit counts as 1. Returns (padded minimum, padded range)."""
    lo, hi = vertices[:, :2].min(axis=0), vertices[:, :2].max(axis=0)
    raw = np.maximum(hi - lo, 1.0)
    return lo - raw * 0.05, raw * 1.1


def corner_uv(xy: np.ndarray, pmin: np.ndarray, prange: np.ndarray, front: bool) -> np.ndarray:
    """The atlas UV of engine-space points (N, 2): the front painting on the
    left half, the back one mirrored on the right; v = 1 - yNorm."""
    norm = (np.asarray(xy, float) - pmin) / prange
    u = norm[:, 0] * 0.5 if front else 0.5 + (1.0 - norm[:, 0]) * 0.5
    return np.stack([u, 1.0 - norm[:, 1]], axis=1)


def mirror_uv(v: np.ndarray) -> np.ndarray:
    """renderer.cpp mirrorUV: fold into [0, 1], kept 0.005 off the edges."""
    v = np.abs(np.asarray(v, float))
    whole = np.floor(v)
    frac = v - whole
    frac = np.where(whole.astype(np.int64) & 1, 1.0 - frac, frac)
    return np.clip(frac, 0.005, 0.995)


def front_weight(p0, p1, p2) -> float:
    """How much of the front painting a face takes: 1 facing the front (the
    engine's nz < 0), 0 facing the back, blended over |facing| < 0.5."""
    n = np.cross(np.subtract(p1, p0), np.subtract(p2, p0))
    length = float(np.linalg.norm(n))
    facing = -float(n[2]) / length if length > 0 else 0.0
    return float(np.clip(0.5 + facing, 0.0, 1.0))


def atlas_paths(key: str, aliases: list[str], atlas_dir: pathlib.Path) -> dict[str, pathlib.Path | None]:
    """Each atlas kind's file: the key's own, else the first alias's (an
    alias is the same mesh). Material 0 prefers flat_ over body_, as the engine."""
    def find(prefix: str) -> pathlib.Path | None:
        for k in [key, *aliases]:
            p = atlas_dir / f"{prefix}_{k}.png"
            if p.is_file():
                return p
        return None
    return {"body": find("flat") or find("body"), "ramp": find("ramp"), "other": find("other")}


def kind_of(prim_type: int, material: int, paths: dict) -> int:
    """The atlas the engine overlays on a primitive, or palette: only plain
    polygons take one; material 2 (transparent) never does."""
    if prim_type != PRIM_POLY or material == 2:
        return KIND_PALETTE
    kind = KIND_BODY if material == 0 else KIND_RAMP if 3 <= material <= 6 else KIND_OTHER if material == 1 else KIND_PALETTE
    return kind if kind == KIND_PALETTE or paths[KIND_NAMES[kind]] is not None else KIND_PALETTE


BRIDGE = -1  # a triangle spanning groups: kept as the original, since the engine stretches it


def triangle_groups(corner_groups: np.ndarray) -> np.ndarray:
    """(T, 3) corner groups -> (T,): the group all three corners share, else
    BRIDGE."""
    g = np.asarray(corner_groups).reshape(-1, 3)
    return np.where((g[:, 0] == g[:, 1]) & (g[:, 1] == g[:, 2]), g[:, 0], BRIDGE)


def srgb_to_linear(c: np.ndarray) -> np.ndarray:
    c = np.asarray(c, float)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def linear_to_srgb(c: np.ndarray) -> np.ndarray:
    c = np.clip(np.asarray(c, float), 0.0, None)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - 0.055)


def budget_level(triangles: int, target: int = TRIANGLE_TARGET) -> int:
    """The highest Catmull-Clark level whose predicted triangle count,
    T * 6 * 4^(L-1), stays within `target` (0 when even level 1 does not)."""
    level = 0
    while level < MAX_LEVEL and triangles * 6 * 4 ** level <= target:
        level += 1
    return level
