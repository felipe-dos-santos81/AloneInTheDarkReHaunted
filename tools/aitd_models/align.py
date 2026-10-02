# SPDX-License-Identifier: GPL-2.0-only
"""Fit a delivered mesh onto the original body's rest mesh with one
similarity transform (uniform scale, rotation, translation), in engine space.

1. Coarse: scale by the ratio of vertical extents, put the feet (max y, the
   engine is y down) and the XZ centre of the surface on the original's, then
   try every yaw in 15-degree steps and keep the lowest chamfer distance.
2. Refine: trimmed ICP (the best 90 % of nearest-point pairs) with Umeyama's
   closed-form similarity, until the step is negligible. The scale may move
   at most ICP_SCALE_LIMIT from the coarse one: a shape that does not fit
   would otherwise shrink towards a point.

Distances are between area-weighted surface samples; nearest neighbours are
brute force in chunks (numpy only)."""
from __future__ import annotations

import math
from dataclasses import dataclass

import numpy as np

COARSE_SAMPLES = 2000
TARGET_SAMPLES = 20000
YAW_STEP_DEG = 15
ICP_ITERATIONS = 60
ICP_KEEP = 0.9
ICP_SCALE_LIMIT = 0.25
CHUNK = 512


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


IDENTITY = Similarity(1.0, np.eye(3), np.zeros(3))


def sample_surface(positions: np.ndarray, triangles: np.ndarray, n: int, seed: int = 0) -> np.ndarray:
    """(n, 3) points spread over the triangles in proportion to their area."""
    tri = positions[triangles]
    area = 0.5 * np.linalg.norm(np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0]), axis=1)
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
    for s in range(0, len(a), CHUNK):
        block = a[s:s + CHUNK]
        d2 = (block * block).sum(axis=1)[:, None] - 2 * block @ b.T + bb[None, :]
        k = d2.argmin(axis=1)
        index[s:s + CHUNK] = k
        dist[s:s + CHUNK] = np.sqrt(np.maximum(d2[np.arange(len(block)), k], 0.0))
    return dist, index


def chamfer(a: np.ndarray, b: np.ndarray) -> np.ndarray:
    """Both directions' nearest distances, concatenated."""
    return np.concatenate([nearest(a, b)[0], nearest(b, a)[0]])


def umeyama(src: np.ndarray, dst: np.ndarray) -> Similarity:
    """Least-squares similarity mapping `src` onto `dst` (Umeyama 1991)."""
    mu_s, mu_d = src.mean(axis=0), dst.mean(axis=0)
    xs, xd = src - mu_s, dst - mu_d
    u, sig, vt = np.linalg.svd(xd.T @ xs / len(src))
    d = np.diag([1.0, 1.0, np.sign(np.linalg.det(u @ vt)) or 1.0])
    rotation = u @ d @ vt
    var = (xs * xs).sum() / len(src)
    scale = float(np.trace(np.diag(sig) @ d) / var)
    return Similarity(scale, rotation, mu_d - scale * rotation @ mu_s)


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
    """Trimmed ICP from `start`: each step fits the best ICP_KEEP of the pairs."""
    current = start
    keep = max(3, int(ICP_KEEP * len(src)))
    for _ in range(ICP_ITERATIONS):
        moved = current.apply(src)
        dist, index = nearest(moved, dst)
        best = np.argsort(dist)[:keep]
        step = umeyama(moved[best], dst[index[best]])
        if abs(current.scale * step.scale / start.scale - 1) > ICP_SCALE_LIMIT:
            break
        current = current.then(step)
        if abs(step.scale - 1) < 1e-7 and np.abs(step.rotation - np.eye(3)).max() < 1e-7 \
                and np.abs(step.translation).max() < 1e-4:
            break
    return current


def align(positions: np.ndarray, triangles: np.ndarray,
          target_positions: np.ndarray, target_triangles: np.ndarray) -> Similarity:
    """The similarity that puts the delivered mesh on the target (engine space)."""
    src = sample_surface(positions, triangles, COARSE_SAMPLES, seed=1)
    dst = sample_surface(target_positions, target_triangles, TARGET_SAMPLES, seed=2)
    return icp(src, dst, coarse(src, dst[::10]))
