# macOS Apple Silicon (arm64) Windowed Build & Run — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the FITD engine (`Tatou`) natively for Apple Silicon (arm64) with SDL3 and run it in windowed mode with an unlocked mouse cursor.

**Architecture:** The engine already links vendored SDL3 and renders through bgfx's Metal backend on macOS. Windowed mode and an unlocked cursor are already the defaults; this plan adds the missing native-arm64 build configuration (a CMake preset plus Makefile arch support), documents it, and verifies the result. No game source is modified.

**Tech Stack:** CMake 4.4.3, Ninja, Apple Clang (Xcode command line tools), vendored SDL3/bgfx/soloud/zlib, GNU Make.

## Global Constraints

- Target architecture: `arm64` only (`CMAKE_OSX_ARCHITECTURES=arm64`). Not `x86_64`, not universal.
- Build type: `Release`. Generator: `Ninja`. Deployment target: `11.3` (matches `TatouSource/Fitd/Info.plist` `LSMinimumSystemVersion`).
- macOS build output is the bundle `build/macos-arm64/Fitd/Tatou.app`; the runnable binary is `Contents/MacOS/Tatou`.
- Windowed mode and unlocked cursor must remain the default behavior — **do not** add `SDL_SetWindowFullscreen`, `SDL_SetWindowRelativeMouseMode`, `SDL_SetWindowMouseGrab`, or `SDL_SetWindowGrab` calls.
- Do **not** modify game/engine behavior. The only permitted engine-source changes are the two macOS build fixes in Task 4 (Steps 0 and 0b): the platform guard for Windows-only console code, and the broken macOS asset-copy POST_BUILD step. Both are behavior-preserving (the POST_BUILD lives inside the Darwin-only block).
- Do **not** change Windows, Linux, Switch, UWP, iOS, or tvOS build paths.
- All shell commands in this plan run from `TatouSource/` unless stated otherwise:
  `cd /Users/felipe.dos.santos/code/mine/AloneInTheDarkReHaunted/TatouSource`

---

### Task 1: Add `macos-arm64` CMake presets

**Files:**
- Modify: `TatouSource/CMakePresets.json`

**Interfaces:**
- Consumes: nothing.
- Produces: configure preset `macos-arm64` (generator `Ninja`, binaryDir `${sourceDir}/build/macos-arm64`, `CMAKE_OSX_ARCHITECTURES=arm64`, `CMAKE_OSX_DEPLOYMENT_TARGET=11.3`, `CMAKE_BUILD_TYPE=Release`, `CMAKE_EXPORT_COMPILE_COMMANDS=ON`) and a matching build preset `macos-arm64`. Task 2 and Task 4 rely on the binaryDir path `build/macos-arm64`.

- [ ] **Step 1: Add the configure preset**

In `TatouSource/CMakePresets.json`, the last entry of the `configurePresets` array is `uwp-arm64-release`. Match that whole block (its `"name": "uwp-arm64-release"` makes the anchor unique):

```json
        {
            "name": "uwp-arm64-release",
            "displayName": "UWP ARM64 Release",
            "generator": "Visual Studio 17 2022",
            "architecture": "ARM64",
            "binaryDir": "${sourceDir}/build/uwp-arm64-release",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Release",
                "CMAKE_SYSTEM_NAME": "WindowsStore",
                "CMAKE_SYSTEM_VERSION": "10.0",
                "CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION": "10.0.22621.0"
            },
            "condition": {
                "type": "equals",
                "lhs": "${hostSystemName}",
                "rhs": "Windows"
            }
        }
    ],
```

Add a comma after that block's closing `}` and insert the new preset so the array ends like this:

```json
        },
        {
            "name": "macos-arm64",
            "displayName": "macOS Apple Silicon (arm64) Release",
            "generator": "Ninja",
            "binaryDir": "${sourceDir}/build/macos-arm64",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Release",
                "CMAKE_OSX_ARCHITECTURES": "arm64",
                "CMAKE_OSX_DEPLOYMENT_TARGET": "11.3",
                "CMAKE_EXPORT_COMPILE_COMMANDS": "ON"
            },
            "condition": {
                "type": "equals",
                "lhs": "${hostSystemName}",
                "rhs": "Darwin"
            }
        }
    ],
```

- [ ] **Step 2: Add the build preset**

Find the end of the `buildPresets` array. The last entry is:

```json
        {
            "name": "uwp-arm64-release",
            "configurePreset": "uwp-arm64-release",
            "configuration": "Release"
        }
    ]
}
```

