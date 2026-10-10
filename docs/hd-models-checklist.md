# HD character models checklist

Manual sign-off for the HD actor slice in Alone in the Dark 1 (spec:
`docs/superpowers/specs/2026-10-01-hd-actor-slice-design.md`, local only).
Build with `make build-fitd`; run with `make run data=DIR`.
Mark each cell `pass`, or `fail:` with a one-line note.

## Capture points

Every before/after comparison loads these saves, so camera, position, pose
and settings match. The game keeps nine save slots as `SAVE<n>.ITD` in its
home folder (the menu's first slot is 1). Carnby's saves (C1–C9) and Emily's
(C10) need separate sets:

1. On the baseline build (stage S0), make C1–C9 in the slots below.
2. Copy the `SAVE*.ITD` files to `data/hd-models-baseline/saves/carnby/`.
3. Start a new game as Emily, save C10 in slot 1, and copy it to
   `data/hd-models-baseline/saves/emily/`.
4. Before capturing, copy the set you need back into the home folder.

Capture with default options, the default window size, HD backgrounds on and
mouse gameplay off. Unless the row says otherwise, take the screenshot 2 s
after loading the save, without touching the controls (the hero idles).

| # | Slot | Where and what | Hero body | Notes |
|---|---|---|---|---|
| C1 | 1 | Attic, start: Carnby idle, lamp unlit, whole body in frame | | |
| C2 | 2 | Attic: Carnby walking (hold Up for 1 s after loading; capture while walking) | | |
| C3 | 3 | Attic: Carnby turning on the spot (hold Left; capture mid-turn) | | |
| C4 | 4 | Attic: the window creature in frame with Carnby, just before it attacks | | |
| C5 | 5 | Attic: Carnby punching the creature (Fight, Action); capture mid-swing | | |
| C6 | 6 | Attic: the creature hits Carnby; capture during the hurt animation | | |
| C7 | 7 | Camera cut: Carnby one step before a camera boundary; walk across and capture the first frame after the cut | | |
| C8 | 8 | Foreground mask: Carnby partly behind scenery the engine masks over him | | |
| C9 | 9 | Dark Bedroom (walkthrough R13): lamp lit and in hand, room dark | | |
| C10 | 1 (Emily's set) | C1 with Emily instead of Carnby | | |

Fill "Hero body" with the hero's `bodyNum` from the save (with the lamp:
Carnby `LISTBODY_011`, Emily `LISTBOD2_011`). For C4–C6 also note the
creature's body (`LISTBODY_024` or one of its aliases).

**Screenshots**: run
`screencapture -l$(osascript -e 'tell app "System Events" to id of window 1 of process "Tatou"') FILE.png`,
or press Cmd+Shift+4, then Space, and click the game window. Save them as
`data/hd-models-baseline/<stage>/C<n>.png` (git-ignored with `data/`).

**Frame time and memory** (scene C9, then C4):

1. Run with the Metal HUD: `MTL_HUD_ENABLED=1 make run data=DIR`.
2. Load the save and wait 10 s.
3. Note the HUD's average frame time (ms) over the next 30 s.
4. In another terminal, `footprint $(pgrep -x Tatou) | tail -1` gives the
   process footprint.

| Stage | C9 frame time (ms) | C4 frame time (ms) | Footprint (MB) | Build (commit) |
|---|---|---|---|---|
| S0 baseline | | | | |
| S3 option off | | | | |
| S3 option on, identity meshes | | | | |
| S5 option on, generated Carnby | | | | |

## Compare mode (the S3 identity oracle)

The game checks by itself that HD models draw where the classic bodies do;
no screen capture is needed.

The game reads `models_hd/` from the folder it starts in (where the saves
are), else from next to the executable. Every build copies `Assets/models_hd`
next to the executable (into `Tatou.app/Contents/Resources/` on macOS).
`make models-install models_hd=DIR` mirrors `DIR/*.hdm` into the game's
folder: the app's Resources on macOS, else `data=DIR`. A models_hd/ there
wins over the build's copy.

1. Build identity models for every body and install them:

   ```bash
   make identity-models bodies=<every canonical key>
   make import-models models_ai=data/models-identity models_hd=data/models-hd-identity
   make models-install models_hd=data/models-hd-identity
   ```

   The game's `models_hd/` now holds only the identity models. A plain
   `make models-install` puts the real ones back; on macOS so does the next
   build. So start the game directly, not with `make run`.
2. In `aitd_remaster.cfg` (same folder) set `graphics.hdModels = true`,
   `debug.hdModelsCompare = true` and `debug.loadSaveOnStart = <n>` (the
   save to open; this skips the intro, language menu and startup options).
3. Run the game for 30 s, then quit. With the game clock held still, it
   writes `hdcompare_lit.png`, `hdcompare_unlit.png`,
   `hdcompare_hidden.png`, `hdcompare_classic.png` and `hdcompare.txt` next
   to the saves.
4. Run `python3 tools/hd_compare.py <that folder>`. Every replaced body must
   print `ok` (silhouette IoU at least 0.85, set with `--limit`).
   `brightness` is the lit HD body over the classic one, for rows 9–10.
5. Set `debug.loadSaveOnStart = -1` and `debug.hdModelsCompare = false`
   again.

Measured results:

- Prototype, identity models, saves 0, 2 and 6: IoU 0.91–0.97, brightness
  0.97–1.10.
- Outward-facing Blender models (2026-10-09), saves 0–6, lit rooms: IoU
  0.89–0.97, brightness 0.94–1.17.
- Smooth shading and softened facets (2026-10-09), saves 0–6, lit rooms:
  IoU 0.89–0.97, brightness 0.95–1.18 (the same saves with the previous
  models: 0.93–1.17).

## Rows

Stages S3–S6 fill these in. "Classic" means the option off; "HD" means on.

| # | Check | S3 identity | S5 Carnby | S6 Emily | S6 creature |
|---|---|---|---|---|---|
| 1 | Option off: C1–C10 match the S0 captures (dust and lamp flicker aside) | | | | |
| 2 | HD: the body stands exactly where the classic one does (C1) | | | | |
| 3 | HD: walk, turn, attack and hurt animations keep the classic timing (C2, C3, C5, C6) | | | | |
| 4 | HD: the classic body never flashes in once the HD one is ready | | | | |
| 5 | HD: after a camera cut the body is correct on the first frame (C7) | | | | |
| 6 | HD: foreground scenery covers the body exactly where it covers the classic one, including parts outside the classic outline (C8) | | | | |
| 7 | HD: several actors overlap in the right order (C4) | | | | |
| 8 | HD: lamp glow and flare sit on the HD lamp's flame (C9) | | | | |
| 9 | HD: in a dark room the body is as dark as the classic one, and the lantern lights both alike (C9; light strengths were tuned on inside-out bodies, so re-check with the outward ones) | | | | |
| 10 | HD: fades to and from black darken the body with the room | | | | |
| 11 | HD: screen shake moves the body with the background | | | | |
| 12 | HD: close-up cameras show no large near polygons that the classic body hides | | | | |
| 13 | HD: blob, planar and wall shadows fall where they did | | | | |
| 14 | HD: with mouse gameplay on, clicking near the hero picks exactly what it picks with the option off | | | | |
| 15 | HD: the inventory and the Tatou intro show the classic bodies | | | | |
| 16 | A missing, truncated or wrong-skeleton `.hdm` draws classic and logs one line | | | | |
| 17 | Keyboard and gamepad play is unchanged | | | | |
| 18 | HD: the seated ghost (room 10, `LISTBODY_141`) shows the room through it as the classic one does, with no holes or doubled layers | | | | |
| 19 | HD: the flying insect's wings (`LISTBODY_238`/`243`) are see-through; its body is solid | | | | |
| 20 | HD: Carnby's and Emily's lamp glass (`LISTBODY_011`, `LISTBOD2_011`) is see-through in C1, C9 and C10 | | | | |
| 21 | HD: bodies shade as rounded forms and their cloth shows no crumpled facets, in a lit room and in the dark room with the lantern (C1, C9, C10); faces and buttons stay crisp; no brightness steps along texture seams; shading steps at the joints no worse than before | | | | |
| 22 | HD: walking, turning and fighting, limbs keep their speed through each pose (no stop-start); planted feet slide no more than with the option off | | | | |
| 23 | HD: elbows, knees and shoulders bend as one rounded surface, without cracks or a shading step at the joint | | | | |
| 24 | With `animation.poseSmoothing` off, HD bodies move exactly like the classic ones; classic bodies and hits are the same with it on or off | | | | |
| 25 | HD: the animations that zoom a bone (`LISTANIM` 193–195, 239, 241; `LISTANI2` 68, 193–195, 239, 241) look as they did before dual-quaternion skinning | | | | |
