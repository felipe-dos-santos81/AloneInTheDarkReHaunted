# Prompt: enhance actor graphics

Improve the visual quality of actors in AloneInTheDarkReHaunted while preserving the original game's identity, animation, and gameplay. Work in this repository and follow its AGENTS.md. Choose changes with the smallest blast radius.

## Visual direction

Aim for a faithful remaster: recognizable silhouettes, costumes, faces, proportions, and restrained materials that fit the fixed-camera backgrounds. Improve facial readability, texture seams, polygon artifacts, and character/background consistency. Avoid indiscriminate smoothing, exaggerated gloss, or unrelated redesigns. Include both playable characters and enemies in the eventual scope, but prove the approach with one representative playable body first.

## Existing foundations to inspect

- `TatouSource/FitdLib/main.cpp` selects the body with `setCurrentBodyNum`, renders it through `AffObjet`, and applies `drawBgOverlay`. Inspect both rendering paths and preserve foreground occlusion and actor ordering.
- `TatouSource/FitdLib/renderer.cpp` contains `AnimNuage`, `AffObjet`, primitive rendering, and atlas selection. It already has HD-specific detail-polygon suppression and lamp-related exceptions. Evaluate these before adding overlapping fixes.
- `TatouSource/FitdLib/modelAtlas.cpp` and `rendererBGFX.cpp` provide existing texture-atlas and GPU rendering infrastructure. Establish what is actually compiled and used; similarly named alternative files are not automatically active implementations.
- `TatouSource/FitdLib/models/{affine3.h,bodyPose.*,renderCamera.*}` provides engine-free pose, skinning-matrix, and float camera math. These helpers are foundations, not evidence of a working HD actor renderer. Verify their runtime integration before claiming it exists. Respect the current 32-group pose limit or explicitly justify a compatible change.
- `tools/aitd_models/`, `tools/models.py`, and `make export-models` provide the original-body export pipeline. Read `docs/model-contract.md` before changing assets or formats. Import/check targets are documented as planned; verify their current implementation status.
- Existing C++ coverage is in `TatouSource/tests/engine/`; Python pipeline coverage is in `tests/tools/`.

Use current source as authority. Graph results may point to another checkout. Do not silently build a persistent semantic index. Fetch current library documentation through Context7 when making library-specific decisions.

## First deliverable: evidence and implementation plan

Trace the live actor path from body/animation state through transforms, materials, GPU submission, and background masks. Inspect available assets and capture a baseline in-game when game data is available. Distinguish observed defects from proposed improvements.

Compare improving the existing atlas path with completing the HD mesh pipeline. Recommend the smallest approach that achieves the visual goal; explain what requires authored assets. Propose exact files, stages, fallback behavior, and verification. Present the plan for approval before implementation.

## Implementation requirements after approval

1. Keep rendering changes separate from simulation. Preserve animation timing, root motion, collisions, hitboxes, combat, saves, actor identity, and keyboard/gamepad/mouse behavior. Mouse picking must retain the engine's integer projection; float render math must not replace it.
2. Reuse the existing rendering and asset boundaries. Keep mathematical and validation code engine-free. Preserve classic rendering when enhancement is disabled and fall back per body when assets are absent, invalid, or unsupported. Reuse the existing options system for any new setting.
3. If HD meshes are needed, complete a vertical slice from validation/import through runtime rendering. Follow the contract's canonical keys and aliases, distinguish `LISTBODY` from `LISTBOD2`, validate skeleton compatibility, and account for coordinate conversion and bind pose. Preserve game namespaces. Do not implement a second animation system.
4. Respect the asset contract: static rest-pose delivery matching the references, Y-up and facing +Z, at most 50,000 triangles, and one base-color PNG/JPEG no larger than 4096 pixels per side (2048 recommended). Let the import process align and derive skinning as specified. Missing artistic assets must be reported explicitly; infrastructure alone is not a completed graphics upgrade.
5. Verify camera alignment, depth, clipping, foreground masks, lighting, shadows, lamp behavior, and menu previews. Handle resource creation, caching, replacement, and destruction without repeated per-frame asset loading.
6. Preserve user-owned art and unrelated edits, including any existing changes to `mainLoop.cpp`. Do not modify vendored libraries or introduce a new dependency without an evidence-backed need.

## Acceptance and handoff

Run `make test-engine`, `make test-tools`, and `make build-fitd` for relevant changes. Add focused tests for new pose/import/validation behavior and fallback failures. Test on supported render backends where available, reporting any untested platform.

Provide matched before/after screenshots at the same camera, position, pose, and settings. Exercise idle, walk, turn, attack, damage, camera cuts, foreground occlusion, multiple actors, and the lamp. Check both protagonists, at least one enemy, and missing/invalid assets before broad rollout. Confirm enhancement-off behavior and accessible input remain intact. Measure frame time and memory in a repeatable scene.

Report implemented changes, asset coverage, tests, visual evidence, performance, and remaining limitations separately. If game data or final models are unavailable, state exactly which runtime acceptance checks remain unverified.
