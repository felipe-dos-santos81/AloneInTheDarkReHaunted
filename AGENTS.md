# AGENTS.md

Guidance for anyone (human or agent) changing this repository.

## Project map

- `TatouSource/` — the CMake project: the Tatou engine (a [FITD](https://github.com/yaz0r/FITD) fork) for
  Alone in the Dark. `FitdLib/` is the engine library, `Fitd/` the executable,
  `ThirdParty/` vendored code ([SDL3](https://github.com/libsdl-org/SDL), bgfx, ImGui, SoLoud, zlib, doctest).
- `TatouSource/FitdLib/mouse/` — left-button mouse gameplay. Files named in
  "Mouse rules" below as engine-free include only standard headers and are
  unit-tested in `TatouSource/tests/engine/`. The engine adapter is
  `mouseWorld.cpp` (frame, intents, public API) with `mouseWorldGeometry.cpp`
  (rooms, cameras, caches), `mouseWorldResolve.cpp` (what a click means),
  `mouseWorldPush.cpp`, `mouseWorldAttack.cpp` and `mouseWorldDebug.cpp`,
  sharing `mouseWorldInternal.h`; the engine includes only `mouseWorld.h`.
- `TatouSource/FitdLib/models/` — HD character model math, engine-free
  (standard headers only) and unit-tested in `TatouSource/tests/engine/`:
  `affine3.h`, `bodyPose.*` (a body's pose as one matrix per bone group; its
  Python twin is `tools/aitd_models/pose.py`), `renderCamera.*` (the
  engine's camera and 320x200 projection in float), `hdmMesh.*` (reads
  the `body_<KEY>.hdm` file `make import-models` writes; its Python twin is
  `tools/aitd_models/hdm.py`, and both read
  `TatouSource/tests/engine/fixtures/tiny.hdm`), `skinnedBody.*` (bone
  matrices and screen box of a replacement), `replacementGate.h` (when a
  body draws as its replacement), `mipChain.*` (mip levels that keep each
  texel's alpha class: hole, translucent or opaque; the same 128/253 limits
  are in `hdm.py` and `model_ps.sc`, and `tests/tools/test_models_hdm.py`
  checks they agree) and `modelLight.*`. Their
  engine adapter is `modelReplacement.*` (loads `models_hd/*.hdm`, draws in
  `AffObjet` instead of the classic primitives, behind `graphics.hdModels`);
  `hdCompare.*` is its developer check (`debug.hdModelsCompare`, scored by
  `tools/hd_compare.py`; `docs/hd-models-checklist.md`).
- `TatouSource/FitdLib/assist/` — accessibility assists. The automatic
  counter-attack: `counterRule.*` is engine-free (standard headers only) and
  unit-tested in `TatouSource/tests/engine/`; `counterAttack.*` is its engine
  adapter. The enemy attack pace: `paceRule.*` is engine-free and unit-tested
  likewise; `attackPace.*` is its engine adapter. The adapters are the only
  assist files that touch engine globals. In-game sign-off:
  `docs/combat-assist-checklist.md`.
- `TatouSource/FitdLib/physics/` — collision rules, engine-free and header-only,
  unit-tested in `TatouSource/tests/engine/`: `collisionEscape.h` (how an
  actor already inside a blocker may move, used by `GereCollision`).
- `tools/` + `tests/tools/` — the Python HD model tools, tested by
  `make test-tools`. `tools/aitd_data/` reads the game data (PAKs,
  palette); `tools/aitd_models/` exports the bodies and imports the
  replacements (`make export-models`, `make import-models`; contract in
  `docs/model-contract.md`, sign-off in `docs/hd-models-checklist.md`);
  `tools/aitd_models/blender/` is the in-repo generator (`make
  blender-models`: Blender refines the original bodies and bakes the
  hand-made `Assets/atlases` onto them; its `stage.py` runs inside Blender
  and imports only `bpy`, `bmesh`, `mathutils` and numpy).
- `docs/` — the guides (`BUILDING.md`, `ARCHITECTURE.md`, `REMASTER.md`,
  `CONTRIBUTING.md`), contracts, checklists (`docs/mouse-gameplay-checklist.md`)
  and the `aitd_remaster.cfg` manual (`docs/configuration.md`: a new key goes there);
  `docs/screenshots/` holds the README's original-vs-HD pairs.
- `graphify-out/` (any depth) — generated knowledge graph (`/graphify`);
  git-ignored, never commit it.

## Commands

```bash
make help                # every target, grouped, with its arguments
make build-fitd          # build the game
make run [data=DIR]      # play from the folder of .PAK files (default data/aitd1)
make test-engine         # C++ unit tests (doctest) for engine-free modules
make test-tools          # Python tool tests
make test                # both
```

## CI

`.github/workflows/build.yml` mirrors `make build-fitd` plus the test suites
on Linux, macOS and Windows (push and PR to `main`). Two platform facts keep
it green: with a Makefile generator always give `cmake --build` a numbered
`--parallel N` (bare `--parallel` is unbounded `make -j` and OOMs the Linux
runner), and never name identifiers `min`, `max`, `near` or `far` in engine
code — windows.h macros; the `NOMINMAX` target define in
`TatouSource/CMakeLists.txt` keeps `min`/`max` out, MSVC defines
`near`/`far` anyway.

## Git

`TatouSource/build/` holds the CMake build trees and is ignored (only the
`vs20xx.bat` scripts there are tracked). `Assets/` art changes locally, so
stage files by name — never `git add -A` or `git commit -a`.

## Threads

SDL events are read on the main thread in `readKeyboard()` (`osystemSDL.cpp`);
the game loop (`PlayWorld`, menus, scripts) runs on the game thread. The two
alternate in lockstep through the `startOfRender`/`endOfRender` semaphores, so
state handed over between `readKeyboard()` and `SDL_SignalSemaphore` needs no
lock. SDL cursor and window calls belong on the main thread.

## Unit tests: fewer, better

Every test is code someone has to read, maintain and wait on in CI. A test
earns its place only if it can fail for a reason no other test already
covers: aim for distinct behaviours verified, not test count or coverage.
This applies to the doctest suite (`TatouSource/tests/engine/`) and the
pytest suite (`tests/tools/`). The "doctest first" rules below still hold for
each new behaviour.

**Before writing a test**

1. Read the existing tests for the code you touch. If a behaviour is already
   covered, extend or adjust that test instead of adding one beside it.
2. List the distinct behaviours to verify, one line each. Merge any two that
   would fail for the same underlying bug.
3. For each one, name the bug that would slip through if you deleted the
   test and the others stayed. If you can't, don't write it.

**Redundant**

- Several inputs from one equivalence class: pick one representative plus
  the boundaries.
- The same logic tested at several layers: test it once, at the lowest layer
  that owns it. Higher layers test only their own wiring and logic.
- Tests that differ only in inputs and expected values: make them one
  table-driven test (a loop over a table in one `TEST_CASE`, or
  `pytest.mark.parametrize`).
- A new test that is a strict subset of an existing, broader one.

**Not tested at all**

- Trivial code with no logic: getters, setters, plain constructors,
  constants, simple delegation.
- The language, the standard library, SDL, bgfx or other dependencies.
- Implementation details: private helpers, internal call order, or mocks
  that restate the implementation. Test observable behaviour through the
  public interface.
- What the type system or compiler already rules out.

**Still tested**

- Each distinct branch or behaviour of the public contract, once.
- Boundaries and edge cases: empty, zero, the largest value, null, off by
  one.
- Error paths with their own handling.
- A regression test for each bug fixed, aimed at that bug.

**When changing code**, update the tests that cover the changed behaviour
rather than adding parallel ones, and delete the tests the change makes
obsolete. Don't add tests for code you didn't change unless asked.

**When done**, state briefly which behaviours you tested and which you
skipped as redundant or trivial, so the reviewer can disagree.

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
     retargets at a camera cut (6 px dead zone after a cut). A cut is a change
     of floor camera, not of `NumCamera`, the room's own slot number. A held
     walk goes on while the hero walks under the pointer; a press on the hero
     does nothing.
   - A held push never asserts the global Action (`0x2000`).
   - One resolver (`resolveAt`) drives both the cursor and the click;
     hovering never changes game state.
   - The floor is the cover zones plus the seams `buildGrid` fills (enclosed
     gaps between zones, at most `kSeamDepth` cells deep); the walk grid, the
     floor pick and the debug overlay all read it the same way.
   - Picking uses only the engine's integer projection
     (`mouse::projectPoint` replicates `transformPoint` + the renderer
     divide), never a float render path.
   - Every screen entered from gameplay calls `mouseWorldTakeOver()` first.
   - A clicked object acts only on touch: the hero leans into it and the
     engine's own collision opens `FoundObjet`; a found script gets Action on
     the touch. Scripted scenery is held-pushed, unless an inventory action
     other than push is armed (`sceneryUse`; an object in hand other than a
     weapon arms its use, `armedActionFor`): then it too is touched and gets
     Action, and so is furniture painted into the background (a type-9 hard
     col; touched when the hero's `HARD_COL` reads its parameter). Action stays held, like the key, only while the hero plays the
     animation it started (`holdAction`), even if the button is released; one
     that starts nothing is one frame. The mouse never calls `FoundObjet`
     itself.
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
4. **Automatic counter-attack** (`FitdLib/assist/`): it runs only through its
   four hooks — `counterAttackNoteHit` (`GereFrappe`'s melee strike),
   `counterAttackFrame` (`PlayWorld`, after `mouseWorldFrame`),
   `counterAttackSteer` (`processTrack` case 1, after `mouseNavSteer`) and
   `counterAttackReset` (inside `mouseWorldTakeOver()`). With the option off,
   or outside AITD1, the game behaves exactly as before. It only produces what
   a player would (Actions → Fight; left, or up for a gun, + Action held) and never applies
   damage or calls `hit()`/`FoundObjet` itself. Any input of the player's own
   cancels it. New behaviour in `counterRule.*` gets a doctest first.
5. **Enemy attack pace** (`FitdLib/assist/`): it runs only through its five
   hooks — `attackPaceAllowsHit` and `attackPaceNoteHit` (`hit()`),
   `attackPaceHoldChase` (`processTrack` case 2, after `speed = 4`),
   `attackPaceAllowsSample` (`LM_SAMPLE` and `life_Sample`) and
   `attackPaceForget` (`InitObjet`). On Normal, or outside AITD1, the game
   behaves exactly as before. It only refuses or delays the start of an
   enemy's melee attack, holds its chase during the added wait, and drops the
   sample its script plays in the frame of a refused attack; it never applies
   damage, never calls `hit()` for anyone and never changes an animation. The hero is never
   paced. New behaviour in `paceRule.*` gets a doctest first.
