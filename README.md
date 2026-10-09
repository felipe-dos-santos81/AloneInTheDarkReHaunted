![BCO b91ab279-5d06-4e0e-9661-41d0371e3ac4(1)](https://github.com/user-attachments/assets/867772e8-ada8-41a1-a9f6-46f80fda76be)

# Alone In The Dark: Re-Haunted

**A faithful remaster of the original 1992 survival horror classic.**

Re-Haunted (AITD-R) is a C++ reimplementation of the Infogrames engine (FITD,
here called Tatou), with modern rendering (bgfx), audio (SoLoud) and input
(SDL3). It runs on Windows, Linux and macOS 11.3+, and is licensed under the
**GNU GPL v2**.

## This fork

This is a fork of
[spacefarergames/AloneInTheDarkReHaunted](https://github.com/spacefarergames/AloneInTheDarkReHaunted)
focused on *Alone in the Dark 1*. It adds:

- **Accessibility.** The whole game plays with the mouse's left button alone
  ([Mouse](#mouse-left-button-only)). An optional automatic counter-attack and
  slower enemy attacks make fights easier. Keyboard and gamepad play are
  unchanged.
- **A native macOS port** for Apple Silicon (arm64)
  ([BUILDING.md](BUILDING.md#macos-apple-silicon)).
- **HD asset tools.** You can pack the HD backgrounds and generate, check and
  import HD character models ([HD assets](#hd-assets)).

*Original project © 2026 Infogrames / Spacefarer Retro Remasters LLC, by Jake
Jackson (jake@spacefarergames.com). It is a free, non-profit project. You can
support it at https://buymeacoffee.com/jakeysbakery or by PayPal to
jake@spacefarergames.com.*

## Game data

The game files are **not** included. You need to own *Alone in the Dark 1*
([Steam](https://store.steampowered.com/app/548090/Alone_in_the_Dark_1/),
[GOG](https://www.gog.com/en/game/alone_in_the_dark_the_trilogy_123) or the
CD).

1. Find the game's `INDARK` folder, which holds `LISTBODY.PAK`, `ETAGE00.PAK`,
   `CAMERA00.PAK`, `OBJETS.ITD`, `VARS.ITD` and the rest:
   - **GOG (macOS):** `Alone in the Dark 1.app` → *Show Package Contents* →
     `Contents/Resources/game/INDARK/`
   - **Steam / GOG (Windows):** `<install folder>/INDARK/`
   - **CD-ROM:** `INDARK/` on the disc
2. Copy its contents into `data/aitd1/` at the root of this repository. Git
   ignores `data/`, so the files are never committed.

On Windows the engine can also find a Steam, GOG or CD install and copy the
files itself (turn this off with `gamedata.steamless = true`). macOS and Linux
builds do not search for them.

The game reads its files from the folder it starts in and writes
`aitd_remaster.cfg` there, so that folder must be writable.

## Build and run

**macOS and Linux**

```bash
git clone --recurse-submodules https://github.com/felipe-dos-santos81/AloneInTheDarkReHaunted.git
cd AloneInTheDarkReHaunted
make deps        # Linux only: build dependencies (apt, dnf or pacman)
make run         # build the game and play from data/aitd1
```

`make help` lists every target, including tests, HD assets and cleanup.

**Windows**

Run `TatouSource\build\vs2022.bat` (or `vs2026.bat`) and open the generated
solution. Set **Fitd** as the startup project, and set its working directory
(Project → Properties → Debugging) to your game data folder. Then press **F5**.
The executable is `Tatou.exe`.

[BUILDING.md](BUILDING.md) has the full instructions for every platform.

## Controls

### Keyboard

| Action | Key |
|--------|-----|
| Walk / turn | Arrow keys or **W A S D** |
| Run | **Left Shift** |
| Action / fight | **Space** |
| Inventory / confirm | **Enter** |
| System menu / cancel | **Escape** |
| Quick turn | **Q** / **E** |
| Options dialog | **F1** or **Home** |
| Fullscreen | **F11** or **Alt+Enter** |

You can rebind keys under **Controls** in the system menu.

### Mouse (left button only)

| Action | Mouse |
|--------|-------|
| Walk | **Hold** on the floor: the hero follows the pointer and stops when you let go |
| Run | **Double-click and hold** |
| Use an object | **Hold** on it: the hero walks to it and touches it |
| Push | **Hold** on pushable scenery |
| Fight | **Click** an enemy |
| Inventory / map / menu | **Click** the icons at the top left; every screen also works by click |

- The cursor's shape shows what a click would do. "Not allowed" means nothing
  would happen.
- The game never locks or confines the cursor.
- Pressing any key or gamepad button takes the hero back from the mouse.
- To turn mouse play off: **F1 → Controls → Mouse gameplay**
  (`controls.mouseGameplay = false`).
- Outside gameplay, a double-click on empty space toggles fullscreen.

### Gamepad (Xbox layout)

| Action | Button (PlayStation) |
|--------|----------------------|
| Move | Left stick / D-pad |
| Run | **L3** |
| Action / fight | **A** (✕) |
| Inventory / confirm | **Start** (Options) |
| System menu / cancel | **B** (○) |
| Quick turn | **LB** / **RB** (L1 / R1) |

Controllers are hot-pluggable, and you can rebind their buttons in the
**Controls** menu.

## Configuration

**F1** opens the options dialog, and the dialog also shows at startup.
Settings are saved in `aitd_remaster.cfg` next to the game data:

| Section | Keys |
|---------|------|
| Graphics | `graphics.hdBackgrounds`, `graphics.hdModels`, `graphics.backgroundScale`, `graphics.filtering`, `graphics.fullscreen` |
| Post-processing | `postprocessing.bloom`, `.filmGrain`, `.ssao`, `.vignette`, `.ssgi`, `.lightProbes` |
| Controls | `controls.key.*`, `controls.pad.*`, `controls.mouseGameplay` (default on), `controls.autoCounterAttack` (hit back once after an enemy's blow, default off), `controls.enemyAttackPace` (`0` normal, `1` slower, `2` much slower) |
| Controller | `controller.enable`, `controller.deadzone`, `controller.sensitivity`, `controller.invertY` |
| Music / font | `music.external`, `music.folder`, `font.enableTTF`, `font.path`, `font.size` |
| Game data | `gamedata.steamless`: `true` disables automatic installation and the Steam overlay |
| Debug | `debug.mouseNavOverlay` draws the mouse walk grid and prints `MTRACE` lines (what each click resolved to and why) |

[REMASTER.md](REMASTER.md) documents the remaster features in detail.

## HD assets

**Backgrounds.** The upscaled camera views and screens live in
`Assets/backgrounds_hd` (the pipeline that made them was retired; it is in git
history). The engine loads them from `backgrounds_hd.hda` when
`graphics.hdBackgrounds = true`.

```bash
make hd-install       # pack backgrounds_hd.hda into the app
```

**Character models.** `make export-models` writes the animated bodies for a
model generator. `make check-models` and `make import-models` validate the
replacements and install them into `Assets/models_hd`; `make models-install`
copies them and the atlases into the app bundle. They draw when
`graphics.hdModels = true`. The details are in
[docs/model-contract.md](docs/model-contract.md) and
[docs/hd-models-checklist.md](docs/hd-models-checklist.md).

## Remaster features

| Feature | Details |
|---------|---------|
| Native life scripts | LISTLIFE converted to C |
| Dynamic lamp lighting | Lights the HD backgrounds, with AO inspired by AITD4 |
| HD backgrounds | 2×–8× camera views, including animated ones; hand-edited depth masks |
| HD and textured models | Replacement meshes and texture atlases |
| TTF fonts | Anti-aliased overlay text through ImGui |
| Post-processing | Bloom, film grain, SSAO, vignette, SSGI, light probes |
| Controllers | Xbox, PlayStation, Switch Pro and other SDL3 gamepads |
| Mouse gameplay | Left button only; never locks the cursor |
| Combat assists | Automatic counter-attack and slower enemy attacks (AITD1) |
| Maps and hints | Interactive mansion maps; highlighted interactable objects |
| Voice-over | CD voice-over for books and letters |
| Quality of life | Fullscreen toggle, transparent menus, crash log (`crash_log.txt`), update check |

## Supported games

| Game | Status |
|------|--------|
| Alone in the Dark 1 | ✅ Completable |
| Jack in the Dark (promo) | ✅ Completable. It ships as a separate release; keep it in its own folder |
| Alone in the Dark 2 | In progress, as [a separate fork](https://github.com/spacefarergames/AloneInTheDarkJackIsBackAgain/) |
| Alone in the Dark 3 | Planned |

## Screenshots and videos

![573227923-7d49eb5a-8d31-4474-a939-ff9876fdc9df](https://github.com/user-attachments/assets/63ea6028-5b75-4003-9db5-a0e3c9040d87)

![573228488-b0206b5f-8026-46d0-89a6-b5dc6b7ba73e](https://github.com/user-attachments/assets/e7fa702a-6d17-4ce2-afc4-573c3be5340e)

<img width="1400" height="876" alt="Jack in the Dark" src="https://github.com/user-attachments/assets/9aa0dba7-30ee-4671-b2e5-9c40caa48872" />

- [Gameplay video](https://www.youtube.com/watch?v=fzi_xK2Jifw)
- [Lamp dynamic lighting](https://www.youtube.com/watch?v=0yaWv7vF3bA)

## Repository layout

```
AloneInTheDarkReHaunted/
├── TatouSource/         # CMake project
│   ├── Fitd/            # executable (Tatou)
│   ├── FitdLib/         # engine library: mouse/, models/, assist/, physics/, shaders/
│   ├── ThirdParty/      # SDL3, bgfx, ImGui, SoLoud, zlib, doctest
│   ├── tests/engine/    # doctest unit tests (make test-engine)
│   └── tools/           # .hda archive tools
├── tools/               # Python texture and model pipeline
├── tests/tools/         # its pytest suite (make test-tools)
├── Assets/              # HD backgrounds, masks, atlases
├── docs/                # contracts and in-game checklists
└── data/                # your game files and exports (git-ignored)
```

[ARCHITECTURE.md](ARCHITECTURE.md) explains the engine's modules and data flow.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). [AGENTS.md](AGENTS.md) holds this
fork's firm rules, such as never locking the cursor and keeping keyboard play
unchanged. Run `make test` before sending a change.

## License

GNU General Public License v2. See [LICENSE](LICENSE). The original game data
is not included.

## Steam assets

![573480799-62b94c3b-7fd4-4b7d-8f81-f504382f798d](https://github.com/user-attachments/assets/e01731c3-5f6a-42a8-9330-227242e1d919)

<img width="1536" height="1024" alt="Logo" src="https://github.com/user-attachments/assets/9277959d-af64-4c71-9bae-70007b945838" />
