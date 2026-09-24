# AGENTS.md

Guidance for anyone (human or agent) changing this repository.

## Project map

- `TatouSource/` — the CMake project: the Tatou engine (a FITD fork) for
  Alone in the Dark. `FitdLib/` is the engine library, `Fitd/` the executable,
  `ThirdParty/` vendored code (SDL3, bgfx, ImGui, SoLoud, zlib, doctest).
- `TatouSource/FitdLib/mouse/` — left-button mouse gameplay. Files named in
  "Mouse rules" below as engine-free include only standard headers and are
  unit-tested in `TatouSource/tests/engine/`. The engine adapter is
  `mouseWorld.cpp` (frame, intents, public API) with `mouseWorldGeometry.cpp`
  (rooms, cameras, caches), `mouseWorldResolve.cpp` (what a click means),
  `mouseWorldPush.cpp`, `mouseWorldAttack.cpp` and `mouseWorldDebug.cpp`,
  sharing `mouseWorldInternal.h`; the engine includes only `mouseWorld.h`.
- `tools/` + `tests/tools/` — the Python texture pipeline (`make test-tools`).
- `docs/` — contracts and checklists (`docs/mouse-gameplay-checklist.md`).
- `graphify-out/` (any depth) — generated knowledge graph (`/graphify`);
  git-ignored, never commit it.

## Commands

```bash
make build-fitd          # build the game
make run data=DIR        # run from a directory holding the original .PAK files
make test-engine         # C++ unit tests (doctest) for engine-free modules
make test-tools          # Python texture-tool tests
make test                # both
```

## Threads

SDL events are read on the main thread in `readKeyboard()` (`osystemSDL.cpp`);
the game loop (`PlayWorld`, menus, scripts) runs on the game thread. The two
alternate in lockstep through the `startOfRender`/`endOfRender` semaphores, so
state handed over between `readKeyboard()` and `SDL_SignalSemaphore` needs no
lock. SDL cursor and window calls belong on the main thread.

## Firm rules

1. **Never lock, grab, confine or warp the OS cursor.** Do not call — or
   mention, even in comments — `SDL_SetWindowMouseGrab`,
   `SDL_SetWindowRelativeMouseMode`, `SDL_SetWindowMouseRect`,
   `SDL_WarpMouseInWindow`, `SDL_WarpMouseGlobal` or `SDL_CaptureMouse`
   anywhere under `TatouSource/FitdLib/` and `TatouSource/Fitd/`. SDL's default mouse auto-capture
   (which keeps delivering the button-up after a drag leaves the window,
   without restraining the cursor) stays on: never set its hint. Vendored
   `ThirdParty/` code is exempt. `make test-engine` enforces this.
2. **Keyboard and gamepad gameplay must keep working unchanged.** Mouse
   steering runs only through `mouseNavSteer()` at the top of `processTrack`
   case 1; with the "Mouse gameplay" option off the game behaves exactly as
   before.
3. **Mouse rules** (`FitdLib/mouse/`):
   - Walk intents are hold-bound: no button, no movement.
   - A held pointer is re-resolved only when it moves; a still pointer never
     retargets at a camera cut (6 px dead zone after a cut).
   - A held push never asserts the global Action (`0x2000`).
   - One resolver (`resolveAt`) drives both the cursor and the click;
     hovering never changes game state.
   - Picking uses only the engine's integer projection
     (`mouse::projectPoint` replicates `transformPoint` + the renderer
     divide), never a float render path.
   - Every screen entered from gameplay calls `mouseWorldTakeOver()` first.
   - A clicked object acts only on touch: the hero leans into it and the
     engine's own collision opens `FoundObjet`; a found script gets one frame
     of Action on the touch. The mouse never calls `FoundObjet` itself.
   - While a script owns the hero (`trackMode != 1`) world clicks resolve to
     blocked (the HUD still works) and a held button is spent.
   - SDL cursor calls happen only in `mouseInputEndMainFrame()` (main thread),
     which also applies ImGui's requested shape while ImGui wants the mouse.
   - Screens hit-test through the `menuMouseHit*` helpers, which record item
     clicks for the fullscreen double-click guard; a hand-rolled hit test calls
     `menuNoteItemClick()`, and a click that ends a wait uses
     `menuMouseSkipClicked()`.
   - Engine-free files (`mouseTypes.h`, `mouseGate.h`, `mouseGesture.*`,
     `mousePick.*`, `mousePoly.*`, `mouseNav.*`, `mouseHudLayout.h`) include
     no FitdLib, SDL or ImGui headers; new behaviour in them gets a doctest
     first.
