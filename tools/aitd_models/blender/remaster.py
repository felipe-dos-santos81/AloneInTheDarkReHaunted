# SPDX-License-Identifier: GPL-2.0-only
"""The plain-Python half of the Blender stage (docs/model-contract.md, "The
in-repo generator"). It runs outside Blender, before and after it:

- prepare: per original triangle, the engine's atlas UVs (front and back),
  its front weight and which atlas it takes, so Blender bakes exactly what
  the engine paints (TatouSource/FitdLib/modelAtlas.cpp, renderer.cpp);
- finish: the colour bake softened, the bakes composited into one sRGB
  PNG, and the refined mesh written as model.glb.

Blender itself only refines geometry and bakes (stage.py)."""
from __future__ import annotations

import json
import pathlib
from dataclasses import dataclass, field

import numpy as np

from aitd_data.files import png_bytes

from ..body import MATERIAL_TRANSPARENT, PRIM_POLY, PRIM_SPHERE, Body
from ..gltf import ARRAY_BUFFER, UNSIGNED_INT, GlbBuilder
from ..hdm import TRANSLUCENT_ALPHA
from ..mesh import Mesh

KIND_PALETTE, KIND_BODY, KIND_RAMP, KIND_OTHER, KIND_GLASS = 0, 1, 2, 3, 4
KIND_NAMES = ("palette", "body", "ramp", "other", "glass")  # stage.KINDS; glass: palette colour, drawn translucent
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
    polygons take one. Material 2 (transparent), on a polygon or a sphere, is
    glass: its palette colour, drawn translucent."""
    if material == MATERIAL_TRANSPARENT and prim_type in (PRIM_POLY, PRIM_SPHERE):
        return KIND_GLASS
    if prim_type != PRIM_POLY:
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
    group (T,) and linear palette colour (T, 3)."""
    rest = engine_rest_vertices(body)
    pmin, prange = projection(rest)
    count = mesh.triangle_count
    uv_front, uv_back = np.zeros((3 * count, 2)), np.zeros((3 * count, 2))
    weight, kind = np.ones(count), np.zeros(count, np.int64)
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
                weight[t], kind[t] = w, k
                t += 1
        else:
            if prim.type == PRIM_SPHERE:
                kind[t:t + n] = k
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
            kind[mine & (kind != KIND_GLASS)] = KIND_PALETTE  # glass is palette colour already
    colour = srgb_to_linear(mesh.colors.reshape(-1, 3, 3)[:, 0])
    return {"uv_front": uv_front, "uv_back": uv_back, "w_front": weight, "kind": kind,
            "tri_group": group, "palette": colour}


def levels(groups: int, triangles: int, bridges: int, edits: Edits) -> list[int]:
    """The subdivision level of each group: the budget's, unless edited."""
    base = budget_level(triangles, bridges)
    return [edits.subdivide.get(g, base) for g in range(groups)]


AO_STRENGTH = 0.3
# The hand-made atlases paint crumpled low-poly facets; soften() evens out their
# brightness steps. Chosen from renders of Carnby (spec 2026-10-09): a radius of
# 24 texels on the 2048 px bake, scaled with the bake's size.
SOFTEN_RADIUS = 24 / 2048
SOFTEN_EPS = 0.3
LUMA = np.array([0.2126, 0.7152, 0.0722])  # Rec. 709, linear


def _box(x: np.ndarray, r: int) -> np.ndarray:
    """The sum of x over the (2r+1)-wide square around each texel, cut at the
    image's edges."""
    h, w = x.shape
    total = np.zeros((h + 1, w + 1))
    total[1:, 1:] = x.cumsum(0).cumsum(1)
    y0, y1 = np.clip(np.arange(h) - r, 0, h), np.clip(np.arange(h) + r + 1, 0, h)
    x0, x1 = np.clip(np.arange(w) - r, 0, w), np.clip(np.arange(w) + r + 1, 0, w)
    return total[y1][:, x1] - total[y0][:, x1] - total[y1][:, x0] + total[y0][:, x0]


