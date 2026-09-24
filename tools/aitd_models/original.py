# SPDX-License-Identifier: GPL-2.0-only
"""original.glb: an AITD1 body as a skinned glTF, for Blender and for the
image-to-3D reference.

Axes: glTF = FLIP @ engine * METRES_PER_UNIT, where FLIP = diag(1, -1, -1)
(a 180-degree turn about X, so winding is kept). The engine is y-down and its
characters face -z; glTF is y-up and faces +z, with +x on the model's left.

Joints: one node `gNN` per group, parented like the groups, whose world
matrix is pose.Pose.joints (the rigid frame). A group that zooms in any
exported animation also gets a child node `gNN_geo` carrying only that zoom,
and its vertices are bound there, because an engine zoom scales the group's
own vertices and never its children. Every vertex has one joint, weight 1."""
from __future__ import annotations

import numpy as np

from .body import Animation, Body
from .gltf import ARRAY_BUFFER, UNSIGNED_BYTE, GlbBuilder, quaternion
from .mesh import build_mesh, face_normals
from .pose import ZOOM, Pose, pose_float, rest_states, skin

FLIP = np.diag([1.0, -1.0, -1.0])
METRES_PER_UNIT = 0.001
TICKS_PER_SECOND = 25.0  # the engine timer; a keyframe's timestamp is its length in ticks


def to_gltf_points(points: np.ndarray) -> np.ndarray:
    return (np.asarray(points, float) @ FLIP.T) * METRES_PER_UNIT


def to_gltf_rigid(m: np.ndarray) -> np.ndarray:
    """Conjugate an engine-space rigid 4x4 into glTF space."""
    out = np.eye(4)
    out[:3, :3] = FLIP @ m[:3, :3] @ FLIP
    out[:3, 3] = FLIP @ m[:3, 3] * METRES_PER_UNIT
    return out


def zoomed_groups(body: Body, animations: list[tuple[str, Animation]]) -> list[int]:
    zoomed = set()
    for _name, anim in animations:
        for frame in anim.frames:
            for gi, (t, d) in enumerate(frame.states[:len(body.groups)]):
                if gi and t == ZOOM and any(d):
                    zoomed.add(gi)
    return sorted(zoomed)


def key_times(anim: Animation) -> list[float]:
    """Frame k is reached after its own length; the clip starts on frame 0
    and, when it has more than one frame, loops back to it."""
    times = [0.0]
    for frame in anim.frames[1:]:
        times.append(times[-1] + max(frame.timestamp, 1) / TICKS_PER_SECOND)
    if len(anim.frames) > 1:
        times.append(times[-1] + max(anim.frames[0].timestamp, 1) / TICKS_PER_SECOND)
    return times


def _frame_states(body: Body, anim: Animation, k: int):
    frame = anim.frames[k % len(anim.frames)]
    return list(frame.states[:len(body.groups)])


def _locals(body: Body, pose: Pose) -> list[np.ndarray]:
    out = []
    for gi, g in enumerate(body.groups):
        world = pose.joints[gi]
        local = world if gi == 0 else np.linalg.inv(pose.joints[g.parent]) @ world
        out.append(to_gltf_rigid(local))
    return out


