# Building FITD (Alone In The Dark: Re-Haunted)

This document covers how to build the Tatou engine (a FITD fork) on every supported platform.

---

## Prerequisites (All Platforms)

| Requirement | Minimum Version | Notes |
|-------------|----------------|-------|
| **Git** | 2.x | Must support `--recurse-submodules` |
| **CMake** | 3.25+ | Needed by the CMake presets (CI uses the same presets) |
| **C++20 compiler** | See per-platform sections | MSVC, GCC, or Clang |

Clone the repository **with submodules** — several third-party libraries (bgfx, SDL3, SoLoud, ImGui, zlib, doctest) are pulled in as Git submodules:

```bash
git clone --recurse-submodules https://github.com/felipe-dos-santos81/AloneInTheDarkReHaunted.git
cd AloneInTheDarkReHaunted
```

If you already cloned without `--recurse-submodules`, run:

```bash
git submodule update --init --recursive
```

The root `Makefile` wraps the primary flow used everywhere, including CI:

```bash
make deps          # install build dependencies (apt, dnf, pacman or Homebrew)
make build-fitd    # configure + build the game
make run data=/path/to/game/data
```

---

## Windows

### Option A — Visual Studio 2022 (recommended)

1. Install **Visual Studio 2022** with the *Desktop development with C++* workload and the **CMake tools for Windows** component.
2. Run the helper batch file:

   ```cmd
   TatouSource\build\vs2022.bat
   ```

   This locates the VS2022 installation via `vswhere`, configures the environment, and generates a Visual Studio 17 (2022) solution in `TatouSource\build\vs2022\`.

3. Open `TatouSource\build\vs2022\FITD.sln`.
4. Set **Fitd** as the startup project.
5. Set the **Working Directory** (Project Properties → Debugging → Working Directory) to the folder containing your game data (e.g. your AITD1 Steam install directory).
6. Select a build configuration (**Debug** or **Release**) and press **F5**.

> The output executable is named `Tatou.exe`.

### Option B — Visual Studio 2026

A `TatouSource\build\vs2026.bat` script is also provided. It works identically but targets Visual Studio 18 (2026):

```cmd
TatouSource\build\vs2026.bat
start TatouSource\build\vs2026\FITD.sln
```

Follow steps 3–6 from Option A above.

### Option C — CMake command-line (any generator)

```cmd
cmake -S TatouSource -B TatouSource\build\custom -DCMAKE_BUILD_TYPE=Release
cmake --build TatouSource\build\custom --target Fitd
```

You may substitute any CMake generator (`"Ninja"`, `"NMake Makefiles"`, `"MinGW Makefiles"`, etc.).

---

## Linux

### 1. Install dependencies

`make deps` runs `TatouSource/install_deps.sh`, which installs the right packages with `apt`, `dnf`, `pacman` or Homebrew depending on the host.

### 2. Configure and build

With CMake 3.25+ (as on CI):

```bash
cd TatouSource
cmake --preset linux-release
cmake --build --preset linux-release --target Fitd --parallel $(nproc)
```

Always give `--parallel` a number on Linux: bare `--parallel` means unbounded `make -j` and exhausts the machine's memory during the bgfx shader-toolchain build.

Without presets (older CMake):

```bash
cmake -S TatouSource -B TatouSource/build/linux-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build TatouSource/build/linux-release --target Fitd --parallel $(nproc)
```

### 3. Run

Run from a writable folder holding the original game data — where the files come from and how to place them is covered in [README → Adding the Original Game Files](README.md#adding-the-original-game-files):

```bash
cd /path/to/game-data
/path/to/TatouSource/build/linux-release/Fitd/Tatou
```

### Windows Subsystem for Linux (WSL)

From the Windows-side checkout, build with the `linux-wsl` preset:

```bash
cd /mnt/<drive>/AloneInTheDarkReHaunted/TatouSource
cmake --preset linux-wsl
cmake --build --preset linux-wsl --target Fitd --parallel 4
```

---

## macOS (Apple Silicon)

> Targets `arm64` natively. Windowed mode and an unlocked mouse cursor are the defaults.

### 1. Install tools

```bash
xcode-select --install              # Apple Clang
brew install cmake ninja pkg-config # via Homebrew
```

### 2. Build (CMake preset)

Run from the `TatouSource` directory:

```bash
cd TatouSource
cmake --preset macos-arm64
cmake --build --preset macos-arm64 --target Fitd
```

The app bundle is written to `TatouSource/build/macos-arm64/Fitd/Tatou.app`.

### 3. Build and run (Makefile)

The root `Makefile` drives the same `TatouSource/build/macos-arm64` tree and
builds arm64. Run these from the repository root:

```bash
make build-fitd                                   # configure + build the game
make run data=/path/to/writable/dir               # build + launch windowed
```

Game data is embedded in the binary, so no original PAK files are required.

> A clean build regenerates the tracked Metal shader headers under
> `TatouSource/FitdLib/shaders/generated/metal/`. If `git status` shows some of
> them modified after a build and you did not intend to change them, restore
> with `git checkout -- TatouSource/FitdLib/shaders/generated/metal`.

### 4. Verify the architecture

```bash
file TatouSource/build/macos-arm64/Fitd/Tatou.app/Contents/MacOS/Tatou
# => Mach-O 64-bit executable arm64
```

The CMake configuration automatically includes the Objective-C++ patch file (`bgfxPatch.mm`) on Darwin.

---

## Build Configurations

| Configuration | Console Window | Optimisation | Debug Symbols | Notes |
|---------------|---------------|--------------|---------------|-------|
| **Debug** | Shown | Off | Full | Default for development |
| **Release** | Hidden (Win) | Full | None | For distribution |
| **RelWithDebInfo** | Hidden (Win) | Full | Full | Profiling builds |
| **MinSizeRel** | Hidden (Win) | Size | None | Minimal binary size |

---

## Address Sanitizer

To enable ASan (and UBSan / LeakSan on non-MSVC), uncomment the `USE_SANITIZER` line in the root CMakeLists:

```cmake
set(USE_SANITIZER ON)
```

Or pass it on the command line:

```bash
cmake -DUSE_SANITIZER=ON ...
```

---

## Continuous Integration

The project includes a GitHub Actions workflow (`.github/workflows/build.yml`) that runs on pushes and pull requests to `main`:

- **Windows** (VS2022, RelWithDebInfo), **Ubuntu** (Release) and **macOS** (Apple Silicon, Release) — builds `Fitd` and runs the doctest engine suite (`engine_tests`)
- **Tool tests** — runs the pytest suite for `tools/` on Ubuntu (real-data tests skip without game files)

For agents, `AGENTS.md` records the two platform gotchas the workflow encodes: bounded `--parallel` on Makefile generators, and the Windows `min`/`max`/`near`/`far` macro rules.

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| **Submodule directories are empty** | Run `git submodule update --init --recursive` |
| **`vs2022.bat` can't find Visual Studio** | Ensure VS2022 is installed with the C++ workload; `vswhere.exe` must be at `%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\` |
| **Missing OpenGL headers on Linux** | Install `libopengl-dev libglx-dev mesa-common-dev` |
| **PipeWire warnings** | `can't load config client.conf` is harmless — audio still works via PulseAudio/ALSA fallback |
| **Build dies with `Terminated` / exit 143 on low-memory Linux** | Compile with fewer jobs: pass `--parallel 2` to `cmake --build` (unbounded `make -j` exhausts memory during the bgfx toolchain build) |
| **Runtime: game data not found** | Set the working directory to the folder containing the game's original data files |
| **Runtime: controller not detected** | Ensure `controller.enable = true` in `aitd_remaster.cfg` and that SDL3 supports your gamepad |
| **Runtime: fullscreen not persisting** | Ensure `graphics.fullscreen = true` is in your `aitd_remaster.cfg`; the setting is saved automatically when you close the system menu |
| **Runtime: console window covers the game** | The game window is automatically raised to the foreground at startup; press **F11** or **Alt+Enter** to go fullscreen |
