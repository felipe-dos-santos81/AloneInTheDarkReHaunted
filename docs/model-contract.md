# Model contract: export → image-to-3D generator → import

This document covers two directions:

- what `make export-models` hands a model generator;
- what the generator must deliver for `make import-models` to accept.

It is the reference for external generators such as `aitd-texture-enhancement`.
Paths are relative to the export folder (`data/models/`) and the delivery
folder (`data/models-ai/`). Contract version: **1**.

## What export writes

```
data/models/
  manifest.json
  bodies/<KEY>/                     one folder per canonical body (76 for AITD1)
    original.glb                    the original body: skeleton, rigid skin, preview animations
    reference/front.png             1024x1024 RGBA, transparent background
    reference/three_quarter.png
    reference/side.png
    reference/back.png
    reference/views.json            the camera of each view (see "Reference views")
```

A `<KEY>` is the body's HQR and entry, for example `LISTBODY_011` (Carnby
with the lamp) or `LISTBOD2_011` (Emily with the lamp). The engine loads
bodies from `LISTBODY` or `LISTBOD2` depending on the chosen hero. The two
sets reuse numbers for different meshes, so the HQR is part of the key.

Byte-identical bodies (aliases) are exported once, under their canonical
key. The canonical key is the first alias in HQR order (`LISTBODY` before
`LISTBOD2`), then by entry index. The 214 animated bodies of AITD1 hold 76
distinct meshes.

## manifest.json (schema 1)

The top level holds:

- `schema`, `contract`, `game` (`"aitd1"`) and `data_dir`
- `axes`: see "Axes"
- `bind_pose`: always `"rest"`
- `reference_size`
- `views`: name and yaw of each view

`bodies[]` has one record for every animated body, canonical or not:

| Field | Meaning |
|---|---|
| `key`, `hqr`, `body` | `LISTBODY_011`, `LISTBODY`, `11` |
| `canonical` | The key whose folder holds this body's export (itself when canonical) |
| `target` | The engine file the import will write: `body_<KEY>.glb` |
| `kind` | `character` (6 or more groups), `prop` (fewer), or `skip` (nothing drawable: no folder) |
| `body_sha256` | SHA-256 of the raw body entry; equal hashes are aliases |
| `skeleton_hash` | FNV-1a 64 of the rest vertices and group hierarchy; the engine checks it before using a model |
| `zv` | The engine's collision box, `[x1, x2, y1, y2, z1, z2]`, engine units |
| `height` | Rest-pose height in engine units (about millimetres) |
| `triangles`, `prim_counts` | Size of the original |
| `groups[]` | Per bone group: `parent` (-1 for the root), `pivot_rest` (engine units), and vertex `count` |
| `preview_anims` | The animations baked into `original.glb` (for example `LISTANIM_010`) |
| `aliases` | Other keys with the identical body |
| `skeleton_siblings` | Keys with the same skeleton but different polygons or colours, such as another costume |

## Axes

`original.glb` uses glTF's own convention: **Y up, metres, the character
faces +Z, and +X is its left**. It relates to the engine's space (y down,
facing −z) by `gltf = diag(1, −1, −1) · engine · 0.001`.

A delivered model may use any scale and offset, because import aligns it.
Its **orientation must match**: Y up, facing +Z.

## Reference views

The four views are orthographic and share one scale. Each is a yaw about the
vertical axis:

| View | Yaw | Shows |
|---|---|---|
| `front` | 0° | The face |
| `three_quarter` | 45° | Front-right |
| `side` | 90° | The right profile, facing image-right |
| `back` | 180° | The back |

- **Colours** are the palette colours, shaded by a headlight.
- **Not drawn:** single-pixel points. Lines are drawn as thin prisms and
  spheres as spheres.
- **`views.json`** gives the shared `framing`:
  - `centre` (engine units, at the image centre)
  - `units_per_pixel`
  - `size`

  It also gives each view's `basis_right_down_forward`: three engine-space
  rows for image right, image down and view direction. A point `p` lands at
  pixel `(right·(p−centre), down·(p−centre)) / units_per_pixel + size/2`.

## original.glb

- **Joints.** One node `gNN` per bone group, parented like the groups. A
  group that zooms in a preview animation has a child `gNN_geo`, which
  carries the zoom so the group's children do not inherit it.
- **Mesh.** Flat-shaded triangles with `COLOR_0` from the palette. Every
  corner is bound with weight 1 to its own group, the way the engine moves
  it.
- **Animations.** One animation per `preview_anims` entry, with keys at the
  engine's 25 Hz timing. Use them to preview how a group deforms; they are
  not a delivery requirement.

## What the generator delivers

For every canonical body it wants to replace (typically `kind: character`):

```
data/models-ai/bodies/<KEY>/model.glb
```

| Rule | Value |
|---|---|
| Content | One static mesh (no skin or animation needed; they are ignored) |
| Pose | The rest pose of `front.png`, arms and legs where the original has them |
| Orientation | Y up, facing +Z, like `original.glb`; any scale and offset |
| Triangles | 50,000 at most (import warns above 30,000) |
| Texture | One base-colour PNG or JPEG, 4096 px at most per side (2048 recommended) |
| Extensions | None required: no Draco, meshopt or KTX2 |

Deliver only for canonical keys, the folders under `bodies/`. Import copies
each delivery to the body's aliases. Skeleton siblings are separate bodies
and need their own delivery.

Import (planned: `make check-models` / `make import-models`) aligns the mesh
to the original. It then derives skin weights from the original's bone
groups and writes `Assets/models_hd/body_<KEY>.glb`.
