# Prompt: enhance actor graphics

> **Status: done.** This prompt started the HD character models work. HD
> models now ship and draw by default (`graphics.hdModels`). The asset
> contract is [model-contract.md](model-contract.md); the in-game sign-off is
> [hd-models-checklist.md](hd-models-checklist.md). The prompt is kept for
> the record, with its file references brought up to date.

Improve how actors look in AloneInTheDarkReHaunted. Keep the original game's
identity, animation and gameplay. Work in this repository and follow its
AGENTS.md. Choose the changes with the smallest blast radius.

## Visual direction

Aim for a faithful remaster: recognizable silhouettes, costumes, faces and
proportions, and restrained materials that fit the fixed-camera
backgrounds. Improve facial readability, texture seams, polygon artifacts and
how characters match the backgrounds. Avoid blanket smoothing, heavy gloss
and redesigns. Cover both playable characters and the enemies in the end, but
prove the approach on one playable body first.

## Code to inspect first

- `TatouSource/FitdLib/main.cpp` selects the body with `setCurrentBodyNum`,
  draws it through `AffObjet` and applies `drawBgOverlay`. Keep foreground
  masking and actor order.
- `TatouSource/FitdLib/renderer.cpp` holds `AnimNuage`, `AffObjet`, primitive
  drawing and atlas selection, including the HD detail-polygon suppression
  and the lamp exceptions. `AffObjet` draws a body's HD replacement instead
  of its primitives when one is loaded.
- `TatouSource/FitdLib/modelAtlas.cpp` and `rendererBGFX.cpp` are the texture
  atlas and GPU paths. Check what is compiled and used: a file with a
  similar name is not necessarily live.
- `TatouSource/FitdLib/modelReplacement.*` is the HD model runtime: it loads
  `models_hd/body_<KEY>.hdm` and draws it, behind `graphics.hdModels`.
  `hdCompare.*` is its developer check.
- `TatouSource/FitdLib/models/` holds the engine-free math: `affine3.h`,
  `bodyPose.*`, `renderCamera.*`, `hdmMesh.*` (reads `.hdm`),
  `skinnedBody.*` (bone matrices and screen box), `replacementGate.h`,
  `mipChain.*` and `modelLight.*`. A pose has at most 32 groups
  (`kMaxPoseGroups`); justify any change to that.
- `tools/aitd_models/` and `tools/models.py` are the model pipeline:
  `make export-models`, `make blender-models`, `make check-models` and
  `make import-models`. Read [model-contract.md](model-contract.md) before
  changing assets or formats.
- C++ tests live in `TatouSource/tests/engine/`; Python tests in
  `tests/tools/`.

The current source is the authority. Graph results may point to another
checkout. Do not build a persistent semantic index without asking. Fetch
current library documentation through Context7 before library-specific
decisions.

## First deliverable: evidence and a plan

1. Trace the live actor path from body and animation state through
   transforms, materials, GPU submission and background masks.
2. Inspect the available assets. If game data is available, capture an
   in-game baseline. Separate observed defects from proposed improvements.
3. Compare improving the atlas path with extending the HD mesh pipeline.
   Recommend the smallest approach that reaches the visual goal, and say
   what needs authored assets.
4. Propose exact files, stages, fallback behavior and verification. Present
   the plan for approval before implementing it.

## Implementation rules, after approval

1. Keep rendering separate from simulation. Keep animation timing, root
   motion, collisions, hitboxes, combat, saves, actor identity and
   keyboard, gamepad and mouse behavior. Mouse picking keeps the engine's
   integer projection; float render math must not replace it.
2. Reuse the existing rendering and asset boundaries. Keep math and
   validation code engine-free. With the enhancement off, draw classic. Fall
   back to classic per body when its asset is missing, invalid or
   unsupported. Put any new setting in the existing options system.
3. For HD meshes, go end to end: validation, import, runtime drawing. Follow
   the contract's canonical keys and aliases, tell `LISTBODY` from
   `LISTBOD2`, check skeleton compatibility, and handle coordinate
   conversion and bind pose. Do not add a second animation system.
4. Follow the asset contract: a static rest pose matching the references,
   Y-up, facing +Z, at most 50,000 triangles, one base-color PNG or JPEG of
   at most 4096 px per side (2048 recommended). The import aligns the mesh
   and derives the skinning. Report missing art explicitly: infrastructure
   alone is not a graphics upgrade.
5. Verify camera alignment, depth, clipping, foreground masks, lighting,
   shadows, the lamp and menu previews. Create, cache, replace and destroy
   resources without loading assets every frame.
6. Keep user-owned art and unrelated edits. Do not change vendored
   libraries or add a dependency without evidence that you need it.

## Acceptance and handoff

- Run `make test-engine`, `make test-tools` and `make build-fitd`. Add
  focused tests for new pose, import and validation behavior and for
  fallback failures. Test every available render backend and name any
  platform left untested.
- Provide matched before/after screenshots: same camera, position, pose and
  settings. Cover idle, walk, turn, attack, damage, camera cuts, foreground
  masks, several actors and the lamp. Check both protagonists, at least one
  enemy, and missing or invalid assets before a broad rollout. Confirm that
  the game is unchanged with the enhancement off and that input still
  works.
- Measure frame time and memory in a repeatable scene.
- Report changes, asset coverage, tests, visual evidence, performance and
  open limitations separately. If game data or final models are missing,
  list the runtime checks left unverified.