Add a comma after that closing `}` and insert the new build preset:

```json
        {
            "name": "uwp-arm64-release",
            "configurePreset": "uwp-arm64-release",
            "configuration": "Release"
        },
        {
            "name": "macos-arm64",
            "configurePreset": "macos-arm64"
        }
    ]
}
```

- [ ] **Step 3: Verify the presets parse and are listed**

Run: `cmake --list-presets`
Expected: output lists `"macos-arm64"` under `Available configure presets`.

Run: `cmake --list-presets=build`
Expected: output lists `"macos-arm64"` under `Available build presets`.

- [ ] **Step 4: Configure with the preset**

Run: `cmake --preset macos-arm64`
Expected: configure completes without error and creates `build/macos-arm64/CMakeCache.txt`.

- [ ] **Step 5: Verify the cache records arm64 and Ninja**

Run:
```bash
grep -E "CMAKE_OSX_ARCHITECTURES|CMAKE_OSX_DEPLOYMENT_TARGET|CMAKE_GENERATOR:" build/macos-arm64/CMakeCache.txt
```
Expected: lines showing `arm64`, `11.3`, and `CMAKE_GENERATOR:INTERNAL=Ninja`.

- [ ] **Step 6: Commit**

```bash
cd /Users/felipe.dos.santos/code/mine/AloneInTheDarkReHaunted
git add TatouSource/CMakePresets.json
git commit -m "build: add macos-arm64 CMake presets"
```

---

### Task 2: Make `TatouSource/Makefile` arch-aware and commit it

**Files:**
- Modify: `TatouSource/Makefile:6-14`
- Commit: `TatouSource/Makefile` (currently untracked)

**Interfaces:**
- Consumes: the binaryDir `build/macos-arm64` produced by Task 1's preset (both must use generator `Ninja` so they share one build tree).
- Produces: `make configure` / `make build` / `make build-fitd` / `make run` on Darwin target arm64 in `build/macos-arm64`, with `ARCH ?= arm64` overridable (`make build ARCH=x86_64`). Task 4 uses `make build-fitd` and `make run`.

- [ ] **Step 1: Replace the variable block**

In `TatouSource/Makefile`, replace lines 6-14:

```make
# Variables
BUILD_TYPE ?= Release
BUILD_DIR ?= build/$(BUILD_TYPE)
JOBS ?= $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
CMAKE = cmake

# Optional per-invocation args
generator ?=
CONFIGURE_FLAGS = -DCMAKE_BUILD_TYPE="$(BUILD_TYPE)" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(if $(generator),-G "$(generator)")
```

with:

```make
# Variables
UNAME_S := $(shell uname -s)

BUILD_TYPE ?= Release

ifeq ($(UNAME_S),Darwin)
# Apple Silicon: default to arm64, Ninja, and a platform-named build dir that
# matches the "macos-arm64" CMake preset so both entry points share one tree.
ARCH ?= arm64
DEPLOY_TARGET ?= 11.3
generator ?= Ninja
BUILD_DIR ?= build/macos-$(ARCH)
DARWIN_FLAGS = -DCMAKE_OSX_ARCHITECTURES="$(ARCH)" -DCMAKE_OSX_DEPLOYMENT_TARGET="$(DEPLOY_TARGET)"
else
BUILD_DIR ?= build/$(BUILD_TYPE)
DARWIN_FLAGS =
endif

JOBS ?= $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
CMAKE = cmake

# Optional per-invocation args
CONFIGURE_FLAGS = -DCMAKE_BUILD_TYPE="$(BUILD_TYPE)" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(DARWIN_FLAGS) $(if $(generator),-G "$(generator)")
```

- [ ] **Step 2: Verify the Makefile configures the shared tree**

Run: `make configure`
Expected: CMake configures `build/macos-arm64` with no error.

Run:
```bash
grep -E "CMAKE_OSX_ARCHITECTURES|CMAKE_OSX_DEPLOYMENT_TARGET|CMAKE_GENERATOR:" build/macos-arm64/CMakeCache.txt
```
Expected: `arm64`, `11.3`, and `CMAKE_GENERATOR:INTERNAL=Ninja` (same tree Task 1 configured, no generator conflict; the Makefile sets the deployment target too, not only the preset).

- [ ] **Step 3: Verify the run target resolves the arm64 bundle path**

Run: `make -n run`
Expected: the printed launch command ends with `/build/macos-arm64/Fitd/Tatou.app/Contents/MacOS/Tatou` (not `build/Release/...`, not `x86_64`).

