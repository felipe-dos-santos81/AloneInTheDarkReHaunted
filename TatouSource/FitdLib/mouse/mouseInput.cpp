///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: main-thread capture and cursor application.
///////////////////////////////////////////////////////////////////////////////

#include "mouseInput.h"

#include <atomic>

#include "bgfxGlue.h"

namespace
{
mouse::FrameQueue s_queue;

std::atomic<int> s_wantShape{ (int)mouse::CursorShape::Default };
std::atomic<bool> s_wantVisible{ true };
int s_appliedShape = -1;
int s_appliedVisible = -1;
SDL_Cursor* s_cursors[(int)mouse::CursorShape::Count] = {};

bool mapPoint(float wx, float wy, mouse::Point* out)
{
    if (!gWindowBGFX)
        return false;
    int w = 0;
    int h = 0;
    SDL_GetWindowSize(gWindowBGFX, &w, &h); // points, like event coordinates
    return mouse::windowToLogical(wx, wy, w, h, out);
}

SDL_SystemCursor systemCursorFor(mouse::CursorShape shape)
{
    switch (shape)
    {
    case mouse::CursorShape::Pointer:    return SDL_SYSTEM_CURSOR_POINTER;
    case mouse::CursorShape::Crosshair:  return SDL_SYSTEM_CURSOR_CROSSHAIR;
    case mouse::CursorShape::Move:       return SDL_SYSTEM_CURSOR_MOVE;
    case mouse::CursorShape::NotAllowed: return SDL_SYSTEM_CURSOR_NOT_ALLOWED;
    default:                             return SDL_SYSTEM_CURSOR_DEFAULT;
    }
}
}

void mouseInputOnEvent(const SDL_Event& event)
{
    mouse::Event e;
    switch (event.type)
    {
    case SDL_EVENT_MOUSE_MOTION:
        e.type = mouse::EventType::Motion;
        e.inside = mapPoint(event.motion.x, event.motion.y, &e.pos);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event.button.button != SDL_BUTTON_LEFT)
            return;
        e.type = mouse::EventType::Down;
        e.inside = mapPoint(event.button.x, event.button.y, &e.pos);
        e.clicks = event.button.clicks;
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event.button.button != SDL_BUTTON_LEFT)
            return;
        e.type = mouse::EventType::Up;
        e.inside = mapPoint(event.button.x, event.button.y, &e.pos);
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        e.type = mouse::EventType::FocusLost;
        break;
    default:
        return;
    }
    s_queue.push(e);
}

void mouseInputEndMainFrame(bool blocked)
{
    float fx = 0.0f;
    float fy = 0.0f;
    const SDL_MouseButtonFlags buttons = SDL_GetMouseState(&fx, &fy);
    mouse::Point pos;
    const bool inside = mapPoint(fx, fy, &pos);
    s_queue.publish(inside, pos, (buttons & SDL_BUTTON_LMASK) != 0, blocked);

    const int shape = s_wantShape.load();
    if (shape != s_appliedShape && shape >= 0 && shape < (int)mouse::CursorShape::Count)
    {
        if (!s_cursors[shape])
            s_cursors[shape] = SDL_CreateSystemCursor(systemCursorFor((mouse::CursorShape)shape));
        if (s_cursors[shape])
            SDL_SetCursor(s_cursors[shape]);
        s_appliedShape = shape;
    }

    const int visible = s_wantVisible.load() ? 1 : 0;
    if (visible != s_appliedVisible)
    {
        if (visible)
            SDL_ShowCursor();
        else
            SDL_HideCursor();
        s_appliedVisible = visible;
    }
}

bool mouseInputTakeFrame(mouse::Frame* out)
{
    return s_queue.take(out);
}

void mouseInputRequestCursor(mouse::CursorShape shape)
{
    s_wantShape.store((int)shape);
}

void mouseInputRequestVisible(bool visible)
{
    s_wantVisible.store(visible);
}
