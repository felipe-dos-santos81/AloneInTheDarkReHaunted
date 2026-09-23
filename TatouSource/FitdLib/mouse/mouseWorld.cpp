///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: the engine adapter.
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "menuMouse.h"
#include "mouseWorld.h"
#include "mouseGate.h"
#include "mouseNav.h"
#include "mousePick.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>

namespace
{
mouse::ClickGate s_screenGate;
bool s_worldActive = false;

// ---- engine geometry --------------------------------------------------------

bool roomValid(int room)
{
    return room >= 0 && room < (int)roomDataTable.size();
}

mouse::RoomOrigin originOf(int room)
{
    const roomDataStruct& r = roomDataTable[room];
    return mouse::RoomOrigin{ r.worldX, r.worldY, r.worldZ };
}

// Cover polygons of every floor camera viewing `room`, in cover units (room/10).
std::vector<std::vector<mouse::XZ>> coverPolys(int room)
{
    std::vector<std::vector<mouse::XZ>> polys;
    for (const cameraDataStruct& cam : g_currentFloorCameraData)
        for (const cameraViewedRoomStruct& viewed : cam.viewedRoomTable)
        {
            if (viewed.viewedRoomIdx != room)
                continue;
            for (const cameraZoneEntryStruct& zone : viewed.coverZones)
            {
                std::vector<mouse::XZ> poly;
                for (int k = 0; k < zone.numPoints; ++k)
                    poly.push_back(mouse::XZ{ zone.pointTable[k].x, zone.pointTable[k].y });
                if (poly.size() >= 3)
                    polys.push_back(std::move(poly));
            }
        }
    return polys;
}

std::vector<mouse::Box> hardCols(int room)
{
    std::vector<mouse::Box> boxes;
    for (const hardColStruct& col : roomDataTable[room].hardColTable)
        boxes.push_back(mouse::Box{ col.zv.ZVX1, col.zv.ZVX2, col.zv.ZVY1, col.zv.ZVY2, col.zv.ZVZ1, col.zv.ZVZ2 });
    return boxes;
}

mouse::Agent agentOf(const tObject& actor)
{
    const int halfX = (actor.zv.ZVX2 - actor.zv.ZVX1) / 2;
    const int halfZ = (actor.zv.ZVZ2 - actor.zv.ZVZ1) / 2;
    return mouse::Agent{ std::max(halfX, halfZ), actor.zv.ZVY1, actor.zv.ZVY2 };
}

std::map<std::tuple<int, int, int, int, int>, std::optional<mouse::Grid>> s_grids;

const mouse::Grid* gridFor(int room, const mouse::Agent& agent)
{
    if (!roomValid(room))
        return nullptr;
    const auto key = std::make_tuple((int)g_currentFloor, room, agent.half, agent.y1, agent.y2);
    auto it = s_grids.find(key);
    if (it == s_grids.end())
    {
        if (s_grids.size() > 64)
            s_grids.clear(); // the hero's Y band changes on stairs: keep this bounded
        it = s_grids.emplace(key, mouse::buildGrid(coverPolys(room), hardCols(room), agent)).first;
    }
    return it->second ? &*it->second : nullptr;
}

// Floor-camera index of the camera on screen, or -1.
int currentFloorCamera()
{
    if (NumCamera < 0 || !roomValid(currentRoom))
        return -1;
    const std::vector<u16>& slots = roomDataTable[currentRoom].cameraIdxTable;
    if (NumCamera >= (int)slots.size())
        return -1;
    const int idx = slots[NumCamera];
    return idx < (int)g_currentFloorCameraData.size() ? idx : -1;
}

// The on-screen camera framed in `room`'s coordinate space.
bool cameraForRoom(int room, mouse::Camera* out)
{
    const int idx = currentFloorCamera();
    if (idx < 0 || !roomValid(room))
        return false;
    const cameraDataStruct& c = g_currentFloorCameraData[idx];
    *out = mouse::frameCamera(c.alpha, c.beta, c.gamma, c.x, c.y, c.z,
                              c.focal1, c.focal2, c.focal3, originOf(room), cosTable);
    return true;
}

std::map<std::tuple<int, int>, std::vector<mouse::PolyFit>> s_fits; // (room, floorY)
int s_fitsCamera = -1;

// Plane fits of `room`'s floor at height floorY under the on-screen camera.
const std::vector<mouse::PolyFit>* fitsFor(int room, int floorY)
{
    const int cam = currentFloorCamera();
    if (cam != s_fitsCamera)
    {
        s_fits.clear();
        s_fitsCamera = cam;
    }
    if (cam < 0 || !roomValid(room))
        return nullptr;
    const auto key = std::make_tuple(room, floorY);
    auto it = s_fits.find(key);
    if (it == s_fits.end())
    {
        mouse::Camera camera;
        if (!cameraForRoom(room, &camera))
            return nullptr;
        std::vector<std::vector<mouse::XZ>> world = coverPolys(room);
        for (auto& poly : world)
            for (auto& p : poly)
                p = mouse::XZ{ p.x * mouse::kCoverScale, p.z * mouse::kCoverScale };
        if (s_fits.size() > 64)
            s_fits.clear();
        it = s_fits.emplace(key, mouse::fitFloor(camera, world, floorY)).first;
    }
    return &it->second;
}

// ---- the hero ----------------------------------------------------------------

bool heroAvailable()
{
    return currentCameraTargetActor >= 0 && currentCameraTargetActor < NUM_MAX_OBJECT &&
           ListObjets[currentCameraTargetActor].indexInWorld >= 0 &&
           roomValid(ListObjets[currentCameraTargetActor].room);
}

tObject& hero()
{
    return ListObjets[currentCameraTargetActor];
}

mouse::HeroPose heroPose()
{
    const tObject& h = hero();
    return mouse::HeroPose{ h.room, mouse::XZ{ h.roomX + h.stepX, h.roomZ + h.stepZ }, h.beta };
}
}

void mouseWorldTakeOver()
{
    s_worldActive = false;
    s_screenGate.arm();
}

bool mouseWorldIsActive()
{
    return s_worldActive;
}

bool mouseScreenClickFilter(bool clickedThisFrame, bool downNow)
{
    return s_screenGate.filter(clickedThisFrame, downNow);
}

void mouseWorldFloorChanged()
{
    s_grids.clear();
    s_fits.clear();
    s_fitsCamera = -1;
}

void mouseWorldDrawDebugOverlay()
{
    if (!g_remasterConfig.debug.mouseNavOverlay || !g_imguiFrameActive || !heroAvailable())
        return;
    const tObject& h = hero();
    mouse::Camera camera;
    if (!cameraForRoom(h.room, &camera))
        return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();

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
