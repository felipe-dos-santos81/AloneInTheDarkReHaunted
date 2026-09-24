///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: holding on scenery to push it (port of m-aitd
// playworld/held_push.py). A held push never asserts Action.
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "menuMouse.h"
#include "mouseWorldInternal.h"

#include <algorithm>
#include <cstdlib>

namespace mouseworld
{
namespace
{
constexpr int kPlayerPushAnim = 5;          // AITD1 hero push animation

constexpr int kPlayerLifeForwardAnim = 254; // m-aitd's value; unverified here until checklist row 29

constexpr bool kForcePushAnim = true;       // m-aitd's value; unverified here until checklist row 29

TargetPose poseOf(const tObject& t)
{
    return TargetPose{ t.room, t.roomX, t.roomY, t.roomZ, t.beta, t.zv };
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
mouse::XZ heldPushPoint(const tObject& target)
{
    const tObject& h = hero();
    HeldPush& push = g_world.push;
    if (push.axis == PushAxis::None)
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
        push.axis = alongZ ? PushAxis::Z : PushAxis::X;
        push.lateral = alongZ ? here.x : here.z;
    }
    if (push.axis == PushAxis::Z)
        return mouse::XZ{ push.lateral, target.roomZ };
    return mouse::XZ{ target.roomX, push.lateral };
}
}

// Validate the held target and keep the intent aimed at it. False = cancelled.
bool refreshHeldTarget()
{
    mouse::NavIntent& in = *g_world.intent;
    const int worldIdx = in.targetObject;
    const tWorldObject* w = worldObject(worldIdx);
    if (!w)
    {
        cancelIntent();
        return false;
    }
    const int actorIdx = w->objIndex;
    if (actorIdx < 0 || actorIdx >= NUM_MAX_OBJECT || ListObjets[actorIdx].indexInWorld != worldIdx ||
        !isHoldActionTarget(actorIdx) || ListObjets[actorIdx].room != hero().room)
    {
        cancelIntent();
        return false;
    }
    const tObject& target = ListObjets[actorIdx];
    if (!in.engaged)
    {
        const TargetPose pose = poseOf(target);
        if (g_world.push.approachPose == pose)
            return true;
        const bool targetMoved = g_world.push.approachPose.has_value();
        g_world.push.approachPose = pose;
        auto payload = holdActionApproach(actorIdx);
        if (!payload)
        {
            cancelIntent();
            return false;
        }
        const mouse::XZ dest{ payload->x, payload->z };
        if (targetMoved || in.dest != dest || in.room != payload->room)
            retarget(in, dest, payload->room);
        return true;
    }
    const mouse::XZ point = heldPushPoint(target);
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

} // namespace mouseworld
