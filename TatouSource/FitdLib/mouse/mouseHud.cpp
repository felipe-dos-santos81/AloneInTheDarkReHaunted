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

void drawIcon(ImDrawList* dl, int index, const mouse::Rect& r, bool enabled, bool hover)
{
    const ImVec2 a = menuGameToScreen((float)r.x1, (float)r.y1);
    const ImVec2 b = menuGameToScreen((float)(r.x2 + 1), (float)(r.y2 + 1));
    const float t = std::max(1.0f, (b.y - a.y) * 0.08f);
    const ImU32 fill = IM_COL32(12, 12, 12, enabled ? 170 : 110);
    const ImU32 line = !enabled ? IM_COL32(120, 120, 110, 140)
                     : hover    ? IM_COL32(255, 230, 150, 255)
                                : IM_COL32(215, 215, 195, 230);
    dl->AddRectFilled(a, b, fill, 3.0f);
    dl->AddRect(a, b, line, 3.0f, 0, t);
    const float w = b.x - a.x;
    const float h = b.y - a.y;
    auto P = [&](float fx, float fy) { return ImVec2(a.x + w * fx, a.y + h * fy); };
    switch (index)
    {
    case 0: // inventory: a satchel
        dl->AddRect(P(0.25f, 0.40f), P(0.75f, 0.80f), line, 2.0f, 0, t);
        dl->AddLine(P(0.40f, 0.40f), P(0.40f, 0.24f), line, t);
        dl->AddLine(P(0.40f, 0.24f), P(0.60f, 0.24f), line, t);
        dl->AddLine(P(0.60f, 0.24f), P(0.60f, 0.40f), line, t);
        break;
    case 1: // map: a folded sheet
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
    default: // menu: three bars
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
    if (!mouseWorldHudState(&st) || !st.visible)
        return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const auto rects = mouse::hudIconRects();
    for (int i = 0; i < mouse::kHudIconCount; ++i)
        drawIcon(dl, i, rects[i], st.iconEnabled[i], st.hoverIcon == i && st.iconEnabled[i]);
    if (st.hasPreview)
        drawDiamond(dl, st.previewX, st.previewY, false);
    if (st.hasDestination)
        drawDiamond(dl, st.destX, st.destY, true);
    if (st.held && st.hasPointer)
        drawRing(dl, st.pointerX, st.pointerY, st.settling);
}
