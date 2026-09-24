# SPDX-License-Identifier: GPL-2.0-only
"""Skeleton checks and identity for AITD1 animated bodies.

`validate` lists every way a body breaks the invariants the pose math relies
on (all 214 animated bodies of LISTBODY/LISTBOD2 pass). `skeleton_hash` is a
64-bit FNV-1a over the rest geometry and hierarchy; the engine computes the
same value (spec §4.1) to reject a replacement made for another body."""
from __future__ import annotations

import struct

from .body import INFO_ANIM, INFO_OPTIMISE, Body

MAX_GROUPS = 32
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
MASK64 = 0xFFFFFFFFFFFFFFFF


def owners(body: Body) -> list[int]:
    """Group index owning each vertex (-1 when no group covers it)."""
    owner = [-1] * len(body.vertices)
    for gi, g in enumerate(body.groups):
        for v in range(g.start, g.start + g.count):
            if 0 <= v < len(owner):
                owner[v] = gi
    return owner


def children(body: Body) -> list[list[int]]:
    kids: list[list[int]] = [[] for _ in body.groups]
    for gi, g in enumerate(body.groups):
        if gi and 0 <= g.parent < len(body.groups):
            kids[g.parent].append(gi)
    return kids


def descendants(body: Body, gi: int) -> list[int]:
    out, stack = [], list(children(body)[gi])
    while stack:
        c = stack.pop()
        out.append(c)
        stack.extend(children(body)[c])
    return sorted(out)


def validate(body: Body) -> list[str]:
    """Every broken invariant, as readable strings; [] means poseable."""
    problems: list[str] = []
    if not body.flags & INFO_ANIM:
        return ["not animated (no INFO_ANIM)"]
    if body.flags & INFO_OPTIMISE:
        return ["INFO_OPTIMISE (AITD2+) bodies are not supported"]
    groups, nv = body.groups, len(body.vertices)
    if not 1 <= len(groups) <= MAX_GROUPS:
        return [f"{len(groups)} groups (must be 1..{MAX_GROUPS})"]
    if sorted(body.order) != list(range(len(groups))):
        problems.append("group order is not a permutation of the groups")
    coverage = [0] * nv
    for gi, g in enumerate(groups):
        if g.self_id != gi:
            problems.append(f"group {gi}: m_numGroup is {g.self_id}")
        if gi == 0 and g.parent != -1:
            problems.append(f"root group has parent {g.parent}")
        if gi and not 0 <= g.parent < gi:
            problems.append(f"group {gi}: parent {g.parent} is not an earlier group")
        if g.count < 0 or g.start < 0 or g.start + g.count > nv:
            problems.append(f"group {gi}: vertex range {g.start}+{g.count} outside 0..{nv}")
            continue
        for v in range(g.start, g.start + g.count):
            coverage[v] += 1
    if any(c != 1 for c in coverage):
        problems.append("group vertex ranges do not cover every vertex exactly once")
    if problems:
        return problems
    owner = owners(body)
    for gi, g in enumerate(groups):
        if not 0 <= g.pivot < nv:
            problems.append(f"group {gi}: pivot {g.pivot} out of range")
        elif gi and owner[g.pivot] != g.parent:
            problems.append(f"group {gi}: pivot vertex {g.pivot} is not in parent group {g.parent}")
    if groups[0].pivot != 0 or body.vertices[0] != (0, 0, 0):
        problems.append("root pivot is not vertex 0 at the origin")
    position = {g: i for i, g in enumerate(body.order)}
    if len(position) == len(groups):
        for gi in range(len(groups)):
            if any(position[d] > position[gi] for d in descendants(body, gi)):
                problems.append(f"group {gi} is processed before one of its descendants")
                break
    return problems


def skeleton_hash(body: Body) -> str:
    """FNV-1a 64 over: u16 vertex count, every vertex as 3 x s16, u16 group
    count, per group (s16 start, s16 count, s16 pivot, s8 parent, s8 self),
    then the order as u16 each; little-endian. Returned as 16 hex digits."""
    blob = bytearray(struct.pack("<H", len(body.vertices)))
    for v in body.vertices:
        blob += struct.pack("<3h", *v)
    blob += struct.pack("<H", len(body.groups))
    for g in body.groups:
        blob += struct.pack("<hhhbb", g.start, g.count, g.pivot, g.parent, g.self_id)
    for o in body.order:
        blob += struct.pack("<H", o)
    h = FNV_OFFSET
    for byte in blob:
        h = ((h ^ byte) * FNV_PRIME) & MASK64
    return f"{h:016x}"
