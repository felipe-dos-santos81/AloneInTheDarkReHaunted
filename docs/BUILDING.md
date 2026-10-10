# Building

How to build the Tatou engine (a [FITD](https://github.com/yaz0r/FITD) fork)
on each platform. Where the game data comes from is in
[README → Game data](../README.md#game-data).

## Prerequisites

| Requirement | Version | Notes |
|-------------|---------|-------|
| **Git** | 2.x | |
| **CMake** | 3.25+ | The CMake presets need it; CI uses the same presets |
| **C++20 compiler** | | MSVC, GCC or Clang. `FitdLib` builds as C++20, the rest as C++17 |

The third-party libraries (bgfx, [SDL3](https://github.com/libsdl-org/SDL),
SoLoud, ImGui, zlib, doctest) are vendored in `TatouSource/ThirdParty/`, so a
plain clone is enough:

```bash
git clone https://github.com/felipe-dos-santos81/alone-in-the-dark-re-haunted-v2.git
cd alone-in-the-dark-re-haunted-v2
```

The root `Makefile` wraps the usual flow (`make help` lists every target):

```bash
make deps                          # install build dependencies (apt, dnf, pacman or Homebrew)
make build-fitd                    # configure and build the game
make run data=/path/to/game/data   # build, then play from that folder
```

Every build copies the HD character models (`Assets/models_hd`) next to the
executable (into `Tatou.app/Contents/Resources` on macOS). The game finds them
there from any working directory.

## Windows

### Visual Studio 2022

1. Install Visual Studio 2022 with the *Desktop development with C++* workload.
2. Generate the solution. The script finds Visual Studio with `vswhere` and
   writes the solution to the `vs2022` folder next to it, so run it from
   `TatouSource\build`:

   ```cmd
   cd TatouSource\build
   vs2022.bat
   ```

3. Open `TatouSource\build\vs2022\FITD.sln`.
4. Make **Fitd** the startup project.
5. Set its working directory (Project → Properties → Debugging → Working
   Directory) to your game data folder.
6. Pick **Debug** or **Release** and press **F5**. The executable is `Tatou.exe`.

### Visual Studio 2026

`vs2026.bat` does the same for Visual Studio 18 (2026) and writes
`TatouSource\build\vs2026\FITD.slnx`. Then follow steps 3–6 above.

### CMake command line

The preset CI uses (Visual Studio 17 2022 generator, RelWithDebInfo, output in
`TatouSource\build\vs2026`):

```cmd
cd TatouSource
cmake --preset windows-release
cmake --build --preset windows-release --target Fitd --parallel 4
```

Or any generator (`Ninja`, `NMake Makefiles`, `MinGW Makefiles`, …):

```cmd
cmake -S TatouSource -B TatouSource\build\custom -DCMAKE_BUILD_TYPE=Release
cmake --build TatouSource\build\custom --target Fitd
```

## Linux

`make deps` runs `TatouSource/install_deps.sh`, which installs the packages
with `apt`, `dnf` or `pacman`. Then build with the preset CI uses:

```bash
cd TatouSource
cmake --preset linux-release
cmake --build --preset linux-release --target Fitd --parallel $(nproc)
```

Always give `--parallel` a number. Bare `--parallel` runs an unbounded
`make -j`, which runs out of memory while building the bgfx shader tools.

Without presets (older CMake):

```bash
cmake -S TatouSource -B TatouSource/build/linux-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build TatouSource/build/linux-release --target Fitd --parallel $(nproc)
```

Run the game from a writable folder that holds the game data:

```bash
cd /path/to/game-data
/path/to/TatouSource/build/linux-release/Fitd/Tatou
```

### WSL

From the Windows-side checkout, use the `linux-wsl` preset:

```bash
cd /mnt/<drive>/alone-in-the-dark-re-haunted-v2/TatouSource
cmake --preset linux-wsl
cmake --build --preset linux-wsl --target Fitd --parallel 4
```

## macOS (Apple Silicon)

The build targets `arm64` natively (macOS 11.3 or later).

Install the tools:

```bash
xcode-select --install              # Apple Clang
brew install cmake ninja pkg-config # or: make deps
```

Build with the preset, from `TatouSource`:

```bash
cd TatouSource
cmake --preset macos-arm64
cmake --build --preset macos-arm64 --target Fitd
```

Or with the `Makefile`, from the repository root. It uses the same
`TatouSource/build/macos-arm64` tree:

```bash
make build-fitd                       # configure and build the game
make run data=/path/to/writable/dir   # build, then launch windowed
```

The app bundle is `TatouSource/build/macos-arm64/Fitd/Tatou.app`. On macOS,
CMake also compiles the Objective-C++ file `Fitd/bgfxPatch.mm`. To check the
architecture:

```bash
file TatouSource/build/macos-arm64/Fitd/Tatou.app/Contents/MacOS/Tatou
# => Mach-O 64-bit executable arm64
```

At startup the app changes its working directory to
`Tatou.app/Contents/Resources`. Files it does not find there come from the
game data embedded in the binary (see [Embedded game data](#embedded-game-data)).

A clean build regenerates the tracked Metal shader headers in
`TatouSource/FitdLib/shaders/generated/metal/`. If `git status` shows them
modified and you did not change a shader, restore them:

```bash
git checkout -- TatouSource/FitdLib/shaders/generated/metal
```

## Embedded game data

Every build compiles the AITD1 and Jack in the Dark data files (PAK, ITD)
from `TatouSource/FitdLib/embedded/` into the binary. The engine uses them when
a file is missing from its working directory. Brazilian Portuguese is embedded
the same way (`make lang-pack`).

## Build configurations

| Configuration | Optimisation | Debug symbols |
|---------------|--------------|---------------|
| **Debug** | Off | Yes |
| **Release** | Full | No |
| **RelWithDebInfo** | Full | Yes |
| **MinSizeRel** | Size | No |

On Windows, a `CMAKE_BUILD_TYPE` of Release, RelWithDebInfo or MinSizeRel at
configure time builds `Tatou.exe` without a console window.

## Address Sanitizer

Configure with `-DUSE_SANITIZER=ON`, or uncomment `set(USE_SANITIZER ON)` in
`TatouSource/CMakeLists.txt`. MSVC gets ASan only; other compilers also get
UBSan and LeakSan.

## Continuous integration

`.github/workflows/build.yml` runs on pushes and pull requests to `main`:

- **Windows** (`windows-release`, RelWithDebInfo), **Linux** (`linux-release`)
  and **macOS** (`macos-arm64`) build `Fitd` and run the doctest suite
  (`engine_tests`).
- **Tool tests** run the pytest suite for `tools/` on Ubuntu. Tests that need
  real game data skip without it.

The platform rules the workflow depends on are in
[AGENTS.md → CI](../AGENTS.md#ci).

## Troubleshooting

| Problem | Solution |
|---------|----------|
| `vs2022.bat` can't find Visual Studio | Install VS2022 with the C++ workload. `vswhere.exe` must be in `%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\` |
| Missing OpenGL headers on Linux | Install `libopengl-dev libglx-dev mesa-common-dev` |
| PipeWire warning `can't load config client.conf` | Harmless. Audio falls back to PulseAudio or ALSA |
| Build dies with `Terminated` (exit 143) on Linux | Too many jobs for the memory. Use `--parallel 2` |
| Game data not found | Start the game from the folder that holds the game data |
| No music | Put one audio file per song in `music/` in the game data folder and check `music.external = true` ([configuration.md](configuration.md#music)). On macOS, `make run` links the folder into the app |
| Controller not detected | Check `controller.enable = true` in `aitd_remaster.cfg` and that SDL3 supports the gamepad |
| Fullscreen does not persist | Set `graphics.fullscreen = true` in `aitd_remaster.cfg`. Closing the system menu saves it |
| Console window covers the game | The game window comes to the front at startup. **F11** or **Alt+Enter** toggles fullscreen |

Every `aitd_remaster.cfg` key is in [configuration.md](configuration.md).
