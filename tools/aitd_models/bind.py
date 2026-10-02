# SPDX-License-Identifier: GPL-2.0-only
"""Skin weights for an aligned delivery, derived from the original body.

1. Nearest surface: each delivered vertex takes the closest point on the
   original rest mesh; that point's barycentric coordinates, given to the
   groups of the triangle's corners, are its weights. A triangle spanning two
   groups blends them, exactly where the original stretched.
2. Smoothing: SMOOTH_ITERATIONS rounds of averaging with mesh neighbours
   (vertices welded by position), each neighbour weighted by
   exp(-(edge length / sigma)^2) with sigma = SMOOTH_SIGMA of the body's
   size (mesh.size: its height, for a standing character), so a dense generated mesh blends over a few centimetres and a
   coarse one (whose edges span whole limbs) barely at all. Each vertex keeps
   only its own group, that group's parent and its children.
3. Pack: the four largest weights, quantised to u8 summing exactly to 255.

The original's polygon winding is inconsistent (LISTBODY_011: 74 triangles
face out, 264 in), so its normals cannot guard against an arm vertex binding
to the torso. Instead each vertex reports whether an unrelated part's
surface is nearly as close (`Binding.ambiguous`), and import warns when many
are. Island cleanup is left out: on the low-poly originals it relabelled
whole limb segments (LISTBODY_024: 71 vertices wrong instead of 32)."""
from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from .mesh import size

SMOOTH_ITERATIONS = 5
SMOOTH_SIGMA = 0.01
AMBIGUOUS_MARGIN = 0.005
WELD_UNITS = 1e-3
CLOSEST_CHUNK = 256  # points per block: closest_on_triangles builds CLOSEST_CHUNK x T x 3 arrays


@dataclass
class Binding:
    weights: np.ndarray   # (V, G) float64, rows sum to 1
    ambiguous: np.ndarray # (V,) bool: an unrelated part's surface is nearly as close
    joints: np.ndarray    # (V, 4) uint8, packed
    packed: np.ndarray    # (V, 4) uint8, sum 255 per row

    @property
    def labels(self) -> np.ndarray:
        return self.weights.argmax(axis=1)


def closest_on_triangles(p: np.ndarray, a: np.ndarray, b: np.ndarray, c: np.ndarray):
    """Closest point of every triangle (a, b, c: (T, 3)) to every point p (P, 3):
    distances (P, T) and barycentrics (P, T, 3). Ericson, Real-Time Collision
    Detection, 5.1.5, vectorised over both axes."""
    p = p[:, None, :]
    ab, ac = (b - a)[None], (c - a)[None]
    ap, bp, cp = p - a[None], p - b[None], p - c[None]
    d1, d2 = (ab * ap).sum(-1), (ac * ap).sum(-1)
    d3, d4 = (ab * bp).sum(-1), (ac * bp).sum(-1)
    d5, d6 = (ab * cp).sum(-1), (ac * cp).sum(-1)
    va, vb, vc = d3 * d6 - d5 * d4, d5 * d2 - d1 * d6, d1 * d4 - d3 * d2
    shape = d1.shape
    bary = np.zeros(shape + (3,))
    done = np.zeros(shape, bool)

    def assign_where(mask, u, v, w):
        nonlocal done
        mask = mask & ~done
        bary[mask] = np.stack([u, v, w], axis=-1)[mask]
        done |= mask

    one, zero = np.ones(shape), np.zeros(shape)
    with np.errstate(divide="ignore", invalid="ignore"):
        assign_where((d1 <= 0) & (d2 <= 0), one, zero, zero)
        assign_where((d3 >= 0) & (d4 <= d3), zero, one, zero)
        assign_where((d6 >= 0) & (d5 <= d6), zero, zero, one)
        t = d1 / (d1 - d3)
        assign_where((vc <= 0) & (d1 >= 0) & (d3 <= 0), 1 - t, t, zero)
        t = d2 / (d2 - d6)
        assign_where((vb <= 0) & (d2 >= 0) & (d6 <= 0), 1 - t, zero, t)
        t = (d4 - d3) / ((d4 - d3) + (d5 - d6))
        assign_where((va <= 0) & ((d4 - d3) >= 0) & ((d5 - d6) >= 0), zero, 1 - t, t)
        denom = va + vb + vc
        v, w = vb / denom, vc / denom
        assign_where(np.ones(shape, bool), 1 - v - w, v, w)
    bary = np.nan_to_num(bary)  # degenerate triangles: any corner
    bary[~np.isfinite(bary).all(-1) | (bary.sum(-1) == 0)] = (1.0, 0.0, 0.0)
    q = bary[..., 0:1] * a[None] + bary[..., 1:2] * b[None] + bary[..., 2:3] * c[None]
    return np.linalg.norm(q - p, axis=-1), bary