- [ ] **Step 4: Verify `ARCH` is overridable**

Run: `make -n configure ARCH=x86_64 | grep -o 'CMAKE_OSX_ARCHITECTURES="[^"]*"'`
Expected: `CMAKE_OSX_ARCHITECTURES="x86_64"` — confirming the default is arm64 but can be overridden.

- [ ] **Step 5: Commit**

```bash
cd /Users/felipe.dos.santos/code/mine/AloneInTheDarkReHaunted
git add TatouSource/Makefile
git commit -m "build: add Apple Silicon arch support to Makefile"
```

---

### Task 3: Fix vendored zlib `fdopen` clash for the modern macOS SDK

**Files:**
- Modify: `TatouSource/ThirdParty/zlib/zutil.h:133-145`

**Interfaces:**
- Consumes: nothing.
- Produces: a `zlibstatic` target that compiles under the current macOS SDK, unblocking Task 4's build. Behavior is unchanged on every platform (Apple still gets `OS_CODE 19` from the existing `#ifdef __APPLE__` branch).

- [ ] **Step 1: Replace the legacy macOS block**

In `TatouSource/ThirdParty/zlib/zutil.h`, replace:

```c
#if defined(MACOS) || defined(TARGET_OS_MAC)
#  define OS_CODE  7
#  ifndef Z_SOLO
#    if defined(__MWERKS__) && __dest_os != __be_os && __dest_os != __win32_os
#      include <unix.h> /* for fdopen */
#    else
#      ifndef fdopen
#        define fdopen(fd,mode) NULL /* No fdopen() */
#      endif
#    endif
#  endif
#endif
```

with:

```c
#if defined(MACOS)
#  define OS_CODE  7
#endif
```

This matches upstream zlib, which removed the `TARGET_OS_MAC`/`fdopen` branch. The current macOS SDK defines `TARGET_OS_MAC` and declares `fdopen` as a function in `<stdio.h>`; the removed macro mangled that declaration. `__APPLE__` still sets `OS_CODE 19` (`zutil.h:162`), so Apple behavior is unchanged.

- [ ] **Step 2: Build the zlib target**

