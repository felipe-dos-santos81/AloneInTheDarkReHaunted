# Mouse gameplay checklist

Manual sign-off for left-button-only play (spec:
`docs/superpowers/specs/2026-09-23-mouse-gameplay-design.md`, local only).
Build with `make build-fitd`, run with `make run data=DIR`. Run every row as
**Emily** and as **Carnby**. Mark each cell `pass`, or `fail:` with a one-line
note. Each check reads "what you do → what you should see".

| # | Check | Emily | Carnby |
|---|---|---|---|
| 1 | Left-click the title, the armadillo, the credits book and the opening cutscene → each one skips | | |
| 2 | Startup menu by mouse → it works. Click a portrait → that character is picked at once. On the story's last page click ▶ → the game starts; click ✕ → character select returns with both portraits drawn (HD and SD) | | |
| 3 | Attic: hold on the floor → the hero walks and follows the pointer. Release → he stops at once | | |
| 4 | Double-click and hold → the hero runs. A second press near the first spot → he keeps that destination | | |
| 5 | Hold still through a camera cut → the hero keeps walking and does not turn | | |
| 6 | Hold across a doorway and on stairs → the hero keeps walking | | |
| 7 | Hold on a wall or the ceiling → the hero walks that way. Hold on the hero → "not allowed" cursor | | |
| 8 | Hold on the lamp → the hero walks into it and the found screen opens on the touch, as by keyboard. Take works | | |
| 9 | Hold on an object with a found script that cannot be picked up → the hero walks into it and its script runs | | |
| 10 | Found screen: hover and click Leave, then Take → both work | | |
| 11 | Hold on the wardrobe → the hero pushes it. Release → the push stops. Action is never set: the hero never freezes | | |
| 12 | Open the inventory mid-push → the push stops cleanly and does not resume | | |
| 13 | Melee: click the first enemy once → the hero faces it and swings; the weapon is not thrown | | |
| 14 | Melee with nothing in hand → "not allowed" cursor; nothing happens | | |
| 15 | Open the inventory with the satchel icon → rows, actions and scroll arrows work; X closes it | | |
| 16 | Read a book → Prev / Close / Next work; Prev is dimmed on page 1, Next on the last page | | |
| 17 | Click the map icon → the map opens; a click closes it | | |
| 18 | Menu icon: save to a slot, then load it → the game returns to that point | | |
| 19 | Game over: click → the game continues | | |
| 20 | Rows 3–19 by keyboard only (no mouse) → unchanged from before | | |
| 21 | Rows 3–19 by gamepad → unchanged from before | | |
| 22 | Double-click empty title or menu space → fullscreen toggles. Double-click a menu item, the world, or a double-click that skips an intro, picture or sequence → no toggle. F11 and Alt+Enter → still toggle | | |
| 23 | F1 → Controls → untick "Mouse gameplay" → world clicks do nothing, no icons, and a double-click in the world toggles fullscreen as before | | |
| 24 | Hold a walk and drag the pointer out of the window → the cursor leaves freely. Release outside → the hero stops | | |
| 25 | Alt-Tab away mid-walk → the hero stops. Come back → nothing resumes until a new press | | |
| 26 | Resize the window, then toggle fullscreen → clicks still land under the pointer | | |
| 27 | Hover each target → cursor shapes: arrow (walk), hand (object, icons), crosshair (enemy), four-way arrow (pushable), doorway with an arrow (floor change, see row 45), not-allowed (nothing possible) | | |
| 28 | After mouse use → the cursor never auto-hides. After keyboard play → it hides after 2 s idle | | |
| 29 | Keyboard push probe: push the attic wardrobe by keyboard and watch the hero's `ANIM` / `newAnim` (no console line prints them; the debugger's "Active objects" window, ` key, exists only in an MSVC Debug build). If the hero's LIFE switches to the push animation (anim 5) by itself while sliding → set `kForcePushAnim = false` in `TatouSource/FitdLib/mouse/mouseWorldPush.cpp`. If it re-queues a walk animation other than 254 every frame → set `kPlayerLifeForwardAnim` to that value. Then re-check row 11 | | |
| 30 | In-game system menu: click Save/Load, then Controls → the click that opens each does not also select an entry | | |
| 31 | Hover an enemy (crosshair), then let a cutscene start or untick "Mouse gameplay" → the cursor returns to the arrow | | |
| 32 | Walk by mouse into a scripted turn (e.g. a fight stance) → the hero turns at keyboard speed, not twice as fast | | |
| 33 | Set `debug.mouseNavOverlay = true` in `aitd_remaster.cfg` → green dots cover the visible floor and avoid furniture and walls; a red cross sits under the pointer; the label names what a click would do; the console never prints "mouse: projection replica differs". Set it back to `false` afterwards | | |
| 34 | Hold on an animated prop or a door (not an enemy) → no attack starts, and the spot behind it is reachable whenever the keyboard can reach it | | |
| 35 | Hold on large furniture that has a found script but cannot be picked up → the hero reaches it and its script runs within about 1 s, with no 6 s stall | | |
| 36 | Hold a walk and trigger a picture or message screen (e.g. walk into a note) → after closing it the old walk does not resume until a new press | | |
| 37 | Play by keyboard, idle 2 s (the cursor hides), then walk into an item → the found screen shows the cursor, and Take/Leave work by mouse | | |
| 38 | Click the hero's own body → "not allowed" cursor; the hero does not move | | |
| 39 | Hold on an object whose nearest side is off screen or blocked by furniture → the hero walks to a side he can reach, preferring a visible one | | |
| 40 | Hold on an item the hero cannot touch (e.g. across a table) → he gives up without taking it, as with the keyboard | | |
| 41 | While a script walks the hero with the HUD shown → "not allowed" cursor over the world. Keep the button held until control returns → no walk starts until a new press | | |
| 42 | Open the F1 dialog → the arrow cursor over it, and ImGui's own shapes (text caret, resize arrows) where it uses them | | |
| 43 | Reopen the found screen, Controls, the save picker and the startup menus with the pointer resting on an entry → the selection stays put until the pointer moves | | |
| 44 | Click floor spots in the corners of L-shaped rooms → the hero walks to the spot clicked, not just towards it | | |
| 45 | Attic, far camera on the arched alcove: hover the pillar right of the arch → the doorway-with-arrow cursor (sharp on a Retina screen). Hold there → the hero walks into the alcove, behind the pillar, and the floor changes (the stairwell, the only way out, is hidden there) | | |
| 46 | Attic: press the hero against the alcove's left wall, then hold on the main-room floor → he walks out round the wall instead of grinding into it | | |
| 47 | Attic trunk (world object 0, life 0; also after it was pushed). With Push armed (the default), hold on it → the hero pushes it. Arm Open/Search in the inventory, then hold on it from its west (-x) side, the side its life script accepts (POSREL 2) → hand cursor; the hero walks up and, on the touch, plays the open gesture to the end even if you release; the trunk opens | | |
| 48 | Attic piano and bookshelf (furniture painted into the background). With Open/Search armed, hold on one → hand cursor; the hero walks beside it, leans in and plays the search gesture to the end; the game answers as with the keyboard (an item found, or its "nothing here" message). With Push armed, click them → the hero walks to the floor as before | | |
| 49 | Bedroom (floor 1, room 4): Use the dresser key from the inventory, then click the dresser with the teddy bear → the hero walks to it, touches it and plays the use gesture; the drawer opens and offers the two small mirrors. With a weapon in hand, the same click → a push, as before | | |
| 50 | Floor 2 stairwell (room 2, top of the stairs): hold at the screen's left edge until the hero walks under the pointer → he keeps walking with the pointer on him; a small drift does not stop him | | |
| 51 | Floor 2: from room 3, hold on the doorway back into room 4 through the camera cut, pointer nearly still → the hero keeps heading for the door, not for what now shows under the pointer | | |
| 52 | Floor 2 landing (room 2, camera 28), with `debug.mouseNavOverlay` on: look at the strip at the screen's left edge, below the hero, leading to the door to room 4 → it shows walk-grid dots. Press or hold there → the hero walks to the pointer, not straight left. Attic: look at the narrow gaps between neighbouring cameras' zones → they show dots too, and walking across them behaves as on the rest of the floor | | |

Signed off by: ______  Date: ______  Build: `git rev-parse --short HEAD` = ______