def build_original_glb(body: Body, palette: np.ndarray,
                       animations: list[tuple[str, Animation]] = ()) -> bytes:
    """`animations` are (name, Animation) pairs whose group count matches the body."""
    for name, anim in animations:
        if anim.num_groups != len(body.groups):
            raise ValueError(f"animation {name} has {anim.num_groups} groups, body has {len(body.groups)}")
    rest = pose_float(body, rest_states(body))
    mesh = build_mesh(body, skin(body, rest.group_matrices()), palette)
    zoomed = zoomed_groups(body, animations)
    g = GlbBuilder()

    # Joint nodes. Node 0 is the mesh; joints follow in group order, then geo joints.
    rest_locals = _locals(body, rest)
    joint_nodes = []
    for gi in range(len(body.groups)):
        m = rest_locals[gi]
        joint_nodes.append(g.add("nodes", {"name": f"g{gi:02d}", "translation": [float(v) for v in m[:3, 3]]}))
    geo_nodes = {}
    for gi in zoomed:
        geo_nodes[gi] = g.add("nodes", {"name": f"g{gi:02d}_geo"})
        g.doc["nodes"][joint_nodes[gi]].setdefault("children", []).append(geo_nodes[gi])
    for gi, grp in enumerate(body.groups[1:], 1):
        g.doc["nodes"][joint_nodes[grp.parent]].setdefault("children", []).append(joint_nodes[gi])
    skin_joints = joint_nodes + [geo_nodes[gi] for gi in zoomed]
    skin_groups = list(range(len(body.groups))) + zoomed  # the group behind each skin joint
    bind_slot = {gi: skin_joints.index(geo_nodes.get(gi, joint_nodes[gi])) for gi in range(len(body.groups))}

    # Inverse bind matrices: the rest joint frames are pure translations.
    ibms = [np.linalg.inv(to_gltf_rigid(rest.joints[gi])).T.reshape(16) for gi in skin_groups]  # column-major
    skin_index = g.add("skins", {"joints": skin_joints, "skeleton": joint_nodes[0],
                                 "inverseBindMatrices": g.accessor(np.array(ibms), "MAT4")})

    positions = to_gltf_points(mesh.positions)
    normals = face_normals(positions)
    normals[~normals.any(axis=1)] = (0.0, 1.0, 0.0)  # degenerate faces: any unit normal
    normals = np.repeat(normals, 3, axis=0)
    joints = np.zeros((len(positions), 4), np.uint8)
    joints[:, 0] = [bind_slot[int(gi)] for gi in mesh.groups]
    weights = np.zeros((len(positions), 4), np.float32)
    weights[:, 0] = 1.0
    attributes = {
        "POSITION": g.accessor(positions, "VEC3", target=ARRAY_BUFFER, bounds=True),
        "NORMAL": g.accessor(normals, "VEC3", target=ARRAY_BUFFER),
        "COLOR_0": g.accessor(mesh.colors, "VEC3", target=ARRAY_BUFFER),
        "JOINTS_0": g.accessor(joints, "VEC4", UNSIGNED_BYTE, target=ARRAY_BUFFER),
        "WEIGHTS_0": g.accessor(weights, "VEC4", target=ARRAY_BUFFER),
    }
    material = g.add("materials", {"name": "palette", "doubleSided": True,
                                   "pbrMetallicRoughness": {"metallicFactor": 0.0, "roughnessFactor": 1.0}})
    mesh_index = g.add("meshes", {"name": "body", "primitives": [{"attributes": attributes, "material": material}]})
    mesh_node = g.add("nodes", {"name": "body", "mesh": mesh_index, "skin": skin_index})
    g.add("scenes", {"nodes": [mesh_node, joint_nodes[0]]})
    g.doc["scene"] = 0

    for name, anim in animations:
        times = key_times(anim)
        poses = [pose_float(body, _frame_states(body, anim, k)) for k in range(len(times))]
        time_acc = g.accessor(np.array(times), "SCALAR", bounds=True)
        channels, samplers = [], []
        for gi in range(len(body.groups)):
            locals_ = [_locals(body, p)[gi] for p in poses]
            quats = []
            for m in locals_:
                q = quaternion(m[:3, :3])
                if quats and np.dot(quats[-1], q) < 0:
                    q = -q
                quats.append(q)
            for path, values, kind in (("translation", [m[:3, 3] for m in locals_], "VEC3"),
                                       ("rotation", quats, "VEC4")):
                samplers.append({"input": time_acc, "interpolation": "LINEAR",
                                 "output": g.accessor(np.array(values), kind)})
                channels.append({"sampler": len(samplers) - 1, "target": {"node": joint_nodes[gi], "path": path}})
        for gi in zoomed:
            samplers.append({"input": time_acc, "interpolation": "LINEAR",
                             "output": g.accessor(np.array([p.zoom[gi] for p in poses]), "VEC3")})
            channels.append({"sampler": len(samplers) - 1, "target": {"node": geo_nodes[gi], "path": "scale"}})
        g.add("animations", {"name": name, "channels": channels, "samplers": samplers})
    return g.to_bytes()
