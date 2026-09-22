# macOS Apple Silicon (arm64) Windowed Build & Run — Design

Date: 2026-09-22
Status: Approved (design)

## Goal

Provide committed, reproducible infrastructure so the FITD engine (the `Tatou`
executable) builds **natively for Apple Silicon (arm64)** using the vendored
**SDL3**, and runs in **windowed mode** with an **unlocked mouse cursor**.

## Context

Findings from the current tree:

- The engine links the vendored SDL3 static library (`ThirdParty/SDL`,
  `SDL3-static`) and renders through bgfx. On macOS bgfx uses the **Metal**
  backend (`bgfxGlue.cpp:483`, `bgfxPatch.mm` `cbSetupMetalLayer`), which is
  native on Apple Silicon.
- **Windowed is already the default.** `graphics.fullscreen` defaults to
  `false` (`FitdLib/configRemaster.cpp:39`), and the compile-time
  `RUN_FULLSCREEN` flag is defined only for iOS/tvOS
  (`FitdLib/config.h:45-48`).
- **The cursor is never locked.** The codebase makes no
  `SDL_SetWindowRelativeMouseMode` or `SDL_SetWindowMouseGrab` calls. SDL3
  relative-mouse mode and mouse grab are opt-in; the default leaves the cursor
  free and unconfined (confirmed against the SDL3 wiki).
- **Game data is embedded** as C++ arrays under `FitdLib/embedded/`, with a
  filesystem fallback in `fileAccess.cpp`. The Steam/GOG/CD auto-copy path
  (`gameDataCopy_EnsureDataFiles`) is a **Windows-only** implementation; the
  non-Windows build returns a stub (`gameDataCopy.cpp:907`). No original PAK
  files are required to run.
- The existing `TatouSource/build/macos-x86_64` tree was **cross-compiled from
  WSL via osxcross** (`CMAKE_HOME_DIRECTORY=/mnt/d/FITD`,
  `CMAKE_OSX_SYSROOT=/opt/osxcross/...`). It is **not** a native build and
  produces an `x86_64` binary.
- `CMakePresets.json` defines presets for Windows, Linux, Switch, and UWP but
  has **no macOS preset**.
- An untracked `TatouSource/Makefile` already provides a
  `deps → configure → build → run` pipeline and resolves the macOS
  `Tatou.app/Contents/MacOS/Tatou` bundle path, but it does not set the target
  architecture.

## Scope

In scope:

- A `macos-arm64` CMake configure + build preset.
- Architecture support in `TatouSource/Makefile` (default `arm64` on Darwin).
- Documentation updates in `BUILDING.md`.
- Verification: build the arm64 binary and run it windowed with a free cursor.

Out of scope:

- Game/engine **behavior** changes. Windowed mode and cursor behavior are
  already correct by default; no enforcement code is added. The one permitted
  engine-source edit is a platform guard for Windows-only console code that
  otherwise fails to compile on macOS (see Files changed).
- Changes to Windows, Linux, Switch, UWP, iOS, or tvOS build paths.
- Committing build artifacts (the `build/` tree stays ignored/untracked).

## Design

### 1. CMake preset (`TatouSource/CMakePresets.json`)

Add a `macos-arm64` configure preset, following the existing per-platform
convention:

- `generator`: `Ninja`
- `binaryDir`: `${sourceDir}/build/macos-arm64`
- `cacheVariables`:
  - `CMAKE_BUILD_TYPE`: `Release`
  - `CMAKE_OSX_ARCHITECTURES`: `arm64`
  - `CMAKE_OSX_DEPLOYMENT_TARGET`: `11.3` (matches
    `LSMinimumSystemVersion` in `Fitd/Info.plist`)
  - `CMAKE_EXPORT_COMPILE_COMMANDS`: `ON`
- `condition`: `hostSystemName == Darwin`

Add a matching `macos-arm64` build preset (`configurePreset: macos-arm64`).

### 2. Makefile (`TatouSource/Makefile`)

Make the pipeline arch-aware without changing its existing shape:

- Add `ARCH ?= arm64`.
- On Darwin, append `-DCMAKE_OSX_ARCHITECTURES=$(ARCH)` to
  `CONFIGURE_FLAGS`.
- On Darwin, default `BUILD_DIR` to `build/macos-$(ARCH)` so the Makefile and
  the preset share **one** build tree (matching the repo's existing
  platform-named dirs such as `build/macos-x86_64`). Linux keeps its current
  `build/$(BUILD_TYPE)` default.
