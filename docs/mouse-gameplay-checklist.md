# Mouse gameplay checklist

Manual sign-off for left-button-only play (spec:
`docs/superpowers/specs/2026-09-23-mouse-gameplay-design.md`, local only).
Run every row for **Emily** and **Carnby**. Mark each cell `pass`, or `fail:`
with a one-line note. Build: `make build-fitd`; run: `make run data=DIR`.

| # | Check | Emily | Carnby |
|---|---|---|---|
| 1 | Title, armadillo, credits book and opening cutscene all skip with a left click | | |
| 2 | Character select, story page and startup menu work by mouse | | |
| 3 | Attic: hold on the floor walks and follows the pointer; release stops at once | | |
| 4 | Double-click and hold runs; a double press near the first spot keeps its destination | | |
| 5 | Holding still through a camera cut keeps walking without turning | | |
| 6 | Holding across a doorway and on stairs keeps walking | | |
| 7 | Hold on a wall or the ceiling walks that way; on the hero shows "not allowed" | | |
| 8 | Hold on the lamp: the hero walks beside it and the found screen opens; Take works | | |
| 9 | Hold on an object with a found script (not pickable): the hero steps into it and its script runs | | |
| 10 | Found screen: Leave and Take by hover + click | | |
| 11 | Wardrobe: hold pushes it; release stops; Action is never set (the hero never freezes) | | |
| 12 | Opening the inventory mid-push stops the push cleanly and it does not resume | | |
| 13 | Melee: one click on the first enemy faces and swings; the weapon is not thrown | | |
| 14 | Melee with nothing in hand: "not allowed" cursor, nothing happens | | |
| 15 | Inventory by the satchel icon: rows, actions, scroll arrows, X closes | | |
| 16 | Read a book: Prev / Close / Next buttons; Prev dimmed on page 1, Next on the last | | |
| 17 | Map by the map icon opens and a click closes it | | |
| 18 | Menu icon: save to a slot, load it back | | |
| 19 | Game over: a click continues | | |
| 20 | Complete keyboard-only pass of rows 3–19 (no mouse): unchanged from before | | |
| 21 | Gamepad pass of rows 3–19: unchanged from before | | |
| 22 | Fullscreen: double-click on empty title/menu space toggles; on a menu item it does not; in the world it does not; F11 and Alt+Enter still toggle | | |
| 23 | F1 → Controls → untick "Mouse gameplay": world clicks do nothing, no icons, double-click in the world toggles fullscreen as before | | |
| 24 | While holding a walk, drag the pointer out of the window: the cursor leaves freely; releasing outside stops the hero | | |
| 25 | Alt-Tab away mid-walk: the hero stops; nothing resumes on return until a new press | | |
| 26 | Resize the window and toggle fullscreen: clicks still land where the pointer is | | |
| 27 | Cursor shapes: arrow (walk), hand (object, icons), crosshair (enemy), four-way (pushable), not-allowed (nothing possible) | | |
| 28 | After mouse use the cursor never auto-hides; after keyboard play it hides after 2 s idle | | |
| 29 | Keyboard push probe: push the attic wardrobe with the keyboard and watch the console; if the hero's LIFE switches to the push animation by itself (anim 5 while sliding), set `kForcePushAnim = false` in `TatouSource/FitdLib/mouse/mouseWorld.cpp`; if it re-queues a walk animation other than 254 every frame, set `kPlayerLifeForwardAnim` to that value. Then re-check row 11 | | |
| 30 | From the in-game system menu, open Save/Load and Controls with a mouse click: the click that opened them does not also select an entry | | |
| 31 | Hover an enemy (crosshair), then let a cutscene start or untick "Mouse gameplay": the cursor returns to the normal arrow | | |
| 32 | While walking by mouse, a scripted turn (e.g. a fight stance) does not spin the hero twice as fast as with the keyboard | | |
| 33 | Set `debug.mouseNavOverlay = true` in aitd_remaster.cfg: green dots cover the visible floor and avoid furniture and walls, the red cross sits under the pointer, the label names what a click would do, and the console never prints "mouse: projection replica differs"; set it back to false afterwards | | |

Signed off by: ______  Date: ______  Build: `git rev-parse --short HEAD` = ______
