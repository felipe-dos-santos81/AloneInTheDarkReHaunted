"""Builders for synthetic AITD1 bodies and animations used by the model tool
tests, in the byte layout of createBodyFromPtr / createAnimationFromPtr
(TatouSource/FitdLib/hqr.cpp)."""
import struct

import numpy as np

# The four-group test body: a root, a spine (group 1) with a head (group 2),
# and a leg (group 3) hanging from the root. Order is leaves first.
CHAIN_VERTICES = [
    (0, 0, 0), (0, -100, 0), (50, 0, 0),   # group 0: origin, pivot of 1, pivot of 3
    (0, -200, 0), (0, -100, 0),            # group 1 (local): tip, pivot of 2
    (30, -50, 0), (0, -80, 20),            # group 2 (local)
    (0, 100, 0), (20, 100, 10),            # group 3 (local)
]
# (start, count, pivot, parent, self)
CHAIN_GROUPS = [(0, 3, 0, -1, 0), (3, 2, 1, 0, 1), (5, 2, 4, 1, 2), (7, 2, 2, 0, 3)]
CHAIN_ORDER = [2, 3, 1, 0]
# (type, material, color, points, size)
CHAIN_PRIMS = [
    (1, 0, 10, [1, 3, 4], 0),   # poly spanning groups 0 and 1
    (1, 0, 20, [4, 5, 6], 0),   # poly spanning groups 1 and 2
    (1, 0, 30, [2, 7, 8], 0),   # poly spanning groups 0 and 3
    (0, 0, 40, [3, 5], 0),      # line
    (3, 0, 50, [6], 25),        # sphere of radius 25
    (2, 0, 60, [0], 0),         # point
]
# Rest positions (pivot chain applied): what every pose function returns at rest.
CHAIN_REST = [
    (0, 0, 0), (0, -100, 0), (50, 0, 0),
    (0, -300, 0), (0, -200, 0),
    (30, -250, 0), (0, -280, 20),
    (50, 100, 0), (70, 100, 10),
]


def body_bytes(vertices=CHAIN_VERTICES, groups=CHAIN_GROUPS, order=CHAIN_ORDER,
               prims=CHAIN_PRIMS, flags=3, zv=(-100, 100, -300, 100, -50, 50),
               scratch=b"\0" * 8, rest_states=None, trailing=b""):
    """Raw body bytes. `groups` are (start, count, pivot, parent, self);
    `rest_states` optionally gives (type, (dx, dy, dz)) per group."""
    out = struct.pack("<H6hH", flags, *zv, len(scratch)) + scratch
    out += struct.pack("<H", len(vertices)) + b"".join(struct.pack("<3h", *v) for v in vertices)
    if flags & 2:
        out += struct.pack("<H", len(groups)) + b"".join(struct.pack("<H", o * 0x10) for o in order)
        for i, (start, count, pivot, parent, self_id) in enumerate(groups):
            t, (dx, dy, dz) = rest_states[i] if rest_states else (0, (0, 0, 0))
            out += struct.pack("<hhhbbhhhh", start * 6, count, pivot * 6, parent, self_id, t, dx, dy, dz)
    out += struct.pack("<H", len(prims))
    for t, material, color, points, size in prims:
        if t in (1, 8, 9, 10):
            out += struct.pack("<BBBB", t, len(points), material, color)
            out += b"".join(struct.pack("<H", p * 6) for p in points)
            if t in (9, 10):
                out += b"\0\0" * len(points)
        elif t == 0:
            out += struct.pack("<BBBBHH", t, material, color, 0, points[0] * 6, points[1] * 6)
        elif t in (2, 6, 7):
            out += struct.pack("<BBBBH", t, material, color, 0, points[0] * 6)
        elif t == 3:
            out += struct.pack("<BBBBHH", t, material, color, 0, size, points[0] * 6)
        else:
            out += struct.pack("<B", t)
    return out + trailing


def anim_bytes(frames):
    """`frames` are (timestamp, (sx, sy, sz), [(type, (dx, dy, dz)), ...])."""
    groups = len(frames[0][2]) if frames else 0
    out = struct.pack("<HH", len(frames), groups)
    for ts, step, states in frames:
        out += struct.pack("<H3h", ts, *step)
        for t, d in states:
            out += struct.pack("<4h", t, *d)
    return out


def states(*per_group):
    """states((0, (0, 0, 0)), ...) -> list; shorthand for readability."""
    return list(per_group)


def rest(n=4):
    return [(0, (0, 0, 0))] * n