def soften(colour: np.ndarray, radius: int, eps: float = SOFTEN_EPS) -> np.ndarray:
    """Linear (H, W, 3) colour with its low-contrast brightness steps evened
    out and its high-contrast ones kept: a guided filter (He, Sun and Tang)
    of log brightness guided by itself, over the texels within `radius`; eps
    is the variance below which a window counts as flat. Only brightness
    moves, never hue, and the texture keeps its mean brightness. Texels with
    no channel above 0 are the bake's empty background: they neither change
    nor count."""
    colour = np.asarray(colour, float)
    covered = colour.max(axis=-1) > 0
    weight = covered.astype(float)
    count = np.maximum(_box(weight, radius), 1.0)
    lum = np.log(np.maximum(colour @ LUMA, 1e-4))
    mean = _box(lum * weight, radius) / count
    var = np.maximum(_box(lum * lum * weight, radius) / count - mean * mean, 0.0)
    a = var / (var + eps)
    b = mean - a * mean
    smooth = _box(a * weight, radius) / count * lum + _box(b * weight, radius) / count
    out = np.where(covered[..., None], colour * np.exp(smooth - lum)[..., None], colour)
    if covered.any():  # smoothing log brightness lowers the plain mean (about 2 %): give the texture its own back
        out[covered] *= (colour @ LUMA)[covered].mean() / (out @ LUMA)[covered].mean()
    return out / np.maximum(out.max(axis=-1, keepdims=True), 1.0)  # past full brightness: scaled whole, hue kept


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


SEAM_DISTANCE = 0.0005  # stage.py's merge_distance (Blender metres): a vertex this close to another piece's open edge lies on it
JOINT_NORMAL_COS = 0.5  # cos 60 degrees: normals this close shade as one surface where pieces meet


def pieces(triangles: np.ndarray, count: int) -> np.ndarray:
    """(count,) the piece of each vertex: vertices joined through shared
    triangles carry the lowest index among them."""
    label = np.arange(count)
    while True:
        low = label[triangles].min(axis=1)
        before = label.copy()
        for k in range(3):
            np.minimum.at(label, triangles[:, k], low)
        label = label[label]  # follow the chains
        if np.array_equal(label, before):
            return label


