///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: the engine adapter.
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "menuMouse.h"
#include "mouseWorld.h"
#include "mouseGate.h"
#include "mouseGesture.h"
#include "mouseHudLayout.h"
#include "mouseNav.h"
#include "mousePick.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
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

// ---- actors (port of m-aitd interaction/combat.py and router.py) --------------

int s_allowSystemMenu = 0;

bool isCombatTarget(int idx)
{
    if (idx < 0 || idx >= NUM_MAX_OBJECT || idx == currentCameraTargetActor)
        return false;
    const tObject& a = ListObjets[idx];
    return a.indexInWorld >= 0 && (a.objectType & AF_ANIMATED);
}

bool isInteractable(int idx)
{
    const tObject& a = ListObjets[idx];
    if (a.indexInWorld < 0 || a.indexInWorld >= (int)ListWorldObjets.size())
        return false;
    if (a.objectType & AF_FOUNDABLE)
        return true;
    return ListWorldObjets[a.indexInWorld].foundLife != -1;
}

// Scripted or movable scenery that is not picked up: held to push. AITD1 only
// (the push animation numbers are AITD1's).
bool isHoldActionTarget(int idx)
{
    if (g_gameId != AITD1 || idx < 0 || idx >= NUM_MAX_OBJECT || idx == currentCameraTargetActor)
        return false;
    const tObject& a = ListObjets[idx];
    if (a.indexInWorld < 0 || a.indexInWorld >= (int)ListWorldObjets.size() || a.bodyNum == -1 || !(a.dynFlags & 1))
        return false;
    const tWorldObject& w = ListWorldObjets[a.indexInWorld];
    if (w.objIndex != idx || w.stage != g_currentFloor)
        return false;
    if (a.objectType & AF_FOUNDABLE)
        return false;
    return (a.objectType & AF_MOVABLE) || a.life != -1;
}

// A click on an enemy swings only from an idle hero with something in hand
// (keyboard Action with an empty hand does nothing either).
bool canStrike(bool requireIdle)
{
    if (!heroAvailable())
        return false;
    if (requireIdle && hero().animActionType != 0)
        return false;
    return currentInventory >= 0 && currentInventory < NUM_MAX_INVENTORY && inHandTable[currentInventory] != -1;
}

bool hudIconAllowed(mouse::HudIcon icon)
{
    if (!s_allowSystemMenu)
        return false;
    switch (icon)
    {
    case mouse::HudIcon::Inventory:
        return statusScreenAllowed && numObjInInventoryTable[currentInventory] > 0;
    case mouse::HudIcon::Map:
        return statusScreenAllowed != 0;
    case mouse::HudIcon::Menu:
        return true;
    }
    return false;
}

mouse::ClickKind hudKindOf(mouse::HudIcon icon)
{
    switch (icon)
    {
    case mouse::HudIcon::Inventory: return mouse::ClickKind::HudInventory;
    case mouse::HudIcon::Map:       return mouse::ClickKind::HudMap;
    default:                        return mouse::ClickKind::HudMenu;
    }
}

// The hero's footprint with its Y band re-framed into `room`.
mouse::Agent agentIn(int room)
{
    mouse::Agent agent = agentOf(hero());
    if (room != hero().room)
    {
        agent.y1 = mouse::reframeY(agent.y1, originOf(hero().room), originOf(room));
        agent.y2 = mouse::reframeY(agent.y2, originOf(hero().room), originOf(room));
    }
    return agent;
}

// Topmost drawn actor whose screen box (exact, or forgiving) holds p.
int pickActorAt(mouse::Point p, bool forgiving)
{
    for (int k = NbAffObjets - 1; k >= 0; --k) // Index is far-to-near painter order
    {
        const int idx = Index[k];
        if (idx < 0 || idx >= NUM_MAX_OBJECT || idx == currentCameraTargetActor)
            continue;
        const tObject& a = ListObjets[idx];
        if (a.indexInWorld < 0 || a.bodyNum == -1 || a.screenXMax < 0 || a.screenYMax < 0)
            continue;
        mouse::Rect box{ a.screenXMin, a.screenYMin, a.screenXMax, a.screenYMax };
        if (forgiving)
            box = mouse::forgivingBox(box);
        if (mouse::contains(box, p))
            return idx;
    }
    return -1;
}

