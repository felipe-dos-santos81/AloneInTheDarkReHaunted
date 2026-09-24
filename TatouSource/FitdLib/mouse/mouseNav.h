///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: walk grid over the camera cover zones, A* and the per-frame
// steering decision. Engine-free: standard headers only.
// Port of m-aitd engine/nav/navmesh.py and navigate.py.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "mousePoly.h"
#include "mouseTypes.h"

namespace mouse
{

// A hard-col box (room-scale units, the room's frame).
struct Box
{
    int x1 = 0;
    int x2 = 0;
    int y1 = 0;
    int y2 = 0;
    int z1 = 0;
    int z2 = 0;
};

// The hero's rotation-invariant footprint: larger horizontal half-extent and Y band.
struct Agent
{
    int half = 0;
    int y1 = 0;
    int y2 = 0;
};

constexpr int kGridStep = 100; // room units per cell

struct Grid
{
    int x0 = 0;
    int z0 = 0;
    int step = kGridStep;
    int nx = 0;
    int nz = 0;
    std::vector<uint8_t> walk; // nx * nz, index i * nz + j

    bool at(int i, int j) const
    {
        return i >= 0 && j >= 0 && i < nx && j < nz && walk[(size_t)i * nz + j] != 0;
    }
    XZ center(int i, int j) const { return XZ{ x0 + i * step, z0 + j * step }; }
    // Column / row whose centre is nearest x / z; may lie off the grid.
    int column(int x) const;
    int row(int z) const;
    bool cellOf(int x, int z, int* i, int* j) const;
    // A move from (i, j) by (di, dj) lands on a walkable cell without cutting
    // a blocked corner (the moves findPath takes).
    bool canStep(int i, int j, int di, int dj) const
    {
        return at(i + di, j + dj) && (!(di && dj) || (at(i + di, j) && at(i, j + dj)));
    }
    bool isWalkable(int x, int z) const;
    bool any() const;
};

// Cover-zone union (cover units) minus hard cols inflated by the agent. Nothing
// when there are no cover zones.
std::optional<Grid> buildGrid(const std::vector<std::vector<XZ>>& coverPolys,
                              const std::vector<Box>& hardCols, const Agent& agent,
                              int step = kGridStep);

using Accept = std::function<bool(XZ)>;

// Closest walkable cell centre to p in up to maxCells rings (p itself if walkable).
std::optional<XZ> nearestWalkable(const Grid& grid, XZ p, int maxCells = 6, const Accept& accept = {});
// Where to stand to reach `target` coming from `from`: the target itself when
// walkable, else the accepted cell nearest `from` on the first ring (up to 12)
// that has one.
std::optional<XZ> approachCell(const Grid& grid, XZ target, XZ from, const Accept& accept = {});

// The cells findPath can reach from one start cell.
struct Reach
{
    Grid grid; // the grid's geometry; walk = 1 for a reachable cell
    bool contains(XZ p) const { return grid.isWalkable(p.x, p.z); }
};
// Nothing when `start` is not on a walkable cell.
std::optional<Reach> reachFrom(const Grid& grid, XZ start);
// A* (8-connected, no corner cutting) then string-pulled; last waypoint is goal.
std::optional<std::vector<XZ>> findPath(const Grid& grid, XZ start, XZ goal);

constexpr int kArriveDistance = 400;   // DISTANCE_TO_POINT_TRESSHOLD (track.cpp)
constexpr int kWaypointDistance = 400; // an intermediate hop counts as reached
constexpr int kGiveUpDistance = 800;   // a stall this close still counts as arrival
constexpr uint32_t kStallMs = 6000;    // no new best distance for this long = stalled

// Replica of the engine's GiveDistance2D (main.cpp), s16 result as int.
int giveDistance2D(int x1, int z1, int x2, int z2);
// Joystick bits mirroring the physical turn for LIFE scripts reading the stick:
// forward, plus bit 8 when CapObjet > 0 (beta decreases), bit 4 when < 0.
int joydMirror(int capResult);

// One live mouse walk. Coordinates are in `room`'s frame.
struct NavIntent
{
    XZ dest;
    int room = -1;
    int targetObject = -1; // world object, -1 for a floor walk
    bool requiresHold = false; // held push
    bool run = false;
    bool steering = false;
    // Leaning into the target (a push in contact, or walking into a reached
    // object): never arrives by distance, only stalls.
    bool engaged = false;
    std::vector<XZ> waypoints;
    bool planned = false;
    int pathRoom = -1;
    std::optional<XZ> stallTarget;
    int stallBest = 0;
    uint32_t stallSinceMs = 0;
};

struct NavDecision
{
    int joyd = 0;
    XZ target;
    bool advance = false;
    bool arrived = false;
    bool abandoned = false;
    bool run = false;
};

struct HeroPose
{
    int room = -1;
    XZ at;  // roomX + stepX, roomZ + stepZ
    int beta = 0;
};

struct NavEnv
{
    const Grid* grid = nullptr;                                   // the hero room's grid
    std::function<XZ(int fromRoom, int toRoom)> linkMidpoint;     // doorway midpoint, fromRoom frame
    std::function<XZ(XZ p, int fromRoom, int toRoom)> reframe;    // room-frame conversion
    std::function<int(int x1, int z1, int beta, int x2, int z2)> capObjet;
};

// One frame of steering for a live intent.
NavDecision decide(NavIntent& intent, const HeroPose& hero, const NavEnv& env, uint32_t nowMs);
// Forget stall progress (after a retarget).
void resetStall(NavIntent& intent);

} // namespace mouse
