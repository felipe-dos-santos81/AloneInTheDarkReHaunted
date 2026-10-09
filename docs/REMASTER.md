# Remaster Features

What Re-Haunted adds to the original [FITD](https://github.com/yaz0r/FITD)
engine. Every `aitd_remaster.cfg` key, with its default, is in
[configuration.md](configuration.md); mouse gameplay and the combat assists are
in the [README](../README.md#mouse-left-button-only).

## Controllers

Xbox, PlayStation, Switch Pro and any other
[SDL3](https://github.com/libsdl-org/SDL) gamepad. Controllers are
hot-pluggable, and every button can be rebound in the **Controls** menu.

- **Move:** left stick (analog) or D-pad. A stick reads as a direction past 0.3.
- **A / Cross:** action, confirm. **B / Circle:** cancel. **Start:** menu
  select. **LB / RB (L1 / R1):** quick turn. **L3:** run.
- **Settings** (`controller.*`): dead zone (0.0–0.9, default 0.15),
  sensitivity (0.1–5.0, default 1.0), invert Y, analog movement.
- **Stick drift:** raise the dead zone to 0.20–0.25. **Too sensitive:** lower
  sensitivity to 0.7–0.9.

## Fullscreen

**F11**, **Alt+Enter**, a double-click on empty space outside gameplay, or
**Display** in the system menu. `graphics.fullscreen` is saved and applied at
startup, and the game window is raised above the console.

## HD graphics

- **HD backgrounds** (`graphics.hdBackgrounds`): upscaled camera views from
  `backgrounds_hd.hda`, including animated ones, with the original image where
  no HD one exists. **Detail** in the system menu switches between original and
  HD.
- **HD depth masks** (`masks.load`): hand-edited masks from `masks_hd/` so 3D
  objects hide correctly behind HD scenery. `masks.dump` writes the generated
  masks as PNG for editing.
- **Textured models:** texture atlases on the classic bodies, cached per floor.
- **HD character models** (`graphics.hdModels`, on by default): refined,
  textured meshes for all 42 classic bodies, skinned to the original bones so
  every animation plays unchanged. Translucent parts (the ghost, the insect's
  wings, lamp glass) draw blended, and a body without a model draws classic.
  Every build copies `Assets/models_hd` next to the game. They are made with
  `make blender-models` and `make import-models`: see
  [model-contract.md](model-contract.md) and
  [hd-models-checklist.md](hd-models-checklist.md).

## Post-processing

Each effect has its own switch and settings under `postprocessing.*`:

- **Bloom:** glow around bright areas, in several passes.
- **Film grain.**
- **SSAO:** contact shadows in corners and creases.
- **Vignette:** darker screen edges.
- **SSGI:** bounce light from nearby surfaces, rendered at half resolution.
- **Light probes:** spherical-harmonics ambient light, loaded per floor and
  camera.

## Text and menus

- **TrueType text** (`font.*`): anti-aliased text drawn through ImGui over or
  instead of the bitmap font. Every build ships IM Fell English
  (`Assets/fonts`, SIL Open Font License), and a `font.path` that does not load
  falls back to it; 14–18 px reads best.
- **Blurred menus** (`graphics.blurredMenu`): a blurred, see-through system
  menu instead of opaque frames.

## Languages

English, French, Italian, Spanish, German and Brazilian Portuguese. Pick one
in the language menu; **Português** is listed below the five original
languages so the flags painted in the menu art stay aligned (it has no flag).

- Portuguese plays the English CD voice-over, as Italian, Spanish and German do.
- The bitmap font has no ã, õ or accented capitals: ã and õ are composed from
  a/o and the tilde of ñ, and À Á Â Ã Ê Í Ó Ô Õ Ú draw as the plain capital.
  TrueType text (`font.enableTTF`) draws them all.
- The remaster's bitmap menus (system, controls, "Please Wait...") are
  translated too, in French, Italian, Spanish, German and Portuguese; the F1
  options dialog only in Portuguese (English in the other languages).
- The text is in `Assets/lang/pt-BR`; `make lang-pack` checks it and writes the
  copy compiled into the game, and `make lang-extract` writes the English and
  French text as UTF-8 for reference. In-game sign-off:
  [translation-checklist.md](translation-checklist.md).

## Maps and hints

- **Maps:** press **Tab** (gamepad **Select**) in the system menu for the
  mansion and underground maps, with room labels and your position.
- **Hints** (`gameplay.hints`, or **Hints** in the system menu): highlights the
  objects you can interact with.

## Atmosphere and sound

- **Dust:** up to 150 particles in the attic and in the intro.
- **Voice-over (AITD1 CD):** books, letters and notebooks are read aloud, page
  by page, with page-turn sounds. The VOC files are looked up in the HDA
  archive, then on the CD (volume `ALONECD`), then on disk.
- **External music:** `music.external` and `music.folder` are read and saved,
  but nothing plays them yet.

## Key bindings

Rebind keys and buttons in the **Controls** menu. The file stores SDL
scancodes (`controls.key.*`) and SDL gamepad buttons (`controls.pad.*`) for
up, down, left, right, action, confirm, cancel, quick turn left and right, and
run; the defaults are in [configuration.md](configuration.md#controls).

## Windows only

- **Crash log:** unhandled exceptions are written to `crash_log.txt`, and the
  game tries to carry on after non-fatal ones.
- **Update check:** at startup a background thread asks GitHub for a newer
  release of the original project and prints a note to the console.

## For developers

- Every build includes the remaster features; there are no special flags.
- `configRemaster.h` defines `RemasterConfig`; `configRemaster.cpp` reads and
  writes `aitd_remaster.cfg` (`loadRemasterConfig`, `saveRemasterConfig`).
- New input actions go in `controlsMenu.cpp`.

## Credits

Alone in the Dark (Infogrames, 1992). FITD engine by yaz0r. SDL3 (input and
windowing), SoLoud (audio), bgfx (rendering) and Dear ImGui (UI).

All original game assets remain the property of their copyright holders.