// Where to stand to push `targetIdx`: the nearest walkable spot beside one of its faces.
std::optional<mouse::Payload> holdActionApproach(int targetIdx)
{
    if (!isHoldActionTarget(targetIdx) || !heroAvailable())
        return std::nullopt;
    const tObject& h = hero();
    const tObject& t = ListObjets[targetIdx];
    if (h.room != t.room)
        return std::nullopt;
    const mouse::Agent agent = agentOf(h);
    const mouse::Grid* grid = gridFor(t.room, agent);
    if (!grid)
        return std::nullopt;
    const int clearance = agent.half + grid->step;
    const mouse::XZ from = heroPose().at;
    auto clampTo = [](int v, int lo, int hi) { return std::max(lo, std::min(v, hi)); };
    const mouse::XZ candidates[4] = {
        { t.zv.ZVX1 - clearance, clampTo(from.z, t.zv.ZVZ1, t.zv.ZVZ2) },
        { t.zv.ZVX2 + clearance, clampTo(from.z, t.zv.ZVZ1, t.zv.ZVZ2) },
        { clampTo(from.x, t.zv.ZVX1, t.zv.ZVX2), t.zv.ZVZ1 - clearance },
        { clampTo(from.x, t.zv.ZVX1, t.zv.ZVX2), t.zv.ZVZ2 + clearance },
    };
    std::optional<mouse::XZ> best;
    int bestCost = 0;
    for (const mouse::XZ& c : candidates)
    {
        if (auto spot = mouse::nearestWalkable(*grid, c))
        {
            const int cost = std::abs(spot->x - from.x) + std::abs(spot->z - from.z);
            if (!best || cost < bestCost)
            {
                best = spot;
                bestCost = cost;
            }
        }
    }
    if (!best)
        return std::nullopt;
    return mouse::Payload{ best->x, best->z, t.room, t.indexInWorld };
}

// Stand next to an interactable object, on the side the hero comes from.
mouse::ClickResult targetFor(int actorIdx)
{
    const tObject& t = ListObjets[actorIdx];
    const tObject& h = hero();
    mouse::XZ dest{ t.roomX, t.roomZ };
    if (const mouse::Grid* grid = gridFor(t.room, agentIn(t.room)))
    {
        mouse::XZ from = heroPose().at;
        if (h.room != t.room)
            from = mouse::reframe(from, originOf(h.room), originOf(t.room));
        if (auto spot = mouse::approachCell(*grid, dest, from))
            dest = *spot;
    }
    return mouse::ClickResult{ mouse::ClickKind::Target, mouse::Payload{ dest.x, dest.z, t.room, t.indexInWorld } };
}

// A pixel with no reachable floor still names a direction to walk in.
mouse::ClickResult steerToward(mouse::Point p)
{
    const tObject& h = hero();
    mouse::Camera camera;
    const auto* fits = fitsFor(h.room, h.roomY);
    if (!fits || !cameraForRoom(h.room, &camera))
        return {};
    auto target = mouse::steerPoint(camera, *fits, h.roomY, heroPose().at, p);
    if (!target)
        return {}; // the hero's feet are off screen, or the pointer is on the hero
    return mouse::ClickResult{ mouse::ClickKind::Steer, mouse::Payload{ target->x, target->z, h.room, -1 } };
}

