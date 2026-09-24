# SPDX-License-Identifier: GPL-2.0-only
"""Parse AITD1 bodies (LISTBODY/LISTBOD2) and animations (LISTANIM/LISTANI2)
exactly as the engine does: createBodyFromPtr and createAnimationFromPtr in
TatouSource/FitdLib/hqr.cpp. Only the AITD1 layout (no INFO_OPTIMISE)."""
from __future__ import annotations

import struct
from dataclasses import dataclass, field

INFO_ANIM = 0x2
INFO_OPTIMISE = 0x8

PRIM_LINE, PRIM_POLY, PRIM_POINT, PRIM_SPHERE = 0, 1, 2, 3
PRIM_BIG_POINT, PRIM_ZIXEL = 6, 7
PRIM_POLY_TEX = (8, 9, 10)
POINT_LIKE = (PRIM_POINT, PRIM_BIG_POINT, PRIM_ZIXEL)


class BodyError(ValueError):
    pass


@dataclass(frozen=True)
class Group:
    start: int      # first vertex index
    count: int      # number of vertices
    pivot: int      # vertex index (in the parent group) this group hangs from
    parent: int     # m_orgGroup, -1 for the root
    self_id: int    # m_numGroup
    state_type: int # rest m_state.m_type
    delta: tuple[int, int, int]  # rest m_state.m_delta


@dataclass(frozen=True)
class Primitive:
    type: int
    material: int
    color: int
    points: tuple[int, ...]
    size: int = 0   # sphere radius


@dataclass(frozen=True)
class Body:
    flags: int
    zv: tuple[int, int, int, int, int, int]  # X1 X2 Y1 Y2 Z1 Z2
    vertices: tuple[tuple[int, int, int], ...]
    groups: tuple[Group, ...]
    order: tuple[int, ...]  # m_groupOrder: group indices in processing order
    primitives: tuple[Primitive, ...]

    @property
    def animated(self) -> bool:
        return bool(self.flags & INFO_ANIM)


@dataclass(frozen=True)
class Frame:
    timestamp: int  # keyframe length in 25 Hz timer ticks
    step: tuple[int, int, int]
    states: tuple[tuple[int, tuple[int, int, int]], ...]  # (type, (dx, dy, dz)) per group


@dataclass(frozen=True)
class Animation:
    num_groups: int
    frames: tuple[Frame, ...] = field(default_factory=tuple)


class _Reader:
    def __init__(self, raw: bytes, what: str):
        self.raw, self.p, self.what = raw, 0, what

    def take(self, fmt: str):
        try:
            values = struct.unpack_from("<" + fmt, self.raw, self.p)
        except struct.error:
            raise BodyError(f"{self.what}: truncated at byte {self.p}")
        self.p += struct.calcsize("<" + fmt)
        return values

    def one(self, fmt: str) -> int:
        return self.take(fmt)[0]


def parse_body(raw: bytes) -> Body:
    r = _Reader(raw, "body")
    flags = r.one("H")
    if flags & INFO_OPTIMISE:
        raise BodyError("body: INFO_OPTIMISE (AITD2+) layout is not supported")
    zv = r.take("6h")
    scratch = r.one("H")
    r.p += scratch  # the engine keeps it for anim bookkeeping; not geometry
    nv = r.one("H")
    flat = r.take(f"{3 * nv}h")
    vertices = tuple(tuple(flat[3 * i:3 * i + 3]) for i in range(nv))
    groups: list[Group] = []
    order: tuple[int, ...] = ()
    if flags & INFO_ANIM:
        ng = r.one("H")
        offsets = r.take(f"{ng}H")
        if any(o % 0x10 for o in offsets):
            raise BodyError("body: group order offset not a multiple of 0x10")
        order = tuple(o // 0x10 for o in offsets)
        for _ in range(ng):
            start, count, pivot, parent, self_id, stype, dx, dy, dz = r.take("hhhbbhhhh")
            groups.append(Group(start // 6, count, pivot // 6, parent, self_id, stype, (dx, dy, dz)))
    prims: list[Primitive] = []
    for _ in range(r.one("H")):
        t = r.one("B")
        if t == PRIM_POLY or t in PRIM_POLY_TEX:
            n, material, color = r.take("BBB")
            pts = tuple(v // 6 for v in r.take(f"{n}H"))
            if t in (9, 10):
                r.p += 2 * n  # per-point UVs, ignored like the engine
            prims.append(Primitive(t, material, color, pts))
        elif t == PRIM_LINE:
            material, color, _even = r.take("BBB")
            prims.append(Primitive(t, material, color, tuple(v // 6 for v in r.take("2H"))))
        elif t in POINT_LIKE:
            material, color, _even = r.take("BBB")
            prims.append(Primitive(t, material, color, (r.one("H") // 6,)))
        elif t == PRIM_SPHERE:
            material, color, _even = r.take("BBB")
            size = r.one("H")
            prims.append(Primitive(t, material, color, (r.one("H") // 6,), size))
        else:
            raise BodyError(f"body: unknown primitive type {t} at byte {r.p - 1}")
    for prim in prims:
        if any(not 0 <= i < nv for i in prim.points):
            raise BodyError(f"body: primitive point out of range (0..{nv - 1})")
    return Body(flags, zv, vertices, tuple(groups), order, tuple(prims))


def parse_anim(raw: bytes) -> Animation:
    r = _Reader(raw, "anim")
    nf, ng = r.take("HH")
    plain = 4 + nf * (8 + ng * 8)
    if len(raw) != plain:
        raise BodyError(f"anim: size {len(raw)} != {plain} (only the AITD1 layout is supported)")
    frames = []
    for _ in range(nf):
        ts, sx, sy, sz = r.take("Hhhh")
        states = []
        for _ in range(ng):
            t, dx, dy, dz = r.take("hhhh")
            states.append((t, (dx, dy, dz)))
        frames.append(Frame(ts, (sx, sy, sz), tuple(states)))
    return Animation(ng, tuple(frames))
