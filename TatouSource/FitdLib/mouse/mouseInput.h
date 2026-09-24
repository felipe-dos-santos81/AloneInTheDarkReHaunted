///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: SDL capture on the main thread, handed to the game thread
// once per frame; cursor shape/visibility applied on the main thread.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <SDL.h>

#include "mouseTypes.h"

// Main thread, from readKeyboard()'s event loop.
void mouseInputOnEvent(const SDL_Event& event);
// Main thread, once per readKeyboard() after the event loop: publishes the
// frame and applies the latest cursor requests. `blocked`: the F1 dialog or
// another ImGui window wants the mouse, and then also owns the cursor shape.
void mouseInputEndMainFrame(bool blocked);

// Game thread: this frame's mouse events (false when none were published).
bool mouseInputTakeFrame(mouse::Frame* out);
// Game thread: desired OS cursor shape / visibility (applied next main frame).
void mouseInputRequestCursor(mouse::CursorShape shape);
void mouseInputRequestVisible(bool visible);
