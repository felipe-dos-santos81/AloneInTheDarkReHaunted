///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: main-thread capture and cursor application.
///////////////////////////////////////////////////////////////////////////////

#include "mouseInput.h"

#include <atomic>
#include <cmath>

#include "bgfxGlue.h"
#include "imgui.h"
#include "configRemaster.h" // mouse trace (debug.mouseNavOverlay)
#include <cstdio>

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

// The exit cursor: an open doorway with an arrow walking into it, in a
// 32-unit design space drawn at `scale` (1 = 32 px). SDL has no such system
// cursor. Every shape gets a one-unit black outline.
SDL_Surface* drawExitCursor(int scale)
{
    const int size = 32 * scale;
    SDL_Surface* surface = SDL_CreateSurface(size, size, SDL_PIXELFORMAT_RGBA32); // zeroed: transparent
    if (!surface)
        return nullptr;
    auto inRect = [](float u, float v, float x1, float y1, float x2, float y2, float grow) {
        return u >= x1 - grow && u <= x2 + grow && v >= y1 - grow && v <= y2 + grow;
    };
    auto inDoor = [&](float u, float v, float grow) { return inRect(u, v, 14, 4, 30, 30, grow); };
    auto inDoorway = [&](float u, float v) { return inRect(u, v, 17, 7, 27, 30, 0); };
    auto inArrow = [&](float u, float v, float grow) {
        const bool shaft = inRect(u, v, 2, 16, 14, 20, grow);
        const bool head = u >= 14 - grow && u <= 22 + grow && std::fabs(v - 18) <= (22 + grow - u) * 7.0f / 8.0f + grow;
        return shaft || head;
    };
    for (int y = 0; y < size; ++y)
    {
        Uint32* row = (Uint32*)((Uint8*)surface->pixels + y * surface->pitch);
        for (int x = 0; x < size; ++x)
        {
            const float u = (x + 0.5f) / scale;
            const float v = (y + 0.5f) / scale;
            if (inArrow(u, v, 0))
                row[x] = SDL_MapSurfaceRGBA(surface, 255, 255, 255, 255);
            else if (inArrow(u, v, 1))
                row[x] = SDL_MapSurfaceRGBA(surface, 0, 0, 0, 255);
            else if (inDoorway(u, v))
                row[x] = SDL_MapSurfaceRGBA(surface, 30, 20, 12, 255);
            else if (inDoor(u, v, 0))
                row[x] = SDL_MapSurfaceRGBA(surface, 240, 240, 210, 255);
            else if (inDoor(u, v, 1))
                row[x] = SDL_MapSurfaceRGBA(surface, 0, 0, 0, 255);
        }
    }
    return surface;
}

SDL_Cursor* createExitCursor()
{
    SDL_Surface* base = drawExitCursor(1);
    if (!base)
        return nullptr;
    if (SDL_Surface* sharp = drawExitCursor(2)) // high-DPI screens
    {
        SDL_AddSurfaceAlternateImage(base, sharp); // takes its own reference
        SDL_DestroySurface(sharp);
    }
    SDL_Cursor* cursor = SDL_CreateColorCursor(base, 22, 18); // the arrow's tip, inside the doorway
    SDL_DestroySurface(base);
    return cursor;
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
        if (g_remasterConfig.debug.mouseNavOverlay) // mouse trace
        {
            int w = 0;
            int h = 0;
            if (gWindowBGFX)
                SDL_GetWindowSize(gWindowBGFX, &w, &h);
            printf("MTRACE press raw=(%.1f,%.1f) window=%dx%d logical=%s(%d,%d)\n", event.button.x, event.button.y, w, h,
                   e.pos ? "" : "none", e.pos ? e.pos->x : -1, e.pos ? e.pos->y : -1);
        }
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
            s_cursors[shape] = shape == (int)mouse::CursorShape::Exit
                                   ? createExitCursor()
                                   : SDL_CreateSystemCursor(systemCursorFor((mouse::CursorShape)shape));
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
