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
#include "mouseInput.h"
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

// ---- the live mouse state --------------------------------------------------

struct World
{
    mouse::PointerState pointer;
    std::optional<mouse::NavIntent> intent;
    mouse::NavDecision decision;
    bool hasDecision = false;
    int attackTarget = -1;
    uint32_t attackStartMs = 0;
    int attackFrames = 0;
    bool lastInputMouse = false;
    int intentFloor = -1;
    mouse::ClickResult hover;
    std::optional<mouse::Point> hoverPos;
};

World s_world;

// ---- held push (port of m-aitd playworld/held_push.py) -------------------------

constexpr int kPlayerStandAnim = 4;         // AITD1 hero stand animation
constexpr int kPlayerPushAnim = 5;          // AITD1 hero push animation
constexpr int kPlayerLifeForwardAnim = 254; // what the hero's LIFE re-queues while walking (Step 1)
constexpr bool kForcePushAnim = true;       // decided in Step 1

// Run InitAnim on the hero (InitAnim works on the "current processed" actor).
void initHeroAnim(int anim, int type, int info)
{
    tObject* savedPtr = currentProcessedActorPtr;
    const int savedIdx = currentProcessedActorIdx;
    currentProcessedActorIdx = currentCameraTargetActor;
    currentProcessedActorPtr = &hero();
    InitAnim(anim, type, info);
    currentProcessedActorPtr = savedPtr;
    currentProcessedActorIdx = savedIdx;
}

// Stop the hero where it stands and put it back in its stand pose (FITD anim.cpp:238-253
// commits the pending step when this transition applies).
void stopHero()
{
    if (!heroAvailable())
        return;
    tObject& h = hero();
    h.speed = 0;
    h.direction = 0;
    h.rotate.numSteps = 0;
    // Never force the stand pose over a script-owned hero (Task 17's trackMode cancel) or
    // while an uninterruptable anim (hit/death) is current or queued: GereAnim commits
    // newAnim unconditionally, bypassing InitAnim's ANIM_UNINTERRUPTABLE refusal.
    if (g_gameId == AITD1 && h.trackMode == 1 && !((h.animType | h.newAnimType) & ANIM_UNINTERRUPTABLE))
    {
        initHeroAnim(kPlayerStandAnim, 0, kPlayerStandAnim);
        h.newAnim = kPlayerStandAnim;
        h.newAnimType = 0;
        h.newAnimInfo = kPlayerStandAnim;
    }
}

void cancelIntent()
{
    const bool held = s_world.intent && s_world.intent->requiresHold;
    s_world.intent.reset();
    s_world.hasDecision = false;
    if (held)
        stopHero();
}

void clearAttack()
{
    s_world.attackTarget = -1;
    s_world.attackFrames = 0;
}

bool latchedPush()
{
    return s_world.intent && s_world.intent->requiresHold;
}

void startIntent(mouse::ClickKind kind, const mouse::Payload& p, bool run)
{
    mouse::NavIntent in;
    in.dest = mouse::XZ{ p.x, p.z };
    in.room = p.room;
    in.targetObject = p.object;
    in.requiresHold = kind == mouse::ClickKind::Push;
    in.run = run && !in.requiresHold;          // leaning on furniture is never a run
    in.steering = kind == mouse::ClickKind::Steer;
    if (in.requiresHold)
    {
        in.originFloor = g_currentFloor;
        in.originRoom = hero().room;
    }
    s_world.intent = in;
    s_world.intentFloor = g_currentFloor;
    s_world.hasDecision = false;
}

void applyDecision(const mouse::Decision& d)
{
    switch (d.type)
    {
    case mouse::DecisionType::OpenHud:
        // The opened screen's first quick click is not a fullscreen double-click.
        menuNoteItemClick();
        localKey = d.kind == mouse::ClickKind::HudInventory ? 0x1C
                 : d.kind == mouse::ClickKind::HudMap       ? 0x0F
                                                            : 0x1B;
        break;
    case mouse::DecisionType::Issue:
        startIntent(d.kind, d.payload, d.run);
        break;
    case mouse::DecisionType::Cancel:
        cancelIntent();
        break;
    default:
        break; // Attack is handled from Task 20 on
    }
}