- Keep the existing `BINARY` resolution (already handles the `.app` bundle
  path) and `run` target (`make run data=<dir>`).

The Makefile is currently untracked; this plan commits it so the arm64
build/run flow is reproducible rather than machine-local.

### 3. Windowed mode

No code change. The default configuration is windowed
(`configRemaster.cpp:39`). The run step must use a working directory that does
not contain an `aitd_remaster.cfg` with `graphics.fullscreen = true`. If no
such file exists, `loadRemasterConfig` uses the in-memory windowed defaults and
does not write a file (`configRemaster.cpp:131-141`).

### 4. Mouse cursor

No code change. The engine never requests relative-mouse mode or mouse grab, so
SDL3 leaves the cursor visible, unconfined, and free to leave the window.

### 5. Run path

`make run data=<dir>` executes
`build/macos-arm64/Fitd/Tatou.app/Contents/MacOS/Tatou` with `<dir>` as the
current working directory. Game data is embedded, so `<dir>` may be any
writable directory (defaults to the Makefile's `data ?= .`).

### 6. Verification (smallest sufficient proof)

1. `make -C TatouSource deps` — confirm `cmake`, `ninja`, `pkg-config` present
   (Homebrew tools already installed).
2. `make -C TatouSource configure generator=Ninja`
3. `make -C TatouSource build-fitd`
4. `file TatouSource/build/macos-arm64/Fitd/Tatou.app/Contents/MacOS/Tatou`
   must report `arm64` (not `x86_64`, not `universal`).
5. `make -C TatouSource run data=<writable-dir>` — confirm:
   - the window opens **windowed** (not fullscreen);
   - the mouse cursor moves freely and can leave the window.

### 7. Documentation (`BUILDING.md`)

Replace the "macOS (experimental)" section with concrete Apple Silicon steps:

- prerequisites (`xcode-select --install`, `brew install cmake ninja pkg-config`);
- preset-based configure/build (`cmake --preset macos-arm64`,
  `cmake --build --preset macos-arm64`) and the `make` equivalents;
- note that the build targets arm64 and runs windowed with an unlocked cursor
  by default.

## Risks

- **CMake 4.x compatibility.** Host CMake is 4.4.3. Third-party minimums are
  `bgfx 3.10.2`, `zlib 3.5.0`, `soloud 3.5.0`, `SDL 3.16` — all at or above the
  CMake 4 floor of 3.5, so no policy failure is expected. If a subproject
  errors, the fix is a `CMAKE_POLICY_VERSION_MINIMUM` cache entry on the
  preset; this will be handled during verification if it occurs.
- **First build cost.** bgfx/SDL/soloud/zlib compile from source; the first
  configure+build is long but one-time.
- **Vendored zlib 1.2.11 vs the modern macOS SDK.** `zutil.h` redefines
  `fdopen` when `TARGET_OS_MAC` is defined, which the current SDK does;
  this breaks compilation. Resolved by removing that legacy branch to match
  upstream zlib (Apple still gets `OS_CODE 19` via `__APPLE__`).

## Files changed

| File | Change |
|------|--------|
| `TatouSource/CMakePresets.json` | add `macos-arm64` configure + build presets |
| `TatouSource/Makefile` | add `ARCH` and Darwin arch flag; commit the file |
| `BUILDING.md`, `TatouSource/BUILDING.md`, `TatouSource/Docs/BUILDING.md` | replace experimental macOS section with arm64 steps (all three copies, to avoid drift) |
| `TatouSource/ThirdParty/zlib/zutil.h` | drop the legacy `TARGET_OS_MAC` `fdopen` macro that fails to compile under the current macOS SDK (discovered during implementation; matches upstream zlib) |
| `TatouSource/FitdLib/main.cpp` | wrap the Windows-only `GetConsoleWindow()`/`ShowWindow(..., SW_HIDE)` block in `#ifdef _WIN32`; it was the only remaining macOS compile error (discovered during implementation) |
| `TatouSource/cmake/copy_existing_files.cmake` | add the missing script the macOS asset-copy POST_BUILD step invokes (never existed in git) |
| `TatouSource/Fitd/CMakeLists.txt` | pass the asset list to that script joined with `\|` instead of a raw semicolon list the shell splits (made `make build-fitd` exit 127) |
