# SPDX-License-Identifier: GPL-2.0-only
"""Pose an AITD1 animated body.

`pose_int` is a line-for-line port of the engine's integer path: AnimNuage,
InitGroupeRot, RotateList, RotateGroupe, TranslateGroupe and ZoomGroupe in
TatouSource/FitdLib/renderer.cpp:234-462 (the non-INFO_OPTIMISE branch).
`pose_exact` runs the very same loops without the integer truncation.
`pose_float` gives one float matrix per group, which is what a GPU skins
with; it equals `pose_exact` to rounding error. The integer path drifts from
both by about 9 units per level of group depth (up to 87 units measured on
real data at depth 10): each rotation floors to an even value, and every
pivot carries its parent's error.

A group state is (type, (dx, dy, dz)): type 0 rotates (10-bit angles, 0x400
= 360 degrees), 1 translates, 2 zooms by (d + 256) / 256, anything else is a
no-op, as in the engine's switch. Group 0's delta is always replaced by the
actor's own angles."""
from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from .body import Body
from .cos_table import COS_TABLE
from .skeleton import children

State = tuple[int, tuple[int, int, int]]
ROTATE, TRANSLATE, ZOOM = 0, 1, 2


def rest_states(body: Body) -> list[State]:
    """Every group at rest: all 214 AITD1 bodies store zero rest deltas."""
    return [(ROTATE, (0, 0, 0)) for _ in body.groups]


def _with_angles(body: Body, states, angles) -> list[State]:
    states = [(int(t), tuple(int(c) for c in d)) for t, d in states]
    if len(states) != len(body.groups):
        raise ValueError(f"{len(states)} states for {len(body.groups)} groups")
    states[0] = (states[0][0], tuple(int(a) for a in angles))
    return states


# ── the engine's vertex loops ────────────────────────────────────────────────

def _rot(a, b, cos_, sin_, exact):
    """One RotateList axis step: (a*S - b*C, a*C + b*S), where C is the table
    at the angle (a sine) and S the table at angle + 0x100 (a cosine)."""
    if exact:
        return (a * sin_ - b * cos_) / 32768.0, (a * cos_ + b * sin_) / 32768.0
    return ((a * sin_ - b * cos_) >> 16) << 1, ((a * cos_ + b * sin_) >> 16) << 1


def _rotate_list(pts, start, count, d, exact):
    ax, ay, az = d
    for i in range(start, start + count):
        x, y, z = pts[i]
        if ay:
            x, z = _rot(x, z, COS_TABLE[ay & 0x3FF], COS_TABLE[(ay + 0x100) & 0x3FF], exact)
        if ax:
            y, z = _rot(y, z, COS_TABLE[ax & 0x3FF], COS_TABLE[(ax + 0x100) & 0x3FF], exact)
        if az:
            x, y = _rot(x, y, COS_TABLE[az & 0x3FF], COS_TABLE[(az + 0x100) & 0x3FF], exact)
        pts[i] = [x, y, z]


def _rotate_groupe(body: Body, pts, pos: int, d, exact) -> None:
    """RotateGroupe: rotate the group at array position `pos`, then scan the
    next (G - m_numGroup) entries, starting with itself, recursing into every
    entry whose parent is this group's number."""
    g = body.groups[pos]
    _rotate_list(pts, g.start, g.count, d, exact)
    for j in range(pos, pos + len(body.groups) - g.self_id):
        if body.groups[j].parent == g.self_id:
            _rotate_groupe(body, pts, j, d, exact)


def _zoom(v, factor, exact):
    if exact:
        return v * (factor + 256) / 256.0
    q = abs(v * (factor + 256)) // 256  # C division truncates toward zero
    return q if v * (factor + 256) >= 0 else -q


def _pose_vertices(body: Body, states, angles, exact: bool) -> list[list]:
    states = _with_angles(body, states, angles)
    pts = [list(v) for v in body.vertices]
    for gi in body.order:
        g = body.groups[gi]
        t, (dx, dy, dz) = states[gi]
        if not (dx or dy or dz):
            continue
        if t == ROTATE:
            _rotate_groupe(body, pts, gi, (dx, dy, dz), exact)
        elif t == TRANSLATE:
            for i in range(g.start, g.start + g.count):
                pts[i] = [pts[i][0] + dx, pts[i][1] + dy, pts[i][2] + dz]
        elif t == ZOOM:
            for i in range(g.start, g.start + g.count):
                x, y, z = pts[i]
                pts[i] = [_zoom(x, dx, exact), _zoom(y, dy, exact), _zoom(z, dz, exact)]
    for g in body.groups:
        for i in range(g.start, g.start + g.count):
            base = pts[g.pivot]  # read per vertex, as the engine does
            pts[i] = [pts[i][0] + base[0], pts[i][1] + base[1], pts[i][2] + base[2]]
    return pts


