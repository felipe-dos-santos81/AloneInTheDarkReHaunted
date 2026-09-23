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

constexpr int kGridStep = 100;  // room units per cell
constexpr int kCoverScale = 10; // cover-zone unit -> room units

// Replica of the engine's testCrossProduct (main.cpp).
int testCrossProduct(int x1, int z1, int x2, int z2, int x3, int z3, int x4, int z4);
// The engine's isInPoly for one polygon and a point (cover units): inside when
// both the -X and +X 10000-unit rays hit an edge.
bool insideTwoRay(int x, int z, const std::vector<XZ>& poly);
// Floor division (Python //) for negative coordinates.
int floorDiv(int a, int b);

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
    bool cellOf(int x, int z, int* i, int* j) const;
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
// Where to stand to reach `target` coming from `from` (up to maxCells rings).
std::optional<XZ> approachCell(const Grid& grid, XZ target, XZ from, int maxCells = 12, const Accept& accept = {});
// A* (8-connected, no corner cutting) then string-pulled; last waypoint is goal.
std::optional<std::vector<XZ>> findPath(const Grid& grid, XZ start, XZ goal);

} // namespace mouse