void endPointerHold()
{
    mouse::onRelease(s_world.pointer);
    mouse::endHold(s_world.pointer, s_world.intent && s_world.intent->steering);
    cancelIntent();
}

void releaseAll()
{
    mouse::resetPointer(s_world.pointer);
    cancelIntent();
    clearAttack();
}

// Not driving the world this frame (option off, cutscene, no hero).
void leaveWorld()
{
    s_worldActive = false;
    if (s_world.intent || s_world.attackTarget >= 0 || s_world.pointer.held)
        releaseAll();
    mouseInputRequestCursor(mouse::CursorShape::Default);
}

// The doorway midpoint linking `from` to `to`, in from's frame (track.cpp follow mode).
mouse::XZ linkMidpoint(int from, int to)
{
    char* link = getRoomLink((unsigned int)from, (unsigned int)to);
    const int x1 = *(s16*)(link + 0);
    const int x2 = *(s16*)(link + 2);
    const int z1 = *(s16*)(link + 8);
    const int z2 = *(s16*)(link + 10);
    return mouse::XZ{ x1 + (x2 - x1) / 2, z1 + (z2 - z1) / 2 };
}

// Re-aim an arrived target click at the object itself, so collision-driven
// FOUND scripts (anim.cpp HARD_COL) fire; a second arrival then dispatches.
bool pushIntoTarget(mouse::NavIntent& in)
{
    if (in.targetObject < 0 || in.targetObject >= (int)ListWorldObjets.size())
        return false;
    const tWorldObject& w = ListWorldObjets[in.targetObject];
    if (w.objIndex == -1 || w.foundLife == -1)
        return false;
    const tObject& a = ListObjets[w.objIndex];
    if (a.objectType & AF_FOUNDABLE)
        return false;
    if (in.dest == mouse::XZ{ a.roomX, a.roomZ })
        return false;
    in.dest = mouse::XZ{ a.roomX, a.roomZ };
    in.room = a.room;
    in.planned = false;
    mouse::resetStall(in);
    return true;
}

// Act on an arrival at a clicked object. Floor walks end silently: the Action
// bit is global and scripts poll it.
void dispatchTarget(int worldIdx)
{
    if (worldIdx < 0 || worldIdx >= (int)ListWorldObjets.size())
        return;
    const tWorldObject& w = ListWorldObjets[worldIdx];
    if (w.objIndex == -1)
        return; // taken or gone while we walked
    const tObject& a = ListObjets[w.objIndex];
    if (a.objectType & AF_FOUNDABLE)
    {
        FoundObjet(worldIdx, 0); // blocking screen; calls mouseWorldTakeOver
        return;
    }
    localClick = 1; // one frame of Action: PlayWorld turns it into action = 0x2000
}

bool corridorHitsActor(mouse::XZ here, mouse::XZ point, int half, const ZVStruct& blocker)
{
    const double delta[2] = { (double)(point.x - here.x), (double)(point.z - here.z) };
    const double origin[2] = { (double)here.x, (double)here.z };
    const double low[2] = { (double)(blocker.ZVX1 - half), (double)(blocker.ZVZ1 - half) };
    const double high[2] = { (double)(blocker.ZVX2 + half), (double)(blocker.ZVZ2 + half) };
    double entry = 0.0;
    double leave = 1.0;
    for (int k = 0; k < 2; ++k)
    {
        if (delta[k] == 0.0)
        {
            if (origin[k] < low[k] || origin[k] > high[k])
                return false;
            continue;
        }
        const double first = (low[k] - origin[k]) / delta[k];
        const double second = (high[k] - origin[k]) / delta[k];
        entry = std::max(entry, std::min(first, second));
        leave = std::min(leave, std::max(first, second));
    }
    return entry <= leave && leave > 0.0 && entry < 1.0;
}

int pathDistance(mouse::XZ start, const std::vector<mouse::XZ>& path)
{
    int total = 0;
    mouse::XZ prev = start;
    for (mouse::XZ p : path)
    {
        total += std::abs(p.x - prev.x) + std::abs(p.z - prev.z);
        prev = p;
    }
    return total;
}