def _open_edges(tri: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Edge k of triangle t is edges[3t + k] (corner k -> k + 1); returns
    (edges, the indices of those used by one triangle only)."""
    edges = np.stack([tri, np.roll(tri, -1, axis=1)], axis=2).reshape(-1, 2)
    _, inv, cnt = np.unique(np.sort(edges, axis=1), axis=0, return_inverse=True, return_counts=True)
    return edges, np.flatnonzero(cnt[inv.reshape(-1)] == 1)


def _snap(positions: np.ndarray, piece: np.ndarray, rim: np.ndarray, distance: float) -> np.ndarray:
    """Open-edge vertices of different pieces within `distance` of each other
    take one position (the lowest-index one's): they then weld exactly."""
    out = positions.copy()
    q = positions[rim]
    d = np.linalg.norm(q[:, None] - q[None], axis=2)
    close = (d < distance) & (piece[rim][:, None] != piece[rim][None])
    for i, j in zip(*np.nonzero(np.triu(close, 1))):
        out[rim[j]] = out[rim[i]]
    return out


def close_seams(positions: np.ndarray, round_positions: np.ndarray, loop_vertex: np.ndarray, loop_uv: np.ndarray,
                distance: float = SEAM_DISTANCE):
    """Close the seams where the pieces meet. Vertices of different pieces'
    open edges within `distance` of each other first take one position (a
    move of at most `distance`); then, until none is left, a vertex of one
    piece lying on an open edge of another splits that edge's triangle at it
    (the new corner takes the vertex's position and the edge's interpolated
    UV and round position). The seam's vertices are then shared by position,
    so they share their skin weights and normal. Returns new
    (positions, round_positions, loop_vertex, loop_uv)."""
    positions, round_positions = np.asarray(positions, float), np.asarray(round_positions, float)
    loop_vertex, loop_uv = np.asarray(loop_vertex, np.int64), np.asarray(loop_uv, float)
    tri = loop_vertex.reshape(-1, 3)
    piece = pieces(tri, len(positions))
    edges, open_edges = _open_edges(tri)
    positions = _snap(positions, piece, np.unique(edges[open_edges].ravel()), distance)
    for _ in range(4):  # a cut can expose another; two rounds settle the bodies measured
        before = len(loop_vertex)
        positions, round_positions, loop_vertex, loop_uv = _cut(positions, round_positions, loop_vertex, loop_uv, distance)
        if len(loop_vertex) == before:
            break
    return positions, round_positions, loop_vertex, loop_uv


def _cut(positions, round_positions, loop_vertex, loop_uv, distance):
    """One round of close_seams' cuts."""
    tri = loop_vertex.reshape(-1, 3)
    uv = loop_uv.reshape(-1, 3, 2)
    piece = pieces(tri, len(positions))
    edges, open_edges = _open_edges(tri)
    candidates = np.unique(edges[open_edges].ravel())
    q = positions[candidates]
    cuts = {}  # edge index -> [(s, vertex)]
    for ei in open_edges:
        a, b = positions[edges[ei, 0]], positions[edges[ei, 1]]
        ab = b - a
        span = float(ab @ ab)
        if span == 0.0:
            continue
        s = (q - a) @ ab / span
        off = np.linalg.norm(a + s[:, None] * ab - q, axis=1)
        inner = distance / np.sqrt(span)
        hit = (off < distance) & (s > inner) & (s < 1 - inner) & (piece[candidates] != piece[edges[ei, 0]])
        if hit.any():
            cuts[ei] = sorted(zip(s[hit].tolist(), candidates[hit].tolist()))
    if not cuts:
        return positions, round_positions, loop_vertex, loop_uv
    new_pos, new_round, out_tri, out_uv = [positions], [round_positions], [], []
    nxt = len(positions)
    for t in range(len(tri)):
        mine = [(k, cuts.get(3 * t + k)) for k in range(3)]
        if not any(c for _, c in mine):
            out_tri.append(tri[t]); out_uv.append(uv[t]); continue
        ring, ring_uv = [], []  # the triangle's outline, cut points included, in winding order
        for k, cut in mine:
            a, b = tri[t, k], tri[t, (k + 1) % 3]
            ring.append(a); ring_uv.append(uv[t, k])
            for sv, v in cut or []:
                new_pos.append(positions[v][None]); new_round.append((round_positions[a] + sv * (round_positions[b] - round_positions[a]))[None])
                ring.append(nxt); ring_uv.append(uv[t, k] + sv * (uv[t, (k + 1) % 3] - uv[t, k])); nxt += 1
        split = [k for k, c in mine if c]
        if len(split) == 1:  # a fan from the corner opposite the cut edge: no flat triangle
            apex = (split[0] + 2) % 3
            order = ring[ring.index(tri[t, apex]):] + ring[:ring.index(tri[t, apex])]
            order_uv = ring_uv[ring.index(tri[t, apex]):] + ring_uv[:ring.index(tri[t, apex])]
            for j in range(1, len(order) - 1):
                out_tri.append(np.array([order[0], order[j], order[j + 1]])); out_uv.append(np.array([order_uv[0], order_uv[j], order_uv[j + 1]]))
        else:  # a fan from the centre
            c = nxt; nxt += 1
            corners = [tri[t, k] for k in range(3)]
            new_pos.append(positions[corners].mean(axis=0)[None]); new_round.append(round_positions[corners].mean(axis=0)[None])
            cuv = uv[t].mean(axis=0)
            for j in range(len(ring)):
                out_tri.append(np.array([ring[j], ring[(j + 1) % len(ring)], c])); out_uv.append(np.array([ring_uv[j], ring_uv[(j + 1) % len(ring)], cuv]))
    return (np.concatenate(new_pos), np.concatenate(new_round), np.stack(out_tri).reshape(-1),
            np.stack(out_uv).reshape(-1, 2))


def joint_normals(positions: np.ndarray, normals: np.ndarray, piece: np.ndarray) -> np.ndarray:
    """Area-weighted (unnormalised) per-vertex `normals` with the vertices of
    different pieces at one position merged: each takes the sum of those
    within 60 degrees of its own, so a seam shades as one surface and the two
    sides of a thin plate stay apart."""
    key = np.round(positions / 1e-7).astype(np.int64)
    _, group, count = np.unique(key, axis=0, return_inverse=True, return_counts=True)
    group = group.reshape(-1)
    out = normals.copy()
    shared = np.flatnonzero(count[group] > 1)
    order = shared[np.argsort(group[shared], kind="stable")]
    unit = normals / np.maximum(np.linalg.norm(normals, axis=1, keepdims=True), 1e-30)
    for members in np.split(order, np.flatnonzero(np.diff(group[order])) + 1):
        if len(set(piece[members].tolist())) < 2:
            continue
        close = unit[members] @ unit[members].T > JOINT_NORMAL_COS
        out[members] = close.astype(float) @ normals[members]
    return out


def model_glb(positions: np.ndarray, round_positions: np.ndarray, loop_vertex: np.ndarray, loop_uv: np.ndarray,
              png: bytes) -> bytes:
    """The delivery: Blender's triangles (one loop per corner, three per
    triangle) as an indexed glTF mesh, Z-up (x, y, z) -> Y-up (x, z, -y),
    one vertex per (position, UV) pair, v flipped to glTF's top-left origin.
    NORMAL is the round surface's (`round_positions`, the same vertices),
    area-weighted per Blender vertex over the same triangles, so the pieces
    a UV seam splits shade as one surface; a zero-area vertex's normal stays
    zero."""
    positions, round_positions, loop_vertex, loop_uv = close_seams(positions, round_positions, loop_vertex, loop_uv)
    pairs = np.column_stack([loop_vertex, np.round(loop_uv * 1e6).astype(np.int64)])
    unique, inverse = np.unique(pairs, axis=0, return_inverse=True)
    inverse = inverse.reshape(-1)
    first = np.zeros(len(unique), np.int64)
    first[inverse[::-1]] = np.arange(len(inverse))[::-1]
    def y_up(p: np.ndarray) -> np.ndarray:
        return np.column_stack([p[:, 0], p[:, 2], -p[:, 1]]).astype(float)
    gltf = y_up(positions)
    corners = y_up(round_positions)[loop_vertex].reshape(-1, 3, 3)
    face = np.cross(corners[:, 1] - corners[:, 0], corners[:, 2] - corners[:, 0])
    smooth = np.zeros_like(gltf)
    for k in range(3):
        np.add.at(smooth, loop_vertex.reshape(-1, 3)[:, k], face)
    smooth = joint_normals(gltf, smooth, pieces(loop_vertex.reshape(-1, 3), len(gltf)))
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
    """model.glb from the stage's outputs in `work` (refined.npz carries a
    mask only for a body with glass), its colour bake softened, and its
    triangle count."""
    refined = np.load(work / "refined.npz")
    if "round" not in refined.files:
        raise ValueError(f"{work / 'refined.npz'} has no round surface (written before it had one): rerun the stage")
    colour = np.load(work / "color.npy")
    colour = soften(colour, max(1, round(SOFTEN_RADIUS * len(colour))))
    ao = np.load(work / "ao.npy")
    mask = refined["mask"] if "mask" in refined.files else None
    png = png_bytes(composite(colour, ao, mask=mask))
    return (model_glb(refined["positions"], refined["round"], refined["loop_vertex"], refined["loop_uv"], png),
            len(refined["loop_vertex"]) // 3)
