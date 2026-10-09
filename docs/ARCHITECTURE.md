# Architecture

Where the code lives and how the main parts fit together. Build steps are in
[BUILDING.md](BUILDING.md); this fork's own modules and their rules are in
[AGENTS.md](../AGENTS.md).

## Overview

[FITD](https://github.com/yaz0r/FITD) reimplements the engine of *Alone in the
Dark* 1–3, *Jack in the Dark* and *Time Gate*. A thin executable (`Fitd`) calls into a static library
(`FitdLib`) that holds all engine logic. `FitdLib` is C++20; the executable,
tools and tests are C++17. Rendering, windowing, audio and input use
third-party libraries vendored in `TatouSource/ThirdParty/`.

```
┌──────────────────────────────────────────────────┐
│                    Fitd (EXE)                    │
│        WinMain / main  →  FitdInit / FitdMain    │
└──────────────────────┬───────────────────────────┘
                       │
┌──────────────────────▼───────────────────────────┐
│                FitdLib (static lib)              │
│                                                  │
│  ┌──────────┐ ┌────────┐ ┌────────┐ ┌─────────┐  │
│  │Game logic│ │Renderer│ │ Audio  │ │  Input  │  │
│  │(Life,    │ │(bgfx,  │ │(SoLoud,│ │(SDL3,   │  │
│  │ rooms,   │ │ shaders│ │ AdLib) │ │ gamepad)│  │
│  │ objects) │ │ ImGui) │ │        │ │         │  │
│  └──────────┘ └────────┘ └────────┘ └─────────┘  │
│                                                  │
│  ┌──────────┐ ┌────────┐ ┌───────────────────┐   │
│  │ Resource │ │ Config │ │ Remaster features │   │
│  │(HQR/PAK, │ │        │ │ (HD bg, TTF font, │   │
│  │  zlib)   │ │        │ │  post-processing) │   │
│  └──────────┘ └────────┘ └───────────────────┘   │
└──────────────────────────────────────────────────┘
         │            │             │
    ┌────▼───┐   ┌────▼───┐   ┌────▼────┐
    │  bgfx  │   │  SDL3  │   │ SoLoud  │
    │  bimg  │   │        │   │         │
    │  ImGui │   │        │   │         │
    └────────┘   └────────┘   └─────────┘
```

## CMake targets

| Target | Type | Description |
|--------|------|-------------|
| `Fitd` | Executable | Entry point (`Fitd/fitd.cpp`). Output: `Tatou.exe`, `Tatou` on Linux, `Tatou.app` on macOS |
| `FitdLib` | Static library | All engine code, plus the ImGui sources |
| `game_assets` | Custom | Copies `Assets/models_hd` and `Assets/fonts` next to the executable on every `Fitd` build |
| `engine_tests` | Executable | doctest suite for the engine-free modules (`tests/engine/`) |
| `build_hda_archive`, `unpack_hda_archive` | Executables | Pack and unpack `.hda` archives (`tools/`) |
| `DOSBoxStub` | Executable (Windows) | Replaces the Steam/GOG `DOSBox.exe` so the store launches `Tatou.exe` |
| `bgfx` (+ `bimg`, `bx`) | Static libraries | GPU rendering (`ThirdParty/bgfx.cmake`) |
| `SDL3-static` | Static library | Windowing, input, platform layer (`ThirdParty/SDL`) |
| `soloud` | Static library | Audio mixing and playback (`ThirdParty/soloud.cmake`) |
| `zlibstatic` | Static library | Decompression (`ThirdParty/zlib`) |

Paths are relative to `TatouSource/`.

## FitdLib module map

The main files in `TatouSource/FitdLib/`, by area. Not every file is listed.

### Core engine

| File(s) | Responsibility |
|---------|---------------|
| `main.cpp` / `main.h` | Startup (`FitdMain`, `OpenProgram`, `detectGame`), camera, collision, scene control |
| `mainLoop.cpp` / `mainLoop.h` | The game loop (`PlayWorld`) |
| `vars.cpp` / `vars.h` | Globals, the game-type enum (`AITD1`, `JACK`, `AITD2`, `AITD3`, `TIMEGATE`), core data structures |
| `common.h` / `config.h` | Shared includes and base definitions |
| `baseTypes.h` / `endianess.h` | Integer types (`s16`, `u32`, …) and byte-order helpers |
| `version.cpp` / `version.h` | Version string |
| `gameTime.cpp` / `gameTime.h` | Game timing and clock |

### Game logic and scripting

| File(s) | Responsibility |
|---------|---------------|
| `life.cpp` / `life.h` | Interpreter for the original *Life* scripts (`LM_DO_MOVE`, `LM_HIT`, `LM_CAMERA`, …) |
| `lifeMacroTable.cpp` | Life opcode table |
| `evalVar.cpp` / `evalVar.h` | Script variable evaluation |
| `AITD1.cpp` / `AITD2.cpp` / `AITD3.cpp` / `JACK.cpp` | Per-game startup and logic |
| `AITD1_Tatou.cpp` | AITD1 status screen and inventory display |
| `anim.cpp` / `anim.h` | Body animation |
| `anim2d.cpp` / `anim2d.h` | 2D sprite animation |
| `animAction.cpp` / `animAction.h` | Actions triggered by animations (hits, sounds) |
| `object.cpp` / `object.h` | Object creation (`InitObjet`) |
| `actorList.cpp` / `actorList.h` | Actor sorting for draw order |
| `track.cpp` / `track.h` | Actor movement along tracks |

### World and rooms

| File(s) | Responsibility |
|---------|---------------|
| `room.cpp` / `room.h` | Room data: collision boxes (`hardColStruct`), scene zones (`sceZoneStruct`), camera zones |
| `floor.cpp` / `floor.h` | Floor loading (`LoadEtage`) |
| `zv.cpp` / `zv.h` | ZV (bounding volume) calculations |

### Rendering

| File(s) | Responsibility |
|---------|---------------|
| `renderer.cpp` / `renderer.h` | 3D object rendering, point transformation, shadows |
| `rendererBGFX.cpp` | bgfx draw submission |
| `bgfxGlue.cpp` / `bgfxGlue.h` | bgfx setup, `StartFrame` / `EndFrame` |
| `screen.cpp` / `screen.h` | Screen buffers |
| `videoMode.cpp` / `videoMode.h` | Display mode settings |
| `polys.cpp` | Polygon rasterisation |
| `lines.cpp` | Line drawing |
| `sprite.cpp` / `sprite.h` | 2D sprites |
| `palette.cpp` / `palette.h` | VGA palette |
| `font.cpp` / `font.h` | Original bitmap font |
| `debugFont.cpp` / `debugFont.h` | Debug overlay text |
| `sequence.cpp` / `sequence.h` | FMV cutscene player |
| `shaders/` | bgfx shader sources (see [Shader programs](#shader-programs)) |

### Remaster features

| File(s) | Responsibility |
|---------|---------------|
| `configRemaster.cpp` / `configRemaster.h` | `RemasterConfig` and `aitd_remaster.cfg` reading and writing |
| `hdBackground.cpp` / `hdBackground.h` | HD background loading (stb_image) |
| `hdBackgroundRenderer.cpp` / `hdBackgroundRenderer.h` | HD background drawing |
| `hdArchive.cpp` / `hdArchive.h` | `.hda` archive reader |
| `postProcessing.cpp` / `postProcessing.h` | Bloom, film grain, SSAO, SSGI |
| `fontTTF.cpp` / `fontTTF.h` | TrueType text through ImGui |
| `imguiBGFX.cpp` / `imguiBGFX.h` | ImGui on bgfx |
| `updateChecker.cpp` / `updateChecker.h` | Checks GitHub Releases for a newer version (Windows) |

### This fork's modules

Mouse gameplay (`mouse/`), the HD character models (`models/`,
`modelReplacement.*`), the combat assists (`assist/`), the collision rules
(`physics/`) and the translation helpers (`text/`: the Portuguese font glyphs
and the menu string table, used through `uiTr.h`) are described, with their
rules, in [AGENTS.md](../AGENTS.md).

### Input

| File(s) | Responsibility |
|---------|---------------|
| `input.cpp` / `input.h` | Keyboard, mouse and gamepad through SDL3 (`readKeyboard`, `updateController`) |
| `controlsMenu.cpp` / `controlsMenu.h` | Key-binding menu |

### Audio

| File(s) | Responsibility |
|---------|---------------|
| `music.cpp` / `music.h` | Music control and track switching |
| `osystemAL.cpp` / `osystemAL.h` | Sound effect and music output |
| `osystemAL_adlib.cpp` | AdLib music through OPL emulation |
| `osystemAL_mp3.cpp` / `osystemAL_mp3.h` | MP3 playback |
| `fmopl.cpp` / `fmopl.h` | Yamaha OPL2 emulator |
| `vocDecoder.cpp` / `vocDecoder.h` | Creative VOC decoder |

### Resources and files

| File(s) | Responsibility |
|---------|---------------|
| `hqr.cpp` / `hqr.h` | HQR resource containers and their memory |
| `pak.cpp` / `pak.h` | PAK archive reader |
| `fileAccess.cpp` / `fileAccess.h` | File loading helpers |
| `unpack.cpp` / `unpack.h` | Decompression |
| `resourceGC.cpp` / `resourceGC.h` | Deferred freeing of HD background assets |
| `save.cpp` / `save.h` | Save and load |
| `embedded/` | The AITD1 and Jack in the Dark data files, and the Brazilian Portuguese text, as C++ arrays (`getEmbeddedFile`), used when a file is not on disk |

### Menus and UI

| File(s) | Responsibility |
|---------|---------------|
| `startupMenu.cpp` / `startupMenu.h` | Title screen and startup menu |
| `systemMenu.cpp` / `systemMenu.h` | In-game system menu |
| `inventory.cpp` / `inventory.h` | Inventory |
| `tatou.cpp` / `tatou.h` | Status screen, inventory UI, character sheet |
| `aitdBox.cpp` / `aitdBox.h` | Dialog boxes and frames |
| `debugger.cpp` / `debugger.h` | Debug views and wireframes |
| `consoleLog.h` | Console logging macros |

### Platform

| File(s) | Responsibility |
|---------|---------------|
| `osystem.h` | OS layer declarations |
| `osystemSDL.cpp` | SDL3 backend: `FitdInit`, window, main-thread loop |
| `exceptionHandler.cpp` / `exceptionHandler.h` | Crash handling |

## Data flow

### Startup

Two threads run the game; [AGENTS.md → Threads](../AGENTS.md#threads) explains
how they hand over.

```
main() / WinMain()                     Fitd/fitd.cpp
  └─ FitdInit()                        osystemSDL.cpp, main thread
       ├─ loadRemasterConfig()         read aitd_remaster.cfg
       ├─ SDL_CreateWindow()
       ├─ detectGame()                 set g_gameId from the data files
       ├─ start the game thread ──────► FitdMain()                main.cpp
       │                                 ├─ initBgfxGlue()
       │                                 ├─ OpenProgram()         load resources, start subsystems
       │                                 └─ startAITD1() / startJACK() / startAITD2() / startAITD3()
       │                                      └─ startGame() → PlayWorld() loop
       └─ loop: readKeyboard(), bgfx::renderFrame()
```

### One tick of `PlayWorld`

1. **Input**: `process_events()` takes the input the main thread read.
2. **Animation and collision**: `GereAnim()`.
3. **Scripts**: `processLife()` for each active actor.
4. **Camera**: switch to `NewNumCamera` when a camera change is due.
5. **2D animation**: `handleAnim2d()`.
6. **Render**: `AllRedraw()`.

Music runs outside the tick: the audio stream callback calls
`callMusicUpdate()`.

### Rendering

```
StartFrame()
  ├─ background (original or HD)
  ├─ 3D actors (AffObjet → transform → bgfx)
  ├─ 2D sprites, text, UI
  ├─ ImGui (TTF text, debug UI)
  ├─ post-processing
  └─ EndFrame() → bgfx::frame()
```

## Shader programs

Shaders live in `FitdLib/shaders/`. At build time bgfx's `shaderc` compiles
them into headers under `shaders/generated/`. A program pairs a vertex shader
(`*_vs.sc`) and a fragment shader (`*_ps.sc`) with a varying definition
(`*.varying.def.sc`); several programs share a vertex shader.

| Program (fragment shader) | Purpose |
|---------------------------|---------|
| `ui` | 2D UI quads |
| `background` / `hdBackground` | Camera backgrounds |
| `maskBackground` / `maskHDBackground` | Depth-masked background overlays |
| `flat` / `textured` | Flat and textured 3D polygons |
| `noise` | Noise and dither effects |
| `ramp` | Gradient ramp shading |
| `sphere` | Sphere-mapped lighting |
| `model` | HD character models (`skinned_vs`): an opaque pass, then a blended pass for translucent texels |
| `particle` | Particles (blood, dust) |
| `lantern_bloom` / `lantern_shadow` | Lantern glow and shadows |
| `brightpass` / `blur` / `composite` | Bloom chain |
| `ssao` / `ssao_blur` | Screen-space ambient occlusion |
| `ssgi` / `ssgi_blur` | Screen-space global illumination |

## Game-specific code

`AITD1.cpp`, `AITD2.cpp`, `AITD3.cpp` and `JACK.cpp` hold each game's startup,
special cases and version differences. `detectGame()` sets `g_gameId`
(`gameTypeEnum` in `vars.h`), and the engine branches on it.

## Third-party libraries

All are vendored under `TatouSource/ThirdParty/`.

| Library | Purpose | Path |
|---------|---------|------|
| **bgfx** (+ bimg, bx) | Rendering (D3D11/12, Vulkan, Metal, OpenGL) | `bgfx.cmake` |
| **[SDL3](https://github.com/libsdl-org/SDL)** | Windowing, input, gamepad, platform layer | `SDL` |
| **SoLoud** | Audio mixing and playback | `soloud.cmake` |
| **Dear ImGui** | Debug UI, TTF text | `imgui` |
| **zlib** | Decompression | `zlib` |
| **doctest** | Engine unit tests | `doctest` |

## Configuration

`RemasterConfig` (`configRemaster.h`) holds the settings in one sub-struct per
area: `ui`, `controller`, `graphics`, `postProcessing`, `animation`, `music`,
`font`, `controls`, `masks`, `sequences`, `backgrounds`, `gameData` and
`debug`. `loadRemasterConfig()` reads `aitd_remaster.cfg` at startup and
`saveRemasterConfig()` writes it back. Every key, with its default, is in
[configuration.md](configuration.md).
