///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: what a click at a pixel would do. One resolver drives both
// the cursor and the click (AGENTS.md).
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "menuMouse.h"
#include "aitd1Inventory.h"
#include "mouseWorldInternal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iterator>

namespace mouseworld
{
bool g_traceResolve = false;

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
    { "exit", mouse::CursorShape::Exit },
    { "on hero", mouse::CursorShape::NotAllowed },
    { "hud:inventory", mouse::CursorShape::Pointer },
    { "hud:map", mouse::CursorShape::Pointer },
    { "hud:menu", mouse::CursorShape::Pointer },
};

static_assert(std::size(kClickKinds) == (size_t)mouse::ClickKind::HudMenu + 1, "one entry per ClickKind");

// How far outside an actor's outline the forgiving pick still reaches: half
// the forgiving box's minimum size.
constexpr double kOutlineSlackPx = mouse::kHitMinimum / 2.0;

// Whether p can show the actor. A static body draws only inside the outline of
// its box as posed, which for a body seen edge-on near the camera (a door
// standing open beside it) is a sliver of its screen box. Animated bodies,
// posed bone by bone, and sprites keep the screen box alone.
bool onActorOutline(const tObject& a, mouse::Point p, double slack)
{
    if (a.objectType & (AF_SPECIAL | AF_OBJ_2D))
        return true;
    const sBody* body = HQR_Get(HQ_Bodys, a.bodyNum);
    mouse::Camera camera;
    if (!body || (body->m_flags & INFO_ANIM) || !cameraForRoom(currentRoom, &camera))
        return true;
    // Where AffObjet draws it: world coordinates are the on-screen room's frame.
    const mouse::Box rest{ body->m_zv.ZVX1, body->m_zv.ZVX2, body->m_zv.ZVY1,
                           body->m_zv.ZVY2, body->m_zv.ZVZ1, body->m_zv.ZVZ2 };
    const mouse::Box posed = mouse::posedBox(rest, a.alpha, a.beta, a.gamma, a.worldX + a.stepX, a.worldY + a.stepY,
                                             a.worldZ + a.stepZ, cosTable);
    return mouse::outlineContains(mouse::clippedBoxOutline(camera, posed), p, slack);
}

// Topmost drawn actor whose screen box (exact, or forgiving) holds p on its outline.
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
        {
            const bool onOutline = onActorOutline(a, p, forgiving ? kOutlineSlackPx : 0.0);
            MTRACE("  actor %s idx=%d %s box=(%d,%d)-(%d,%d) world=%d body=%d type=0x%x life=%d room=%d at=(%d,%d,%d) beta=%d\n",
                   onOutline ? "hit" : "box only, off its outline:", idx, forgiving ? "forgiving" : "exact", a.screenXMin,
                   a.screenYMin, a.screenXMax, a.screenYMax, a.indexInWorld, a.bodyNum, a.objectType, a.life, a.room, a.roomX,
                   a.roomY, a.roomZ, a.beta);
            if (onOutline)
                return idx;
        }
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
    MTRACE("  -> target actor=%d dest=(%d,%d) room=%d\n", actorIdx, dest.x, dest.z, t.room);
    return mouse::ClickResult{ mouse::ClickKind::Target, mouse::Payload{ dest.x, dest.z, t.room, t.indexInWorld } };
}

// The floor under a pixel, in the hero's room or a room the on-screen camera
// also views, as the floor pick finds it (before snapping to the walk grid).
struct FloorHit
{
    int room;
    int floorY;   // the hero's floor height in `room`'s frame
    mouse::XZ at; // in `room`'s frame
};

std::optional<FloorHit> floorUnder(mouse::Point p)
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
        const mouse::Grid* grid = gridFor(room, agentIn(room));
        auto hit = mouse::pickFloor(*fits, p, [&](mouse::XZ c) { return grid && grid->isSeam(c.x, c.z); });
        MTRACE("  floor room=%d y=%d pick=%s(%d,%d)\n", room, floorY, hit ? "" : "none", hit ? hit->x : 0, hit ? hit->z : 0);
        if (hit)
            return FloorHit{ room, floorY, *hit };
    }
    return std::nullopt;
}

constexpr u32 kSceZoneFloorChange = 10; // GereDec's "stage" zone

