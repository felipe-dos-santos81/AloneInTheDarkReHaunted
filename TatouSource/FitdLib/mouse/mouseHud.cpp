///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: HUD drawing.
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "menuMouse.h"
#include "mouseHud.h"
#include "mouseHudLayout.h"
#include "mouseWorld.h"

#include <algorithm>
#include <cmath>

// Declared at file scope (not inside the anonymous namespace below): a local
// extern nested in an unnamed namespace binds to that namespace and would
// leave this referring to an internal-linkage symbol with no definition.
extern int outputResolution[2];

namespace
{
float logicalScale()
{
    return (float)outputResolution[1] / 200.0f;
}

// Lighter than the menu buttons: the icons sit over live gameplay.
constexpr MenuButtonStyle HUD_ICON_STYLE = {
    IM_COL32(12, 12, 12, 170), IM_COL32(12, 12, 12, 110),
    IM_COL32(215, 215, 195, 230), IM_COL32(255, 230, 150, 255), IM_COL32(120, 120, 110, 140),
    3.0f,
};

void drawIcon(ImDrawList* dl, mouse::HudIcon icon, const mouse::Rect& r, bool enabled, bool hover)
{
    const MenuButtonGeometry g = menuButtonGeometry(r, 0.08f);
    const ImVec2 a = g.a;
    const ImVec2 b = g.b;
    const float t = g.thickness;
    const ImU32 line = menuDrawButtonFrame(dl, g, hover, enabled, HUD_ICON_STYLE);
    const float w = b.x - a.x;
    const float h = b.y - a.y;
    auto P = [&](float fx, float fy) { return ImVec2(a.x + w * fx, a.y + h * fy); };
    switch (icon)
    {
    case mouse::HudIcon::Inventory: // a satchel
        dl->AddRect(P(0.25f, 0.40f), P(0.75f, 0.80f), line, 2.0f, 0, t);
        dl->AddLine(P(0.40f, 0.40f), P(0.40f, 0.24f), line, t);
        dl->AddLine(P(0.40f, 0.24f), P(0.60f, 0.24f), line, t);
        dl->AddLine(P(0.60f, 0.24f), P(0.60f, 0.40f), line, t);
        break;
    case mouse::HudIcon::Map: // a folded sheet
        dl->AddLine(P(0.20f, 0.30f), P(0.40f, 0.22f), line, t);
        dl->AddLine(P(0.40f, 0.22f), P(0.60f, 0.30f), line, t);
        dl->AddLine(P(0.60f, 0.30f), P(0.80f, 0.22f), line, t);
        dl->AddLine(P(0.20f, 0.78f), P(0.40f, 0.70f), line, t);
        dl->AddLine(P(0.40f, 0.70f), P(0.60f, 0.78f), line, t);
        dl->AddLine(P(0.60f, 0.78f), P(0.80f, 0.70f), line, t);
        dl->AddLine(P(0.20f, 0.30f), P(0.20f, 0.78f), line, t);
        dl->AddLine(P(0.40f, 0.22f), P(0.40f, 0.70f), line, t);
        dl->AddLine(P(0.60f, 0.30f), P(0.60f, 0.78f), line, t);
        dl->AddLine(P(0.80f, 0.22f), P(0.80f, 0.70f), line, t);
        break;
    case mouse::HudIcon::Menu: // three bars
        for (float fy : { 0.32f, 0.50f, 0.68f })
            dl->AddLine(P(0.25f, fy), P(0.75f, fy), line, t);
        break;
    }
}

void drawDiamond(ImDrawList* dl, float lx, float ly, bool solid)
{
    const ImVec2 c = menuGameToScreen(lx, ly);
    const float s = 4.0f * logicalScale();
    const ImU32 col = solid ? IM_COL32(240, 240, 210, 255) : IM_COL32(240, 240, 210, 110);
    dl->AddQuad(ImVec2(c.x, c.y - s), ImVec2(c.x + s, c.y), ImVec2(c.x, c.y + s), ImVec2(c.x - s, c.y),
                col, solid ? 2.0f : 1.0f);
}

void drawRing(ImDrawList* dl, int lx, int ly, bool settling)
{
    const ImVec2 c = menuGameToScreen((float)lx, (float)ly);
    const float r = 9.0f * logicalScale();
    const ImU32 col = IM_COL32(240, 240, 210, 200);
    if (!settling)
    {
        dl->AddCircle(c, r, col, 32, 1.5f);
        return;
    }
    for (int i = 0; i < 8; ++i) // dashed while a camera cut settles
    {
        const float angle = (float)i * 3.14159265f / 4.0f;
        dl->AddCircleFilled(ImVec2(c.x + r * std::cos(angle), c.y + r * std::sin(angle)), 1.5f, col);
    }
}
}

void mouseHudDraw()
{
    if (!g_imguiFrameActive)
        return;
    MouseHudState st;
    if (!mouseWorldHudState(&st))
        return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const auto rects = mouse::hudIconRects();
    for (int i = 0; i < mouse::kHudIconCount; ++i)
    {
        const mouse::HudIcon icon = (mouse::HudIcon)i;
        drawIcon(dl, icon, rects[i], st.iconEnabled[i], st.hoverIcon == icon && st.iconEnabled[i]);
    }
    if (st.preview)
        drawDiamond(dl, (float)st.preview->x, (float)st.preview->y, false);
    if (st.destination)
        drawDiamond(dl, (float)st.destination->x, (float)st.destination->y, true);
    if (st.held && st.pointer)
        drawRing(dl, st.pointer->x, st.pointer->y, st.settling);
}
