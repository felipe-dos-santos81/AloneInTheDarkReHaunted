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

import json
import pathlib
from dataclasses import dataclass, field

import numpy as np

from aitd_textures.files import png_bytes

from ..body import PRIM_POLY, PRIM_SPHERE, Body
from ..gltf import ARRAY_BUFFER, UNSIGNED_INT, GlbBuilder
from ..mesh import Mesh

KIND_PALETTE, KIND_BODY, KIND_RAMP, KIND_OTHER = 0, 1, 2, 3
KIND_NAMES = ("palette", "body", "ramp", "other")
MATERIAL_TRANSPARENT = 2  # the engine draws it blended, 50 %
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
    if prim_type != PRIM_POLY or material == MATERIAL_TRANSPARENT:
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


def budget_level(triangles: int, bridges: int = 0, target: int = TRIANGLE_TARGET) -> int:
    """The highest Catmull-Clark level whose predicted triangle count stays
    within `target` (0 when even level 1 does not). Bridges stay as they are;
    each other triangle becomes 6 * 4^(L-1). The real count, after degenerate
    faces drop out, is at most the prediction."""
    level = 0
    while level < MAX_LEVEL and bridges + (triangles - bridges) * 6 * 4 ** level <= target:
        level += 1
    return level


PROJECTIONS = ("front", "back", "blend", "palette")
EDIT_FIELDS = {"subdivide", "crease", "projection", "skip"}


class EditError(ValueError):
    pass


@dataclass
class Edits:
    subdivide: dict[int, int] = field(default_factory=dict)
    crease: set[int] = field(default_factory=set)
    projection: dict[int, str] = field(default_factory=dict)
    skip: str | None = None


def _group(name, groups: int) -> int:
    if not (isinstance(name, str) and len(name) == 3 and name[0] == "g" and name[1:].isascii()
            and name[1:].isdigit() and int(name[1:]) < groups):
        raise EditError(f"{name!r} is not a group of this body (g00..g{groups - 1:02d})")
    return int(name[1:])


def _section(doc: dict, name: str, kind: type, file: str):
    value = doc.get(name, kind())
    if not isinstance(value, kind):
        raise EditError(f"{file}: {name} must be a JSON {'object' if kind is dict else 'list'}")
    return value


def read_edits(path: pathlib.Path, groups: int) -> Edits:
    """A body's edit file; no file means no edits. Any unknown field or bad
    value raises EditError."""
    if not path.is_file():
        return Edits()
    try:
        doc = json.loads(path.read_text())
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise EditError(f"{path.name}: {exc}")
    if not isinstance(doc, dict):
        raise EditError(f"{path.name}: not a JSON object")
    unknown = set(doc) - EDIT_FIELDS
    if unknown:
        raise EditError(f"{path.name}: unknown field {', '.join(sorted(unknown))}")
    edits = Edits()
    for name, level in _section(doc, "subdivide", dict, path.name).items():
        if not isinstance(level, int) or isinstance(level, bool) or not 0 <= level <= MAX_LEVEL:
            raise EditError(f"{path.name}: subdivide {name} must be an integer 0..{MAX_LEVEL}")
        edits.subdivide[_group(name, groups)] = level
    edits.crease = {_group(name, groups) for name in _section(doc, "crease", list, path.name)}
    for name, how in _section(doc, "projection", dict, path.name).items():
        if not isinstance(how, str) or how not in PROJECTIONS:
            raise EditError(f"{path.name}: projection {name} must be one of {', '.join(PROJECTIONS)}")
        edits.projection[_group(name, groups)] = how
    if "skip" in doc:
        reason = doc["skip"].get("reason") if isinstance(doc["skip"], dict) else None
        if not isinstance(reason, str) or not reason.strip():
            raise EditError(f"{path.name}: skip needs a non-empty reason")
        edits.skip = reason
    return edits


def corner_arrays(body: Body, mesh: Mesh, paths: dict, edits: Edits) -> dict[str, np.ndarray]:
    """Per triangle of `mesh` (build_mesh's order, which original.glb keeps):
    engine UVs front and back (3T, 2), front weight (T,), atlas kind (T,),
    whether it is the transparent material 2, polygon or sphere (T,), group (T,) and linear
    palette colour (T, 3)."""
    rest = engine_rest_vertices(body)
    pmin, prange = projection(rest)
    count = mesh.triangle_count
    uv_front, uv_back = np.zeros((3 * count, 2)), np.zeros((3 * count, 2))
    weight, kind = np.ones(count), np.zeros(count, np.int64)
    transparent = np.zeros(count, np.uint8)
    t = 0
    for pi, prim in enumerate(body.primitives):
        n = int(np.count_nonzero(mesh.prim_index == pi))
        k = kind_of(prim.type, prim.material, paths)
        if prim.type == PRIM_POLY and len(prim.points) >= 3:
            ids = prim.points
            w = front_weight(rest[ids[0]], rest[ids[1]], rest[ids[2]])
            for j in range(1, len(ids) - 1):
                corners = rest[[ids[0], ids[j], ids[j + 1]], :2]
                a, b = corner_uv(corners, pmin, prange, True), corner_uv(corners, pmin, prange, False)
                if k == KIND_RAMP:
                    a, b = mirror_uv(a), mirror_uv(b)
                uv_front[3 * t:3 * t + 3], uv_back[3 * t:3 * t + 3] = a, b
                weight[t], kind[t], transparent[t] = w, k, prim.material == MATERIAL_TRANSPARENT
                t += 1
        else:
            transparent[t:t + n] = prim.type == PRIM_SPHERE and prim.material == MATERIAL_TRANSPARENT
            t += n
    if t != count:
        raise ValueError(f"{t} triangles placed, the mesh has {count}")
    group = triangle_groups(mesh.groups.reshape(-1, 3))
    for g, how in edits.projection.items():
        mine = group == g
        if how == "front":
            weight[mine] = 1.0
        elif how == "back":
            weight[mine] = 0.0
        elif how == "palette":
            kind[mine] = KIND_PALETTE
    colour = srgb_to_linear(mesh.colors.reshape(-1, 3, 3)[:, 0])
    return {"uv_front": uv_front, "uv_back": uv_back, "w_front": weight, "kind": kind,
            "transparent": transparent, "tri_group": group, "palette": colour}


