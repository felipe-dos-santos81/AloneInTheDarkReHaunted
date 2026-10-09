# SPDX-License-Identifier: GPL-2.0-only
"""The Blender half of the in-repo generator. Runs inside Blender only:

    Blender --background --factory-startup --python stage.py -- WORK_DIR

or, through MCP for Blender's execute_blender_code, `stage.run(WORK_DIR,
keep=True)`, which works in a new scene of its own and leaves it open for a
look. WORK_DIR holds job.json and corners.npz from remaster.py; the stage
writes refined.npz (positions, the round surface its normals come from and,
for a body that has glass, the transparency mask),
color.npy and ao.npy there. It imports only Blender's
own modules and numpy."""
import json
import math
import pathlib
import sys

import bmesh
import bpy
import numpy as np
from mathutils.bvhtree import BVHTree

KINDS = ("palette", "body", "ramp", "other", "glass")  # remaster.KIND_NAMES


def _socket(sockets, identifier: str):
    """A Mix node's colour sockets share their names with its float ones."""
    return next(s for s in sockets if s.identifier == identifier)


def _source(job: dict, corners) -> bpy.types.Object:
    """original.glb's mesh at rest, its armature removed, textured the way
    the engine paints it: per corner the front and back atlas UVs, a front
    weight and the linear palette colour; per face the atlas kind."""
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=job["original"])
    imported = [o for o in bpy.data.objects if o not in before]
    src = next(o for o in imported if o.type == "MESH" and "g00" in o.vertex_groups)
    world = src.matrix_world.copy()
    for o in imported:  # the armature and any sphere helper: never anything that was open before
        if o is not src:
            bpy.data.objects.remove(o, do_unlink=True)
    src.parent = None
    src.matrix_world = world
    for m in list(src.modifiers):
        src.modifiers.remove(m)
    me = src.data
    count = len(me.polygons)
    if count != len(corners["kind"]):
        raise RuntimeError(f"original.glb has {count} triangles, corners.npz {len(corners['kind'])}")
    for name in ("front", "back"):
        uv = corners[f"uv_{name}"]
        me.uv_layers.new(name=name).data.foreach_set("uv", np.column_stack([uv[:, 0], 1.0 - uv[:, 1]]).ravel())
    me.attributes.new("w_front", "FLOAT", "CORNER").data.foreach_set("value", np.repeat(corners["w_front"], 3))
    rgba = np.column_stack([np.repeat(corners["palette"], 3, axis=0), np.ones(3 * count)]).astype(np.float32)
    me.attributes.new("pal", "FLOAT_COLOR", "CORNER").data.foreach_set("color", rgba.ravel())
    me.materials.clear()  # the import's own glTF material would shift every index
    for name in KINDS:
        me.materials.append(_material(name, job["atlases"].get(name)))
    me.polygons.foreach_set("material_index", corners["kind"].astype(np.int32))
    return src


def _material(name: str, atlas: str | None) -> bpy.types.Material:
    """Emission of the palette colour, or of the atlas over it by alpha,
    front and back samples mixed by the front weight."""
    mat = bpy.data.materials.new(name)
    nt = mat.node_tree
    nt.nodes.clear()
    out, emit = nt.nodes.new("ShaderNodeOutputMaterial"), nt.nodes.new("ShaderNodeEmission")
    nt.links.new(emit.outputs[0], out.inputs["Surface"])
    pal = nt.nodes.new("ShaderNodeAttribute")
    pal.attribute_name = "pal"
    if atlas is None:
        nt.links.new(pal.outputs["Color"], emit.inputs["Color"])
        return mat
    image = bpy.data.images.load(atlas)
    image.alpha_mode = "STRAIGHT"
    sides = []
    for uv_name in ("front", "back"):
        uv = nt.nodes.new("ShaderNodeUVMap")
        uv.uv_map = uv_name
        tex = nt.nodes.new("ShaderNodeTexImage")
        tex.image, tex.interpolation, tex.extension = image, "Linear", "EXTEND"
        nt.links.new(uv.outputs[0], tex.inputs[0])
        over = nt.nodes.new("ShaderNodeMix")
        over.data_type = "RGBA"
        nt.links.new(tex.outputs["Alpha"], _socket(over.inputs, "Factor_Float"))
        nt.links.new(pal.outputs["Color"], _socket(over.inputs, "A_Color"))
        nt.links.new(tex.outputs["Color"], _socket(over.inputs, "B_Color"))
        sides.append(over)
    weight = nt.nodes.new("ShaderNodeAttribute")
    weight.attribute_name = "w_front"
    mix = nt.nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    nt.links.new(weight.outputs["Fac"], _socket(mix.inputs, "Factor_Float"))
    nt.links.new(_socket(sides[1].outputs, "Result_Color"), _socket(mix.inputs, "A_Color"))
    nt.links.new(_socket(sides[0].outputs, "Result_Color"), _socket(mix.inputs, "B_Color"))
    nt.links.new(_socket(mix.outputs, "Result_Color"), emit.inputs["Color"])
    return mat


