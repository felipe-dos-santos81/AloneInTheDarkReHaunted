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

// Floor data changed (LoadEtage): drop cached grids and plane fits.
void mouseWorldFloorChanged();

// Debug: draw the hero room's walk grid and the floor pick under the pointer
// (only when debug.mouseNavOverlay is true in aitd_remaster.cfg).
void mouseWorldDrawDebugOverlay();

// Once per PlayWorld frame, right after the input latch (localKey/localJoyD/
// localClick). Handles cutscene skips, HUD icons, gestures and the mouse walk.
void mouseWorldFrame(int allowSystemMenu);

// Keyboard or gamepad input this frame: the mouse lets go of the hero.
void mouseWorldKeyboardTookOver();

// processTrack case 1: true when the mouse steered `actor` this frame (the
// tank-control code must not run), false to let keyboard controls run as before.
struct tObject;
bool mouseNavSteer(tObject* actor);

// What the HUD should draw this frame (logical 320x200 coordinates).
struct MouseHudState
{
    bool visible = false;
    bool iconEnabled[3] = { false, false, false };
    int hoverIcon = -1;
    bool hasDestination = false;
    float destX = 0.0f;
    float destY = 0.0f;
    bool hasPreview = false;
    float previewX = 0.0f;
    float previewY = 0.0f;
    bool hasPointer = false;
    int pointerX = 0;
    int pointerY = 0;
    bool held = false;
    bool settling = false;
};

bool mouseWorldHudState(MouseHudState* out);

// True while the mouse drives gameplay and was the last input: the idle
// auto-hide must not hide the cursor.
bool mouseWorldWantsCursor();
