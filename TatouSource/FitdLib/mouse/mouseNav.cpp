///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: walk grid, ring searches, A*. Engine-free.
///////////////////////////////////////////////////////////////////////////////

#include "mouseNav.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <queue>
#include <utility>

namespace mouse
{

int Grid::column(int x) const
{
    return floorDiv(x - x0 + step / 2, step);
}

int Grid::row(int z) const
{
    return floorDiv(z - z0 + step / 2, step);
}

bool Grid::cellOf(int x, int z, int* i, int* j) const
{
    const int ci = column(x);
    const int cj = row(z);
    if (ci < 0 || cj < 0 || ci >= nx || cj >= nz)
        return false;
    *i = ci;
    *j = cj;
    return true;
}

bool Grid::isWalkable(int x, int z) const
{
    int i = 0;
    int j = 0;
    return cellOf(x, z, &i, &j) && at(i, j);
}

bool Grid::any() const
{
    return std::any_of(walk.begin(), walk.end(), [](uint8_t w) { return w != 0; });
}

std::optional<Grid> buildGrid(const std::vector<std::vector<XZ>>& coverPolys,
                              const std::vector<Box>& hardCols, const Agent& agent, int step)
{
    int minX = INT_MAX, maxX = INT_MIN, minZ = INT_MAX, maxZ = INT_MIN;
    for (const auto& poly : coverPolys)
        for (XZ p : poly)
        {
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minZ = std::min(minZ, p.z);
            maxZ = std::max(maxZ, p.z);
        }
    if (minX == INT_MAX)
        return std::nullopt;

    Grid g;
    g.step = step;
    g.x0 = minX * kCoverScale;
    g.z0 = minZ * kCoverScale;
    g.nx = ((maxX - minX) * kCoverScale) / step + 1;
    g.nz = ((maxZ - minZ) * kCoverScale) / step + 1;
    g.walk.assign((size_t)g.nx * g.nz, 0);

    for (int i = 0; i < g.nx; ++i)
        for (int j = 0; j < g.nz; ++j)
        {
            const int wx = g.x0 + i * step;
            const int wz = g.z0 + j * step;
            const bool inside = std::any_of(coverPolys.begin(), coverPolys.end(),
                                            [&](const std::vector<XZ>& poly) { return insideCoverZone(wx, wz, poly); });
            if (!inside)
                continue;
            bool blocked = false;
            for (const Box& col : hardCols)
            {
                if (!(agent.y1 < col.y2 && col.y1 < agent.y2))
                    continue; // outside the hero's Y band: room links fall out here
                if (wx - agent.half < col.x2 && col.x1 < wx + agent.half &&
                    wz - agent.half < col.z2 && col.z1 < wz + agent.half)
                {
                    blocked = true;
                    break;
                }
            }
            g.walk[(size_t)i * g.nz + j] = blocked ? 0 : 1;
        }
    return g;
}

namespace
{
constexpr int kApproachRings = 12;

// The lowest-scoring accepted walkable cell centre on the first Chebyshev ring
// around (oi, oj), from firstRadius out to maxCells, that has one.
template <typename Score>
std::optional<XZ> ringSearch(const Grid& grid, int oi, int oj, int firstRadius, int maxCells,
                             const Accept& accept, Score score)
{
    for (int radius = firstRadius; radius <= maxCells; ++radius)
    {
        std::optional<XZ> best;
        double bestScore = 0.0;
        for (int di = -radius; di <= radius; ++di)
            for (int dj = -radius; dj <= radius; ++dj)
            {
                if (std::max(std::abs(di), std::abs(dj)) != radius)
                    continue;
                const int i = oi + di;
                const int j = oj + dj;
                if (!grid.at(i, j))
                    continue;
                const XZ c = grid.center(i, j);
                if (accept && !accept(c))
                    continue;
                const double s = score(i, j);
                if (!best || s < bestScore)
                {
                    best = c;
                    bestScore = s;
                }
            }
        if (best)
            return best;
    }
    return std::nullopt;
}
}

std::optional<XZ> nearestWalkable(const Grid& grid, XZ p, int maxCells, const Accept& accept)
{
    if (grid.isWalkable(p.x, p.z))
        return p;
    int oi = 0;
    int oj = 0;
    if (!grid.cellOf(p.x, p.z, &oi, &oj))
        return std::nullopt;
    return ringSearch(grid, oi, oj, 1, maxCells, accept, [&](int i, int j) {
        return (double)((i - oi) * (i - oi) + (j - oj) * (j - oj));
    });
}

std::optional<XZ> approachCell(const Grid& grid, XZ target, XZ from, const Accept& accept)
{
    if (grid.isWalkable(target.x, target.z) && (!accept || accept(target)))
        return target;
    const int oi = std::clamp(grid.column(target.x), 0, grid.nx - 1);
    const int oj = std::clamp(grid.row(target.z), 0, grid.nz - 1);
    const double fi = (double)(from.x - grid.x0) / grid.step;
    const double fj = (double)(from.z - grid.z0) / grid.step;
    return ringSearch(grid, oi, oj, 0, kApproachRings, accept, [&](int i, int j) {
        return (i - fi) * (i - fi) + (j - fj) * (j - fj);
    });
}

std::optional<Reach> reachFrom(const Grid& grid, XZ start)
{
    int si = 0;
    int sj = 0;
    if (!grid.cellOf(start.x, start.z, &si, &sj) || !grid.at(si, sj))
        return std::nullopt;
    Reach reach{ grid };
    std::fill(reach.grid.walk.begin(), reach.grid.walk.end(), 0);
    std::vector<std::pair<int, int>> open{ { si, sj } };
    reach.grid.walk[(size_t)si * grid.nz + sj] = 1;
    while (!open.empty())
    {
        const auto [ci, cj] = open.back();
        open.pop_back();
        for (int di = -1; di <= 1; ++di)
            for (int dj = -1; dj <= 1; ++dj)
            {
                if ((!di && !dj) || !grid.canStep(ci, cj, di, dj))
                    continue;
                uint8_t& seen = reach.grid.walk[(size_t)(ci + di) * grid.nz + (cj + dj)];
                if (!seen)
                {
                    seen = 1;
                    open.push_back({ ci + di, cj + dj });
                }
            }
    }
    return reach;
}

namespace
{
bool lineClear(const Grid& g, std::pair<int, int> a, std::pair<int, int> b)
{
    const int steps = std::max(std::abs(b.first - a.first), std::abs(b.second - a.second));
    if (steps == 0)
        return true;
    if (!g.at(a.first, a.second))
        return false;
    int pi = a.first;
    int pj = a.second;
    for (int k = 1; k <= steps; ++k)
    {
        const int i = (int)std::lround(a.first + (double)(b.first - a.first) * k / steps);
        const int j = (int)std::lround(a.second + (double)(b.second - a.second) * k / steps);
        if (!g.canStep(pi, pj, i - pi, j - pj))
            return false;
        pi = i;
        pj = j;
    }
    return true;
}

std::vector<XZ> stringPull(const Grid& g, const std::vector<std::pair<int, int>>& cells, XZ goal)
{
    std::vector<XZ> points;
    size_t index = 0;
    while (index + 1 < cells.size())
    {
        size_t far = cells.size() - 1;
        while (far > index + 1 && !lineClear(g, cells[index], cells[far]))
            --far;
        points.push_back(g.center(cells[far].first, cells[far].second));
        index = far;
    }
    if (points.empty())
        return { goal };
    points.back() = goal;
    return points;
}
}

std::optional<std::vector<XZ>> findPath(const Grid& g, XZ start, XZ goal)
{
    int si = 0, sj = 0, gi = 0, gj = 0;
    if (!g.cellOf(start.x, start.z, &si, &sj) || !g.cellOf(goal.x, goal.z, &gi, &gj))
        return std::nullopt;
    if (!g.at(si, sj) || !g.at(gi, gj))
        return std::nullopt;
    if (si == gi && sj == gj)
        return std::vector<XZ>{ goal };

    const int count = g.nx * g.nz;
    const int startIdx = si * g.nz + sj;
    const int goalIdx = gi * g.nz + gj;
    std::vector<int> cost((size_t)count, INT_MAX);
    std::vector<int> came((size_t)count, -1);
    using Node = std::pair<int, int>; // (priority, index)
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;
    cost[startIdx] = 0;
    came[startIdx] = startIdx;
    open.push({ 0, startIdx });
    static const int kNeighbours[8][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
                                           { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };
    while (!open.empty())
    {
        const int idx = open.top().second;
        open.pop();
        if (idx == goalIdx)
            break;
        const int ci = idx / g.nz;
        const int cj = idx % g.nz;
        for (const auto& n : kNeighbours)
        {
            const int di = n[0];
            const int dj = n[1];
            const int ni = ci + di;
            const int nj = cj + dj;
            if (!g.canStep(ci, cj, di, dj))
                continue;
            const int nidx = ni * g.nz + nj;
            const int next = cost[idx] + ((di && dj) ? 14 : 10);
            if (next < cost[nidx])
            {
                cost[nidx] = next;
                came[nidx] = idx;
                const int estimate = std::max(std::abs(ni - gi), std::abs(nj - gj)) * 10;
                open.push({ next + estimate, nidx });
            }
        }
    }
    if (came[goalIdx] == -1)
        return std::nullopt;

    std::vector<std::pair<int, int>> cells;
    for (int idx = goalIdx;; idx = came[idx])
    {
        cells.push_back({ idx / g.nz, idx % g.nz });
        if (idx == startIdx)
            break;
    }
    std::reverse(cells.begin(), cells.end());
    return stringPull(g, cells, goal);
}

int giveDistance2D(int x1, int z1, int x2, int z2)
{
    x1 -= x2;
    if ((int16_t)x1 < 0)
        x1 = -(int16_t)x1;
    z1 -= z2;
    if ((int16_t)z1 < 0)
        z1 = -(int16_t)z1;
    if ((x1 + z1) > 0xFFFF)
        return 0x7D00;
    return (int)(int16_t)(x1 + z1);
}

int joydMirror(int capResult)
{
    // _turn_toward: beta - angle*256; GereManualRot: bit 4 -> beta + 0x100,
    // bit 8 -> beta - 0x100. Equivalence is direction == -angle.
    if (capResult > 0)
        return 1 | 8;
    if (capResult < 0)
        return 1 | 4;
    return 1;
}

void resetStall(NavIntent& intent)
{
    intent.stallTarget.reset();
    intent.stallBest = 0;
    intent.stallSinceMs = 0;
}

namespace
{
void repath(NavIntent& in, const HeroPose& hero, const NavEnv& env)
{
    in.pathRoom = hero.room;
    in.planned = true;
    if (in.steering)
    {
        // A steer names a direction: re-frame it into the room just entered.
        if (in.room != hero.room)
        {
            in.dest = env.reframe(in.dest, in.room, hero.room);
            in.room = hero.room;
        }
        in.waypoints = { in.dest };
        return;
    }
    if (in.room != hero.room)
    {
        // One hop: the doorway linking us to the target room (AITD1 follow mode).
        in.waypoints = { env.linkMidpoint(hero.room, in.room) };
        return;
    }
    if (env.grid)
    {
        if (auto path = findPath(*env.grid, hero.at, in.dest))
        {
            in.waypoints = *path;
            return;
        }
    }
    in.waypoints = { in.dest }; // degraded: let the engine's collision slide
}

bool stalled(NavIntent& in, XZ target, int distance, uint32_t nowMs)
{
    if (in.stallTarget != target || distance < in.stallBest)
    {
        in.stallTarget = target;
        in.stallBest = distance;
        in.stallSinceMs = nowMs;
        return false;
    }
    return nowMs - in.stallSinceMs >= kStallMs;
}
}

NavDecision decide(NavIntent& in, const HeroPose& hero, const NavEnv& env, uint32_t nowMs)
{
    if (!in.planned || in.pathRoom != hero.room || in.waypoints.empty())
        repath(in, hero, env);
    while (in.waypoints.size() > 1 &&
           giveDistance2D(hero.at.x, hero.at.z, in.waypoints.front().x, in.waypoints.front().z) < kWaypointDistance)
        in.waypoints.erase(in.waypoints.begin());

    NavDecision d;
    d.target = in.waypoints.front();
    const int distance = giveDistance2D(hero.at.x, hero.at.z, d.target.x, d.target.z);
    // Only the destination room reports arrival: a cross-room waypoint is the doorway.
    // A push in contact never arrives: it leans until it stalls.
    if (!in.engaged && in.room == hero.room && in.waypoints.size() == 1 && distance < kArriveDistance)
    {
        d.arrived = true;
        return d;
    }
    if (stalled(in, d.target, distance, nowMs))
    {
        const bool close = distance < kGiveUpDistance;
        // A stall this close still counts as arrival only in the destination
        // room: a cross-room waypoint is the doorway, and a foundable behind a
        // closed door in a neighbouring room must never dispatch.
        d.arrived = close && in.room == hero.room;
        d.abandoned = !d.arrived;
        return d;
    }
    d.joyd = joydMirror(env.capObjet(hero.at.x, hero.at.z, hero.beta, d.target.x, d.target.z));
    d.advance = true;
    d.run = in.run;
    return d;
}

} // namespace mouse
