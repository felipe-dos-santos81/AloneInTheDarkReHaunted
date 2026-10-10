# aitd_remaster.cfg

The game reads `aitd_remaster.cfg` from the folder it starts in (in the macOS
app, `Tatou.app/Contents/Resources/`) and writes it back when you change an
option in the **F1** dialog. If the file is missing, the game uses the defaults
below until the first save.

The file has one `key = value` per line, and `#` starts a comment. Booleans
are `true` or `false`, and strings are quoted. Unknown keys are ignored.

`config.version` is the file's format. Older files saved the old defaults, so
loading one changes them once and saves the file: before version 2 it turns
`graphics.hdModels` on, before version 3 it turns `backgrounds.dump` off,
before version 4 it sets `animation.poseSmoothingStrength` from the old
default 0.72 to the new 0.5.

## Graphics

| Key | Default | Meaning |
|---|---|---|
| `graphics.hdBackgrounds` | `true` | Draw the HD camera views from `backgrounds_hd.hda` (`make hd-install`) |
| `graphics.backgroundScale` | `2` | Size of the HD art, as a multiple of 320x200 (1–4). The shipped art is 4 |
| `graphics.hdModels` | `true` | Draw the HD character models from `models_hd/` in the game data folder, or else next to the executable. Each build copies them next to the executable (into the app on macOS). A body without a model draws classic, and a missing folder logs one line |
| `graphics.msaa` | `4` | Anti-aliasing for 3D models: 0, 2, 4, 8 or 16 |
| `graphics.renderer` | `auto` | `auto`, `d3d11`, `d3d12`, `opengl`, `vulkan` or `metal` |
| `graphics.fullscreen` | `false` | Start in fullscreen (**F11** toggles it) |
| `graphics.blurredMenu`, `graphics.menuBlurAmount` | `true`, `5.0` | Blur the scene behind menus |
| `graphics.useArtwork` | `true` | Artwork behind the start and system menus |
| `gameplay.hints` | `true` | Highlight objects you can interact with |

## Post-processing

| Key | Default | Meaning |
|---|---|---|
| `postprocessing.bloom` | `true` | Glow around bright areas: `bloomThreshold` 0.45, `bloomIntensity` 0.55, `bloomPasses` 2 |
| `postprocessing.filmGrain` | `true` | Film grain: `filmGrainIntensity` 0.025 |
| `postprocessing.ssao` | `true` | Ambient occlusion: `ssaoRadius` 400, `ssaoIntensity` 0.8 |
| `postprocessing.ssgi` | `true` | Screen-space bounce light: `ssgiRadius` 300, `ssgiIntensity` 0.6, `ssgiNumSamples` 16 |
| `postprocessing.lightProbes` | `true` | Ambient light from probes: `lightProbeIntensity` 0.5 |
| `postprocessing.vignette` | `true` | Darker corners: `vignetteIntensity` 0.35, `vignetteRadius` 0.75 |
| `postprocessing.colorGrading` | `true` | `exposure` 0.05, `contrast` 1.06, `saturation` 0.96, `temperature` −0.02, `shadowLift` 0.012, `highlightRolloff` 0.18 |

The values after each switch are its own keys (`postprocessing.bloomThreshold = 0.45`).

## Animation

| Key | Default | Meaning |
|---|---|---|
| `animation.poseSmoothing` | `true` | HD models only: a curve through the keyframes, so limbs keep their speed through each pose. Classic bodies and gameplay (hits included) keep the original linear motion |
| `animation.poseSmoothingStrength` | `0.5` | 0 is linear, 1 the full curve; 0.5 keeps planted feet within 5 % of the original's drift |

## Controls

| Key | Default | Meaning |
|---|---|---|
| `controls.mouseGameplay` | `true` | Play with the mouse's left button ([README](../README.md#mouse-left-button-only)) |
| `controls.autoCounterAttack` | `false` | Hit back once after an enemy's blow (AITD1) |
| `controls.enemyAttackPace` | `0` | Enemy attacks: `0` normal, `1` slower, `2` much slower (AITD1) |
| `controls.key.*` | arrows, Space, Enter, Escape, Q, E, Left Shift | Keys for `up`, `down`, `left`, `right`, `action`, `confirm`, `cancel`, `quickturnleft`, `quickturnright`, `run`, as [SDL](https://github.com/libsdl-org/SDL) scancodes |
| `controls.pad.*` | D-pad, A, Start, B, LB, RB, L3 | The same actions on a gamepad, as SDL gamepad buttons |

Rebind them in **Controls** in the system menu rather than by hand.

## Controller

| Key | Default | Meaning |
|---|---|---|
| `controller.enable` | `true` | Use a connected gamepad |
| `controller.analogMovement` | `true` | Walk with the left stick |
| `controller.deadzone` | `0.15` | Stick dead zone, 0–1 |
| `controller.sensitivity` | `1.00` | Stick sensitivity |
| `controller.invertY` | `false` | Invert the vertical axis |

## Font and interface

| Key | Default | Meaning |
|---|---|---|
| `font.enableTTF`, `font.path`, `font.size` | `true`, `"fonts/IMFellEnglish-Regular.ttf"`, `16` | Draw text with a TrueType font. Every build ships IM Fell English (`Assets/fonts`); a path that does not load falls back to it |
| `font.hideOriginal` | `true` | With the TTF font on, hide the original bitmap text |
| `interface.showOptionsAtStartup` | `true` | Open the options dialog at startup |

## Game data

| Key | Default | Meaning |
|---|---|---|
| `gamedata.steamless` | `false` | `true` stops the game from finding and copying an installed game (Windows) and disables the Steam overlay |
| `gamedata.jackmode` | `false` | Run *Jack in the Dark* from the `JACK` folder instead of AITD1 |

## Asset loading and dumping

| Key | Default | Meaning |
|---|---|---|
| `masks.load` | `true` | Load the hand-edited HD depth masks |
| `sequences.load` | `true` | Load HD replacement frames for the full-screen sequences |
| `masks.dump`, `sequences.dump` | `false` | Write the generated masks or the decoded sequence frames to PNG |
| `backgrounds.dump` | `false` | Write every original background to `backgrounds_dump/` at each launch |

## Debug

| Key | Default | Meaning |
|---|---|---|
| `debug.loadSaveOnStart` | `-1` | Load this save slot at startup (`-1`: off) |
| `debug.hdModelsCompare` | `false` | Write HD/classic comparison frames, scored by `tools/hd_compare.py` ([checklist](hd-models-checklist.md)) |
| `debug.mouseNavOverlay` | `false` | Draw the mouse walk grid and print `MTRACE` lines |
| `debug.graphicsValidation` | `false` | Graphics API validation (slow) |
| `debug.logLifeScripts`, `debug.dumpLifeScripts` | `false` | Log or dump the life scripts |
| `debug.generateNativeLifeScripts`, `debug.enableNativeLifeScripts` | `false` | Generate the life scripts as C, or run the generated C |

## Not used yet

The game reads and saves `graphics.filtering`, `graphics.wallDepth`,
`music.external` and `music.folder`, but nothing in the engine acts on them.