def _piece(src: bpy.types.Object, faces: np.ndarray, level: int, flat: bool, job: dict) -> bpy.types.Object:
    """One group's faces as a surface, subdivided with its open and sharp
    edges creased (or plainly, when the group is kept flat), then pulled
    fully back onto that group's original faces. Its point attribute
    `round` holds where each vertex sits on the subdivided surface before
    the pull, creased on its open edges only: the shape its normals come
    from (the positions themselves, for a piece with no subdivision or kept
    flat)."""
    bm = bmesh.new()
    bm.from_mesh(src.data)
    bm.faces.ensure_lookup_table()
    keep = set(faces.tolist())
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.index not in keep], context="FACES")
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=job["merge_distance"])
    crease = bm.edges.layers.float.get("crease_edge") or bm.edges.layers.float.new("crease_edge")
    for e in bm.edges:
        if e.is_boundary or e.calc_face_angle(0.0) > job["crease_angle"]:
            e[crease] = 1.0
    open_edges = np.array([e.is_boundary for e in bm.edges], np.float32)
    mesh = bpy.data.meshes.new("piece")
    bm.to_mesh(mesh)
    bm.free()
    target = bpy.data.objects.new("target", mesh.copy())
    piece = bpy.data.objects.new("piece", mesh)
    for o in (target, piece):
        bpy.context.scene.collection.objects.link(o)
        o.matrix_world = src.matrix_world
    if level > 0:
        sub = piece.modifiers.new("subdivide", "SUBSURF")
        sub.levels = sub.render_levels = level
        if flat:
            sub.subdivision_type = "SIMPLE"
    wrap = piece.modifiers.new("pull", "SHRINKWRAP")
    wrap.target, wrap.wrap_method = target, "NEAREST_SURFACEPOINT"
    done = bpy.data.objects.new("refined", bpy.data.meshes.new_from_object(piece.evaluated_get(bpy.context.evaluated_depsgraph_get())))
    bpy.context.scene.collection.objects.link(done)
    done.matrix_world = src.matrix_world
    shape = np.empty(len(done.data.vertices) * 3, np.float32)
    done.data.vertices.foreach_get("co", shape)
    if level > 0 and not flat:
        mesh.attributes["crease_edge"].data.foreach_set("value", open_edges)
        mesh.update()
        piece.modifiers.remove(wrap)
        smooth = piece.evaluated_get(bpy.context.evaluated_depsgraph_get()).data
        if len(smooth.vertices) != len(done.data.vertices):
            raise RuntimeError(f"the round surface has {len(smooth.vertices)} vertices, the piece {len(done.data.vertices)}")
        smooth.vertices.foreach_get("co", shape)
    done.data.attributes.new("round", "FLOAT_VECTOR", "POINT").data.foreach_set("vector", shape)
    bpy.data.objects.remove(piece)
    bpy.data.objects.remove(target)
    return done


# Directions a face looks along to see which of its sides is open: its normal,
# and six more 45 degrees off it (along, tangent, bitangent).
RAYS = [(1.0, 0.0, 0.0)] + [(math.cos(math.pi / 4), math.sin(math.pi / 4) * math.cos(k * math.pi / 3),
                             math.sin(math.pi / 4) * math.sin(k * math.pi / 3)) for k in range(6)]


def _hits(tree, centre, frame, side: int) -> list[float]:
    """Distances to what the RAYS see from `centre`, on the `side` (1 front,
    -1 back) of the face whose normal, tangent and bitangent are `frame`."""
    n, t, b = frame
    casts = (tree.ray_cast(centre + n * (side * 1e-5), (n * a + t * u + b * v) * side)[3] for a, u, v in RAYS)
    return [d for d in casts if d is not None]


def _islands(bm) -> list[list]:
    """The faces of `bm` in edge-connected islands."""
    seen, islands = set(), []
    for start in bm.faces:
        if start in seen:
            continue
        seen.add(start)
        stack, island = [start], []
        while stack:
            face = stack.pop()
            island.append(face)
            for edge in face.edges:
                for other in edge.link_faces:
                    if other not in seen:
                        seen.add(other)
                        stack.append(other)
        islands.append(island)
    return islands