// One clearance waypoint when another actor blocks contact with the target.
std::optional<std::vector<mouse::XZ>> heldContactDetour(int targetIdx, mouse::XZ point, const mouse::Grid* grid)
{
    if (!grid)
        return std::nullopt;
    const tObject& h = hero();
    const int half = agentOf(h).half;
    const mouse::XZ here = heroPose().at;
    const tObject* blocker = nullptr;
    for (int idx = 0; idx < NUM_MAX_OBJECT; ++idx)
    {
        const tObject& a = ListObjets[idx];
        if (idx == targetIdx || idx == currentCameraTargetActor || a.indexInWorld < 0 || a.room != h.room)
            continue;
        if (a.COL_BY != currentCameraTargetActor)
            continue;
        if (!(h.zv.ZVY1 < a.zv.ZVY2 && a.zv.ZVY1 < h.zv.ZVY2))
            continue;
        if (corridorHitsActor(here, point, half, a.zv))
        {
            blocker = &a;
            break;
        }
    }
    if (!blocker)
        return std::nullopt;
    const int margin = half + mouse::kWaypointDistance + grid->step + 1;
    const ZVStruct& b = blocker->zv;
    const bool alongX = std::abs(point.x - here.x) >= std::abs(point.z - here.z);
    std::vector<mouse::XZ> options;
    if (alongX)
        options = { mouse::XZ{ here.x, b.ZVZ2 + margin }, mouse::XZ{ here.x, b.ZVZ1 - margin } };
    else
        options = { mouse::XZ{ b.ZVX2 + margin, here.z }, mouse::XZ{ b.ZVX1 - margin, here.z } };
    auto clears = [&](mouse::XZ w) {
        if (alongX)
            return w.z - mouse::kWaypointDistance >= b.ZVZ2 + half || w.z + mouse::kWaypointDistance <= b.ZVZ1 - half;
        return w.x - mouse::kWaypointDistance >= b.ZVX2 + half || w.x + mouse::kWaypointDistance <= b.ZVX1 - half;
    };
    std::optional<std::vector<mouse::XZ>> best;
    for (mouse::XZ candidate : options)
    {
        auto walkable = mouse::nearestWalkable(*grid, candidate);
        if (!walkable || !clears(*walkable))
            continue;
        auto path = mouse::findPath(*grid, here, *walkable);
        if (path && (!best || pathDistance(here, *path) < pathDistance(here, *best)))
            best = path;
    }
    return best;
}

// Aim an engaged push at the target's centre until first contact, then straight
// into the touched face with the lateral coordinate frozen (no glisser slide).
mouse::XZ heldPushPoint(mouse::NavIntent& in, const tObject& target)
{
    const tObject& h = hero();
    if (in.pushAxis == 0)
    {
        if (target.COL_BY != currentCameraTargetActor)
            return mouse::XZ{ target.roomX, target.roomZ };
        const mouse::XZ here = heroPose().at;
        const bool overlapsX = h.zv.ZVX1 < target.zv.ZVX2 && target.zv.ZVX1 < h.zv.ZVX2;
        const bool overlapsZ = h.zv.ZVZ1 < target.zv.ZVZ2 && target.zv.ZVZ1 < h.zv.ZVZ2;
        bool alongZ;
        if (overlapsX != overlapsZ)
            alongZ = overlapsX;
        else
            alongZ = std::abs(target.roomZ - here.z) >= std::abs(target.roomX - here.x);
        in.pushAxis = alongZ ? 'z' : 'x';
        in.pushLateral = alongZ ? here.x : here.z;
    }
    if (in.pushAxis == 'z')
        return mouse::XZ{ in.pushLateral, target.roomZ };
    return mouse::XZ{ target.roomX, in.pushLateral };
}