// A floor-change zone whose outline holds p: walk into it. The outline is the
// zone standing on the hero's floor, at least hero-high, so it answers even
// when a wall hides the zone's floor (the attic stairwell behind the pillar of
// camera 4, the only camera filming it). Floor drawn in front of all of it
// still means that floor.
std::optional<mouse::ClickResult> exitAt(mouse::Point p)
{
    const tObject& h = hero();
    const tWorldObject* w = worldObject(h.indexInWorld);
    if (!w || w->floorLife == -1)
        return std::nullopt; // GereDec ignores the zone for an actor with no floor life
    const mouse::Grid* grid = gridFor(h.room, agentOf(h));
    mouse::Camera camera;
    if (!grid || !cameraForRoom(h.room, &camera))
        return std::nullopt;
    const mouse::XZ from = heroPose().at;
    const int feetY = h.roomY + h.stepY; // the height GereDec tests the zone at
    const std::vector<sceZoneStruct>& zones = roomDataTable[h.room].sceZoneTable;
    auto zoneBox = [](const sceZoneStruct& z) {
        return mouse::Box{ z.zv.ZVX1, z.zv.ZVX2, z.zv.ZVY1, z.zv.ZVY2, z.zv.ZVZ1, z.zv.ZVZ2 };
    };
    for (size_t i = 0; i < zones.size(); ++i)
    {
        const sceZoneStruct& zone = zones[i];
        if (zone.type != kSceZoneFloorChange || feetY < zone.zv.ZVY1 || feetY > zone.zv.ZVY2)
            continue; // not a floor change, or never reached at the hero's height
        const mouse::Box outline{ zone.zv.ZVX1, zone.zv.ZVX2, std::min(zone.zv.ZVY1, h.zv.ZVY1), feetY,
                                  zone.zv.ZVZ1, zone.zv.ZVZ2 };
        if (!mouse::boxSilhouetteContains(camera, outline, p))
            continue;
        if (auto floor = floorUnder(p))
        {
            const mouse::XZ at = mouse::reframe(floor->at, originOf(floor->room), originOf(h.room));
            if (mouse::nearerThanBox(camera, outline, at.x, h.roomY, at.z))
                continue; // floor drawn in front of the exit
        }
        const mouse::Reach* reach = reachFor(*grid, from);
        auto spot = mouse::zoneCell(*grid, outline, from, [&](mouse::XZ c) {
            if (reach && !reach->contains(c))
                return false;
            // AITD1's GereDec stops at the first zone the hero stands in.
            for (size_t k = 0; g_gameId == AITD1 && k < i; ++k)
                if (mouse::contains(zoneBox(zones[k]), c.x, feetY, c.z))
                    return false;
            return true;
        });
        MTRACE("  exit zone hit (%d..%d, %d..%d) spot=%s(%d,%d)\n", outline.x1, outline.x2, outline.z1, outline.z2,
               spot ? "" : "none", spot ? spot->x : 0, spot ? spot->z : 0);
        if (spot)
            return mouse::ClickResult{ mouse::ClickKind::Exit, mouse::Payload{ spot->x, spot->z, h.room, -1 } };
    }
    return std::nullopt;
}

// What the hero holds: bare hands (the Actions object, or nothing), a weapon,
// or another object Action uses.
mouse::Hand heldHand()
{
    if (currentInventory < 0 || currentInventory >= NUM_MAX_INVENTORY)
        return mouse::Hand::BareHands;
    const int inHand = inHandTable[currentInventory];
    if (inHand < 0 || inHand == kAitd1ActionsObject)
        return mouse::Hand::BareHands;
    const tWorldObject* w = worldObject(inHand);
    return w && isAitd1WeaponFoundLife(w->foundLife) ? mouse::Hand::Weapon : mouse::Hand::Object;
}

} // namespace

// The inventory action armed for the Action key (AITD1 only).
int armedAction()
{
    if (g_gameId != AITD1 || !vars)
        return mouse::kArmedNothing;
    return mouse::armedActionFor(heldHand(), vars[kAitd1ArmedActionVar]);
}