mouse::ClickResult pickFloorOrSteer(mouse::Point p)
{
    const tObject& h = hero();
    const mouse::RoomOrigin heroOrigin = originOf(h.room);
    std::vector<int> rooms{ h.room };
    const int cam = currentFloorCamera();
    if (cam >= 0)
        for (const cameraViewedRoomStruct& viewed : g_currentFloorCameraData[cam].viewedRoomTable)
            if (viewed.viewedRoomIdx != h.room && roomValid(viewed.viewedRoomIdx))
                rooms.push_back(viewed.viewedRoomIdx);

    for (int room : rooms)
    {
        const int floorY = mouse::reframeY(h.roomY, heroOrigin, originOf(room));
        const auto* fits = fitsFor(room, floorY);
        if (!fits)
            continue;
        auto hit = mouse::pickFloor(*fits, p);
        if (!hit)
            continue;
        mouse::XZ dest = *hit;
        const mouse::Grid* grid = gridFor(room, agentIn(room));
        if (grid && grid->any())
        {
            mouse::Camera camera;
            if (!cameraForRoom(room, &camera))
                return steerToward(p);
            auto snapped = mouse::nearestWalkable(*grid, dest, 6, [&](mouse::XZ c) {
                auto s = mouse::projectPoint(camera, c.x, floorY, c.z);
                return s && s->x >= 0.0 && s->x < mouse::kLogicalW && s->y >= 0.0 && s->y < mouse::kLogicalH &&
                       std::fabs(s->x - p.x) <= mouse::kSnapBudgetPx && std::fabs(s->y - p.y) <= mouse::kSnapBudgetPx;
            });
            if (!snapped)
                return steerToward(p);
            dest = *snapped;
        }
        return mouse::ClickResult{ mouse::ClickKind::Walk, mouse::Payload{ dest.x, dest.z, room, -1 } };
    }
    return steerToward(p);
}

// What a click at p would do. One resolver behind both the cursor and the click.
mouse::ClickResult resolveAt(mouse::Point p)
{
    if (NumCamera < 0 || !heroAvailable())
        return {};
    if (auto icon = mouse::hudIconAt(p))
    {
        if (!hudIconAllowed(*icon))
            return {};
        return mouse::ClickResult{ hudKindOf(*icon), {} };
    }
    int actor = pickActorAt(p, false);
    if (actor < 0)
        actor = pickActorAt(p, true);
    if (actor >= 0 && isCombatTarget(actor))
    {
        if (!canStrike(true))
            return {}; // aimed at the enemy: never a fall-through to a walk
        return mouse::ClickResult{ mouse::ClickKind::Attack, mouse::Payload{ 0, 0, -1, actor } };
    }
    if (actor >= 0 && !isInteractable(actor))
    {
        if (isHoldActionTarget(actor))
            if (auto payload = holdActionApproach(actor))
                return mouse::ClickResult{ mouse::ClickKind::Push, *payload };
        actor = -1; // inert scenery: the pixel means what the floor behind it means
    }
    if (actor >= 0)
        return targetFor(actor);
    return pickFloorOrSteer(p);
}

mouse::ClickResult resolveClick(mouse::Point p)
{
    return s_worldActive ? resolveAt(p) : mouse::ClickResult{};
}

const char* kindName(mouse::ClickKind kind)
{
    switch (kind)
    {
    case mouse::ClickKind::Walk:         return "walk";
    case mouse::ClickKind::Steer:        return "steer";
    case mouse::ClickKind::Target:       return "target";
    case mouse::ClickKind::Push:         return "push";
    case mouse::ClickKind::Attack:       return "attack";
    case mouse::ClickKind::HudInventory: return "hud:inventory";
    case mouse::ClickKind::HudMap:       return "hud:map";
    case mouse::ClickKind::HudMenu:      return "hud:menu";
    default:                             return "blocked";
    }
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

    // What a click under the pointer would do (resolver, ignoring the world-active gate).
    {
        ImVec2 at = menuGetGameMouse();
        if (at.x >= 0.0f)
        {
            const mouse::ClickResult r = resolveAt(mouse::Point{ (int)at.x, (int)at.y });
            dl->AddText(ImVec2(ImGui::GetIO().MousePos.x + 14, ImGui::GetIO().MousePos.y + 14),
                        IM_COL32(255, 255, 255, 255), kindName(r.kind));
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
