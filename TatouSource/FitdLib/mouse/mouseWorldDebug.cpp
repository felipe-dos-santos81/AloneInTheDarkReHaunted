///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: the debug overlay (debug.mouseNavOverlay): walk grid, the
// floor pick under the pointer, and a projection cross-check.
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "menuMouse.h"
#include "mouseWorldInternal.h"

#include <cmath>
#include <cstdio>

using namespace mouseworld;

void mouseWorldDrawDebugOverlay()
{
    if (!g_remasterConfig.debug.mouseNavOverlay || !g_imguiFrameActive || !heroAvailable())
        return;
    const tObject& h = hero();
    mouse::Camera camera;
    if (!cameraForRoom(h.room, &camera))
        return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    // What a click under the pointer would do (resolver, ignoring the world-active gate).
    {
        ImVec2 at = menuGetGameMouse();
        if (at.x >= 0.0f)
        {
            const mouse::ClickResult r = resolveAt(mouse::Point{ (int)at.x, (int)at.y });
            dl->AddText(ImVec2(ImGui::GetIO().MousePos.x + 14, ImGui::GetIO().MousePos.y + 14),
                        IM_COL32(255, 255, 255, 255), kindInfo(r.kind).name);
        }
    }

    // Walkable cells of the hero's room at the hero's floor height.
    if (const mouse::Grid* grid = gridFor(h.room, agentOf(h)))
    {
        for (int i = 0; i < grid->nx; ++i)
            for (int j = 0; j < grid->nz; ++j)
            {
                if (!grid->at(i, j))
                    continue;
                const mouse::XZ c = grid->center(i, j);
                if (auto s = mouse::projectPoint(camera, c.x, h.roomY, c.z))
                    dl->AddCircleFilled(menuGameToScreen((float)s->x, (float)s->y), 2.0f, IM_COL32(80, 220, 120, 160));
            }
    }

    // Floor pick under the pointer, drawn where the replica projects it back.
    ImVec2 gm = menuGetGameMouse();
    if (gm.x >= 0.0f)
    {
        if (const auto* fits = fitsFor(h.room, h.roomY))
        {
            if (auto hit = mouse::pickFloor(*fits, mouse::Point{ (int)gm.x, (int)gm.y }))
            {
                if (auto s = mouse::projectPoint(camera, hit->x, h.roomY, hit->z))
                {
                    const ImVec2 p = menuGameToScreen((float)s->x, (float)s->y);
                    dl->AddLine(ImVec2(p.x - 6, p.y - 6), ImVec2(p.x + 6, p.y + 6), IM_COL32(255, 60, 60, 255), 2.0f);
                    dl->AddLine(ImVec2(p.x + 6, p.y - 6), ImVec2(p.x - 6, p.y + 6), IM_COL32(255, 60, 60, 255), 2.0f);
                }
            }
        }
    }

    // Cross-check the projection replica against the renderer's live globals
    // (valid after AllRedraw) whenever the hero stands in the camera's room.
    if (h.room == currentRoom)
    {
        float X = (float)(h.worldX + h.stepX - translateX);
        float Y = (float)(h.worldY + h.stepY);
        float Z = (float)(h.worldZ + h.stepZ - translateZ);
        Y -= translateY;
        transformPoint(&X, &Y, &Z);
        const float depth = (float)(s16)Z + (float)cameraPerspective;
        auto replica = mouse::projectPoint(camera, h.worldX + h.stepX, h.worldY + h.stepY, h.worldZ + h.stepZ);
        if (depth > 50.0f && replica)
        {
            const float engineX = ((float)(s16)X * cameraFovX) / depth + cameraCenterX;
            static bool s_reported = false;
            if (!s_reported && std::fabs(engineX - (float)replica->x) > 0.01f)
            {
                s_reported = true;
                printf("mouse: projection replica differs from the renderer (%.3f vs %.3f)\n",
                       engineX, (float)replica->x);
            }
        }
    }
}
