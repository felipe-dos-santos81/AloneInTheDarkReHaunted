///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: main-thread capture and cursor application.
///////////////////////////////////////////////////////////////////////////////

#include "mouseInput.h"

#include <atomic>

#include "bgfxGlue.h"
#include "imgui.h"

namespace
{
mouse::FrameQueue s_queue;

std::atomic<int> s_wantShape{ (int)mouse::CursorShape::Default };
std::atomic<bool> s_wantVisible{ true };
int s_appliedShape = -1;
int s_appliedVisible = -1;
SDL_Cursor* s_cursors[(int)mouse::CursorShape::Count] = {};

std::optional<mouse::Point> mapPoint(float wx, float wy)
{
    if (!gWindowBGFX)
        return std::nullopt;
    int w = 0;
    int h = 0;
    SDL_GetWindowSize(gWindowBGFX, &w, &h); // points, like event coordinates
    return mouse::windowToLogical(wx, wy, w, h);
}

SDL_SystemCursor systemCursorFor(mouse::CursorShape shape)
{
    switch (shape)
    {
    case mouse::CursorShape::Pointer:    return SDL_SYSTEM_CURSOR_POINTER;
    case mouse::CursorShape::Crosshair:  return SDL_SYSTEM_CURSOR_CROSSHAIR;
    case mouse::CursorShape::Move:       return SDL_SYSTEM_CURSOR_MOVE;
    case mouse::CursorShape::NotAllowed: return SDL_SYSTEM_CURSOR_NOT_ALLOWED;
    case mouse::CursorShape::Text:       return SDL_SYSTEM_CURSOR_TEXT;
    case mouse::CursorShape::ResizeNS:   return SDL_SYSTEM_CURSOR_NS_RESIZE;
    case mouse::CursorShape::ResizeEW:   return SDL_SYSTEM_CURSOR_EW_RESIZE;
    case mouse::CursorShape::ResizeNESW: return SDL_SYSTEM_CURSOR_NESW_RESIZE;
    case mouse::CursorShape::ResizeNWSE: return SDL_SYSTEM_CURSOR_NWSE_RESIZE;
    default:                             return SDL_SYSTEM_CURSOR_DEFAULT;
    }
}

// The shape ImGui asked for in its last frame. ImGui's own backend may not set
// cursors (ImGuiConfigFlags_NoMouseCursorChange, bgfxGlue.cpp): it would do so
// from the game thread. This runs between the game thread's frames, so the
// value is the finished frame's.
mouse::CursorShape imguiShape()
{
    switch (ImGui::GetMouseCursor())
    {
    case ImGuiMouseCursor_TextInput:  return mouse::CursorShape::Text;
    case ImGuiMouseCursor_ResizeAll:  return mouse::CursorShape::Move;
    case ImGuiMouseCursor_ResizeNS:   return mouse::CursorShape::ResizeNS;
    case ImGuiMouseCursor_ResizeEW:   return mouse::CursorShape::ResizeEW;
    case ImGuiMouseCursor_ResizeNESW: return mouse::CursorShape::ResizeNESW;
    case ImGuiMouseCursor_ResizeNWSE: return mouse::CursorShape::ResizeNWSE;
    case ImGuiMouseCursor_Hand:       return mouse::CursorShape::Pointer;
    case ImGuiMouseCursor_NotAllowed: return mouse::CursorShape::NotAllowed;
    default:                          return mouse::CursorShape::Default;
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
        e.pos = mapPoint(event.motion.x, event.motion.y);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event.button.button != SDL_BUTTON_LEFT)
            return;
        e.type = mouse::EventType::Down;
        e.pos = mapPoint(event.button.x, event.button.y);
        e.clicks = event.button.clicks;
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event.button.button != SDL_BUTTON_LEFT)
            return;
        e.type = mouse::EventType::Up;
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
    s_queue.publish(mapPoint(fx, fy), (buttons & SDL_BUTTON_LMASK) != 0, blocked);

    // While the F1 dialog or another ImGui window wants the mouse, it owns the shape.
    const int shape = blocked ? (int)imguiShape() : s_wantShape.load();
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
