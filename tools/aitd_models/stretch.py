# SPDX-License-Identifier: GPL-2.0-only
"""Posed stretch: how far a bound mesh's triangles stretch when the body
plays its preview animations.

The poses come from the export's original.glb alone (its joint nodes'
keyed transforms, with the gNN_geo zoom nodes), so this needs no game data.
A delivery whose limbs are fused -- a bridge of triangles between two legs,
or an arm and the torso -- tears as soon as the parts move apart. The
originals themselves stretch where a polygon spans two groups (LISTBODY_011:
8.6 % of the area beyond 1.5x), but never beyond 9.2x on any of the 42
characters, and their identity imports never beyond 9.1x; Carnby with his
legs fused into one hull tears 0.88 % of his area beyond 10x (measured
2026-10-02). Hence a high ratio and a small area."""
from __future__ import annotations

import numpy as np

from .gltf import Glb, trs_matrix
from .original import FLIP, METRES_PER_UNIT

STRETCH_RATIO = 10.0     # a triangle edge longer than this many times its rest length is torn
STRETCH_AREA_PCT = 0.1   # % of the surface area allowed to tear

# engine -> glTF as a 4x4, and back
_TO_GLTF = np.diag([*(FLIP.diagonal() * METRES_PER_UNIT), 1.0])
_TO_ENGINE = np.linalg.inv(_TO_GLTF)


def _group_slots(glb: Glb, groups: int) -> list[int]:
    """The skin joint each group's own vertices are bound to: gNN_geo when
    the group zooms, else gNN."""
    skin = glb.doc["skins"][0]
    names = [glb.doc["nodes"][j].get("name", "") for j in skin["joints"]]
    slots = []
    for gi in range(groups):
        geo, joint = f"g{gi:02d}_geo", f"g{gi:02d}"
        slots.append(names.index(geo) if geo in names else names.index(joint))
    return slots


def _skin_matrices(glb: Glb, nodes: list[dict], slots: list[int]) -> np.ndarray:
    doc = glb.doc
    world: dict[int, np.ndarray] = {}

    def walk(i, parent):
        world[i] = parent @ trs_matrix(nodes[i])
        for c in nodes[i].get("children", []):
            walk(c, world[i])

    for root in doc["scenes"][doc.get("scene", 0)]["nodes"]:
        walk(root, np.eye(4))
    skin = doc["skins"][0]
    ibm = glb.accessor(skin["inverseBindMatrices"]).reshape(-1, 4, 4).transpose(0, 2, 1)
    return np.stack([_TO_ENGINE @ world[skin["joints"][s]] @ ibm[s] @ _TO_GLTF for s in slots])


def preview_skins(glb: Glb, groups: int) -> list[np.ndarray]:
    """One (groups, 4, 4) array per key of every animation in original.glb:
    the engine-space matrix that takes a rest-pose vertex of each group to
    its posed place. Empty when the file has no animation."""
    slots = _group_slots(glb, groups)
    out = []
    for anim in glb.doc.get("animations", []):
        keys = len(glb.accessor(anim["samplers"][0]["input"]))
        values = [glb.accessor(anim["samplers"][ch["sampler"]]["output"]) for ch in anim["channels"]]
        for k in range(keys):
            nodes = [dict(n) for n in glb.doc["nodes"]]
            for ch, vals in zip(anim["channels"], values):
                nodes[ch["target"]["node"]][ch["target"]["path"]] = [float(v) for v in vals[k]]
            out.append(_skin_matrices(glb, nodes, slots))
    return out


def _edges(positions: np.ndarray, triangles: np.ndarray) -> np.ndarray:
    p = positions[triangles]                               # (T, 3, 3)
    return np.linalg.norm(p - np.roll(p, -1, axis=1), axis=2)  # (T, 3): ab, bc, ca


def posed_stretch(positions: np.ndarray, triangles: np.ndarray, joints: np.ndarray, weights: np.ndarray,
                  skins: list[np.ndarray]) -> tuple[np.ndarray, np.ndarray]:
    """(per-triangle worst edge ratio over every pose, the triangles' rest
    areas). `joints` (V, 4) and `weights` (V, 4, summing to 1) are the
    binding the engine skins with: `joints` holds engine group indices, which
    index `skins` (one (groups, 4, 4) matrix per pose) -- not glTF skin-joint
    slots, which differ for the groups that zoom (their gNN_geo joint is
    appended after the plain ones)."""
    positions = np.asarray(positions, float)
    rest = _edges(positions, triangles)
    usable = rest > 1e-6 * max(float(np.ptp(positions, axis=0).max()), 1.0)
    worst = np.ones(len(triangles))
    homog = np.c_[positions, np.ones(len(positions))]
    for m in skins:
        posed = np.zeros_like(positions)
        for slot in range(joints.shape[1]):
            moved = np.einsum("vij,vj->vi", m[joints[:, slot]], homog)[:, :3]
            posed += weights[:, slot, None] * moved
        ratio = np.where(usable, _edges(posed, triangles) / np.where(usable, rest, 1.0), 1.0)
        worst = np.maximum(worst, ratio.max(axis=1))
    a = positions[triangles]
    area = 0.5 * np.linalg.norm(np.cross(a[:, 1] - a[:, 0], a[:, 2] - a[:, 0]), axis=1)
    return worst, area


def torn_pct(worst: np.ndarray, area: np.ndarray, ratio: float = STRETCH_RATIO) -> float:
    """% of the surface area whose worst edge ratio exceeds `ratio`."""
    total = float(area.sum())
    return 100.0 * float(area[worst > ratio].sum()) / total if total > 0 else 0.0
