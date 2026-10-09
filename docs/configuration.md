# aitd_remaster.cfg

The game reads `aitd_remaster.cfg` from the folder it starts in (in the macOS
app, `Tatou.app/Contents/Resources/`) and writes it back when you change an
option in the **F1** dialog. If the file is missing, the game writes it with the
defaults below.

The file has one `key = value` per line, and `#` starts a comment. Booleans
are `true` or `false`, and strings are quoted. Unknown keys are ignored.

`config.version` is the file's format. A file older than version 2 had
`graphics.hdModels` saved off because that was the old default, so loading it
turns HD models on once; the next save writes version 2.

## Graphics

| Key | Default | Meaning |
|---|---|---|
| `graphics.hdBackgrounds` | `false` | Draw the HD camera views from `backgrounds_hd.hda` (`make hd-install`) |
| `graphics.backgroundScale` | `2` | Size of the HD art, as a multiple of 320x200 (1–4). The shipped art is 4 |
| `graphics.hdModels` | `true` | Draw the HD character models from `models_hd/` next to the game data. Each build copies them next to the game (into the app on macOS) and `make run` into the data folder. A body without a model draws classic, and a missing folder logs one line |
| `graphics.msaa` | `4` | Anti-aliasing for 3D models: 0, 2, 4, 8 or 16 |
| `graphics.renderer` | `auto` | `auto`, `d3d11`, `d3d12`, `opengl`, `vulkan` or `metal` |
| `graphics.fullscreen` | `false` | Start in fullscreen (**F11** toggles it) |
| `graphics.blurredMenu`, `graphics.menuBlurAmount` | `false`, `5.0` | Blur the scene behind menus |
| `graphics.useArtwork` | `true` | Artwork behind the start and system menus |
| `gameplay.hints` | `true` | Highlight objects you can interact with |

## Post-processing

| Key | Default | Meaning |
|---|---|---|
| `postprocessing.bloom` | `true` | Glow around bright areas: `bloomThreshold` 0.45, `bloomIntensity` 0.55, `bloomPasses` 2 |
| `postprocessing.filmGrain` | `true` | Film grain: `filmGrainIntensity` 0.025 |
| `postprocessing.ssao` | `true` | Ambient occlusion: `ssaoRadius` 400, `ssaoIntensity` 0.8 |
| `postprocessing.ssgi` | `false` | Screen-space bounce light: `ssgiRadius` 300, `ssgiIntensity` 0.6, `ssgiNumSamples` 16 |
| `postprocessing.lightProbes` | `false` | Ambient light from probes: `lightProbeIntensity` 0.5 |
| `postprocessing.vignette` | `false` | Darker corners: `vignetteIntensity` 0.35, `vignetteRadius` 0.75 |
| `postprocessing.colorGrading` | `true` | `exposure` 0.05, `contrast` 1.06, `saturation` 0.96, `temperature` −0.02, `shadowLift` 0.012, `highlightRolloff` 0.18 |

The values after each switch are its own keys (`postprocessing.bloomThreshold = 0.45`).

## Animation

| Key | Default | Meaning |
|---|---|---|
| `animation.poseSmoothing` | `true` | Blend skeletal poses between keyframes |
| `animation.poseSmoothingStrength` | `0.72` | How much, 0–1 |

## Controls

| Key | Default | Meaning |
|---|---|---|
| `controls.mouseGameplay` | `true` | Play with the mouse's left button ([README](../README.md#mouse-left-button-only)) |
| `controls.autoCounterAttack` | `false` | Hit back once after an enemy's blow (AITD1) |
| `controls.enemyAttackPace` | `0` | Enemy attacks: `0` normal, `1` slower, `2` much slower (AITD1) |
| `controls.key.*` | arrows, Space, Enter, Escape, Q, E, Left Shift | Keys for `up`, `down`, `left`, `right`, `action`, `confirm`, `cancel`, `quickturnleft`, `quickturnright`, `run`, as SDL scancodes |
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
| `font.enableTTF`, `font.path`, `font.size` | `false`, `"BLKCHCRY.TTF"`, `16` | Draw text with a TrueType font |
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
| `backgrounds.dump` | `true` | Write every original background to `backgrounds_dump/` at each launch. Set it to `false` unless you need the originals |

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
