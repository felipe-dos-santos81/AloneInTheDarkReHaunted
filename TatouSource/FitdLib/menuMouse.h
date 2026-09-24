///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Shared mouse-input helpers for all game menus.
//
// Pattern:
//   static MenuHoverAnchor s_hover;
//   bool moved = menuMouseMoved(s_hover, localKey || localJoyD);
//   if (moved) { int item = menuMouseHitList(...); if (item >= 0) sel = item; }
//   if (menuMouseClicked() && menuMouseHitList(...) >= 0) { /* confirm */ }
//
// A click that the hit helpers place on an item is recorded for the
// fullscreen double-click guard by the helpers themselves; only screens that
// hit-test by hand call menuNoteItemClick().
///////////////////////////////////////////////////////////////////////////////
#pragma once
#include "imguiBGFX.h"
#include <SDL.h>

#include <algorithm>

#include "mouse/mouseTypes.h"
#include "mouse/mouseWorld.h"
#include "mouse/mouseInput.h"

// SDL_GetTicks() of the last mouse click that activated a menu item. The
// fullscreen double-click (input.cpp) is refused shortly after one.
extern Uint32 g_menuItemClickMs;

// Call wherever a mouse click activates a menu item the hit helpers below
// did not find.
inline void menuNoteItemClick()
{
    g_menuItemClickMs = (Uint32)SDL_GetTicks();
}

// ImGui frame of the last screen click menuMouseClicked() reported.
inline int& menuClickFrame()
{
    static int frame = -2;
    return frame;
}