// Validate the held target and keep the intent aimed at it. False = cancelled.
bool refreshHeldTarget()
{
    mouse::NavIntent& in = *s_world.intent;
    const int worldIdx = in.targetObject;
    if (worldIdx < 0 || worldIdx >= (int)ListWorldObjets.size())
    {
        cancelIntent();
        return false;
    }
    const int actorIdx = ListWorldObjets[worldIdx].objIndex;
    if (actorIdx < 0 || actorIdx >= NUM_MAX_OBJECT || ListObjets[actorIdx].indexInWorld != worldIdx ||
        !isHoldActionTarget(actorIdx) || ListObjets[actorIdx].room != hero().room)
    {
        cancelIntent();
        return false;
    }
    const tObject& target = ListObjets[actorIdx];
    if (!in.engaged)
    {
        const std::array<int, 11> pose = { target.room, target.roomX, target.roomY, target.roomZ, target.beta,
                                           target.zv.ZVX1, target.zv.ZVX2, target.zv.ZVY1, target.zv.ZVY2,
                                           target.zv.ZVZ1, target.zv.ZVZ2 };
        if (in.hasApproachPose && in.approachPose == pose)
            return true;
        const bool targetMoved = in.hasApproachPose;
        in.hasApproachPose = true;
        in.approachPose = pose;
        auto payload = holdActionApproach(actorIdx);
        if (!payload)
        {
            cancelIntent();
            return false;
        }
        const mouse::XZ dest{ payload->x, payload->z };
        if (targetMoved || in.dest != dest || in.room != payload->room)
        {
            in.dest = dest;
            in.room = payload->room;
            in.planned = false;
            mouse::resetStall(in);
        }
        return true;
    }
    const mouse::XZ point = heldPushPoint(in, target);
    if (in.dest != point || in.room != target.room)
        mouse::resetStall(in);
    in.dest = point;
    in.room = target.room;
    if (in.waypoints.size() > 1)
        in.waypoints.back() = point;
    else
    {
        auto detour = heldContactDetour(actorIdx, point, gridFor(hero().room, agentOf(hero())));
        in.waypoints = detour ? *detour : std::vector<mouse::XZ>{};
        in.waypoints.push_back(point);
    }
    in.planned = true;
    in.pathRoom = hero().room;
    if (kForcePushAnim)
    {
        tObject& h = hero();
        const bool pendingForward = h.newAnim == kPlayerLifeForwardAnim && h.newAnimType == 1 && h.newAnimInfo == -1;
        initHeroAnim(kPlayerPushAnim, 1, -1);
        if (h.ANIM == kPlayerPushAnim && pendingForward)
        {
            // keep an active push pose advancing instead of alternating back to walk
            h.newAnim = -1;
            h.newAnimType = 0;
            h.newAnimInfo = -1;
        }
    }
    return true;
}

void handleArrival()
{
    mouse::NavIntent& in = *s_world.intent;
    const mouse::NavDecision d = s_world.decision;
    if (in.requiresHold)
    {
        if (d.arrived && !d.abandoned && !in.engaged)
        {
            in.engaged = true; // standing at the face: from now on, lean into it
            if (refreshHeldTarget())
            {
                s_world.hasDecision = false;
                localJoyD = 0;
            }
            return;
        }
        cancelIntent(); // pushed as far as it goes, or stuck
        localJoyD = 0;
        return;
    }
    if (d.arrived && pushIntoTarget(in))
    {
        s_world.hasDecision = false;
        localJoyD = 0;
        return;
    }
    const int target = in.targetObject;
    s_world.intent.reset();
    s_world.hasDecision = false;
    localJoyD = 0;
    if (d.arrived && target >= 0)
        dispatchTarget(target);
}

void tickNavigation(uint32_t now)
{
    s_world.hasDecision = false;
    if (!s_world.intent)
        return;
    if (hero().trackMode != 1)
    {
        cancelIntent(); // a script owns the hero: nothing to steer or dispatch
        return;
    }
    mouse::NavIntent& in = *s_world.intent;
    const tObject& h = hero();
    if (in.requiresHold && (g_currentFloor != in.originFloor || h.room != in.originRoom))
    {
        cancelIntent();
        return;
    }
    if (in.requiresHold && !refreshHeldTarget())
        return;
    mouse::NavEnv env;
    env.grid = gridFor(h.room, agentOf(h));
    env.linkMidpoint = [](int from, int to) { return linkMidpoint(from, to); };
    env.reframe = [](mouse::XZ p, int from, int to) { return mouse::reframe(p, originOf(from), originOf(to)); };
    env.capObjet = [](int x1, int z1, int beta, int x2, int z2) { return CapObjet(x1, z1, beta, x2, z2); };
    s_world.decision = mouse::decide(in, heroPose(), env, now, !in.engaged);
    s_world.hasDecision = true;
    localJoyD = s_world.decision.joyd; // LIFE scripts reading the stick see a live one
    if (s_world.decision.arrived || s_world.decision.abandoned)
        handleArrival();
}