Run: `cmake --build build/macos-arm64 --target zlibstatic`
Expected: builds with no errors. (Pre-existing `-Wdeprecated-non-prototype` warnings from zlib's K&R-style declarations are expected and unrelated.)

- [ ] **Step 3: Commit**

```bash
cd /Users/felipe.dos.santos/code/mine/AloneInTheDarkReHaunted
git add TatouSource/ThirdParty/zlib/zutil.h
git commit -m "fix(zlib): drop legacy TARGET_OS_MAC fdopen macro that breaks modern macOS SDK"
```

---

### Task 4: Build and verify the arm64 windowed, unlocked-cursor binary

**Files:**
- Modify: `TatouSource/FitdLib/main.cpp:5529-5532` (platform guard only)
- Create: `TatouSource/cmake/copy_existing_files.cmake`
- Modify: `TatouSource/Fitd/CMakeLists.txt:86-96` (asset-copy argument only)

**Interfaces:**
- Consumes: `macos-arm64` preset (Task 1), Makefile targets (Task 2), and the zlib fix (Task 3).
- Produces: a verified `build/macos-arm64/Fitd/Tatou.app/Contents/MacOS/Tatou` reported as `arm64`, confirmed to launch windowed with a free cursor.

- [ ] **Step 0: Guard the Windows-only console code**

`FitdLib/main.cpp:5529-5532` calls `GetConsoleWindow()` / `ShowWindow(..., SW_HIDE)` with no platform guard, which fails to compile off Windows (`unknown type name 'HWND'`, `use of undeclared identifier 'GetConsoleWindow'`, `use of undeclared identifier 'SW_HIDE'`). This is the only remaining compile error in the `Fitd` target — verified with `ninja -C build/macos-arm64 -k 0 Fitd`.

Wrap only those lines in `#ifdef _WIN32`, matching the existing guarded copies in `Fitd/fitd.cpp:98-104` and `FitdLib/osystemSDL.cpp:195`:

```cpp
    if (g_remasterConfig.ui.showOptionsAtStartup)
    {// Hide the console window at startup; it will be shown after the main window is created
#ifdef _WIN32
        HWND hConsole = GetConsoleWindow();
        if (hConsole)
            ShowWindow(hConsole, SW_HIDE);
#endif
        remasterOptionsBeginStartupGate();
```

Behavior on Windows is unchanged; on non-Windows the block was never compiled before.

- [ ] **Step 0b: Repair the macOS asset-copy POST_BUILD step**

`Fitd/CMakeLists.txt:88-96` runs a POST_BUILD step that copies optional assets into `Tatou.app/Contents/Resources`. It is broken two ways: it runs `cmake -P "${CMAKE_SOURCE_DIR}/cmake/copy_existing_files.cmake"`, a script that has never existed in git; and it passes `"-DFILES=${MACOS_ASSETS}"` where `MACOS_ASSETS` is a list, so the emitted shell command contains unquoted `;` and the shell tries to execute each path as a command. Result: `make build-fitd` exits 127 *after* the binary links successfully, and no `Resources` directory is produced.

Create `TatouSource/cmake/copy_existing_files.cmake`:

```cmake
# Copy the files listed in FILES into DESTINATION, skipping any that do not
# exist. Invoked via `cmake -P` from the Fitd macOS POST_BUILD step.
#
# FILES is a single "|"-separated string so the shell cannot split the list on
# semicolons.

if(NOT DEFINED DESTINATION)
    message(FATAL_ERROR "copy_existing_files.cmake: DESTINATION is required")
endif()

if(NOT DEFINED FILES OR FILES STREQUAL "")
    return()
endif()

file(MAKE_DIRECTORY "${DESTINATION}")

string(REPLACE "|" ";" _files "${FILES}")
foreach(_file IN LISTS _files)
    if(EXISTS "${_file}")
        file(COPY "${_file}" DESTINATION "${DESTINATION}")
    endif()
endforeach()
```

In `Fitd/CMakeLists.txt`, make exactly two edits: add the `string(REPLACE ...)` line immediately before `add_custom_command`, and change the `"-DFILES=${MACOS_ASSETS}"` argument to `"-DFILES=\"${MACOS_ASSETS_ARG}\""`.

The escaped quotes are required: CMake's Ninja generator emits custom-command arguments **raw** (this block does not use `VERBATIM`), so a bare `a|b|c` would be read by `/bin/sh` as a pipe. Escaping the quotes keeps it a single argument, matching the block's existing `-DDESTINATION="…"` idiom. The resulting block is:

```cmake
    # Copy any assets that might not exist yet at configure time via a post-build step.
    # Missing optional assets should not make the executable link fail.
    # Join with "|" so the shell cannot split the list on semicolons.
    string(REPLACE ";" "|" MACOS_ASSETS_ARG "${MACOS_ASSETS}")
    add_custom_command(TARGET Fitd POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DDESTINATION="$<TARGET_BUNDLE_CONTENT_DIR:Fitd>/Resources"
            "-DFILES=\"${MACOS_ASSETS_ARG}\""
            -P "${CMAKE_SOURCE_DIR}/cmake/copy_existing_files.cmake"
        COMMAND chmod +x
            "$<TARGET_BUNDLE_CONTENT_DIR:Fitd>/MacOS/Tatou"
        COMMENT "Copying game assets into Tatou.app/Contents/Resources"
    )
```

- [ ] **Step 1: Build the game executable**

Run: `make build-fitd`
Expected: build completes. (First build compiles bgfx/SDL/soloud/zlib from source and can take several minutes.)

- [ ] **Step 2: Verify the binary architecture**

Run:
```bash
file build/macos-arm64/Fitd/Tatou.app/Contents/MacOS/Tatou
```
Expected: `Mach-O 64-bit executable arm64` — must **not** say `x86_64` and must **not** say `universal`.

- [ ] **Step 3: Static check — no mouse grab / relative mode calls**

Run:
```bash
rg -n "SDL_SetWindowRelativeMouseMode|SDL_SetRelativeMouseMode|SDL_SetWindowMouseGrab|SDL_SetWindowGrab" FitdLib Fitd
```
Expected: no matches (exit code 1). This confirms the engine never locks or confines the cursor.

- [ ] **Step 4: Prepare a clean data directory**

Run:
```bash
mkdir -p /var/folders/h_/rk2gng5d0x99pw7x_3dg6mj40000gn/T/opencode/aitd-run
ls /var/folders/h_/rk2gng5d0x99pw7x_3dg6mj40000gn/T/opencode/aitd-run
```
Expected: the directory exists and is empty (no `aitd_remaster.cfg`), so the engine loads the windowed defaults.

- [ ] **Step 5: Run the game**

Run: `make run data=/var/folders/h_/rk2gng5d0x99pw7x_3dg6mj40000gn/T/opencode/aitd-run`

Expected, observed manually:
1. A **windowed** game window appears (has title bar / borders, does not cover the whole screen).
2. The mouse cursor is **visible and free** — it can be moved out of the game window and back, and other apps can be clicked without the cursor snapping back.

Then quit the game (close the window or `Cmd+Q`).

- [ ] **Step 6: Commit the two macOS build fixes**

Steps 0 and 0b are the only committed changes in this task. Do not commit build artifacts; confirm `git status` shows only the three source files staged.

If neither fix is committed yet:

```bash
cd /Users/felipe.dos.santos/code/mine/AloneInTheDarkReHaunted
git add TatouSource/FitdLib/main.cpp TatouSource/cmake/copy_existing_files.cmake TatouSource/Fitd/CMakeLists.txt
git commit -m "fix(macos): guard Windows-only console code and repair asset-copy step"
```

If Step 0's guard is already committed, commit the remaining Step 0b files on their own:

```bash
cd /Users/felipe.dos.santos/code/mine/AloneInTheDarkReHaunted
git add TatouSource/cmake/copy_existing_files.cmake TatouSource/Fitd/CMakeLists.txt
git commit -m "fix(macos): repair asset-copy POST_BUILD step"
```

- [ ] **Step 7: Record the result**

If the window appears fullscreen or the cursor is confined, STOP and report — do not patch further; the fix belongs in the build/config path and must be re-designed.

---

### Task 5: Document macOS arm64 build in all three `BUILDING.md` copies

**Files:**
- Modify: `BUILDING.md` (macOS section, currently lines 278-297)
- Modify: `TatouSource/BUILDING.md` (macOS section, currently lines 112-131)
- Modify: `TatouSource/Docs/BUILDING.md` (macOS section, currently lines 119-138)

**Interfaces:**
- Consumes: preset name `macos-arm64` and Makefile targets from Tasks 1-2.
- Produces: identical, current instructions in all three copies (no drift).

- [ ] **Step 1: Replace the macOS section in each file**

In each of the three files, replace this block:

```markdown
## macOS (experimental)

> macOS support compiles but is less tested than Windows and Linux.

### 1. Install tools

```bash
xcode-select --install          # Apple Clang
brew install cmake ninja        # via Homebrew
```

### 2. Build

```bash
mkdir -p build/macos && cd build/macos
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ../..
cmake --build . --target Fitd
```

The CMake configuration automatically includes the Objective-C++ patch file (`bgfxPatch.mm`) on Darwin.
```

with:

````markdown
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

The `TatouSource/Makefile` targets the same `build/macos-arm64` tree with
`ARCH=arm64` by default:

```bash
cd TatouSource
make build-fitd                                   # configure + build the game
make run data=/path/to/writable/dir               # launch windowed
```

Game data is embedded in the binary, so no original PAK files are required.

### 4. Verify the architecture

```bash
file TatouSource/build/macos-arm64/Fitd/Tatou.app/Contents/MacOS/Tatou
# => Mach-O 64-bit executable arm64
```

The CMake configuration automatically includes the Objective-C++ patch file (`bgfxPatch.mm`) on Darwin.
````

- [ ] **Step 2: Verify all three copies are consistent**

Run:
```bash
cd /Users/felipe.dos.santos/code/mine/AloneInTheDarkReHaunted
for f in BUILDING.md TatouSource/BUILDING.md TatouSource/Docs/BUILDING.md; do
  echo "== $f =="; grep -n "macOS (Apple Silicon)\|macos-arm64" "$f";
done
```
Expected: each file shows the `macOS (Apple Silicon)` heading and the `macos-arm64` preset references; none still says `macOS (experimental)`.

- [ ] **Step 3: Commit**

```bash
cd /Users/felipe.dos.santos/code/mine/AloneInTheDarkReHaunted
git add BUILDING.md TatouSource/BUILDING.md TatouSource/Docs/BUILDING.md
git commit -m "docs: document macOS arm64 build and run"
```

---

## Notes for the implementer

- `TatouSource/build/` already contains `macos-x86_64` (an osxcross cross-compile from WSL) and other platform trees. Do not delete or modify them.
- `TatouSource/build/macos-arm64/` will be large; it must not be committed (verify `git status` shows no build artifacts staged).
- If CMake 4.4.3 rejects a third-party subproject with a policy error, add `"CMAKE_POLICY_VERSION_MINIMUM": "3.5"` to the `macos-arm64` configure preset's `cacheVariables` and re-run Task 1 Step 4. Third-party minimums currently in tree are bgfx 3.10.2, SDL 3.16, zlib/soloud 3.5.0, so this is not expected.
