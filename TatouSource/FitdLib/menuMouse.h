///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Shared mouse-input helpers for all game menus.
//
// Pattern:
//   static ImVec2 s_lastMouse = {-1.f, -1.f};
//   bool moved = menuMouseMoved(s_lastMouse, localKey || localJoyD);
//   if (moved) { int item = menuMouseHitList(...); if (item >= 0) sel = item; }
//   if (menuMouseClicked() && menuMouseHitList(...) >= 0) { /* confirm */ }
///////////////////////////////////////////////////////////////////////////////
#pragma once
#include "imguiBGFX.h"
#include <SDL.h>

#include "mouse/mouseWorld.h"

// SDL_GetTicks() of the last mouse click that activated a menu item. The
// fullscreen double-click (input.cpp) is refused shortly after one.
extern Uint32 g_menuItemClickMs;

// Call wherever a mouse click activates a menu item.
inline void menuNoteItemClick()
{
    g_menuItemClickMs = (Uint32)SDL_GetTicks();
}

// ---------------------------------------------------------------------------
// Gameplay cursor auto-hide
// Call menuUpdateGameplayCursor() once per gameplay frame (not in menus).
// Call menuRestoreCursorForMenu() when entering any menu so the cursor is
// always visible inside menus and the inactivity timer resets on exit.
// ---------------------------------------------------------------------------
static const Uint32 CURSOR_HIDE_DELAY_MS = 2000; // hide after 2 s of no movement

// Shared state for cursor auto-hide (file-scope so helpers can access it)
struct MenuCursorState
{
    ImVec2 lastPos   = { -9999.0f, -9999.0f };
    Uint32 lastMoveT = SDL_GetTicks(); // initialise so cursor doesn't hide before first move
    bool   hidden    = false;
};

inline MenuCursorState& menuCursorState()
{
    static MenuCursorState s;
    return s;
}

// Call once per gameplay frame. Hides the cursor after inactivity,
// restores it immediately when the mouse moves.
inline void menuUpdateGameplayCursor()
{
    MenuCursorState& st = menuCursorState();
    // Use integer SDL coords to avoid ImGui float jitter that would prevent hiding
    float fx, fy;
    SDL_GetMouseState(&fx, &fy);
    ImVec2 cur = { fx, fy };
    bool moved = ((int)cur.x != (int)st.lastPos.x || (int)cur.y != (int)st.lastPos.y);

    if (moved)
    {
        st.lastPos   = cur;
        st.lastMoveT = SDL_GetTicks();
        if (st.hidden)
        {
            SDL_ShowCursor();
            st.hidden = false;
        }
    }
    else if (!st.hidden &&
             (SDL_GetTicks() - st.lastMoveT) >= CURSOR_HIDE_DELAY_MS)
    {
        SDL_HideCursor();
        st.hidden = true;
    }
}

// Call when entering any menu: ensures cursor is visible and resets the
// inactivity timer so it won't immediately re-hide after returning to gameplay.
inline void menuRestoreCursorForMenu()
{
    MenuCursorState& st = menuCursorState();
    if (st.hidden)
    {
        SDL_ShowCursor();
        st.hidden = false;
    }
    // Reset timer so cursor stays visible for the full delay after menu exit
    st.lastMoveT = SDL_GetTicks();
    st.lastPos   = ImGui::GetIO().MousePos;
}


// Convert display-space mouse position to the 320x200 game coordinate space.
// Returns {-1,-1} when the display size is unavailable.
inline ImVec2 menuGetGameMouse()
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f)
        return { -1.0f, -1.0f };
    return { io.MousePos.x * 320.0f / io.DisplaySize.x,
             io.MousePos.y * 200.0f / io.DisplaySize.y };
}

// Returns true when the mouse has physically moved since the last call AND
// no keyboard/gamepad input was active this frame.
// lastPos must be a persistent ImVec2 (static local) initialised to {-1,-1}.
// hasInput should be (localKey || localJoyD) — any non-zero value suppresses hover.
static constexpr float MOUSE_MOVE_THRESHOLD = 3.0f; // pixels before hover updates

inline bool menuMouseMoved(ImVec2& lastPos, bool hasInput)
{
    ImVec2 cur = ImGui::GetIO().MousePos;

    // First call after menu open (sentinel): seed lastPos without firing hover.
    if (lastPos.x < 0.0f && lastPos.y < 0.0f)
    {
        lastPos = cur;
        return false;
    }

    float dx = cur.x - lastPos.x;
    float dy = cur.y - lastPos.y;
    bool moved = !hasInput && (dx * dx + dy * dy) >= (MOUSE_MOVE_THRESHOLD * MOUSE_MOVE_THRESHOLD);
    if (moved)
        lastPos = cur;
    if (hasInput) moved = false;
    return moved;
}

// Returns true when the left mouse button was just pressed this frame, except
// right after a screen opened: the press that opened it is swallowed until the
// button has been released (mouseWorldTakeOver arms the gate).
inline bool menuMouseClicked()
{
    ImGuiIO& io = ImGui::GetIO();
    return mouseScreenClickFilter(io.MouseClicked[0], io.MouseDown[0]);
}