def surface_weights(points: np.ndarray, tri_positions: np.ndarray, tri_groups: np.ndarray,
                    related: np.ndarray, margin: float) -> tuple[np.ndarray, np.ndarray]:
    """(P, G) barycentric weights of each point's closest original surface
    point, and (P,) whether a triangle of an unrelated group (no corner group
    is the chosen triangle's, a parent or a child of one) lies within 1.5x the
    best distance + `margin`: such a vertex could belong to either part.
    `tri_positions` is (T, 3, 3), `tri_groups` (T, 3), `related` (G, G) bool."""
    a, b, c = tri_positions[:, 0], tri_positions[:, 1], tri_positions[:, 2]
    out = np.zeros((len(points), len(related)))
    ambiguous = np.zeros(len(points), bool)
    member = np.zeros((len(tri_groups), len(related)), bool)
    member[np.arange(len(tri_groups))[:, None], tri_groups] = True
    near = (member.astype(int) @ related.astype(int)) > 0  # (T, G): triangle touches g or a relative of g
    for s in range(0, len(points), CLOSEST_CHUNK):
        dist, bary = closest_on_triangles(points[s:s + CLOSEST_CHUNK], a, b, c)
        best = dist.argmin(axis=1)
        rows = np.arange(len(best))
        for k in range(3):
            np.add.at(out, (s + rows, tri_groups[best, k]), bary[rows, best, k])
        unrelated = ~(member[best].astype(int) @ near.T.astype(int) > 0)  # (P, T)
        limit = 1.5 * dist[rows, best] + margin
        ambiguous[s:s + CLOSEST_CHUNK] = (unrelated & (dist <= limit[:, None])).any(axis=1)
    return out, ambiguous


def weld(positions: np.ndarray) -> np.ndarray:
    """(V,) id of each vertex's position class (vertices within WELD_UNITS share one)."""
    _, ids = np.unique(np.round(positions / WELD_UNITS).astype(np.int64), axis=0, return_inverse=True)
    return ids.reshape(-1)


def edges(triangles: np.ndarray, ids: np.ndarray) -> np.ndarray:
    """(E, 2) unique undirected edges between welded vertex ids."""
    t = ids[triangles]
    e = np.concatenate([t[:, [0, 1]], t[:, [1, 2]], t[:, [2, 0]]])
    e = np.sort(e[e[:, 0] != e[:, 1]], axis=1)
    return np.unique(e, axis=0)


def allowed_groups(parents: list[int]) -> np.ndarray:
    """(G, G) bool: group g may keep weight on h when h is g, its parent or a child."""
    n = len(parents)
    ok = np.eye(n, dtype=bool)
    for g, p in enumerate(parents):
        if p >= 0:
            ok[g, p] = ok[p, g] = True
    return ok


def smooth(w: np.ndarray, e: np.ndarray, length: np.ndarray, sigma: float, allowed: np.ndarray,
           iterations: int = SMOOTH_ITERATIONS) -> np.ndarray:
    """`w` is per welded id (N, G); `length` (E,) is each edge's length."""
    if not len(e):
        return w
    k = np.exp(-(length / sigma) ** 2)[:, None]
    weight = np.bincount(e.ravel(), weights=np.concatenate([k[:, 0], k[:, 0]]), minlength=len(w))[:, None]
    for _ in range(iterations):
        labels = w.argmax(axis=1)
        total = np.zeros_like(w)
        np.add.at(total, e[:, 0], k * w[e[:, 1]])
        np.add.at(total, e[:, 1], k * w[e[:, 0]])
        mix = weight / (1.0 + weight)  # a lone vertex keeps its weights
        w = ((1 - mix) * w + np.divide(total, 1.0 + weight)) * allowed[labels]
        w /= w.sum(axis=1, keepdims=True)
    return w


def pack(w: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Top four weights as (joints, u8 weights summing to 255), largest first."""
    if w.shape[1] < 4:  # a body with fewer than four groups
        w = np.pad(w, ((0, 0), (0, 4 - w.shape[1])))
    top = np.argsort(-w, axis=1, kind="stable")[:, :4]
    vals = np.take_along_axis(w, top, axis=1)
    vals = vals / vals.sum(axis=1, keepdims=True) * 255
    q = np.floor(vals).astype(np.int64)
    short = 255 - q.sum(axis=1)
    order = np.argsort(-(vals - q), axis=1, kind="stable")  # largest remainders get the rest
    for k in range(4):
        q[np.arange(len(q)), order[:, k]] += (short > k)
    joints = np.where(q > 0, top, 0)
    return joints.astype(np.uint8), q.astype(np.uint8)


def bind(positions: np.ndarray, triangles: np.ndarray, tri_positions: np.ndarray,
         tri_groups: np.ndarray, parents: list[int]) -> Binding:
    """`positions` (V, 3) aligned delivery, engine space; `tri_positions`
    (T, 3, 3) and `tri_groups` (T, 3) the original rest mesh; `parents` per group."""
    ids = weld(positions)
    first = np.unique(ids, return_index=True)[1]
    related = allowed_groups(parents)
    body_size = size(tri_positions)
    w, ambiguous = surface_weights(positions[first], tri_positions, tri_groups, related, AMBIGUOUS_MARGIN * body_size)
    e = edges(triangles, ids)
    welded = positions[first]
    w = smooth(w, e, np.linalg.norm(welded[e[:, 0]] - welded[e[:, 1]], axis=1), SMOOTH_SIGMA * body_size, related)
    w = w[ids]
    joints, packed = pack(w)
    return Binding(w, ambiguous[ids], joints, packed)