def pose_int(body: Body, states, angles=(0, 0, 0)) -> list[list[int]]:
    """Model-space vertices exactly as AnimNuage leaves them in pointBuffer."""
    return _pose_vertices(body, states, angles, exact=False)


def pose_exact(body: Body, states, angles=(0, 0, 0)) -> np.ndarray:
    """(N, 3): the engine's loops in real arithmetic (no truncation)."""
    return np.array(_pose_vertices(body, states, angles, exact=True), dtype=float)


# ── float matrices ───────────────────────────────────────────────────────────

def _sin_cos(angle: int) -> tuple[float, float]:
    return COS_TABLE[angle & 0x3FF] / 32768.0, COS_TABLE[(angle + 0x100) & 0x3FF] / 32768.0


def rotation(d) -> np.ndarray:
    """3x3 matrix of RotateList: Y, then X, then Z, so R = Rz @ Rx @ Ry. An
    axis is skipped only when its raw angle is 0 (1024 reads table[0] = 4)."""
    ax, ay, az = d
    r = np.eye(3)
    if ay:
        s, c = _sin_cos(ay)
        r = np.array([[c, 0, -s], [0, 1, 0], [s, 0, c]]) @ r
    if ax:
        s, c = _sin_cos(ax)
        r = np.array([[1, 0, 0], [0, c, -s], [0, s, c]]) @ r
    if az:
        s, c = _sin_cos(az)
        r = np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]]) @ r
    return r


def _affine(linear=None, translation=None) -> np.ndarray:
    m = np.eye(4)
    if linear is not None:
        m[:3, :3] = linear
    if translation is not None:
        m[:3, 3] = translation
    return m


@dataclass
class Pose:
    joints: np.ndarray  # (G, 4, 4) rigid frame of each group, model space
    zoom: np.ndarray    # (G, 3) scale of the group's own vertices (1 when none)

    def group_matrices(self) -> np.ndarray:
        """(G, 4, 4): model-from-local matrix for each group's own rest
        vertices: the joint frame times the group's own zoom."""
        return np.stack([j @ np.diag([*z, 1.0]) for j, z in zip(self.joints, self.zoom)])


def pose_float(body: Body, states, angles=(0, 0, 0)) -> Pose:
    """Requires skeleton.validate(body) == []: a group is processed before
    its ancestors, so its own operation is always the innermost one.

    Raises ValueError when group 0 translates: the engine's pivot pass then
    adds root vertex 0 to itself, which no matrix reproduces (and no AITD1
    animation does; group 0 always rotates)."""
    states = _with_angles(body, states, angles)
    if states[0][0] == TRANSLATE and any(states[0][1]):
        raise ValueError("group 0 translates; the engine's result is not an affine pose")
    n = len(body.groups)
    kids = children(body)

    def subtree(gi: int) -> list[int]:
        out = [gi]
        for c in kids[gi]:
            out += subtree(c)
        return out

    local = np.stack([np.eye(4)] * n)
    zoom = np.ones((n, 3))
    for gi in body.order:
        t, d = states[gi]
        if not any(d):
            continue
        if t == ROTATE:
            r = _affine(rotation(d))
            for x in subtree(gi):
                local[x] = r @ local[x]
        elif t == TRANSLATE:
            local[gi] = _affine(translation=d) @ local[gi]
        elif t == ZOOM:
            zoom[gi] = [(c + 256) / 256.0 for c in d]
    joints = np.zeros((n, 4, 4))
    model = np.zeros((n, 4, 4))
    for gi, g in enumerate(body.groups):
        pivot = np.zeros(3) if gi == 0 else (model[g.parent] @ np.array([*body.vertices[g.pivot], 1.0]))[:3]
        joints[gi] = _affine(translation=pivot) @ local[gi]
        model[gi] = joints[gi] @ np.diag([*zoom[gi], 1.0])
    return Pose(joints, zoom)


def skin(body: Body, matrices: np.ndarray) -> np.ndarray:
    """(N, 3): every rest vertex through its own group's matrix."""
    verts = np.array(body.vertices, dtype=float)
    out = np.zeros_like(verts)
    for gi, g in enumerate(body.groups):
        m = matrices[gi]
        rows = slice(g.start, g.start + g.count)
        out[rows] = verts[rows] @ m[:3, :3].T + m[:3, 3]
    return out
