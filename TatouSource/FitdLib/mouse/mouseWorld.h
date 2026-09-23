///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: the engine adapter. The only mouse file that touches engine
// globals. See AGENTS.md "Mouse rules".
///////////////////////////////////////////////////////////////////////////////
#pragma once

// Call first thing when any screen opens from gameplay (menus, map, inventory,
// found object, book) and when PlayWorld exits: cancels the walk, clears
// attack/push, resets gestures and arms the screen click gate.
void mouseWorldTakeOver();

// True while mouse gameplay drives the world this frame (option on, in
// PlayWorld, not in a cutscene, no screen open).
bool mouseWorldIsActive();

// Gate for screen clicks (menuMouseClicked): false until the button has been
// released after a takeover.
bool mouseScreenClickFilter(bool clickedThisFrame, bool downNow);
