# HD character models checklist

Manual sign-off for the HD actor slice (spec:
`docs/superpowers/specs/2026-10-01-hd-actor-slice-design.md`, local only).
Alone in the Dark 1. Build: `make build-fitd`; run: `make run data=DIR`.
Mark each cell `pass`, or `fail:` with a one-line note.

## Capture points

Every before/after comparison uses these saves, so camera, position, pose
and settings match. Make each save once, on the baseline build (stage S0),
in the slot given (the menu's first slot is 1). The game keeps saves as
`SAVE<n>.ITD` in its home folder and has nine slots, so Carnby's set (C1–C9)
and Emily's (C10) are kept apart: after saving C1–C9, copy the `SAVE*.ITD`
files to `data/hd-models-baseline/saves/carnby/`; start a new game as Emily,
save C10 in slot 1 and copy it to `data/hd-models-baseline/saves/emily/`.
Copy a set back into the home folder before capturing with it.

Settings for every capture: default options, the default window size, HD
backgrounds on, mouse gameplay off. Take each screenshot 2 s after loading
the save, without touching the controls (the hero idles), unless the row
says otherwise.

| # | Slot | Where and what | Hero body | Notes |
|---|---|---|---|---|
| C1 | 1 | Attic, start: Carnby idle, lamp unlit, whole body in frame | | |
| C2 | 2 | Attic: Carnby walking across the room (hold Up for 1 s after loading, capture while walking) | | |
| C3 | 3 | Attic: Carnby turning on the spot (hold Left, capture mid-turn) | | |
| C4 | 4 | Attic: the window creature in frame with Carnby (several actors), just before it attacks | | |
| C5 | 5 | Attic: Carnby punching the creature (Fight, Action) — capture mid-swing | | |
| C6 | 6 | Attic: Carnby hit by the creature — capture during the hurt animation | | |
| C7 | 7 | A camera cut: Carnby a step before a camera boundary; walk across, capture the first frame after the cut | | |
| C8 | 8 | A foreground mask: Carnby partly behind foreground scenery that the engine masks over him | | |
| C9 | 9 | Dark Bedroom (walkthrough R13): lamp lit and in hand, room dark | | |
| C10 | 1 (Emily's set) | C1 with Emily instead of Carnby | | |

Fill "Hero body" from the save (the hero's `bodyNum`; Carnby with the lamp is
`LISTBODY_011`, Emily with the lamp `LISTBOD2_011`); C4–C6 also note the
creature's body (`LISTBODY_024` or one of its aliases).

**Screenshots**: `screencapture -l$(osascript -e 'tell app "System Events" to id of window 1 of process "Tatou"') FILE.png`,
or Cmd+Shift+4 then Space and click the game window. Save them as
`data/hd-models-baseline/<stage>/C<n>.png` (git-ignored with `data/`).

**Frame time and memory** (scene C9, then C4): run with the Metal HUD,
`MTL_HUD_ENABLED=1 make run data=DIR`, load the save, wait 10 s, and note
the HUD's average frame time (ms) over the next 30 s. In another terminal,
`footprint $(pgrep -x Tatou) | tail -1` gives the process footprint.

| Stage | C9 frame time (ms) | C4 frame time (ms) | Footprint (MB) | Build (commit) |
|---|---|---|---|---|
| S0 baseline | | | | |
| S3 option off | | | | |
| S3 option on, identity meshes | | | | |
| S5 option on, generated Carnby | | | | |

## Compare mode (the S3 identity oracle)

The game itself checks that HD models draw where the classic bodies do, with
no screen capture needed:

1. Import every body as its own model and put the files where the game
   looks (`models_hd/` next to the saves; on macOS that is
   `TatouSource/build/macos-arm64/Fitd/Tatou.app/Contents/Resources/`):
   `make identity-models bodies=<every canonical key>` then
   `make import-models models_ai=data/models-identity models_hd=data/models-hd-identity`
   and copy `data/models-hd-identity/*.hdm` into `models_hd/`.
2. In `aitd_remaster.cfg` (same folder) set `graphics.hdModels = true`,
   `debug.hdModelsCompare = true` and `debug.loadSaveOnStart = <n>` (the
   save to open: the intro, language menu and startup options are skipped).
3. Run the game for 30 s, then quit. It writes `hdcompare_lit.png`,
   `hdcompare_unlit.png`, `hdcompare_hidden.png`, `hdcompare_classic.png`
   and `hdcompare.txt` next to the saves, the game clock held still while
   it captures.
4. `python3 tools/hd_compare.py <that folder>`: every replaced body must
   print `ok` (silhouette IoU at least 0.85 by default); `brightness` is
   the lit HD body over the classic one, for rows 9-10.

Set `debug.loadSaveOnStart = -1` and `debug.hdModelsCompare = false` again
afterwards. Measured on the prototype (identity models, saves 0, 2 and 6):
IoU 0.91-0.97, brightness 0.97-1.10.

## Rows

Stages S3–S6 fill these in. "Classic" means the option off; "HD" on.

| # | Check | S3 identity | S5 Carnby | S6 Emily | S6 creature |
|---|---|---|---|---|---|
| 1 | Option off: C1–C10 match the S0 captures (dust and lamp flicker aside) | | | | |
| 2 | HD: the body stands exactly where the classic one does (C1) | | | | |
| 3 | HD: walk, turn, attack and hurt animations follow the classic timing (C2, C3, C5, C6) | | | | |
| 4 | HD: no frame of the classic body flashes in when the HD one is ready | | | | |
| 5 | HD: camera cut shows the body correctly on the first frame (C7) | | | | |
| 6 | HD: foreground scenery covers the body exactly where it covers the classic one, including any part outside the classic outline (C8) | | | | |
| 7 | HD: several actors overlap in the right order (C4) | | | | |
| 8 | HD: lamp glow and flare sit on the HD lamp's flame (C9) | | | | |
| 9 | HD: in a dark room the body is as dark as the classic one, apart from the lamp's light (C9) | | | | |
| 10 | HD: fades to and from black darken the body with the room | | | | |
| 11 | HD: screen shake moves the body with the background | | | | |
| 12 | HD: close-up cameras show no large near polygons the classic body hides | | | | |
| 13 | HD: blob, planar and wall shadows still fall where they did | | | | |
| 14 | HD: clicking near the hero with mouse gameplay on picks exactly what it picks with the option off | | | | |
| 15 | HD: inventory and the Tatou intro show the classic bodies | | | | |
| 16 | A missing, truncated or wrong-skeleton `.hdm` draws classic and logs one line | | | | |
| 17 | Keyboard and gamepad play is unchanged | | | | |