def levels(groups: int, triangles: int, bridges: int, edits: Edits) -> list[int]:
    """The subdivision level of each group: the budget's, unless edited."""
    base = budget_level(triangles, bridges)
    return [edits.subdivide.get(g, base) for g in range(groups)]


AO_STRENGTH = 0.3
TRANSLUCENT_ALPHA = 128  # the engine draws such a texel blended, as the transparent material 2 (mipChain.h)


def composite(colour: np.ndarray, ao: np.ndarray, strength: float = AO_STRENGTH,
              mask: np.ndarray | None = None) -> np.ndarray:
    """Linear (H, W, 3) colour and (H, W) ambient occlusion, rows bottom-up
    as Blender stores them -> (H, W, 3) uint8 sRGB, rows top-down. With an
    (H, W) transparency mask: (H, W, 4), alpha TRANSLUCENT_ALPHA where it is
    at least 0.5, else opaque."""
    lit = np.asarray(colour, float) * (1.0 - strength * (1.0 - np.asarray(ao, float)[..., None]))
    rgb = (np.clip(linear_to_srgb(lit), 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8)
    if mask is not None:
        alpha = np.where(np.asarray(mask, float) >= 0.5, TRANSLUCENT_ALPHA, 255).astype(np.uint8)
        rgb = np.dstack([rgb, alpha])
    return rgb[::-1]


def model_glb(positions: np.ndarray, loop_vertex: np.ndarray, loop_uv: np.ndarray, png: bytes) -> bytes:
    """The delivery: Blender's triangles (one loop per corner, three per
    triangle) as an indexed glTF mesh, Z-up (x, y, z) -> Y-up (x, z, -y),
    one vertex per (position, UV) pair, v flipped to glTF's top-left origin.
    NORMAL is area-weighted per Blender vertex, so the pieces a UV seam splits
    shade as one surface; a zero-area vertex's normal stays zero."""
    loop_vertex = np.asarray(loop_vertex, np.int64)
    loop_uv = np.asarray(loop_uv, float)
    pairs = np.column_stack([loop_vertex, np.round(loop_uv * 1e6).astype(np.int64)])
    unique, inverse = np.unique(pairs, axis=0, return_inverse=True)
    inverse = inverse.reshape(-1)
    first = np.zeros(len(unique), np.int64)
    first[inverse[::-1]] = np.arange(len(inverse))[::-1]
    gltf = np.column_stack([positions[:, 0], positions[:, 2], -positions[:, 1]]).astype(float)
    corners = gltf[loop_vertex].reshape(-1, 3, 3)
    face = np.cross(corners[:, 1] - corners[:, 0], corners[:, 2] - corners[:, 0])
    smooth = np.zeros_like(gltf)
    for k in range(3):
        np.add.at(smooth, loop_vertex.reshape(-1, 3)[:, k], face)
    length = np.linalg.norm(smooth, axis=1, keepdims=True)
    smooth = np.divide(smooth, length, out=np.zeros_like(smooth), where=length > 0)
    p = gltf[loop_vertex[first]]
    n = smooth[loop_vertex[first]]
    uv = loop_uv[first]
    g = GlbBuilder()
    material = g.textured_material(png, "image/png", "remaster")
    g.single_mesh_scene({"attributes": {
        "POSITION": g.accessor(p, "VEC3", target=ARRAY_BUFFER, bounds=True),
        "NORMAL": g.accessor(n, "VEC3", target=ARRAY_BUFFER),
        "TEXCOORD_0": g.accessor(np.column_stack([uv[:, 0], 1.0 - uv[:, 1]]), "VEC2", target=ARRAY_BUFFER)},
        "indices": g.accessor(inverse, "SCALAR", UNSIGNED_INT), "material": material})
    return g.to_bytes()


def finish_glb(work: pathlib.Path) -> tuple[bytes, int]:
    """model.glb from the stage's outputs in `work` (mask.npy only for a body
    with transparent triangles), and its triangle count."""
    refined = np.load(work / "refined.npz")
    colour = np.load(work / "color.npy")
    ao = np.load(work / "ao.npy")
    mask = np.load(work / "mask.npy") if (work / "mask.npy").is_file() else None
    png = png_bytes(composite(colour, ao, mask=mask))
    return model_glb(refined["positions"], refined["loop_vertex"], refined["loop_uv"], png), len(refined["loop_vertex"]) // 3