mouse::CursorShape shapeFor(mouse::ClickKind kind)
{
    switch (kind)
    {
    case mouse::ClickKind::Walk:
    case mouse::ClickKind::Steer:        return mouse::CursorShape::Default;
    case mouse::ClickKind::Target:
    case mouse::ClickKind::HudInventory:
    case mouse::ClickKind::HudMap:
    case mouse::ClickKind::HudMenu:      return mouse::CursorShape::Pointer;
    case mouse::ClickKind::Attack:       return mouse::CursorShape::Crosshair;
    case mouse::ClickKind::Push:         return mouse::CursorShape::Move;
    default:                             return mouse::CursorShape::NotAllowed;
    }
}

// Resolve what is under the pointer for the cursor; hovering never changes state.
void updateHover()
{
    if (!s_world.hoverPos)
    {
        s_world.hover = {};
        mouseInputRequestCursor(mouse::CursorShape::Default);
        return;
    }
    if (s_world.pointer.held && latchedPush())
        s_world.hover = mouse::ClickResult{ mouse::ClickKind::Push, {} }; // the push cursor sticks while held
    else
        s_world.hover = resolveAt(*s_world.hoverPos);
    mouseInputRequestCursor(shapeFor(s_world.hover.kind));
}

// A room-frame floor point on the logical screen, or nothing.
std::optional<mouse::Vec2> screenOf(int room, mouse::XZ p)
{
    if (!heroAvailable() || !roomValid(room))
        return std::nullopt;
    mouse::Camera camera;
    if (!cameraForRoom(room, &camera))
        return std::nullopt;
    const int floorY = mouse::reframeY(hero().roomY, originOf(hero().room), originOf(room));
    return mouse::projectPoint(camera, p.x, floorY, p.z);
}
}