// A hit helper found an item under a click from this frame or the one before
// (screens hit-test before or after reading the click): stamp it.
inline void menuNoteHit()
{
    if (ImGui::GetFrameCount() - menuClickFrame() <= 1)
        menuNoteItemClick();
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
            mouseInputRequestVisible(true);
            st.hidden = false;
        }
    }
    else if (!st.hidden && !mouseWorldWantsCursor() &&
             (SDL_GetTicks() - st.lastMoveT) >= CURSOR_HIDE_DELAY_MS)
    {
        mouseInputRequestVisible(false);
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
        mouseInputRequestVisible(true);
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

// Where a screen's hover last saw the pointer. Keep one per screen (a static
// local is fine): it re-seeds itself whenever the screen was not polling.
struct MenuHoverAnchor
{
    ImVec2 pos = { -1.0f, -1.0f };
    int frame = -1000; // ImGui frame of the last menuMouseMoved call
};

static constexpr float MOUSE_MOVE_THRESHOLD = 3.0f; // pixels before hover updates
// A gap this long in polling means the screen was closed or a nested screen
// ran; loops that pump a few frames per iteration stay well under it.
static constexpr int MENU_HOVER_RESEED_FRAMES = 30;

// Returns true when the mouse has physically moved since the last call AND
// no keyboard/gamepad input was active this frame. The first call after the
// screen opens (or after a nested screen) only seeds the anchor, so a reopen
// never jumps the selection to wherever the pointer rests.
// hasInput should be (localKey || localJoyD) — any non-zero value suppresses hover.
inline bool menuMouseMoved(MenuHoverAnchor& anchor, bool hasInput)
{
    const ImVec2 cur = ImGui::GetIO().MousePos;
    const int frame = ImGui::GetFrameCount();
    const bool reseed = frame - anchor.frame > MENU_HOVER_RESEED_FRAMES;
    anchor.frame = frame;
    if (reseed)
    {
        anchor.pos = cur;
        return false;
    }

    const float dx = cur.x - anchor.pos.x;
    const float dy = cur.y - anchor.pos.y;
    if (hasInput || (dx * dx + dy * dy) < (MOUSE_MOVE_THRESHOLD * MOUSE_MOVE_THRESHOLD))
        return false;
    anchor.pos = cur;
    return true;
}

// Returns true when the left mouse button was just pressed this frame, except
// right after a screen opened: the press that opened it is swallowed until the
// button has been released (mouseWorldTakeOver arms the gate).
inline bool menuMouseClicked()
{
    ImGuiIO& io = ImGui::GetIO();
    const bool clicked = mouseScreenClickFilter(io.MouseClicked[0], io.MouseDown[0]);
    if (clicked)
        menuClickFrame() = ImGui::GetFrameCount();
    return clicked;
}

// A click that ends a wait (intro, picture, sequence, game over, the map): it
// acts like a menu item, so its double-click never toggles fullscreen.
inline bool menuMouseSkipClicked()
{
    const bool clicked = menuMouseClicked();
    if (clicked)
        menuNoteItemClick();
    return clicked;
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
        {
            menuNoteHit();
            return i;
        }
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
    const int button = hit(leftCx) ? 0 : hit(rightCx) ? 1 : -1;
    if (button >= 0)
        menuNoteHit();
    return button;
}

// Hit-test an inclusive rectangle in game space.
inline bool menuMouseHitRect(float gameX, float gameY, const mouse::Rect& r)
{
    const bool hit = gameX >= (float)r.x1 && gameX <= (float)r.x2 &&
                     gameY >= (float)r.y1 && gameY <= (float)r.y2;
    if (hit)
        menuNoteHit();
    return hit;
}

// 320x200 game coordinates -> ImGui foreground draw-list coordinates
// (window pixels), the same mapping fontTTF.cpp uses.
inline ImVec2 menuGameToScreen(float gx, float gy)
{
    extern int outputResolution[2];
    return ImVec2(gx * (float)outputResolution[0] / 320.0f,
                  gy * (float)outputResolution[1] / 200.0f);
}

// Window-pixel corners of an inclusive 320x200 rect, and a stroke width that
// is `strokeFraction` of its height (at least one pixel).
struct MenuButtonGeometry
{
    ImVec2 a;
    ImVec2 b;
    float thickness;
};

inline MenuButtonGeometry menuButtonGeometry(const mouse::Rect& r, float strokeFraction)
{
    const ImVec2 a = menuGameToScreen((float)r.x1, (float)r.y1);
    const ImVec2 b = menuGameToScreen((float)(r.x2 + 1), (float)(r.y2 + 1));
    return MenuButtonGeometry{ a, b, std::max(1.0f, (b.y - a.y) * strokeFraction) };
}

// Colours and corner rounding of a drawn button frame.
struct MenuButtonStyle
{
    ImU32 fill;
    ImU32 fillDisabled;
    ImU32 line;
    ImU32 lineHover;
    ImU32 lineDisabled;
    float rounding;
};

static constexpr MenuButtonStyle MENU_BUTTON_STYLE = {
    IM_COL32(16, 16, 16, 200), IM_COL32(16, 16, 16, 200),
    IM_COL32(210, 210, 190, 255), IM_COL32(255, 230, 150, 255), IM_COL32(110, 110, 100, 170),
    2.0f,
};

// Shared frame for drawn buttons (menus and the gameplay HUD). Returns the
// line colour for the glyph.
inline ImU32 menuDrawButtonFrame(ImDrawList* dl, const MenuButtonGeometry& g, bool hover, bool enabled,
                                 const MenuButtonStyle& style = MENU_BUTTON_STYLE)
{
    const ImU32 fill = enabled ? style.fill : style.fillDisabled;
    const ImU32 line = !enabled ? style.lineDisabled : hover ? style.lineHover : style.line;
    dl->AddRectFilled(g.a, g.b, fill, style.rounding);
    dl->AddRect(g.a, g.b, line, style.rounding, 0, g.thickness);
    return line;
}

// A small "X" button over the inclusive 320x200 rect, drawn at window resolution.
inline void menuDrawCloseButton(const mouse::Rect& r, bool hover)
{
    if (!g_imguiFrameActive)
        return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const MenuButtonGeometry g = menuButtonGeometry(r, 0.12f);
    const ImVec2 a = g.a;
    const ImVec2 b = g.b;
    const float t = g.thickness;
    ImU32 line = menuDrawButtonFrame(dl, g, hover, true);
    float px = (b.x - a.x) * 0.30f;
    float py = (b.y - a.y) * 0.25f;
    dl->AddLine(ImVec2(a.x + px, a.y + py), ImVec2(b.x - px, b.y - py), line, t);
    dl->AddLine(ImVec2(b.x - px, a.y + py), ImVec2(a.x + px, b.y - py), line, t);
}

// A small triangle button (dir -1 points left, +1 right), drawn at window resolution.
inline void menuDrawArrowButton(const mouse::Rect& r, int dir, bool hover, bool enabled)
{
    if (!g_imguiFrameActive)
        return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const MenuButtonGeometry g = menuButtonGeometry(r, 0.12f);
    const ImVec2 a = g.a;
    const ImVec2 b = g.b;
    ImU32 line = menuDrawButtonFrame(dl, g, hover, enabled);
    float cy = (a.y + b.y) * 0.5f;
    float h = (b.y - a.y) * 0.30f;
    float left = a.x + (b.x - a.x) * 0.32f;
    float right = b.x - (b.x - a.x) * 0.32f;
    if (dir < 0)
        dl->AddTriangleFilled(ImVec2(left, cy), ImVec2(right, cy - h), ImVec2(right, cy + h), line);
    else
        dl->AddTriangleFilled(ImVec2(right, cy), ImVec2(left, cy - h), ImVec2(left, cy + h), line);
}
