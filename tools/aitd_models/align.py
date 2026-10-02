# SPDX-License-Identifier: GPL-2.0-only
"""Fit a delivered mesh onto the original body's rest mesh with one
similarity transform (uniform scale, rotation, translation), in engine space.

1. Coarse: scale by the ratio of vertical extents, put the feet (max y, the
   engine is y down) and the XZ centre of the surface on the original's, then
   try every yaw in 15-degree steps and keep the lowest chamfer distance.
2. Refine: trimmed ICP (the best 90 % of nearest-point pairs) with Umeyama's
   closed-form rotation and offset (`rigid_fit`), until the step is negligible, then put
   the feet back on the original's. The scale stays the coarse one: the
   original's height is the authority, and letting ICP fit a scale shrinks a
   mesh that only resembles the original (trimming drops the pairs that lie
   outside it: 0.93 on Carnby with a 2 %-of-height deviation).

Distances are between area-weighted surface samples; nearest neighbours are
brute force in chunks (numpy only)."""
from __future__ import annotations

import math
from dataclasses import dataclass

import numpy as np

from .mesh import Surface, triangle_cross

COARSE_SAMPLES = 2000
TARGET_SAMPLES = 20000
YAW_STEP_DEG = 15
ICP_ITERATIONS = 60
ICP_KEEP = 0.9
NEAREST_CHUNK = 512  # points per block: a block's distance matrix is NEAREST_CHUNK x len(b)


@dataclass(frozen=True)
class Similarity:
    scale: float
    rotation: np.ndarray     # (3, 3)
    translation: np.ndarray  # (3,)

    def apply(self, points: np.ndarray) -> np.ndarray:
        return self.scale * (np.asarray(points, float) @ self.rotation.T) + self.translation

    def then(self, other: "Similarity") -> "Similarity":
        """`other` applied after `self`."""
        return Similarity(other.scale * self.scale, other.rotation @ self.rotation,
                          other.scale * (other.rotation @ self.translation) + other.translation)



def sample_surface(surface: Surface, n: int, seed: int = 0) -> np.ndarray:
    """(n, 3) points spread over the triangles in proportion to their area."""
    tri = surface.positions[surface.triangles]
    area = 0.5 * np.linalg.norm(triangle_cross(*surface), axis=1)
    if area.sum() <= 0:
        raise ValueError("mesh has no area")
    rng = np.random.default_rng(seed)
    pick = rng.choice(len(tri), size=n, p=area / area.sum())
    r1, r2 = rng.random(n), rng.random(n)
    flip = r1 + r2 > 1
    r1[flip], r2[flip] = 1 - r1[flip], 1 - r2[flip]
    t = tri[pick]
    return t[:, 0] + r1[:, None] * (t[:, 1] - t[:, 0]) + r2[:, None] * (t[:, 2] - t[:, 0])


def nearest(a: np.ndarray, b: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """For each point of `a`: distance to, and index of, its nearest point in `b`."""
    dist, index = np.empty(len(a)), np.empty(len(a), np.int64)
    bb = (b * b).sum(axis=1)
    for s in range(0, len(a), NEAREST_CHUNK):
        block = a[s:s + NEAREST_CHUNK]
        d2 = (block * block).sum(axis=1)[:, None] - 2 * block @ b.T + bb[None, :]
        k = d2.argmin(axis=1)
        index[s:s + NEAREST_CHUNK] = k
        dist[s:s + NEAREST_CHUNK] = np.sqrt(np.maximum(d2[np.arange(len(block)), k], 0.0))
    return dist, index


def chamfer(a: np.ndarray, b: np.ndarray) -> np.ndarray:
    """Both directions' nearest distances, concatenated."""
    return np.concatenate([nearest(a, b)[0], nearest(b, a)[0]])


def rigid_fit(src: np.ndarray, dst: np.ndarray) -> Similarity:
    """Least-squares rotation and offset mapping `src` onto `dst` (Umeyama
    1991 without the scale; the result's scale is 1)."""
    mu_s, mu_d = src.mean(axis=0), dst.mean(axis=0)
    u, _sig, vt = np.linalg.svd((dst - mu_d).T @ (src - mu_s) / len(src))
    d = np.diag([1.0, 1.0, np.sign(np.linalg.det(u @ vt)) or 1.0])
    rotation = u @ d @ vt
    return Similarity(1.0, rotation, mu_d - rotation @ mu_s)


def yaw(deg: float) -> np.ndarray:
    """Rotation about the engine's vertical (y) axis."""
    t = math.radians(deg)
    c, s = math.cos(t), math.sin(t)
    return np.array([[c, 0.0, -s], [0.0, 1.0, 0.0], [s, 0.0, c]])


def coarse(src: np.ndarray, dst: np.ndarray) -> Similarity:
    """Scale by vertical extent, match feet and XZ centre, best of 24 yaws."""
    scale = float(np.ptp(dst[:, 1]) / np.ptp(src[:, 1]))
    best = None
    for deg in range(0, 360, YAW_STEP_DEG):
        r = yaw(deg)
        moved = scale * src @ r.T
        t = np.array([dst[:, 0].mean() - moved[:, 0].mean(), dst[:, 1].max() - moved[:, 1].max(),
                      dst[:, 2].mean() - moved[:, 2].mean()])
        score = chamfer(moved + t, dst).mean()
        if best is None or score < best[0]:
            best = (score, Similarity(scale, r, t))
    return best[1]


def icp(src: np.ndarray, dst: np.ndarray, start: Similarity) -> Similarity:
    """Trimmed rigid ICP from `start` (its scale is kept): each step fits the
    best ICP_KEEP of the pairs; then the feet (max y) go back onto `dst`'s."""
    current = start
    keep = max(3, int(ICP_KEEP * len(src)))
    for _ in range(ICP_ITERATIONS):
        moved = current.apply(src)
        dist, index = nearest(moved, dst)
        best = np.argsort(dist)[:keep]
        step = rigid_fit(moved[best], dst[index[best]])
        current = current.then(step)
        if np.abs(step.rotation - np.eye(3)).max() < 1e-7 and np.abs(step.translation).max() < 1e-4:
            break
    feet = dst[:, 1].max() - current.apply(src)[:, 1].max()
    return Similarity(current.scale, current.rotation, current.translation + (0.0, feet, 0.0))


def align(source: Surface, target: Surface) -> Similarity:
    """The similarity that puts the delivered mesh on the target (engine space)."""
    src = sample_surface(source, COARSE_SAMPLES, seed=1)
    dst = sample_surface(target, TARGET_SAMPLES, seed=2)
    return icp(src, dst, coarse(src, dst[::10]))
