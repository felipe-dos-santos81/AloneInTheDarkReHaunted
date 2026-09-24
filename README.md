![BCO b91ab279-5d06-4e0e-9661-41d0371e3ac4(1)](https://github.com/user-attachments/assets/867772e8-ada8-41a1-a9f6-46f80fda76be)

# Alone In The Dark: Re-Haunted

**A faithful remaster of the original 1992 survival horror classic.**

## About This Fork

This repository is a fork of [spacefarergames/AloneInTheDarkReHaunted](https://github.com/spacefarergames/AloneInTheDarkReHaunted) that focuses on *Alone in the Dark 1*. It aims to:

- **Improve game accessibility.** The whole game plays with the mouse's left button alone (see [Mouse](#mouse-left-button-only)); keyboard and gamepad play are unchanged.
- **Provide a native macOS port** (Apple Silicon / arm64). See the [macOS section of BUILDING.md](BUILDING.md#macos-apple-silicon) for build steps.

### About The Project

*Copyright © 2026 Infogrames / Spacefarer Retro Remasters LLC*
*Author: Jake Jackson (jake@spacefarergames.com)*

**A cross-platform engine reimplementation for the classic *Alone in the Dark* trilogy (1992–1995), with modern remaster enhancements.**

OS supported with binaries - Windows (64bit) Linux (64bit) macOS 11.3 (64bit)

AITD-R (also known as *Alone In The Dark Re-Haunted*) lets you play the original *Alone in the Dark* games on modern hardware. It is a from-scratch C++ reimplementation of the Infogrames engine, built with modern rendering (bgfx), audio (SoLoud), and input (SDL3) backends. The project is released under the **GNU General Public License v2**.

> **You must own the original game data** — purchase *Alone in the Dark* on [Steam](https://store.steampowered.com/app/548090/Alone_in_the_Dark_1/) or [GOG](https://www.gog.com/en/game/alone_in_the_dark_the_trilogy_123). The game data files are **not** included.
>
> This is a Free, Non-profit passion project but takes us a lot of coffee to get it done! Donate to us at https://buymeacoffee.com/jakeysbakery
> PayPal- jake@spacefarergames.com

---

### Adding the Original Game Files

The original game files are **not** included, so you still need to supply them. On macOS the engine does not search Steam, GOG or CD installs for you, so copy the files in by hand:

1. Find the `INDARK` folder in your copy of *Alone in the Dark 1*. It holds the `.PAK` and `.ITD` files (`LISTBODY.PAK`, `ETAGE00.PAK`, `CAMERA00.PAK`, `OBJETS.ITD`, `VARS.ITD`, …).
   - **GOG (macOS):** right-click `Alone in the Dark 1.app` → *Show Package Contents* → `Contents/Resources/game/INDARK/`
   - **Steam / GOG (Windows):** `<install folder>/INDARK/`
   - **CD-ROM:** `INDARK/` on the disc
2. Copy everything in that folder into `data/aitd1/` at the root of this repository. Git ignores `data/`, so your game files will never be committed.

   ```
   AloneInTheDarkReHaunted/
   └── data/
       └── aitd1/
           ├── CAMERA00.PAK
           ├── ETAGE00.PAK
           ├── LISTBODY.PAK
           ├── OBJETS.ITD
           └── …
   ```

3. Build the game and start it from that folder:

   ```bash
   make build-fitd
   make run data=data/aitd1
   ```

The game loads its files from the folder you start it in and writes `aitd_remaster.cfg` there too, so that folder must be writable. `make help` lists every other target: tests, HD assets and cleanup.

### HD Textures: Export, Upscale, Import

The engine already loads high-resolution backgrounds from `backgrounds_hd.hda`
when `graphics.hdBackgrounds = true` in `aitd_remaster.cfg`. The texture tool
in `tools/` lets you produce your own set from the original game data:

```bash
make tools-deps          # once: Python venv with Pillow, numpy, pytest
make export-textures     # originals (320x200 PNG) -> data/textures/{backgrounds,screens} + manifest.json
# upscale data/textures/** with any tool into data/textures-ai/**, same file names
make check-textures      # validate data/textures-ai the way the engine will load it (writes nothing)
make import-textures     # copy validated PNGs into Assets/backgrounds_hd, derive *_DARK variants
make hd-install          # pack Assets/backgrounds_hd into backgrounds_hd.hda and copy it into the app bundle
make run data=data/aitd1
```

Files use the engine's own names (`CAMERA02_007.png`, `ITD_RESS_013.png`).
`import-textures` also accepts a tree written by m-aitd's
`tools/export_textures.py` — `backgrounds/floorNN/cameraNNN.png`,
`screens/ressNN.png` and `alt_backgrounds/floorNN/cameraNNN.png` (the five
cameras the game swaps once the sorcerer is dead) — so an upscale made from
that exporter imports without renaming. Anything beside those three folders
(`guides/`, `.quality/`, `palette.png`, …) is ignored.
Any resolution with a 16:10 aspect works; integer multiples of 320x200 are
recommended. A file that is missing from `data/textures-ai` leaves the
existing HD art in place; a file identical to the original is skipped. Dark
rooms use `<name>_DARK.png`, which `import-textures` derives from your
upscale (`dark=all` for every camera, `dark=none` to skip). `data/` is
git-ignored, so neither the game files nor the exports are ever committed.

Some rooms and screens play a looping clip instead of a still: whenever
`anim_<NAME>/` exists in `Assets/backgrounds_hd`, the engine plays it and
never shows `<NAME>.png` (`import-textures` warns when that hides one of
your stills). `export-textures` turns each clip into a job under
`data/textures/animations/<NAME>/` and lists it in `manifest.json`; an
upscaler that follows [docs/texture-contract.md](docs/texture-contract.md)
writes new frames to `animations/<NAME>/frames/`, and `import-textures`
validates them and replaces the engine's clip (the start menu's frames go
to `StartupMenuBackground_NNN.png`, which is only shown with
`graphics.useArtwork = false` in `aitd_remaster.cfg` — artwork is on by
default). Every clip plays at 12.5 frames a second. For an animated
camera, `dark=all` also derives a full-size `anim_<NAME>_DARK/` clip;
`mirror` refreshes only the `_DARK` clips that already exist.

---

## Screenshots (In-Game)
![573227923-7d49eb5a-8d31-4474-a939-ff9876fdc9df](https://github.com/user-attachments/assets/63ea6028-5b75-4003-9db5-a0e3c9040d87)

![573228488-b0206b5f-8026-46d0-89a6-b5dc6b7ba73e](https://github.com/user-attachments/assets/e7fa702a-6d17-4ce2-afc4-573c3be5340e)

## Now Supports Jack in The Dark Promo Game (HD)
<img width="1400" height="876" alt="image" src="https://github.com/user-attachments/assets/9aa0dba7-30ee-4671-b2e5-9c40caa48872" />


## Video (YouTube)
https://www.youtube.com/watch?v=fzi_xK2Jifw

## Lamp Dynamic Lighting
https://www.youtube.com/watch?v=0yaWv7vF3bA

<img width="1920" height="1080" alt="vlcsnap-2026-04-22-13h52m48s765" src="https://github.com/user-attachments/assets/9f25879b-5c99-4313-b427-b17670c19a83" />

## Supported Games

| Game | Steam | GOG | Status |
|------|-------|-----|--------|
| Alone in the Dark 1 | [Store page](https://store.steampowered.com/app/548090/Alone_in_the_Dark_1/) | [Trilogy](https://www.gog.com/en/game/alone_in_the_dark_the_trilogy_123) | ✅ Completable
| Jack in The Dark Promo Game | [Store page](https://store.steampowered.com/app/548090/Alone_in_the_Dark_1/) | [Trilogy](https://www.gog.com/en/game/alone_in_the_dark_the_trilogy_123) | ✅ Completable

## Future Forks (And Project Names)
| Alone in the Dark 2 - Jack Is Back Again |✅ In Progress (https://github.com/spacefarergames/AloneInTheDarkJackIsBackAgain/)
| Alone in the Dark 3 - Rhinestone Cowboy | ✅ Planned

---

## Quick Start (Windows)

```bash
# 1. Clone with submodules
git clone --recurse-submodules https://github.com/felipe-dos-santos81/AloneInTheDarkReHaunted.git
cd AloneInTheDarkReHaunted\TatouSource\build

# 2. Generate the VS solution
vs2022.bat          # Visual Studio 2022
# — or —
vs2026.bat          # Visual Studio 2026
```

In Visual Studio:

1. Set **Fitd** as the startup project.
2. Set the **Working Directory** (Project → Properties → Debugging) to the folder containing your game data files (e.g. your AITD1 Steam install directory).
3. Press **F5** to build and run.

> The output executable is named `Tatou.exe`.

## Quick Start (Linux)

```bash
git clone --recurse-submodules https://github.com/felipe-dos-santos81/AloneInTheDarkReHaunted.git
cd AloneInTheDarkReHaunted
make deps                          # build dependencies (apt, dnf or pacman)
make build-fitd                    # builds into TatouSource/build/Release
make run data=/path/to/game/data   # run from the folder holding the .PAK files
```

For full multi-platform build instructions see **[BUILDING.md](BUILDING.md)**.

---

## How to Play

Note: For Jack in The Dark, download the seperate JackInTheDark release from the Releases. Keep both AITD1 and Jack in the Dark seperate to avoid conflicts. By default, it will install automatically when Steamless = false in the remaster cfg.

### Starting a New Game

1. Launch `Tatou.exe` from the game data directory (the folder containing the original `.PAK` / `.ITD` files).
2. The engine auto-detects which game is installed based on the data files present.
3. For AITD1: after the Infogrames logo and armadillo animation, select your character — **Edward Carnby** (detective) or **Emily Hartwood** (heiress).
4. The game begins in the attic of Derceto Manor.
5. Copy the built DOSBox.exe into your Alone In The Dark Steam/GOG game folder alongside Tatou.exe. When Steam launches "DOSBox.exe", it will start the FITD engine instead.

### Automatic Installation
Beginning with Version 2.0, Rehaunted will automatically find and copy over the original game files if installed from Steam, GOG or the CD version is in the drive. If found, the game will copy the files and you will be good to go. In the event it doesn't copy them into the game directory. As an extra bonus, it will copy itself over afterwards to make it easy to launch from Steam / GOG. The Steam Overlay will then work too!

### Gameplay Basics

*Alone in the Dark* is a survival-horror adventure. You explore a haunted mansion, solve puzzles, and fight (or flee from) supernatural enemies.

- **Movement** — Walk forward, backward, and turn left/right (tank controls).
- **Action** — Press the action key/button near objects to interact: open doors, pick up items, push objects, and fight enemies.
- **Inventory** — Press **F1** (or access via the menu) to open your inventory. Select items to use, equip, or examine them. Combine items by placing them in your "hand" slot.
- **Combat** — Equip a weapon from inventory, then use the action key with directional input to punch, kick, slash, or shoot. Some enemies can only be defeated with specific items or approaches.
- **Running** — Hold forward and press the action key to run (useful for dodging enemies).
- **Quick Turn** — Press **Q** or **E** (or shoulder buttons on a controller) to perform a quick 180° turn.

### Saving and Loading

- Press **Escape** to open the **System Menu** during gameplay.
- Select **Save** to write your progress to a save slot.
- Select **Load** to restore a previous save.
- Settings (fullscreen, sound, detail level, hints, controls) are also accessible from this menu and are saved automatically when the menu closes.

### Tips

- Search everything — many items are hidden in drawers, cabinets, and on shelves.
- Read all books and letters — they contain crucial puzzle hints and story background.
- Not every enemy needs to be fought — sometimes running or pushing furniture to block a doorway is the best strategy.
- Save often — Derceto is unforgiving.

---

## Controls

### Keyboard (Default)

| Action | Key | Description |
|--------|-----|-------------|
| Move Forward | **↑** (Up Arrow) | Walk forward | W
| Move Backward | **↓** (Down Arrow) | Walk backward | S
| Turn Left | **←** (Left Arrow) | Turn left | A
| Turn Right | **→** (Right Arrow) | Turn right | D
| Action / Fight | **Space** | Interact with objects, attack in combat |
| Confirm / Enter | **Enter** | Confirm menu selections |
| Cancel / Menu | **Escape** | Open system menu, cancel dialogs |
| Quick Turn Left | **Q** | 180° turn to the left |
| Quick Turn Right | **E** | 180° turn to the right |
| Fullscreen Toggle | **F11** or **Alt+Enter** | Toggle fullscreen / windowed mode |
| Fullscreen Toggle | **Double-click** | Double-click empty space on the title screen or in a menu (in the world a double-click runs) |

> All keyboard bindings are fully rebindable via the **Controls** option in the in-game system menu, or by editing `aitd_remaster.cfg`.

### Mouse (Left Button Only)

| Action | Mouse | Notes |
|--------|-------|-------|
| Walk | **Hold** on the floor | The hero follows the pointer and stops the moment you let go |
| Run | **Double-click and hold** | The second press's hold runs |
| Use an object | **Hold** on it | The hero walks to it and steps into it: pickable items open the found screen, scripted objects get their Action |
| Push | **Hold** on pushable scenery | Pushes while held |
| Fight | **Click** an enemy | Faces it and swings the weapon in hand |
| Inventory / Map / Menu | **Click** the icons, top left | Every screen and menu also works by click |

- The cursor shape shows what a click would do; "not allowed" means nothing.
- The game never locks or confines the cursor to its window.
- Any key or gamepad input takes the hero back from the mouse.
- Turn it off with **F1 → Controls → Mouse gameplay** (`controls.mouseGameplay = false`).

### Gamepad (Default — Xbox Layout)

| Action | Button | PlayStation Equivalent |
|--------|--------|-----------------------|
| Move | **Left Stick** / **D-Pad** | Left Stick / D-Pad |
| Action / Fight | **A** | **✕ (Cross)** |
| Confirm / Enter | **Start** | **Options** |
| Cancel / Menu | **B** | **○ (Circle)** |
| Quick Turn Left | **LB** | **L1** |
| Quick Turn Right | **RB** | **R1** |

- Controllers are hot-pluggable — connect or disconnect at any time.
- Analog stick deadzone and sensitivity are configurable in `aitd_remaster.cfg`.
- All gamepad bindings are rebindable via the **Controls** menu.

### In-Game System Menu

Press **Escape** during gameplay to access:

| Option | Description |
|--------|-------------|
| Continue | Return to the game |
| Save | Save your game to a slot |
| Load | Load a saved game |
| Music On/Off | Toggle ADLIB / external music |
| Sound On/Off | Toggle sound effects |
| Detail Low/High | Toggle between original graphics and HD remaster mode |
| Display: Windowed/Fullscreen | Toggle fullscreen mode (persists across sessions) |
| Controls | Rebind keyboard and gamepad controls |
| Hints: On/Off | Toggle interactive object hints |
| Quit | Quit to the main menu |

---

## Remaster Features

The *Re-Haunted* fork adds several enhancements on top of the original FITD engine:

| Feature | Status | Details |
|---------|--------|---------|
| **Dynamic Recomp of Life System** | ✅ Available | All native code, LISTLIFE Life System converted to C |
| **Dynamic Lamp Lighting** | ✅ Available | Now features illumination of HD BGS and AO inspired by AITD4 |
| **HD backgrounds** | ✅ Available | Upscaled camera views (2×–8×) with PNG/TGA support, including animated backgrounds |
| **Textured 3D models** | ✅ Available | High-quality textured replacements for core 3D models via texture atlas |
| **TTF font rendering** | ✅ Available | Smooth anti-aliased overlay fonts via ImGui (configurable font, size, and style) |
| **Post-processing** | ✅ Available | Bloom, film grain, SSAO, vignette, SSGI, light probes |
| **Controller support** | ✅ Available | Xbox, PlayStation, Switch Pro, and other SDL3-compatible gamepads with rebindable controls |
| **Fullscreen mode** | ✅ Available | Toggle via F11, Alt+Enter, double-click outside gameplay, or system menu; persists in config |
| **Mouse gameplay** | ✅ Available | Left-button-only play: hold to walk, double-click-and-hold to run, click objects, enemies and HUD icons; never locks the cursor |
| **In-game maps** | ✅ Available | Interactive mansion and underground maps with real-time position tracking |
| **Interactive hints** | ✅ Available | Highlights interactable objects in the game world |
| **Voice-over playback** | ✅ Available | CD voice-over for AITD1 book/letter reading sequences |
| **HD depth masks** | ✅ Available | Hand-edited masks for correct 3D object occlusion with HD backgrounds |
| **Atmospheric particles** | ✅ Available | Dust mote particles in the attic (floor 7) |
| **Transparent menus** | ✅ Available | Blurred, semi-transparent system menu overlays |
| **Crash recovery** | ✅ Available | Exception handler with crash logging to `crash_log.txt` |
| **Auto-update checker** | ✅ Available | Non-blocking check for new GitHub releases at startup |

All remaster features are configurable through `aitd_remaster.cfg` — see [`fitd_remaster.cfg.example`](fitd_remaster.cfg.example) for the full reference. Detailed documentation is available in [`REMASTER.md`](REMASTER.md).

---

## Configuration
<img width="1069" height="762" alt="image" src="https://github.com/user-attachments/assets/10519c13-ce2e-4c75-84a2-ea852cc4cff2" />

Since 2.4 update, Re-Haunted now has a built in Options Dialog which can now be triggered with HOME/F1 and on startup.
Copy `fitd_remaster.cfg.example` to `aitd_remaster.cfg` alongside the game data and edit to taste. Key sections:

| Section | Key Settings |
|---------|-------------|
| **Controller** | `controller.enable`, `controller.deadzone`, `controller.sensitivity`, `controller.invertY` |
| **Graphics** | `graphics.hdBackgrounds`, `graphics.backgroundScale`, `graphics.filtering`, `graphics.fullscreen` |
| **Post-processing** | `postprocessing.bloom`, `postprocessing.filmGrain`, `postprocessing.ssao`, `postprocessing.vignette`, `postprocessing.ssgi`, `postprocessing.lightProbes` |
| **Music** | `music.external`, `music.folder` |
| **Font** | `font.enableTTF`, `font.path`, `font.size`, `font.hideOriginal` |
| **Controls** | `controls.key.*`, `controls.pad.*` — per-action keyboard scancode and gamepad button bindings; `controls.mouseGameplay` — left-button mouse play (default on) |
| **Debug** | `debug.mouseNavOverlay` — draw the mouse walk grid and floor pick over the game |
| **Gameplay** | `gameplay.hints` — interactive hint overlay |
| **Masks** | `masks.dump`, `masks.load` — HD depth mask dumping and loading |
| **Sequence Dumping** | `sequences.dump`, `sequences.load` - HD sequence replacements (for AITD2 / AITD 3 only) |
| **Game Data Settings** | `gamedata.steamless` - Steamless mode - Disables Steam Overlay and automatic installation into Steam / GOG copy. Set to false if you wish to run standalone, manual setup. |

---

## Project Layout

```
FITD/
├── Fitd/                  # Executable entry point (WinMain / main)
├── FitdLib/               # Core engine static library (~75 source files)
│   ├── mouse/             # Left-button mouse gameplay
│   ├── shaders/           # bgfx shader programs (.sc)
│   └── embedded/          # Embedded game data (PAK files, textures, etc.)
├── ThirdParty/            # Git submodules
│   ├── bgfx.cmake         # Cross-platform rendering (bgfx + bimg + bx)
│   ├── SDL/               # SDL3 — windowing, input, audio
│   ├── soloud.cmake       # SoLoud audio library
│   ├── imgui/             # Dear ImGui (debug UI, TTF font overlay)
│   └── zlib/              # Compression (HQR/PAK archives)
├── tests/engine/          # doctest unit tests (make test-engine)
├── tools/                 # Build tools (HDA archive builder, etc.)
├── build/                 # Generated build directories
├── .github/workflows/     # CI (CMake multi-platform)
├── CMakeLists.txt         # Root CMake project
├── README.md              # This file
├── BUILDING.md            # Detailed build instructions
├── ARCHITECTURE.md        # Codebase architecture guide
├── REMASTER.md            # Remaster feature documentation
├── CONTRIBUTING.md        # Contributor guidelines
├── RELEASE_NOTES.md       # Version history and changelogs
├── TTF_FONT_README.md     # TTF font feature documentation
└── LICENSE                # GNU General Public License v2
```

For a deeper dive into the code modules and data flow, see **[ARCHITECTURE.md](ARCHITECTURE.md)**.

---

## Contributing

Contributions are welcome! Please see **[CONTRIBUTING.md](CONTRIBUTING.md)** for guidelines on setting up a development environment, coding standards, and the pull request workflow. **[AGENTS.md](AGENTS.md)** holds this fork's firm rules (never lock the cursor, keep keyboard play unchanged, the mouse invariants); run `make test` before sending changes.

---

## License

This project is licensed under the **GNU General Public License v2** — see the [LICENSE](LICENSE) file for details.

The original game data files are **not** included and must be obtained separately (e.g., from Steam or GOG).

## Steam Assets
Box Art
![573480799-62b94c3b-7fd4-4b7d-8f81-f504382f798d](https://github.com/user-attachments/assets/e01731c3-5f6a-42a8-9330-227242e1d919)

Logo
<img width="1536" height="1024" alt="Copilot_20260403_111155" src="https://github.com/user-attachments/assets/9277959d-af64-4c71-9bae-70007b945838" />