namespace
{

// Furniture painted into the background (a type-9 hard col) whose outline holds
// p, while an action other than push is armed: walk beside it, touch it and send
// the armed action, as the keyboard's walk into it and press Action. Its scripts
// (the attic's lives 7 and 8) watch the hero's HARD_COL, not an actor. Floor
// drawn in front of all of it still means that floor.
std::optional<mouse::ClickResult> furnitureAt(mouse::Point p)
{
    if (mouse::sceneryUse(armedAction()) != mouse::SceneryUse::TouchAction)
        return std::nullopt;
    const tObject& h = hero();
    const mouse::Grid* grid = gridFor(h.room, agentOf(h));
    mouse::Camera camera;
    if (!grid || !cameraForRoom(h.room, &camera))
        return std::nullopt;
    const mouse::XZ from = heroPose().at;
    for (const hardColStruct& col : roomDataTable[h.room].hardColTable)
    {
        if (col.type != kHardColScenario)
            continue;
        const mouse::Box box{ col.zv.ZVX1, col.zv.ZVX2, col.zv.ZVY1, col.zv.ZVY2, col.zv.ZVZ1, col.zv.ZVZ2 };
        if (!mouse::boxSilhouetteContains(camera, box, p))
            continue;
        if (auto floor = floorUnder(p))
        {
            const mouse::XZ at = mouse::reframe(floor->at, originOf(floor->room), originOf(h.room));
            if (mouse::nearerThanBox(camera, box, at.x, h.roomY, at.z))
                continue; // floor drawn in front of the furniture
        }
        // Stand beside the face nearest the hero, where it can reach.
        const mouse::XZ face{ std::clamp(from.x, box.x1, box.x2), std::clamp(from.z, box.z1, box.z2) };
        const mouse::Reach* reach = reachFor(*grid, from);
        auto spot = mouse::approachCell(*grid, face, from, [&](mouse::XZ c) { return !reach || reach->contains(c); });
        MTRACE("  furniture zone %d (%d..%d, %d..%d) spot=%s(%d,%d)\n", (int)col.parameter, box.x1, box.x2, box.z1,
               box.z2, spot ? "" : "none", spot ? spot->x : 0, spot ? spot->z : 0);
        if (!spot)
            continue;
        mouse::Payload payload{ spot->x, spot->z, h.room, -1 };
        payload.zone = (int)col.parameter;
        return mouse::ClickResult{ mouse::ClickKind::Target, payload };
    }
    return std::nullopt;
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
    {
        MTRACE("  -> steer: no bearing (feet off screen or pointer on hero)\n");
        return {}; // the hero's feet are off screen, or the pointer is on the hero
    }
    {
        const double dx = target->x - heroPose().at.x, dz = target->z - heroPose().at.z, n = std::hypot(dx, dz);
        MTRACE("  -> steer hero=(%d,%d) target=(%d,%d) dir=(%.3f,%.3f)\n", heroPose().at.x, heroPose().at.z, target->x,
               target->z, dx / n, dz / n);
    }
    return mouse::ClickResult{ mouse::ClickKind::Steer, mouse::Payload{ target->x, target->z, h.room, -1 } };
}

constexpr int kHardColRoomLink = 4; // the lintel over a doorway; its parameter is the room beyond (getRoomLink)

// A doorway into another room whose opening holds p: walk through it to the
// first walkable cell beyond. The opening is the lintel's footprint from the
// hero's floor up to the lintel. Asked only where no floor shows: in the
// dressing room the dresser hides the doorway's own floor and the opening
// above it shows only the bedroom's walls.
std::optional<mouse::ClickResult> doorwayAt(mouse::Point p)
{
    const tObject& h = hero();
    mouse::Camera camera;
    if (!cameraForRoom(h.room, &camera))
        return std::nullopt;
    for (const hardColStruct& col : roomDataTable[h.room].hardColTable)
    {
        const int to = (int)col.parameter;
        if (col.type != kHardColRoomLink || to == h.room || !roomValid(to) || col.zv.ZVY2 >= h.roomY)
            continue;
        const mouse::Box opening{ col.zv.ZVX1, col.zv.ZVX2, col.zv.ZVY2, h.roomY, col.zv.ZVZ1, col.zv.ZVZ2 };
        if (!mouse::boxSilhouetteContains(camera, opening, p))
            continue;
        const mouse::Grid* grid = gridFor(to, agentIn(to));
        if (!grid)
            continue;
        const mouse::XZ middle{ (opening.x1 + opening.x2) / 2, (opening.z1 + opening.z2) / 2 };
        auto spot = mouse::nearestWalkable(*grid, mouse::reframe(middle, originOf(h.room), originOf(to)));
        MTRACE("  doorway to room %d (%d..%d, %d..%d) spot=%s(%d,%d)\n", to, opening.x1, opening.x2, opening.z1, opening.z2,
               spot ? "" : "none", spot ? spot->x : 0, spot ? spot->z : 0);
        if (spot)
            return mouse::ClickResult{ mouse::ClickKind::Walk, mouse::Payload{ spot->x, spot->z, to, -1 } };
    }
    return std::nullopt;
}

mouse::ClickResult pickFloorOrSteer(mouse::Point p)
{
    auto floor = floorUnder(p);
    if (!floor)
    {
        if (auto doorway = doorwayAt(p))
            return *doorway;
        return steerToward(p);
    }
    mouse::XZ dest = floor->at;
    const mouse::Grid* grid = gridFor(floor->room, agentIn(floor->room));
    if (grid && grid->any())
    {
        mouse::Camera camera;
        if (!cameraForRoom(floor->room, &camera))
            return steerToward(p);
        auto snapped = mouse::nearestWalkable(*grid, dest, 6, [&](mouse::XZ c) {
            auto s = visibleAt(camera, c, floor->floorY);
            return s && std::fabs(s->x - p.x) <= mouse::kSnapBudgetPx && std::fabs(s->y - p.y) <= mouse::kSnapBudgetPx;
        });
        if (!snapped)
        {
            MTRACE("  floor pick has no walkable cell within the snap budget\n");
            return steerToward(p);
        }
        dest = *snapped;
    }
    MTRACE("  -> walk dest=(%d,%d) room=%d\n", dest.x, dest.z, floor->room);
    return mouse::ClickResult{ mouse::ClickKind::Walk, mouse::Payload{ dest.x, dest.z, floor->room, -1 } };
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
    MTRACE("resolve logical=(%d,%d) camera=%d hero room=%d at=(%d,%d) beta=%d armed=%d\n", p.x, p.y, NumCamera, hero().room,
           heroPose().at.x, heroPose().at.z, hero().beta, armedAction());
    if (hero().trackMode != 1)
        return {}; // a script walks the hero: nothing to walk to or strike (the HUD still works)
    int actor = pickActorAt(p, false);
    if (actor < 0)
        actor = pickActorAt(p, true);
    if (actor >= 0 && isCombatTarget(actor))
    {
        MTRACE("  -> combat target %d (canStrike=%d)\n", actor, (int)canStrike(true));
        if (!canStrike(true) && !punchAimedAt(actor))
            return {}; // aimed at the enemy: never a fall-through to a walk
        return mouse::ClickResult{ mouse::ClickKind::Attack, mouse::Payload{ 0, 0, -1, -1, actor } };
    }
    if (actor >= 0 && !isInteractable(actor))
    {
        if (isHoldActionTarget(actor) && mouse::sceneryUse(armedAction()) == mouse::SceneryUse::TouchAction)
        {
            MTRACE("  scripted scenery %d with action %d armed: touch it and send Action\n", actor, armedAction());
            return targetFor(actor); // the contact sends Action on the touch
        }
        if (isHoldActionTarget(actor))
            if (auto payload = holdActionApproach(actor))
            {
                MTRACE("  -> push actor=%d stand=(%d,%d)\n", actor, payload->x, payload->z);
                return mouse::ClickResult{ mouse::ClickKind::Push, *payload };
            }
        MTRACE("  actor %d is inert scenery: falls through\n", actor);
        actor = -1; // inert scenery: the pixel means what the floor behind it means
    }
    if (actor >= 0)
        return targetFor(actor);
    const tObject& h = hero();
    if (h.screenXMax >= 0 && h.screenYMax >= 0 &&
        mouse::contains(mouse::Rect{ h.screenXMin, h.screenYMin, h.screenXMax, h.screenYMax }, p))
    {
        MTRACE("  -> on the hero\n");
        return mouse::ClickResult{ mouse::ClickKind::OnHero, {} }; // never the floor behind the hero
    }
    if (auto furniture = furnitureAt(p))
        return *furniture;
    if (auto exit = exitAt(p))
        return *exit;
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
