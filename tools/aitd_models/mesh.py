# SPDX-License-Identifier: GPL-2.0-only
"""Turn a posed AITD1 body into a flat-shaded triangle soup.

Every face gets its own three vertices (no sharing), so each triangle keeps
its palette colour and a flat normal, like the engine draws it. Polygons are
fanned from their first point; spheres become icospheres of the primitive's
radius; lines become thin square prisms; points, big points and zixels are
dropped (they are pixel-sized details the reference renders cannot show)."""
from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from .body import POINT_LIKE, PRIM_LINE, PRIM_POLY, PRIM_POLY_TEX, PRIM_SPHERE, Body
from .skeleton import owners

LINE_HALF_WIDTH = 3.0  # engine units: a 6-unit square prism
SPHERE_SUBDIVISIONS = 2


@dataclass
class Mesh:
    positions: np.ndarray  # (3T, 3) float64, engine space
    colors: np.ndarray     # (3T, 3) float32 in 0..1
    groups: np.ndarray     # (3T,) int: group of the body vertex each corner comes from
    prim_index: np.ndarray # (T,) int: source primitive of each triangle

    @property
    def triangle_count(self) -> int:
        return len(self.prim_index)


def icosphere(subdivisions: int = SPHERE_SUBDIVISIONS) -> tuple[np.ndarray, np.ndarray]:
    """Unit icosphere: (V, 3) vertices and (F, 3) int faces, outward CCW."""
    t = (1 + 5 ** 0.5) / 2
    verts = [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t),
             (0, -1, -t), (0, 1, -t), (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]
    faces = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4),
             (11, 10, 2), (10, 7, 6), (7, 1, 8), (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8),
             (3, 8, 9), (4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]
    verts = [np.array(v, float) / np.linalg.norm(v) for v in verts]
    for _ in range(subdivisions):
        cache: dict[tuple[int, int], int] = {}

        def mid(a: int, b: int) -> int:
            key = (min(a, b), max(a, b))
            if key not in cache:
                m = verts[a] + verts[b]
                verts.append(m / np.linalg.norm(m))
                cache[key] = len(verts) - 1
            return cache[key]

        new = []
        for a, b, c in faces:
            ab, bc, ca = mid(a, b), mid(b, c), mid(c, a)
            new += [(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)]
        faces = new
    return np.array(verts), np.array(faces, dtype=np.int64)


def _prism(a: np.ndarray, b: np.ndarray, ga: int, gb: int) -> list[tuple[np.ndarray, list[int]]]:
    axis = b - a
    length = np.linalg.norm(axis)
    if length < 1e-6:
        return []
    axis /= length
    helper = np.array([0.0, 1.0, 0.0]) if abs(axis[1]) < 0.9 else np.array([1.0, 0.0, 0.0])
    u = np.cross(axis, helper)
    u *= LINE_HALF_WIDTH / np.linalg.norm(u)
    w = np.cross(axis, u)
    w *= LINE_HALF_WIDTH / np.linalg.norm(w)
    ring_a = [a + u + w, a - u + w, a - u - w, a + u - w]
    ring_b = [p + (b - a) for p in ring_a]
    tris = []
    for i in range(4):
        j = (i + 1) % 4
        tris.append((np.array([ring_a[i], ring_a[j], ring_b[j]]), [ga, ga, gb]))
        tris.append((np.array([ring_a[i], ring_b[j], ring_b[i]]), [ga, gb, gb]))
    return tris


def build_mesh(body: Body, posed: np.ndarray, palette: np.ndarray) -> Mesh:
    """`posed` is (N, 3) model-space vertices (e.g. pose.skin at rest);
    `palette` is (256, 3) uint8."""
    owner = owners(body)
    unit_v, unit_f = icosphere()
    tris: list[np.ndarray] = []
    cols: list[np.ndarray] = []
    grps: list[int] = []
    prims: list[int] = []
    for pi, prim in enumerate(body.primitives):
        color = palette[prim.color].astype(np.float32) / 255.0
        if prim.type == PRIM_POLY or prim.type in PRIM_POLY_TEX:
            ids = prim.points
            new = [(np.array([posed[ids[0]], posed[ids[k]], posed[ids[k + 1]]]),
                    [owner[ids[0]], owner[ids[k]], owner[ids[k + 1]]]) for k in range(1, len(ids) - 1)]
        elif prim.type == PRIM_SPHERE:
            centre, g = posed[prim.points[0]], owner[prim.points[0]]
            new = [(centre + unit_v[f] * prim.size, [g, g, g]) for f in unit_f]
        elif prim.type == PRIM_LINE:
            a, b = prim.points
            new = _prism(posed[a], posed[b], owner[a], owner[b])
        elif prim.type in POINT_LIKE:
            new = []
        else:
            raise ValueError(f"unexpected primitive type {prim.type}")
        for tri, tri_groups in new:
            tris.append(tri)
            cols.append(np.repeat(color[None], 3, axis=0))
            grps.extend(tri_groups)
            prims.append(pi)
    if not tris:
        return Mesh(np.zeros((0, 3)), np.zeros((0, 3), np.float32), np.zeros(0, int), np.zeros(0, int))
    return Mesh(np.concatenate(tris), np.concatenate(cols),
                np.array(grps), np.array(prims))


def face_normals(positions: np.ndarray) -> np.ndarray:
    """(T, 3) unit normals of consecutive triangles (zero for degenerate)."""
    tri = positions.reshape(-1, 3, 3)
    n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    length = np.linalg.norm(n, axis=1, keepdims=True)
    return np.divide(n, length, out=np.zeros_like(n), where=length > 1e-12)
