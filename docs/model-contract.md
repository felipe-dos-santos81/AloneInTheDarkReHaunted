# Model contract: export → model generator → import

This document covers two directions:

- what `make export-models` hands a model generator;
- what the generator must deliver for `make import-models` to accept.

It is the reference for any generator, such as the in-repo one,
`make blender-models` (see "The in-repo generator: Blender").
Paths are relative to the export folder (`data/models/`) and the delivery
folder (`data/models-ai/`). Contract version: **1**.

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
| `target` | The engine file the import will write: `body_<KEY>.hdm` |
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
| Alpha | Optional. Below 128 a hole; 128–252 translucent, blended at its own alpha (128 is the engine's transparent material 2, 50 %); from 253 opaque. The engine tells the classes apart on the unfiltered texel, so a filtered hole edge never blends |
| Extensions | None required: no Draco, meshopt or KTX2 |

Deliver only for canonical keys, the folders under `bodies/`. Import copies
each delivery to the body's aliases. Skeleton siblings are separate bodies
and need their own delivery.

`make check-models` (a dry run) and `make import-models` align the mesh to
the original, check the fit, derive skin weights from the original's bone
groups and write `Assets/models_hd/body_<KEY>.hdm` (see "Import output").
Every build of the game copies `Assets/models_hd` and `Assets/atlases` next to
it (into the app bundle on macOS), and `make run` copies them into the data
folder elsewhere (`make models-install` copies other folders). The game reads
`models_hd/` from the folder it starts in, or else next to the executable, and
draws them with `graphics.hdModels = true`, the default.

The make targets read the bodies and the palette from the game data.
`tools/models.py import` without `--data` reads `palette.bin` and each
`body.bin` from the export instead, so a generator host can run the same
checks with no game data. On that path, every `body.bin` is checked against
the manifest's `body_sha256`, so one that is missing or differs fails that
body alone, while a missing `palette.bin` sends the import back to the game
data (checked by skeleton hash alone: the game-data path never compares the
SHA-256). An export written before these files existed still needs the game
data.

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

The posed stretch catches limbs the generator fused together: a bridge
between two legs, or an arm and the torso, tears when they move apart. The
originals stretch where a polygon spans two groups, but none of the 42 AITD1
characters goes beyond 9.2x, nor their identity imports beyond 9.1x; Carnby
with his legs fused into one hull tears 0.88 % of his area beyond 10x. A body
without preview animations skips the check (`"stretch": "no animation"` in its
report).

The fit is a similarity transform: import undoes any scale, offset or yaw,
but not a mirrored or posed mesh. The scale comes from the height alone
(feet to top of head made equal to the original's); rotation and offset are
then refined, and the feet put back on the original's.

`make identity-models bodies=KEY` writes each original as its own delivery
(the original mesh, its palette colours in a 16x16 texture) into
`data/models-identity/`, never into `data/models-ai/`. It must import back
onto itself; it is the pipeline's end-to-end check. Import it away from the
tracked `Assets/models_hd/`:

```
make identity-models bodies=LISTBODY_011
make import-models models_ai=data/models-identity models_hd=data/models-hd-identity
```

Next to each debug `.glb` (`data/models-hd-debug/` by default), import writes
`body_<KEY>.json` for every body it looked at, imported or failed: its
status, the reason it failed, warnings and every fit metric -- the stretch
metrics for every body that has preview animations, taken even when the fit
failed, because binding and the stretch check run before the fit failure is
returned: a rejected body still reports whether its fused limbs tear. A body
with no preview animations reports `"stretch": "no animation"` instead of
`stretch_torn_pct` / `stretch_max`. On a body that failed the fit, trust
`stretch_torn_pct` only as far as `ambiguous_pct` allows: a mesh that does not
fit binds unreliably, so some of its tearing can be an artifact of the binding
rather than of the mesh --
and the `.hdm` names it wrote, or would have written. A dry run (`make
check-models`) writes nothing, unless `report=DIR` (`--report DIR`) asks for
the reports there; given to a real import, `--report` takes the reports away
from the debug folder, and the debug `.glb` still goes to `--debug`.

A report says what the check decided, not what reached the disk: a dry run
fills it exactly as a real import does, with `status: "imported"` and the
`.hdm` names in `files` for every body that passed -- bare names, one per file
import would write (the body's own, plus one per alias), with no folder (the
engine folder is not in the report). A gate must read the process exit code
(0 nothing failed, 1 at least one body failed, 2 a usage or data error) to
know a delivery is clean, and look in the output folder for the `.hdm` files
themselves; neither `status` nor `files` says a file was written.

## The in-repo generator: Blender

`make blender-models [bodies=KEY,...] [BLENDER=PATH]` delivers every
canonical character body (`kind: character`) into `data/models-ai/`, then
`make check-models` and `make import-models` judge and import it as any
delivery. It needs the export and Blender (5.2 or later; `BLENDER` defaults
to the macOS app), not the game data.

Per body (`tools/aitd_models/blender/`):

1. `remaster.py` (outside Blender) works out, per original triangle, the
   engine's atlas UVs for the front and the back painting, how much of each
   it takes, and which hand-made atlas in `Assets/atlases/` paints it
   (`body_`/`flat_`, `ramp_`, `other_`, the key's own or an alias's), all as
   `modelAtlas.cpp` and `renderer.cpp` do. Only plain polygons take an
   atlas, never a transparent one (glass); the rest keep their palette colour.
2. `stage.py` (headless Blender) splits the original into one piece per
   bone group, subdivides each with its open and sharp edges creased, and
   pulls it back onto its own original surface. A triangle spanning groups
   stays as it is: the engine stretches it, and subdivided it would tear
   past the import's stretch check. Every group, unless an edit sets it, takes the highest level
   whose predicted count, the spanning triangles plus 6·4^(L−1) for each
   other one, stays within 30,000. The originals mostly face inward, so the
   stage gives each connected island one winding and turns it to whichever
   side sees out of the body. It then unwraps the result and bakes the
   textured original onto it, its ambient occlusion and, for a body with
   transparent polygons or spheres (the glass kind), a mask of them.
3. `remaster.py` composites the bakes into one 2048 px PNG, the mask as
   alpha 128, and writes `model.glb`.

Working files go to `data/models-blender/<KEY>/`; `data/models-ai/run.md`
lists every body: delivered (triangles, texture source, seconds), failed
(the error) or skipped.

**Edits.** `tools/aitd_models/blender/edits/<KEY>.json` adjusts one body;
none is needed by default. Its fields, all optional (an unknown one fails
the body):

| Field | Meaning |
|---|---|
| `subdivide` | `{"gNN": level}`: a group's level (0..4) instead of the budget's |
| `crease` | `["gNN", ...]`: groups kept flat (plain subdivision) |
| `projection` | `{"gNN": "front" \| "back" \| "blend" \| "palette"}`: which painting a group takes |
| `skip` | `{"reason": "..."}`: no delivery; the body stays classic |

**Through MCP for Blender**, run the same stage in the open Blender and
leave its scene up to look at, after `make blender-models bodies=KEY` has
written the body's working files:

```python
import sys; sys.path.insert(0, "<repo>/tools/aitd_models/blender")
import stage; stage.run("<repo>/data/models-blender/LISTBODY_011", keep=True)
```

## Import output: body_<KEY>.hdm

The engine reads one file per body, written once per canonical delivery and
copied to every alias. Little-endian, no padding:

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
−z, engine units). Joints are bone groups; the four weights sum to exactly
255, and an unused slot has weight 0 and joint 0. UVs keep glTF's
convention (origin at the image's top left).
