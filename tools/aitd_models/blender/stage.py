# SPDX-License-Identifier: GPL-2.0-only
"""The Blender half of the in-repo generator. Runs inside Blender only:

    Blender --background --factory-startup --python stage.py -- WORK_DIR

or, through MCP for Blender's execute_blender_code, `stage.run(WORK_DIR,
keep=True)`, which works in a new scene of its own and leaves it open for a
look. WORK_DIR holds job.json and corners.npz from remaster.py; the stage
writes refined.npz, color.npy, ao.npy and, for a body with transparent
triangles, mask.npy there. It imports only Blender's
own modules and numpy."""
import json
import math
import pathlib
import sys

import bmesh
import bpy
import numpy as np
from mathutils.bvhtree import BVHTree

KINDS = ("palette", "body", "ramp", "other")


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
    me.attributes.new("transparent", "FLOAT", "CORNER").data.foreach_set(
        "value", np.repeat(corners["transparent"], 3).astype(np.float32))
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
    fully back onto that group's original faces."""
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


def _orient_outward(model: bpy.types.Object) -> int:
    """The originals mix both windings (most of a body faces inward), and the
    engine lights an HD face on its front only, as the AO bake does. Flip each
    face whose front sees more of the body than its back, or as much but
    nearer (the nearer wall is the inside); returns how many."""
    bm = bmesh.new()
    bm.from_mesh(model.data)
    bm.normal_update()
    tree = BVHTree.FromBMesh(bm)
    flip = []
    for face in bm.faces:
        n = face.normal
        if n.length == 0:
            continue
        centre, t = face.calc_center_median(), n.orthogonal().normalized()
        b = n.cross(t)
        front = _hits(tree, centre, (n, t, b), 1)
        if not front:
            continue  # an open front already faces outward
        back = _hits(tree, centre, (n, t, b), -1)
        if len(front) > len(back) or (len(front) == len(back) and min(front) < min(back)):
            flip.append(face)
    bmesh.ops.reverse_faces(bm, faces=flip)
    bm.to_mesh(model.data)
    bm.free()
    return len(flip)


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
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.quads_convert_to_tris()
    bpy.ops.object.mode_set(mode="OBJECT")
    # orient after triangulating: splitting an oriented quad turned 8 % of the ghost's triangles inward
    print(f"oriented {_orient_outward(model)} faces outward", flush=True)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=1.15, island_margin=0.003)
    bpy.ops.object.mode_set(mode="OBJECT")
    return model


def _mask_material() -> bpy.types.Material:
    """Emission of the source's "transparent" attribute: 1 on the engine's
    transparent material 2, else 0."""
    mat = bpy.data.materials.new("transparent")
    nt = mat.node_tree
    nt.nodes.clear()
    out, emit, attr = (nt.nodes.new(t) for t in ("ShaderNodeOutputMaterial", "ShaderNodeEmission", "ShaderNodeAttribute"))
    attr.attribute_name = "transparent"
    nt.links.new(attr.outputs["Fac"], emit.inputs["Color"])
    nt.links.new(emit.outputs[0], out.inputs["Surface"])
    return mat


def _bake(model, src, job: dict, translucent: bool):
    """The source's emission onto the model (selected to active), then the
    model's own ambient occlusion and, for a body with transparent
    triangles, the source's transparency mask: linear float images, rows
    bottom-up. Returns the colour, AO and mask (or None) arrays and the
    emission image."""
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    size = job["bake_size"]
    target = bpy.data.materials.new("bake")
    node = target.node_tree.nodes.new("ShaderNodeTexImage")
    model.data.materials.clear()
    model.data.materials.append(target)
    out, images = [], []
    passes = [("EMIT", "EMIT", job["samples_emit"], True), ("AO", "AO", job["samples_ao"], False)]
    if translucent:
        passes.append(("MASK", "EMIT", job["samples_emit"], True))
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
        saved = [slot.material for slot in src.material_slots]
        if name == "MASK":
            mask = _mask_material()
            for slot in src.material_slots:
                slot.material = mask
        bpy.ops.object.bake(type=kind, margin=8, **extra)
        for slot, material in zip(src.material_slots, saved):
            slot.material = material
        pixels = np.empty(size * size * 4, np.float32)
        image.pixels.foreach_get(pixels)  # no 16M-float Python list
        out.append(pixels.reshape(size, size, 4))
    return out[0][..., :3], out[1][..., 0], out[2][..., 0] if translucent else None, images[0]


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
    colour, ao, mask, emission = _bake(model, src, job, bool(corners["transparent"].any()))
    me = model.data
    world = np.array(model.matrix_world)
    co = np.zeros(len(me.vertices) * 3, np.float32)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3) @ world[:3, :3].T + world[:3, 3]
    loop_vertex = np.zeros(len(me.loops), np.int32)
    me.loops.foreach_get("vertex_index", loop_vertex)
    loop_uv = np.zeros(len(me.loops) * 2, np.float32)
    me.uv_layers.active.data.foreach_get("uv", loop_uv)
    np.savez(work / "refined.npz", positions=co, loop_vertex=loop_vertex, loop_uv=loop_uv.reshape(-1, 2))
    np.save(work / "color.npy", colour.astype(np.float16))
    np.save(work / "ao.npy", ao.astype(np.float16))
    if mask is not None:
        np.save(work / "mask.npy", mask.astype(np.float16))
    if keep:
        _show(model, src, emission)
    else:
        bpy.ops.wm.read_factory_settings(use_empty=True)


if __name__ == "__main__":
    run(sys.argv[sys.argv.index("--") + 1])