def synthetic_palette_rgb():
    return np.array([(i, 255 - i, i // 2) for i in range(256)], dtype=np.uint8)


# ── glTF evaluation (an independent skinning of a written .glb) ─────────────

def gltf_skinned_positions(glb, anim_index=None, key=0):
    """POSITION of mesh 0 skinned at animation key `key` (rest pose when
    `anim_index` is None), evaluated from the node graph, the skin and the
    animation samplers alone -- nothing from aitd_models.pose."""
    from aitd_models.gltf import trs_matrix
    doc = glb.doc
    nodes = [dict(n) for n in doc["nodes"]]
    if anim_index is not None:
        anim = doc["animations"][anim_index]
        for ch in anim["channels"]:
            values = glb.accessor(anim["samplers"][ch["sampler"]]["output"])
            nodes[ch["target"]["node"]][ch["target"]["path"]] = [float(v) for v in values[key]]
    world = {}

    def walk(i, parent):
        world[i] = parent @ trs_matrix(nodes[i])
        for c in nodes[i].get("children", []):
            walk(c, world[i])

    for root in doc["scenes"][doc.get("scene", 0)]["nodes"]:
        walk(root, np.eye(4))
    skin = doc["skins"][0]
    ibm = glb.accessor(skin["inverseBindMatrices"]).reshape(-1, 4, 4).transpose(0, 2, 1)
    joint = [world[j] @ ibm[k] for k, j in enumerate(skin["joints"])]
    attrs = doc["meshes"][0]["primitives"][0]["attributes"]
    pos = glb.accessor(attrs["POSITION"])
    js = glb.accessor(attrs["JOINTS_0"]).astype(int)[:, 0]
    return np.array([(joint[j] @ np.array([*p, 1.0]))[:3] for p, j in zip(pos, js)])


# ── a synthetic INDARK for export tests ──────────────────────────────────────

def chain_anim_bytes():
    """Two keyframes on the 4-group chain: a rotation, then translate + zoom."""
    return anim_bytes([
        (10, (0, 0, -20), [(0, (0, 0, 0)), (0, (0, 0, 256)), (0, (100, 0, 0)), (0, (0, 0, 0))]),
        (20, (0, 0, -30), [(0, (0, 0, 0)), (1, (0, -10, 5)), (2, (256, 0, -128)), (0, (0, 64, 0))]),
    ])


def write_model_data_dir(d):
    """An INDARK holding ITD_RESS (palette at entry 3) and the body and
    animation PAKs the model exporter reads:

    LISTBODY 0 chain            -> canonical LISTBODY_000
             1 not animated     -> ignored
             2 chain again      -> alias of LISTBODY_000
             3 truncated        -> skipped
             4 pivot outside parent group -> skipped
             5 points only      -> kind "skip", no folder
    LISTBOD2 0 chain            -> alias of LISTBODY_000
             1 chain, recoloured -> canonical LISTBOD2_001, skeleton sibling
    LISTANIM / LISTANI2: 0 chain animation (4 groups), 1 a 3-group
             animation, 2 a wrong-size entry (warning only)."""
    from helpers import pak_bytes, synthetic_palette
    d.mkdir(parents=True, exist_ok=True)
    entries = [b"filler"] * 4
    entries[3] = synthetic_palette()
    (d / "ITD_RESS.PAK").write_bytes(pak_bytes(entries))
    chain = body_bytes()
    flat = body_bytes(flags=1, vertices=[(0, 0, 0), (10, 0, 0), (0, 10, 0)], prims=[(1, 0, 5, [0, 1, 2], 0)])
    bad_pivot = body_bytes(groups=[(0, 3, 0, -1, 0), (3, 2, 5, 0, 1), (5, 2, 4, 1, 2), (7, 2, 2, 0, 3)])
    points_only = body_bytes(prims=[(2, 0, 60, [0], 0), (7, 0, 61, [3], 0)])
    recoloured = body_bytes(prims=[(t, m, c + 1, p, s) for t, m, c, p, s in CHAIN_PRIMS])
    (d / "LISTBODY.PAK").write_bytes(pak_bytes([chain, flat, chain, chain[:40], bad_pivot, points_only]))
    (d / "LISTBOD2.PAK").write_bytes(pak_bytes([chain, recoloured]))
    three = anim_bytes([(5, (0, 0, 0), [(0, (0, 0, 0))] * 3)])
    anims = pak_bytes([chain_anim_bytes(), three, b"\x01\x00\x04\x00junk"])
    (d / "LISTANIM.PAK").write_bytes(anims)
    (d / "LISTANI2.PAK").write_bytes(anims)
    return d
