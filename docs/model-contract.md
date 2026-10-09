# Model contract: export → model generator → import

This document covers two directions:

- what `make export-models` hands a model generator;
- what the generator must deliver for `make import-models` to accept it.

Every generator follows it, including the in-repo one, `make blender-models`
(see "The in-repo generator: Blender"). Paths are relative to the export
folder (`data/models/`) and the delivery folder (`data/models-ai/`).
Contract version: **1**.

## What export writes

```
data/models/
  manifest.json
  palette.bin                       the game palette as stored (768 bytes)
  bodies/<KEY>/                     one folder per drawable canonical body (75 for AITD1)
    original.glb                    the original body: skeleton, rigid skin, preview animations
    body.bin                        the raw body entry (its SHA-256 is the manifest's body_sha256)
    reference/front.png             1024x1024 RGBA, transparent background
    reference/three_quarter.png
    reference/side.png
    reference/back.png
    reference/views.json            the camera of each view (see "Reference views")
```

A `<KEY>` is the body's HQR and entry, for example `LISTBODY_011` (Carnby
with the lamp) or `LISTBOD2_011` (Emily with the lamp). The engine loads
bodies from `LISTBODY` or `LISTBOD2`, depending on the chosen hero. The two
sets reuse numbers for different meshes, so the key includes the HQR.

Byte-identical bodies (aliases) are exported once, under their canonical
key: the first alias in HQR order (`LISTBODY` before `LISTBOD2`), then by
entry index. The 214 animated bodies of AITD1 hold 76 distinct meshes.

## manifest.json (schema 1)

The top level holds:

- `schema`, `contract`, `game` (`"aitd1"`) and `data_dir`
- `axes`: see "Axes"
- `bind_pose`: always `"rest"`
- `reference_size`
- `views`: name and yaw of each view

`bodies[]` has one record per animated body, canonical or not:

| Field | Meaning |
|---|---|
| `key`, `hqr`, `body` | `LISTBODY_011`, `LISTBODY`, `11` |
| `canonical` | The key whose folder holds this body's export (itself when canonical) |
| `target` | The engine file import writes: `body_<KEY>.hdm` |
| `kind` | `character` (6 or more groups), `prop` (fewer), or `skip` (nothing drawable: no folder) |
| `body_sha256` | SHA-256 of the raw body entry; equal hashes are aliases |
| `skeleton_hash` | FNV-1a 64 of the rest vertices and group hierarchy; the engine checks it before it uses a model |
| `zv` | The engine's collision box, `[x1, x2, y1, y2, z1, z2]`, engine units |
| `height` | Rest-pose height in engine units (about millimetres) |
| `triangles`, `prim_counts` | Size of the original |
| `groups[]` | Per bone group: `parent` (-1 for the root), `pivot_rest` (engine units) and vertex `count` |
| `preview_anims` | The animations baked into `original.glb` (for example `LISTANIM_010`) |
| `aliases` | Other keys with the identical body |
| `skeleton_siblings` | Keys with the same skeleton but different polygons or colours, such as another costume |

## Axes

`original.glb` uses glTF's convention: **Y up, metres, the character faces
+Z, and +X is its left**. The engine's space is y down, facing −z; the two
relate by `gltf = diag(1, −1, −1) · engine · 0.001`.

A delivered model may use any scale and offset: import aligns it. Its
**orientation must match**: Y up, facing +Z.

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

  For each view it also gives `basis_right_down_forward`: three engine-space
  rows for image right, image down and view direction. A point `p` lands at
  pixel `(right·(p−centre), down·(p−centre)) / units_per_pixel + size/2`.

## original.glb

- **Joints.** One node `gNN` per bone group, parented like the groups. A
  group that zooms in a preview animation has a child `gNN_geo`. The child
  carries the zoom, so the group's children do not inherit it.
- **Mesh.** Flat-shaded triangles with `COLOR_0` from the palette. Every
  corner is bound with weight 1 to its own group, as the engine moves it.
- **Animations.** One animation per `preview_anims` entry, keyed at the
  engine's 25 Hz timing. Use them to preview how a group deforms. They are
  not a delivery requirement.

## What the generator delivers

For each canonical body it replaces (typically `kind: character`):

```
data/models-ai/bodies/<KEY>/model.glb
```

