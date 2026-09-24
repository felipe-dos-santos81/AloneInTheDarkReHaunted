///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: what a click at a pixel would do. One resolver drives both
// the cursor and the click (AGENTS.md).
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "menuMouse.h"
#include "mouseWorldInternal.h"

#include <cmath>
#include <cstdlib>
#include <iterator>

namespace mouseworld
{
namespace
{
bool isInteractable(int idx)
{
    const tObject& a = ListObjets[idx];
    const tWorldObject* w = worldObject(a.indexInWorld);
    if (!w)
        return false;
    if (a.objectType & AF_FOUNDABLE)
        return true;
    return w->foundLife != -1;
}

// What each HUD icon opens: its click kind and the key PlayWorld already
// handles for it. Indexed by mouse::HudIcon.
struct HudIconAction
{
    mouse::ClickKind kind;
    int key;
};

constexpr HudIconAction kHudIconActions[mouse::kHudIconCount] = {
    { mouse::ClickKind::HudInventory, 0x1C }, // Enter
    { mouse::ClickKind::HudMap, 0x0F },       // Tab
    { mouse::ClickKind::HudMenu, 0x1B },      // Esc
};

constexpr ClickKindInfo kClickKinds[] = {
    { "blocked", mouse::CursorShape::NotAllowed },
    { "walk", mouse::CursorShape::Default },
    { "steer", mouse::CursorShape::Default },
    { "target", mouse::CursorShape::Pointer },
    { "push", mouse::CursorShape::Move },
    { "attack", mouse::CursorShape::Crosshair },
    { "hud:inventory", mouse::CursorShape::Pointer },
    { "hud:map", mouse::CursorShape::Pointer },
    { "hud:menu", mouse::CursorShape::Pointer },
};

static_assert(std::size(kClickKinds) == (size_t)mouse::ClickKind::HudMenu + 1, "one entry per ClickKind");

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

// Stand next to an interactable object, on the side the hero comes from: the
// nearest cell beside it that the hero can reach and the player can see, else
// any reachable one, else the object's centre.
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
        // Reachability is known only in the hero's own room (or not at all when
        // the hero stands off the grid); the next room is entered by its door.
        const mouse::Reach* reach = h.room == t.room ? reachFor(*grid, from) : nullptr;
        auto reachable = [&](mouse::XZ c) { return !reach || reach->contains(c); };
        mouse::Camera camera;
        const bool haveCamera = cameraForRoom(t.room, &camera);
        const int floorY = mouse::reframeY(h.roomY, originOf(h.room), originOf(t.room));
        auto reachableAndSeen = [&](mouse::XZ c) {
            return reachable(c) && haveCamera && visibleAt(camera, c, floorY).has_value();
        };
        if (auto spot = mouse::approachCell(*grid, dest, from, reachableAndSeen))
            dest = *spot;
        else if (auto reachableSpot = mouse::approachCell(*grid, dest, from, reachable))
            dest = *reachableSpot;
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
                auto s = visibleAt(camera, c, floorY);
                return s && std::fabs(s->x - p.x) <= mouse::kSnapBudgetPx && std::fabs(s->y - p.y) <= mouse::kSnapBudgetPx;
            });
            if (!snapped)
                return steerToward(p);
            dest = *snapped;
        }
        return mouse::ClickResult{ mouse::ClickKind::Walk, mouse::Payload{ dest.x, dest.z, room, -1 } };
    }
    return steerToward(p);
}
}

bool isCombatTarget(int idx)
{
    if (idx < 0 || idx >= NUM_MAX_OBJECT || idx == currentCameraTargetActor)
        return false;
    const tObject& a = ListObjets[idx];
    return a.indexInWorld >= 0 && (a.objectType & AF_ANIMATED);
}

// The world object at `worldIdx`, or null when the index is out of range.
const tWorldObject* worldObject(int worldIdx)
{
    if (worldIdx < 0 || worldIdx >= (int)ListWorldObjets.size())
        return nullptr;
    return &ListWorldObjets[worldIdx];
}

// Scripted or movable scenery that is not picked up: held to push. AITD1 only
// (the push animation numbers are AITD1's).
bool isHoldActionTarget(int idx)
{
    if (g_gameId != AITD1 || idx < 0 || idx >= NUM_MAX_OBJECT || idx == currentCameraTargetActor)
        return false;
    const tObject& a = ListObjets[idx];
    const tWorldObject* w = worldObject(a.indexInWorld);
    if (!w || a.bodyNum == -1 || !(a.dynFlags & 1))
        return false;
    if (w->objIndex != idx || w->stage != g_currentFloor)
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
    if (!g_world.allowSystemMenu)
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

const ClickKindInfo& kindInfo(mouse::ClickKind kind)
{
    return kClickKinds[(size_t)kind];
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
    const mouse::XZ candidates[4] = {
        { t.zv.ZVX1 - clearance, std::clamp(from.z, t.zv.ZVZ1, t.zv.ZVZ2) },
        { t.zv.ZVX2 + clearance, std::clamp(from.z, t.zv.ZVZ1, t.zv.ZVZ2) },
        { std::clamp(from.x, t.zv.ZVX1, t.zv.ZVX2), t.zv.ZVZ1 - clearance },
        { std::clamp(from.x, t.zv.ZVX1, t.zv.ZVX2), t.zv.ZVZ2 + clearance },
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

// What a click at p would do. One resolver behind both the cursor and the click.
mouse::ClickResult resolveAt(mouse::Point p)
{
    if (NumCamera < 0 || !heroAvailable())
        return {};
    if (auto icon = mouse::hudIconAt(p))
    {
        if (!hudIconAllowed(*icon))
            return {};
        return mouse::ClickResult{ kHudIconActions[(size_t)*icon].kind, {} };
    }
    if (hero().trackMode != 1)
        return {}; // a script walks the hero: nothing to walk to or strike (the HUD still works)
    int actor = pickActorAt(p, false);
    if (actor < 0)
        actor = pickActorAt(p, true);
    if (actor >= 0 && isCombatTarget(actor))
    {
        if (!canStrike(true))
            return {}; // aimed at the enemy: never a fall-through to a walk
        return mouse::ClickResult{ mouse::ClickKind::Attack, mouse::Payload{ 0, 0, -1, -1, actor } };
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
    const tObject& h = hero();
    if (h.screenXMax >= 0 && h.screenYMax >= 0 &&
        mouse::contains(mouse::Rect{ h.screenXMin, h.screenYMin, h.screenXMax, h.screenYMax }, p))
        return {}; // on the hero: blocked, never the floor behind the hero
    return pickFloorOrSteer(p);
}

int hudKeyFor(mouse::ClickKind kind)
{
    for (const HudIconAction& action : kHudIconActions)
        if (action.kind == kind)
            return action.key;
    return 0;
}

} // namespace mouseworld