void mouseWorldTakeOver()
{
    s_worldActive = false;
    s_screenGate.arm();
    releaseAll();
    mouseInputRequestCursor(mouse::CursorShape::Default);
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

void mouseWorldFrame(int allowSystemMenu)
{
    s_allowSystemMenu = allowSystemMenu;
    mouse::Frame frame;
    const bool haveFrame = mouseInputTakeFrame(&frame);

    // Cutscenes and intros: a left click skips exactly like the Action key.
    if (!allowSystemMenu)
    {
        leaveWorld();
        if (menuMouseClicked())
            localClick = 1;
        return;
    }
    if (!g_remasterConfig.controls.mouseGameplay || NumCamera < 0 || !heroAvailable())
    {
        leaveWorld();
        return;
    }
    s_worldActive = true;
    if (frame.blocked)
    {
        releaseAll(); // F1 dialog or an ImGui window owns the mouse
        mouseInputRequestCursor(mouse::CursorShape::Default);
        return;
    }

    const uint32_t now = (uint32_t)SDL_GetTicks();
    const int camera = NumCamera;
    const mouse::Resolver resolve = [](mouse::Point p) { return resolveAt(p); };

    // A script took the hero (cutscene, death, scripted walk), or the floor changed.
    if (s_world.intent && hero().trackMode != 1)
        cancelIntent();
    if (s_world.intentFloor != g_currentFloor)
    {
        if (s_world.pointer.held)
            mouse::rebase(s_world.pointer);
        cancelIntent();
        s_world.intentFloor = g_currentFloor;
    }

    for (const mouse::Event& e : frame.events)
    {
        const std::optional<mouse::Point> pos = e.inside ? std::optional<mouse::Point>(e.pos) : std::nullopt;
        switch (e.type)
        {
        case mouse::EventType::Motion:
            s_world.lastInputMouse = true;
            mouse::onMove(s_world.pointer, pos);
            break;
        case mouse::EventType::Down:
            s_world.lastInputMouse = true;
            mouse::onPress(s_world.pointer, pos);
            if (pos)
                applyDecision(mouse::pressDecision(s_world.pointer, *pos, e.clicks, camera, resolve, latchedPush()));
            break;
        case mouse::EventType::Up:
            endPointerHold();
            break;
        case mouse::EventType::FocusLost:
            releaseAll();
            break;
        }
    }
    // A release SDL never delivered: the button is up now.
    if (haveFrame && s_world.pointer.held && !frame.leftDown)
        endPointerHold();

    const std::optional<mouse::Point> pointerNow =
        haveFrame ? (frame.inside ? std::optional<mouse::Point>(frame.pos) : std::nullopt) : s_world.pointer.pos;

    // Held pointer follow: once per frame, re-resolving only when it moved.
    if (s_world.pointer.held)
        applyDecision(mouse::holdDecision(s_world.pointer, pointerNow, camera, resolve, latchedPush(),
                                          s_world.intent.has_value()));

    // Every walk is hold-bound.
    if (s_world.intent && !s_world.pointer.held)
        cancelIntent();

    tickNavigation(now);
    s_world.hoverPos = pointerNow;
    updateHover();
}

void mouseWorldKeyboardTookOver()
{
    s_world.lastInputMouse = false;
    if (s_world.intent || s_world.attackTarget >= 0)
    {
        cancelIntent();
        clearAttack();
    }
    if (s_world.pointer.held)
        s_world.pointer.spent = true; // no follow resumes on this hold
}

bool mouseNavSteer(tObject* actor)
{
    if (!s_worldActive || !heroAvailable() || actor != &hero())
        return false;
    if (!s_world.intent)
        return false;
    if (!s_world.hasDecision || !s_world.decision.advance)
    {
        actor->speed = 0; // instant stop, like this fork's tank controls
        actor->direction = 0;
        actor->rotate.numSteps = 0;
        return true;
    }
    // Follow mode's turn toward a point (track.cpp case 2), aimed at the waypoint.
    const int angle = CapObjet(actor->roomX + actor->stepX, actor->roomZ + actor->stepZ, actor->beta,
                               s_world.decision.target.x, s_world.decision.target.z);
    if (actor->rotate.numSteps == 0 || actor->direction != angle)
        InitRealValue(actor->beta, actor->beta - (angle * 256), 60, &actor->rotate);
    actor->direction = angle;
    if (angle == 0)
        actor->rotate.numSteps = 0;
    else
        actor->beta = updateActorRotation(&actor->rotate);
    actor->speed = s_world.decision.run ? 5 : 4; // 5 is FITD's run speed
    return true;
}

bool mouseWorldHudState(MouseHudState* out)
{
    *out = MouseHudState{};
    if (!s_worldActive || !s_allowSystemMenu)
        return false;
    out->visible = true;
    for (int i = 0; i < mouse::kHudIconCount; ++i)
        out->iconEnabled[i] = hudIconAllowed((mouse::HudIcon)i);
    if (s_world.hoverPos)
    {
        if (auto icon = mouse::hudIconAt(*s_world.hoverPos))
            out->hoverIcon = (int)*icon;
        out->hasPointer = true;
        out->pointerX = s_world.hoverPos->x;
        out->pointerY = s_world.hoverPos->y;
    }
    out->held = s_world.pointer.held;
    out->settling = mouse::settling(s_world.pointer);
    if (s_world.intent && !s_world.intent->steering)
    {
        if (auto s = screenOf(s_world.intent->room, s_world.intent->dest))
        {
            out->hasDestination = true;
            out->destX = (float)s->x;
            out->destY = (float)s->y;
        }
    }
    const mouse::ClickKind k = s_world.hover.kind;
    if (!s_world.pointer.held && !out->hasDestination && (k == mouse::ClickKind::Walk || k == mouse::ClickKind::Target))
    {
        if (auto s = screenOf(s_world.hover.payload.room, mouse::XZ{ s_world.hover.payload.x, s_world.hover.payload.z }))
        {
            out->hasPreview = true;
            out->previewX = (float)s->x;
            out->previewY = (float)s->y;
        }
    }
    return true;
}

bool mouseWorldWantsCursor()
{
    return g_remasterConfig.controls.mouseGameplay && s_world.lastInputMouse;
}