VOTERS = 200  # faces of an island that cast rays, evenly spread


def _orient_outward(model: bpy.types.Object) -> int:
    """Triangulate, then face every island of the mesh outward: the originals
    mix both windings (most of a body faces inward), and the engine lights an
    HD face on its front only, as the AO bake does. Each island first gets one
    winding, then flips when its faces' fronts see more of the body than their
    backs, or as much but nearer (the nearer wall is the inside); returns how
    many faces end up flipped from the original winding."""
    bm = bmesh.new()
    bm.from_mesh(model.data)
    bmesh.ops.triangulate(bm, faces=bm.faces[:], quad_method="BEAUTY", ngon_method="BEAUTY")
    bm.normal_update()
    before = {face: face.normal.copy() for face in bm.faces}
    islands = _islands(bm)
    for island in islands:
        bmesh.ops.recalc_face_normals(bm, faces=island)
    bm.normal_update()
    tree = BVHTree.FromBMesh(bm)
    flip = []
    for island in islands:
        votes, nearest = [0, 0], [math.inf, math.inf]
        # ponytail: a sample of VOTERS faces decides a large island; vote them all if one ever comes out wrong
        for face in island[::max(1, len(island) // VOTERS)]:
            n = face.normal
            if n.length == 0:
                continue
            centre, t = face.calc_center_median(), n.orthogonal().normalized()
            b = n.cross(t)
            for k, side in enumerate((1, -1)):
                hits = _hits(tree, centre, (n, t, b), side)
                votes[k] += len(hits)
                nearest[k] = min([nearest[k], *hits])
        if votes[0] > votes[1] or (votes[0] == votes[1] and nearest[0] < nearest[1]):
            flip.extend(island)
    bmesh.ops.reverse_faces(bm, faces=flip)
    bm.normal_update()
    changed = sum(face.normal.dot(before[face]) < 0 for face in bm.faces)
    bm.to_mesh(model.data)
    bm.free()
    return changed


def _refined(src: bpy.types.Object, corners, job: dict) -> bpy.types.Object:
    groups = corners["tri_group"]
    # a bridge (group -1) spans groups and stays the original triangles: the engine stretches it
    parts = [_piece(src, np.flatnonzero(groups == g), job["levels"][g] if g >= 0 else 0, g < 0 or g in job["flat"], job)
             for g in sorted(set(groups.tolist()))]
    bpy.ops.object.select_all(action="DESELECT")
    for p in parts:
        p.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()  # one object; its pieces stay unconnected
    model = bpy.context.view_layer.objects.active
    model.name = "model"
    print(f"oriented {_orient_outward(model)} faces outward", flush=True)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=1.15, island_margin=0.003)
    bpy.ops.object.mode_set(mode="OBJECT")
    return model


def _mask_material(value: float) -> bpy.types.Material:
    """A plain emission of `value`: 1 for the glass kind (the engine's
    transparent material 2), 0 for the others."""
    mat = bpy.data.materials.new(f"mask {value:g}")
    nt = mat.node_tree
    nt.nodes.clear()
    out, emit = nt.nodes.new("ShaderNodeOutputMaterial"), nt.nodes.new("ShaderNodeEmission")
    emit.inputs["Color"].default_value = (value, value, value, 1.0)
    nt.links.new(emit.outputs[0], out.inputs["Surface"])
    return mat


def _shade_round(model: bpy.types.Object) -> None:
    """Shade the model smooth with the round surface's normals: per vertex
    the area-weighted normal of its `round` attribute over the model's
    triangles, the normals remaster.model_glb delivers."""
    me = model.data
    shape = np.zeros(len(me.vertices) * 3, np.float32)
    me.attributes["round"].data.foreach_get("vector", shape)
    shape = shape.reshape(-1, 3)
    corner = np.zeros(len(me.loops), np.int32)
    me.loops.foreach_get("vertex_index", corner)
    tri = corner.reshape(-1, 3)  # _orient_outward triangulated the model
    face = np.cross(shape[tri[:, 1]] - shape[tri[:, 0]], shape[tri[:, 2]] - shape[tri[:, 0]])
    normal = np.zeros_like(shape)
    for k in range(3):
        np.add.at(normal, tri[:, k], face)
    normal /= np.maximum(np.linalg.norm(normal, axis=1, keepdims=True), 1e-12)
    me.shade_smooth()
    me.normals_split_custom_set_from_vertices(normal.tolist())


def _bake(model, src, job: dict, translucent: bool):
    """The source's emission onto the model (selected to active), for a body
    with transparent triangles the source's transparency mask, then the
    model's own ambient occlusion, shaded with the round surface's normals:
    linear float images, rows bottom-up. Returns the colour, AO and mask (or
    None) arrays and the emission image."""
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    size = job["bake_size"]
    target = bpy.data.materials.new("bake")
    node = target.node_tree.nodes.new("ShaderNodeTexImage")
    model.data.materials.clear()
    model.data.materials.append(target)
    out, images = {}, []
    passes = [("EMIT", "EMIT", job["samples_emit"], True)]
    if translucent:
        passes.append(("MASK", "EMIT", 1, True))  # 0 or 1 per texel, thresholded at 0.5: one sample is enough
    passes.append(("AO", "AO", job["samples_ao"], False))  # last: the round normals stay on the model after it
    for name, kind, samples, from_source in passes:
        image = bpy.data.images.new(name, size, size, alpha=False, float_buffer=True)
        node.image = image
        images.append(image)
        target.node_tree.nodes.active = node
        scene.cycles.samples = samples
        src.hide_render = not from_source
        bpy.ops.object.select_all(action="DESELECT")
        if from_source:
            src.select_set(True)
        model.select_set(True)
        bpy.context.view_layer.objects.active = model
        extra = {"use_selected_to_active": True, "cage_extrusion": job["cage"], "max_ray_distance": job["ray"]} if from_source else {}
        if name == "MASK":  # the source keeps the mask materials after it: AO does not read the source
            for slot, kind_name in zip(src.material_slots, KINDS):
                slot.material = _mask_material(1.0 if kind_name == "glass" else 0.0)
        if name == "AO":
            _shade_round(model)
        bpy.ops.object.bake(type=kind, margin=8, **extra)
        pixels = np.empty(size * size * 4, np.float32)
        image.pixels.foreach_get(pixels)  # no 16M-float Python list
        out[name] = pixels.reshape(size, size, 4)
    return out["EMIT"][..., :3], out["AO"][..., 0], out["MASK"][..., 0] if translucent else None, images[0]


def _show(model, src, image) -> None:
    """For a stage left open: the model shows its emission bake and the
    source, which sits in the same place, steps aside."""
    tree = model.data.materials[0].node_tree
    tex = next(n for n in tree.nodes if n.type == "TEX_IMAGE")
    tex.image = image
    emit = tree.nodes.new("ShaderNodeEmission")
    tree.links.new(tex.outputs["Color"], emit.inputs["Color"])
    tree.links.new(emit.outputs[0], next(n for n in tree.nodes if n.type == "OUTPUT_MATERIAL").inputs["Surface"])
    src.hide_viewport = src.hide_render = True


def run(work, keep: bool = False) -> None:
    if not keep and not bpy.app.background:
        raise RuntimeError("stage.run in a live Blender needs keep=True; it would reset the open file")
    work = pathlib.Path(work)
    job = json.loads((work / "job.json").read_text())
    corners = np.load(work / "corners.npz")
    if keep:  # an open Blender: work in a scene of its own, leave the user's alone
        bpy.context.window.scene = bpy.data.scenes.new(f"remaster {job['key']}")
    else:
        bpy.ops.wm.read_factory_settings(use_empty=True)
    src = _source(job, corners)
    model = _refined(src, corners, job)
    colour, ao, mask, emission = _bake(model, src, job, bool((corners["kind"] == KINDS.index("glass")).any()))
    me = model.data
    world = np.array(model.matrix_world)
    co = np.zeros(len(me.vertices) * 3, np.float32)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3) @ world[:3, :3].T + world[:3, 3]
    loop_vertex = np.zeros(len(me.loops), np.int32)
    me.loops.foreach_get("vertex_index", loop_vertex)
    loop_uv = np.zeros(len(me.loops) * 2, np.float32)
    me.uv_layers.active.data.foreach_get("uv", loop_uv)
    shape = np.zeros(len(me.vertices) * 3, np.float32)
    me.attributes["round"].data.foreach_get("vector", shape)
    shape = shape.reshape(-1, 3) @ world[:3, :3].T + world[:3, 3]
    refined = {"positions": co, "round": shape, "loop_vertex": loop_vertex, "loop_uv": loop_uv.reshape(-1, 2)}
    if mask is not None:
        refined["mask"] = mask.astype(np.float16)
    np.savez(work / "refined.npz", **refined)
    np.save(work / "color.npy", colour.astype(np.float16))
    np.save(work / "ao.npy", ao.astype(np.float16))
    if keep:
        _show(model, src, emission)
    else:
        bpy.ops.wm.read_factory_settings(use_empty=True)


if __name__ == "__main__":
    run(sys.argv[sys.argv.index("--") + 1])
