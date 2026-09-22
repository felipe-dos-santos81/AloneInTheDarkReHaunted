# Texture contract: export → upscaler → import

This is what `make export-textures` hands an upscaler and what
`make import-textures` accepts back. It is the reference for external
upscalers such as `aitd-texture-enhancement`. Paths are relative to the
export folder (`data/textures/`) and the upscale folder
(`data/textures-ai/`), which must use the same layout.

## What export writes

```
data/textures/
  backgrounds/CAMERA0F_NNN.png        144 camera plates, 320x200 RGB
  screens/ITD_RESS_NNN.png            13 full-screen images, 320x200 RGB
  animations/<NAME>/
    reference/frame_0001.png …        the clip found at the first export for <NAME> (write-once)
    still.png                         only when no plate above is named <NAME>.png
  manifest.json
```

`manifest.json` (schema 2) lists every file:

- `images[]`: `path`, `target` (the engine's file name), `kind`
  (`camera`/`screen`), `pak`, `entry`, `floor`, `size`, and `sha256` of the
  decoded RGB pixels.
- `animations[]`: one job per animated background:

| Field | Meaning |
|---|---|
| `name` | Engine name: `CAMERA03_008`, `ITD_RESS_002_NOTATOU`, `StartupMenuBackground`, … |
| `kind` | `camera`, `screen` or `menu` |
| `floor` | Camera floor 0–7, else `null`; use it as the colour-match group |
| `still` | The 320x200 still to render and animate |
| `reference` | Folder of `frame_NNNN.png`: the clip found at the first export, kept afterward for motion guidance only — it is not updated by a later export; delete `data/textures/animations/<NAME>/reference/` to have the next export copy the engine's current clip again |
| `reference_frames`, `reference_size` | Its frame count and size |
| `frames_dir` | Where to write the new frames |
| `fps` | `12.5`: the engine plays every clip at 0.08 s a frame, looping |
| `loop` | Always `true` |
| `max_frames` | Hard frame limit (`512` for the menu), else `null` |
| `engine` | Where import puts the frames (informational) |
| `active` | `false` if the engine never plays it (`_DISABLED`); may be skipped |

## What an upscaler must write

1. **Images.** For each `images[]` record, write the upscale at the same
   relative path. Any 16:10 size up to 8192 px per side is accepted; an
   integer multiple of 320x200 (1280x800 at 4x) is recommended.
2. **Animations.** For each `animations[]` job:
   1. Render `still` like an image. When `still` is also an `images[]` path,
      reuse that render, so the animation starts from the room's still.
   2. For `kind: camera`, colour-match within the group given by `floor`,
      not by parsing paths. The five alt-camera screens the game swaps in
      once the sorcerer is dead have `kind: screen` and `floor: null`;
      colour-match each against the floor of the camera it replaces:
      `ITD_RESS_015` and `016` against floor 7 (cameras 0 and 1), and
      `ITD_RESS_017`, `018` and `019` against floor 6 (cameras 0, 5 and 8).
   3. Animate the rendered still with an image-to-video model, using it as
      both first and last frame so the clip loops without a jump.
      `reference` shows what moves (fire, rain, curtains); never copy its
      pixels into the output.
   4. Resample to `fps` (12.5) and write `frames_dir/frame_0001.png`,
      `frame_0002.png`, … numbered without gaps. Every frame is RGB and the
      size of the rendered still; frame 1 is the rendered still.
   5. Choose the length. Staying near `reference_frames` is recommended,
      and so is keeping `width × height × 3 × frames` under 1 GiB, since the
      engine holds every frame decoded. `max_frames` is a hard limit.
3. **Everything else.** Copy `animations/*/reference/`, `still.png` and
   `manifest.json` through unchanged, or leave them out. Never repaint a
   reference clip.

## What import checks

Images: the name maps to an engine file, the PNG decodes, 16:10 within 1%,
at most 8192 px per side; a file identical to the original is skipped. A
size that is not an integer multiple of 320x200 is a warning; the file is
still imported.

Animations, all before anything is written:

| Error (sequence rejected) | Warning (imported) |
|---|---|
| no job named `<NAME>` in the manifest | size not a multiple of 320x200 |
| any file other than `frame_NNNN.png`, a gap, or no frames | last frame differs from the first by more than 8 on average (loop seam) |
| more than `max_frames` frames | decoded size over 1 GiB |
| a frame that does not decode, or differs in size from frame 1 | |
| frame 1 not 16:10, or over 8192 px per side | |

An accepted scene clip replaces `anim_<NAME>/` in `Assets/backgrounds_hd`
completely; the menu's frames become `StartupMenuBackground_NNN.png`, shown
only when `graphics.useArtwork = false` in `aitd_remaster.cfg` (artwork is
on by default) — an upscaler may decide the menu job is not worth rendering
if that is not the target configuration.
A still imported for a slot that keeps its `anim_<NAME>/` is reported as
shadowed: the engine plays the animation instead.