// Hit-test a vertical list of equal-height items.
// gameX/gameY: mouse position in game space (from menuGetGameMouse).
// x1/x2: horizontal bounds of the list in game space.
// startY: top Y of the first item.
// itemH: pixel height of each item (typically SIZE_FONT = 16).
// count: number of items.
// Returns 0-based item index, or -1 if the cursor is outside the list.
inline int menuMouseHitList(float gameX, float gameY,
                            int x1, int x2,
                            int startY, int itemH, int count)
{
    if (gameX < (float)x1 || gameX > (float)x2) return -1;
    for (int i = 0; i < count; i++)
    {
        if (gameY >= (float)(startY + i * itemH) &&
            gameY <  (float)(startY + i * itemH + itemH))
            return i;
    }
    return -1;
}

// Hit-test two horizontally placed buttons (used by FoundObjet / PickupObject).
// Returns 0 for the left button, 1 for the right button, -1 for miss.
// leftCx / rightCx: centre X of each button in game space.
// cy: centre Y.  halfW/halfH: half-extents of each button hit area.
inline int menuMouseHitTwoButtons(float gameX, float gameY,
                                  int leftCx, int rightCx,
                                  int cy, int halfW, int halfH)
{
    auto hit = [&](int cx) {
        return gameX >= (float)(cx - halfW) && gameX <= (float)(cx + halfW) &&
               gameY >= (float)(cy - halfH) && gameY <= (float)(cy + halfH);
    };
    if (hit(leftCx))  return 0;
    if (hit(rightCx)) return 1;
    return -1;
}

// Hit-test an inclusive rectangle in game space.
inline bool menuMouseHitRect(float gameX, float gameY, int x1, int y1, int x2, int y2)
{
    return gameX >= (float)x1 && gameX <= (float)x2 &&
           gameY >= (float)y1 && gameY <= (float)y2;
}

// 320x200 game coordinates -> ImGui foreground draw-list coordinates
// (window pixels), the same mapping fontTTF.cpp uses.
inline ImVec2 menuGameToScreen(float gx, float gy)
{
    extern int outputResolution[2];
    return ImVec2(gx * (float)outputResolution[0] / 320.0f,
                  gy * (float)outputResolution[1] / 200.0f);
}

// Shared frame for the small drawn buttons below. Returns the line colour.
inline ImU32 menuDrawButtonFrame(ImDrawList* dl, ImVec2 a, ImVec2 b, bool hover, bool enabled, float thickness)
{
    const ImU32 fill = IM_COL32(16, 16, 16, 200);
    const ImU32 line = !enabled ? IM_COL32(110, 110, 100, 170)
                     : hover    ? IM_COL32(255, 230, 150, 255)
                                : IM_COL32(210, 210, 190, 255);
    dl->AddRectFilled(a, b, fill, 2.0f);
    dl->AddRect(a, b, line, 2.0f, 0, thickness);
    return line;
}

// A small "X" button over the inclusive 320x200 rect, drawn at window resolution.
inline void menuDrawCloseButton(int x1, int y1, int x2, int y2, bool hover)
{
    if (!g_imguiFrameActive)
        return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 a = menuGameToScreen((float)x1, (float)y1);
    ImVec2 b = menuGameToScreen((float)(x2 + 1), (float)(y2 + 1));
    float t = (b.y - a.y) * 0.12f;
    if (t < 1.0f) t = 1.0f;
    ImU32 line = menuDrawButtonFrame(dl, a, b, hover, true, t);
    float px = (b.x - a.x) * 0.30f;
    float py = (b.y - a.y) * 0.25f;
    dl->AddLine(ImVec2(a.x + px, a.y + py), ImVec2(b.x - px, b.y - py), line, t);
    dl->AddLine(ImVec2(b.x - px, a.y + py), ImVec2(a.x + px, b.y - py), line, t);
}

// A small triangle button (dir -1 points left, +1 right), drawn at window resolution.
inline void menuDrawArrowButton(int x1, int y1, int x2, int y2, int dir, bool hover, bool enabled)
{
    if (!g_imguiFrameActive)
        return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 a = menuGameToScreen((float)x1, (float)y1);
    ImVec2 b = menuGameToScreen((float)(x2 + 1), (float)(y2 + 1));
    float t = (b.y - a.y) * 0.12f;
    if (t < 1.0f) t = 1.0f;
    ImU32 line = menuDrawButtonFrame(dl, a, b, hover, enabled, t);
    float cy = (a.y + b.y) * 0.5f;
    float h = (b.y - a.y) * 0.30f;
    float left = a.x + (b.x - a.x) * 0.32f;
    float right = b.x - (b.x - a.x) * 0.32f;
    if (dir < 0)
        dl->AddTriangleFilled(ImVec2(left, cy), ImVec2(right, cy - h), ImVec2(right, cy + h), line);
    else
        dl->AddTriangleFilled(ImVec2(right, cy), ImVec2(left, cy - h), ImVec2(left, cy + h), line);
}