| Rule | Value |
|---|---|
| Content | One static mesh (skin and animation are ignored) |
| Pose | The rest pose of `front.png`, arms and legs where the original has them |
| Orientation | Y up, facing +Z, like `original.glb`; any scale and offset |
| Triangles | 50,000 at most (import warns above 30,000) |
| Texture | One base-colour PNG or JPEG, 4096 px at most per side (2048 recommended) |
| Alpha | Optional. Below 128: a hole. 128–252: translucent, blended at its own alpha (128 is the engine's transparent material 2, 50 %). 253 and up: opaque. The engine classifies the unfiltered texel, so a filtered hole edge never blends |
| Extensions | None required: no Draco, meshopt or KTX2 |

Deliver only for canonical keys, the folders under `bodies/`. Import copies
each delivery to the body's aliases. Skeleton siblings are separate bodies
and need their own delivery.

`make check-models` (a dry run) and `make import-models` align the mesh to
the original, check the fit, derive skin weights from the original's bone
groups and write `Assets/models_hd/body_<KEY>.hdm` (see "Import output").

Every build of the game copies `Assets/models_hd` next to the executable
(into the app's `Contents/Resources` on macOS). `make models-install` copies
other models, and the atlases, into the data folder or the app. The game
reads `models_hd/` from the folder it starts in, else from next to the
executable. It draws the models while `graphics.hdModels` is `true`, the
default.

### Running import without the game data

- The make targets read the bodies and the palette from the game data
  (`gamedata`, default `data/aitd1`).
- `tools/models.py import` without `--data` reads `palette.bin` and each
  `body.bin` from the export instead. A generator host can then run the same
  checks with no game data.
- On that path each `body.bin` must match the manifest's `body_sha256`. A
  missing or different one fails that body alone.
- A missing `palette.bin` sends the import back to the game data. An export
  written before these files existed therefore still needs the game data.
- The game-data path checks the skeleton hash only; it never compares the
  SHA-256.

### What import checks

| Check | Fails | Warns |
|---|---|---|
| Triangles | more than 50,000 | more than 30,000 |
| Texture side | more than 4096 px | more than 2048 px |
| Base-colour images | not exactly one, not embedded in the `.glb`, or not PNG/JPEG | |
| `extensionsRequired` | any | |
| Primitives | anything but triangles | |
| Fit (chamfer p95, as % of the body's size: its largest extent, a standing character's height) | more than 4 % | |
| Silhouette IoU, worst reference view | less than 0.85 | |
| Collision box | | the mesh reaches outside ZV + 10 % |
| Binding | | more than 1 % of vertices as close to an unrelated part |
| Posed stretch: the bound mesh at every key of `original.glb`'s preview animations | more than 0.1 % of the surface area has an edge over 10x its rest length | |

**Posed stretch** catches limbs the generator fused together: a bridge
between two legs, or between an arm and the torso, tears when they move
apart. The originals stretch where a polygon spans two groups, but:

- none of the 42 AITD1 characters goes beyond 9.2x;
- none of their identity imports goes beyond 9.1x;
- Carnby with his legs fused into one hull tears 0.88 % of his area beyond
  10x.

A body without preview animations skips the check (`"stretch": "no
animation"` in its report).

**The fit** is a similarity transform. Import undoes any scale, offset or
yaw, but not a mirrored or posed mesh. The scale comes from the height
alone: feet to top of head equals the original's. Import then refines
rotation and offset, and puts the feet back on the original's.

**The identity check.** `make identity-models bodies=KEY` writes each
original as its own delivery (the original mesh, its palette colours in a
16x16 texture) into `data/models-identity/`, never into `data/models-ai/`.
It must import back onto itself: this is the pipeline's end-to-end check.
Import it away from the tracked `Assets/models_hd/`:

```
make identity-models bodies=LISTBODY_011
make import-models models_ai=data/models-identity models_hd=data/models-hd-identity
```

### Import reports

For every body it looks at, imported or failed, import writes
`body_<KEY>.json`. It holds:

- `status` and `reason` (why it failed);
- `warnings`;
- `metrics`: every fit metric;
- `files`: the `.hdm` names it wrote, or would have written.

Stretch metrics:

- A body with preview animations reports `stretch_torn_pct` and
  `stretch_max`, even when the fit failed: binding and the stretch check run
  before the fit failure is returned. So a rejected body still reports
  whether its fused limbs tear.
- A body without preview animations reports `"stretch": "no animation"`
  instead.
- On a body that failed the fit, trust `stretch_torn_pct` only as far as
  `ambiguous_pct` allows. A mesh that does not fit binds unreliably, so some
  of its tearing can come from the binding, not the mesh.

Where files go:

| Run | `.hdm` | Debug `.glb` | Reports |
|---|---|---|---|
| Real import | `--dest` (`models_hd=DIR`) | `--debug` (default `data/models-hd-debug/`) | Beside the debug `.glb`, or in `--report` when given |
| Dry run (`make check-models`) | none | none | Only in `--report` when given |

`report=DIR` passes `--report DIR` to both make targets.

A report says what the check decided, not what reached the disk:

- A dry run fills it exactly as a real import does: `status: "imported"`
  and the `.hdm` names in `files` for every body that passed.
- `files` lists bare names, one per file import would write (the body's
  own, plus one per alias), without the engine folder.
- Neither `status` nor `files` says a file was written. To know a delivery
  is clean, a gate reads the process exit code: 0 nothing failed, 1 at
  least one body failed, 2 a usage or data error. To find the `.hdm` files,
  look in the output folder.

## The in-repo generator: Blender

`make blender-models [bodies=KEY,...] [BLENDER=PATH]` delivers every
canonical character body (`kind: character`) into `data/models-ai/`. Then
`make check-models` and `make import-models` judge and import it like any
delivery. It needs the export and Blender (5.2 or later; `BLENDER` defaults
to the macOS app), not the game data.

Per body (`tools/aitd_models/blender/`):

1. `remaster.py` (outside Blender) works out, per original triangle:
   - the engine's atlas UVs for the front and the back painting;
   - how much of each it takes;
   - which hand-made atlas in `Assets/atlases/` paints it (`body_`/`flat_`,
     `ramp_`, `other_`, the key's own or an alias's).

   It does all this as `modelAtlas.cpp` and `renderer.cpp` do. Only plain
   polygons take an atlas, never a transparent one (glass). The rest keep
   their palette colour.
2. `stage.py` (headless Blender):
   - splits the original into one piece per bone group;
   - subdivides each piece with its open and sharp edges creased, and pulls
     it back onto its own original surface. A triangle that spans groups
     stays as it is: the engine stretches it, and subdivided it would tear
     past the import's stretch check;
   - gives every group, unless an edit sets it, the highest level whose
     predicted body count stays within 30,000. The prediction is the
     spanning triangles plus 6·4^(L−1) for each other triangle;
   - gives each connected island one winding and turns it to the side that
     sees out of the body (the originals mostly face inward);
   - keeps where each vertex sat on its subdivided piece before the pull,
     creased on the piece's open edges only (the round surface). The
     delivered normals come from it, and the ambient occlusion is baked with
     them, so the body shades as rounded forms while its vertices stay on
     the original's polygons;
   - unwraps the result and bakes onto it the textured original, its ambient
     occlusion and, for a body with transparent polygons or spheres (the
     glass kind), a mask of them.
3. `remaster.py` evens out the colour bake's brightness steps (the hand-made
   atlases paint crumpled low-poly facets; a self-guided filter on log
   brightness over 24-texel windows of the 2048 px bake flattens the weak
   steps, keeps strong detail such as faces and buttons, and never changes
   hue), then composites the bakes into one 2048 px PNG, with the mask as
   alpha 128, and writes `model.glb` with the round surface's normals.

Working files go to `data/models-blender/<KEY>/`. `data/models-ai/run.md`
lists every body: delivered (triangles, texture source, seconds), failed
(the error) or skipped.

**Edits.** `tools/aitd_models/blender/edits/<KEY>.json` adjusts one body.
None is needed by default. All fields are optional; an unknown one fails
the body:

| Field | Meaning |
|---|---|
| `subdivide` | `{"gNN": level}`: a group's level (0..4) instead of the budget's |
| `crease` | `["gNN", ...]`: groups kept flat (plain subdivision) |
| `projection` | `{"gNN": "front" \| "back" \| "blend" \| "palette"}`: which painting a group takes |
| `skip` | `{"reason": "..."}`: no delivery; the body stays classic |

**Through MCP for Blender.** First run `make blender-models bodies=KEY` to
write the body's working files. Then run the same stage in the open Blender,
which keeps its scene up to inspect:

```python
import sys; sys.path.insert(0, "<repo>/tools/aitd_models/blender")
import stage; stage.run("<repo>/data/models-blender/LISTBODY_011", keep=True)
```

## Import output: body_<KEY>.hdm

The engine reads one file per body. Import writes it once per canonical
delivery and copies it to every alias. Little-endian, no padding:

| Offset | Field |
|---|---|
| 0 | `char[4]` magic `AHDM` |
| 4 | `u16` version, 1 |
| 6 | `u16` group count (1..32; must equal the body's) |
| 8 | `u64` skeleton hash (the manifest's `skeleton_hash`; the engine compares it with the loaded body's) |
| 16 | `u32` vertex count (3..150,000), `u32` index count (a multiple of 3, 3..150,000) |
| 24 | `u32` texture bytes (1 byte..64 MiB), `u8` texture kind (1 PNG, 2 JPEG), `u8` flags (1: some texels are translucent, so the engine draws a blended pass; other bits zero), 2 bytes that must be zero |
| 32 | vertices, 40 bytes each: `f32x3` position, `f32x3` normal, `f32x2` uv, `u8x4` joints, `u8x4` weights |
| … | `u32` indices, then the texture bytes as delivered |
| end − 4 | `u32` CRC-32 (zlib) of every byte before it |

Positions and normals are in engine space and the rest pose (y down, facing
−z, engine units). Joints are bone groups. The four weights sum to exactly
255; an unused slot has weight 0 and joint 0. UVs keep glTF's convention
(origin at the image's top left).
