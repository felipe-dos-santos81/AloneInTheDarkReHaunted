///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: what the engine adapter's files (mouseWorld*.cpp) share.
// Nothing outside them includes this; the engine talks to mouseWorld.h.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstdint>
#include <optional>
#include <tuple>
#include <vector>

#include "common.h"
#include "mouseGesture.h"
#include "mouseHudLayout.h"
#include "mouseNav.h"
#include "mousePick.h"

namespace mouseworld
{

// ---- the engine's rooms, cameras and hero (mouseWorldGeometry.cpp) ----------

bool roomValid(int room);
mouse::RoomOrigin originOf(int room);
// The actor's rotation-invariant footprint and Y band.
mouse::Agent agentOf(const tObject& actor);
// The hero's footprint with its Y band re-framed into `room`.
mouse::Agent agentIn(int room);
// `room`'s walk grid for `agent` (cached), or null without cover zones.
const mouse::Grid* gridFor(int room, const mouse::Agent& agent);
// The cells reachable from `from` on `grid` (cached), or null off the grid.
const mouse::Reach* reachFor(const mouse::Grid& grid, mouse::XZ from);
// Floor-camera index of the camera on screen, or -1.
int currentFloorCamera();
// The on-screen camera framed in `room`'s coordinate space.
bool cameraForRoom(int room, mouse::Camera* out);
// Plane fits of `room`'s floor at height floorY under the on-screen camera (cached).
const std::vector<mouse::PolyFit>* fitsFor(int room, int floorY);
// Where a room-frame floor point shows on the 320x200 screen, when it does.
std::optional<mouse::Vec2> visibleAt(const mouse::Camera& camera, mouse::XZ p, int floorY);
// A room-frame floor point on the logical screen, on or off it, or nothing.
std::optional<mouse::Vec2> screenOf(int room, mouse::XZ p);
// The doorway midpoint linking `from` to `to`, in from's frame.
mouse::XZ linkMidpoint(int from, int to);
// Floor data changed: drop the grids, reachability and plane fits.
void clearGeometryCaches();

bool heroAvailable();
tObject& hero();
mouse::HeroPose heroPose();

// ---- what a click means (mouseWorldResolve.cpp) -----------------------------

// The cursor a click kind shows and its debug-overlay name.
struct ClickKindInfo
{
    const char* name;
    mouse::CursorShape cursor;
};
const ClickKindInfo& kindInfo(mouse::ClickKind kind);

// The world object at `worldIdx`, or null when the index is out of range.
const tWorldObject* worldObject(int worldIdx);
bool isCombatTarget(int idx);
// Scripted or movable scenery that is not picked up: held to push (AITD1 only).
bool isHoldActionTarget(int idx);
// A swing needs something in hand, and an idle hero to start one.
bool canStrike(bool requireIdle);
bool hudIconAllowed(mouse::HudIcon icon);
// The key PlayWorld already handles for a HUD click kind.
int hudKeyFor(mouse::ClickKind kind);
// Where to stand to push `targetIdx`, or nothing.
std::optional<mouse::Payload> holdActionApproach(int targetIdx);
// What a click at p would do. Hovering never changes game state.
mouse::ClickResult resolveAt(mouse::Point p);

// ---- the live mouse state (mouseWorld.cpp) ----------------------------------

enum class PushAxis : uint8_t
{
    None,
    X,
    Z,
};

// A pushed object's place, shape and heading, to notice when it moves.
struct TargetPose
{
    int room = -1;
    int x = 0;
    int y = 0;
    int z = 0;
    int beta = 0;
    ZVStruct zv{};
};

inline bool operator==(const TargetPose& a, const TargetPose& b)
{
    return std::tie(a.room, a.x, a.y, a.z, a.beta, a.zv.ZVX1, a.zv.ZVX2, a.zv.ZVY1, a.zv.ZVY2, a.zv.ZVZ1, a.zv.ZVZ2) ==
           std::tie(b.room, b.x, b.y, b.z, b.beta, b.zv.ZVX1, b.zv.ZVX2, b.zv.ZVY1, b.zv.ZVY2, b.zv.ZVZ1, b.zv.ZVZ2);
}

// A held push's own state, beside its NavIntent.
struct HeldPush
{
    int originRoom = -1;                    // the push ends if the hero leaves it
    std::optional<TargetPose> approachPose; // the target as its approach was planned
    PushAxis axis = PushAxis::None;         // the touched face's axis, fixed at first contact
    int lateral = 0;                        // the coordinate frozen across that axis
};

struct World
{
    mouse::PointerState pointer;
    std::optional<mouse::NavIntent> intent;
    HeldPush push; // meaningful while intent->requiresHold
    std::optional<mouse::NavDecision> decision; // this frame's steering for the intent
    int attackTarget = -1;
    uint32_t attackStartMs = 0;
    int attackFrames = 0;
    bool actionSent = false; // the live intent's touched object got its Action
    bool lastInputMouse = false;
    int intentFloor = -1;
    int allowSystemMenu = 0; // PlayWorld's allowSystemMenu this frame
    mouse::ClickResult hover;
    std::optional<mouse::Point> hoverPos;
    bool wroteJoyD = false; // localJoyD was written by the mouse this frame
};

extern World g_world;

// Write localJoyD and record that the mouse drove it this frame.
void setJoyD(int value);
// Run InitAnim on the hero.
void initHeroAnim(int anim, int type, int info);
// Stop an actor dead: no step, no turn in progress.
void haltActor(tObject& a);
// Stop the hero where it stands, back in its stand pose when that is safe.
void stopHero();
void cancelIntent();
// Aim a live intent somewhere new: plan afresh and forget stall progress.
void retarget(mouse::NavIntent& in, mouse::XZ dest, int room);

// ---- held push (mouseWorldPush.cpp) -----------------------------------------

// Validate the held target and keep the intent aimed at it. False = cancelled.
bool refreshHeldTarget();

// ---- melee (mouseWorldAttack.cpp) -------------------------------------------

void clearAttack();
// Accept a click on an enemy: stop, face it, and hold Action until the swing ends.
void armAttack(int actorIdx);
// One frame of FITD's own melee input for an accepted click. False when none.
bool tickAttack(uint32_t now);

} // namespace mouseworld
