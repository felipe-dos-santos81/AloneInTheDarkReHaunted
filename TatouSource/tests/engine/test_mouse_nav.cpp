///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Engine unit tests: walk grid, ring searches and steering.
///////////////////////////////////////////////////////////////////////////////

#include "doctest.h"
#include "mouseNav.h"

using namespace mouse;

namespace
{
// Verbatim logic of the engine's isInPoly (main.cpp) over one closed polygon,
// point form. Kept here, not linked, to pin the replica to the engine.
int engineTestCrossProduct(int x1, int z1, int x2, int z2, int x3, int z3, int x4, int z4)
{
    int returnFlag = 0;
    int xAB = x1 - x2;
    int yCD = z3 - z4;
    int xCD = x3 - x4;
    int yAB = z1 - z2;
    int xAC = x1 - x3;
    int yAC = z1 - z3;
    int DotProduct = (xAB * yCD) - (xCD * yAC);
    if (DotProduct)
    {
        int Dda = xAC * yCD - xCD * yAC;
        int Dmu = -xAB * yAC + xAC * yAB;
        if (DotProduct < 0)
        {
            DotProduct = -DotProduct;
            Dda = -Dda;
            Dmu = -Dmu;
        }
        if (Dda >= 0 && Dmu >= 0 && DotProduct >= Dda && DotProduct >= Dmu)
            returnFlag = 1;
    }
    return returnFlag;
}

bool engineIsInPolyPoint(int xMid, int zMid, const std::vector<XZ>& open)
{
    std::vector<XZ> table = open;
    table.push_back(open.front()); // floor.cpp copies the first point to the end
    int flag = 0;
    for (size_t j = 0; j < open.size(); ++j)
    {
        if (engineTestCrossProduct(xMid, zMid, xMid - 10000, zMid, table[j].x, table[j].z, table[j + 1].x, table[j + 1].z))
            flag |= 1;
        if (engineTestCrossProduct(xMid, zMid, xMid + 10000, zMid, table[j].x, table[j].z, table[j + 1].x, table[j + 1].z))
            flag |= 2;
    }
    return flag == 3;
}

// Cover units (room / 10): a 100x60 room.
const std::vector<XZ> kRoom{ XZ{ 0, 0 }, XZ{ 100, 0 }, XZ{ 100, 60 }, XZ{ 0, 60 } };
const std::vector<XZ> kEll{ XZ{ 0, 0 }, XZ{ 60, 0 }, XZ{ 60, 20 }, XZ{ 20, 20 }, XZ{ 20, 60 }, XZ{ 0, 60 } };
const Agent kHero{ 50, -1000, 0 };
}

TEST_CASE("insideTwoRay matches the engine's isInPoly on square and concave polygons")
{
    for (const auto* poly : { &kRoom, &kEll })
        for (int x = -5; x <= 105; x += 3)
            for (int z = -5; z <= 65; z += 3)
                CHECK_MESSAGE(insideTwoRay(x, z, *poly) == engineIsInPolyPoint(x, z, *poly),
                              "x=" << x << " z=" << z);
}

TEST_CASE("buildGrid rasterises cover zones and subtracts inflated hard cols in the hero's band")
{
    Box table{ 400, 600, -800, 0, 250, 350 }; // a table in the middle of the room
    Box ceilingBeam{ 0, 1000, -3000, -2500, 0, 600 }; // above the hero's band
    auto grid = buildGrid({ kRoom }, { table, ceilingBeam }, kHero);
    REQUIRE(grid);
    CHECK(grid->x0 == 0);
    CHECK(grid->z0 == 0);
    CHECK(grid->nx == 11);
    CHECK(grid->nz == 7);
    CHECK(grid->isWalkable(100, 100));
    CHECK_FALSE(grid->isWalkable(500, 300)); // inside the table
    CHECK_FALSE(grid->isWalkable(400, 300)); // within the hero's half-extent of it
    CHECK(grid->isWalkable(800, 500));       // the beam does not block
    CHECK(grid->any());
}

TEST_CASE("buildGrid has no grid for a room no camera covers")
{
    CHECK_FALSE(buildGrid({}, {}, kHero));
}

TEST_CASE("nearestWalkable searches rings and honours accept and the ring limit")
{
    Box wall{ 0, 1000, -800, 0, 200, 400 }; // cells z=200..400 blocked across the room
    auto grid = buildGrid({ kRoom }, { wall }, kHero);
    REQUIRE(grid);
    auto spot = nearestWalkable(*grid, XZ{ 500, 300 });
    REQUIRE(spot);
    CHECK(grid->isWalkable(spot->x, spot->z));
    CHECK_FALSE(nearestWalkable(*grid, XZ{ 500, 300 }, 1)); // the nearest walkable row is 2 rings away
    CHECK_FALSE(nearestWalkable(*grid, XZ{ 500, 300 }, 6, [](XZ) { return false; }));
}

TEST_CASE("approachCell stands on the side the hero comes from")
{
    Box crate{ 450, 550, -800, 0, 250, 350 };
    auto grid = buildGrid({ kRoom }, { crate }, kHero);
    REQUIRE(grid);
    auto fromWest = approachCell(*grid, XZ{ 500, 300 }, XZ{ 0, 300 });
    auto fromEast = approachCell(*grid, XZ{ 500, 300 }, XZ{ 1000, 300 });
    REQUIRE(fromWest);
    REQUIRE(fromEast);
    CHECK(fromWest->x < 500);
    CHECK(fromEast->x > 500);
}

TEST_CASE("approachCell honours accept, on the target itself and around it")
{
    auto grid = buildGrid({ kRoom }, {}, kHero);
    REQUIRE(grid);
    // A walkable target is its own approach unless accept refuses it.
    CHECK(approachCell(*grid, XZ{ 500, 300 }, XZ{ 0, 300 }) == XZ{ 500, 300 });
    auto west = approachCell(*grid, XZ{ 500, 300 }, XZ{ 0, 300 }, [](XZ c) { return c.x < 450; });
    REQUIRE(west);
    CHECK(west->x < 450);
    CHECK_FALSE(approachCell(*grid, XZ{ 500, 300 }, XZ{ 0, 300 }, [](XZ) { return false; }));
}

TEST_CASE("reachFrom marks the cells findPath can reach, never through a cut corner")
{
    // Two blocks leave the north-east corner joined to the rest only diagonally.
    Box south{ 700, 1000, -800, 0, 0, 350 };
    Box west{ 0, 650, -800, 0, 350, 600 };
    auto grid = buildGrid({ kRoom }, { south, west }, kHero);
    REQUIRE(grid);
    auto reach = reachFrom(*grid, XZ{ 100, 100 });
    REQUIRE(reach);
    CHECK(reach->contains(XZ{ 100, 100 }));
    CHECK(reach->contains(XZ{ 500, 100 }));
    CHECK_FALSE(reach->contains(XZ{ 900, 500 })); // walkable, but only via a blocked corner
    CHECK_FALSE(reach->contains(XZ{ 5000, 5000 })); // off the grid
    CHECK(grid->isWalkable(900, 500));
    CHECK_FALSE(findPath(*grid, XZ{ 100, 100 }, XZ{ 900, 500 }));
    CHECK_FALSE(reachFrom(*grid, XZ{ 900, 200 })); // the start itself is blocked
}

TEST_CASE("findPath routes around a wall and string-pulls to few waypoints")
{
    Box wall{ 450, 550, -800, 0, 0, 450 }; // wall from the south edge, gap at the north
    auto grid = buildGrid({ kRoom }, { wall }, kHero);
    REQUIRE(grid);
    auto path = findPath(*grid, XZ{ 100, 100 }, XZ{ 900, 100 });
    REQUIRE(path);
    CHECK(path->back() == XZ{ 900, 100 });
    CHECK(path->size() >= 2);
    CHECK(path->size() <= 4);
    for (XZ p : *path)
        CHECK(grid->isWalkable(p.x, p.z));
}

TEST_CASE("findPath: same cell, off-grid and blocked ends")
{
    auto grid = buildGrid({ kRoom }, {}, kHero);
    REQUIRE(grid);
    auto same = findPath(*grid, XZ{ 100, 100 }, XZ{ 120, 90 });
    REQUIRE(same);
    CHECK(same->size() == 1);
    CHECK(same->front() == XZ{ 120, 90 });
    CHECK_FALSE(findPath(*grid, XZ{ -5000, 100 }, XZ{ 100, 100 }));
    auto straight = findPath(*grid, XZ{ 100, 100 }, XZ{ 900, 500 });
    REQUIRE(straight);
    CHECK(straight->size() == 1); // open room: one line of sight
}

TEST_CASE("floorDiv floors negative numbers")
{
    CHECK(floorDiv(15, 10) == 1);
    CHECK(floorDiv(-15, 10) == -2);
    CHECK(floorDiv(-10, 10) == -1);
}

namespace
{
NavEnv envWith(const Grid* grid, int cap = 1)
{
    NavEnv env;
    env.grid = grid;
    env.linkMidpoint = [](int, int) { return XZ{ 5000, 5000 }; };
    env.reframe = [](XZ p, int, int) { return XZ{ p.x - 100, p.z + 100 }; };
    env.capObjet = [cap](int, int, int, int, int) { return cap; };
    return env;
}

NavIntent walkTo(XZ dest, int room)
{
    NavIntent in;
    in.dest = dest;
    in.room = room;
    return in;
}
}

TEST_CASE("giveDistance2D mirrors the engine's Manhattan distance")
{
    CHECK(giveDistance2D(0, 0, 300, -400) == 700);
    CHECK(giveDistance2D(100, 100, 100, 100) == 0);
}

TEST_CASE("joydMirror reproduces the physical turn: CapObjet > 0 is bit 8, < 0 is bit 4")
{
    CHECK(joydMirror(1) == (1 | 8));
    CHECK(joydMirror(-1) == (1 | 4));
    CHECK(joydMirror(0) == 1);
}

TEST_CASE("decide walks, then reports arrival within the arrival distance in the destination room")
{
    auto grid = buildGrid({ kRoom }, {}, kHero);
    REQUIRE(grid);
    NavEnv env = envWith(&*grid, -1);
    NavIntent in = walkTo(XZ{ 900, 500 }, 2);
    in.run = true;

    NavDecision far = decide(in, HeroPose{ 2, XZ{ 100, 100 }, 0 }, env, 0);
    CHECK(far.advance);
    CHECK(far.run);
    CHECK(far.joyd == (1 | 4));
    CHECK(far.target == XZ{ 900, 500 });

    NavDecision near = decide(in, HeroPose{ 2, XZ{ 800, 450 }, 0 }, env, 10);
    CHECK(near.arrived);
    CHECK_FALSE(near.advance);
}

TEST_CASE("decide aims a cross-room destination at the room link and never arrives there")
{
    auto grid = buildGrid({ kRoom }, {}, kHero);
    NavEnv env = envWith(&*grid);
    NavIntent in = walkTo(XZ{ 5000, 5000 }, 7);
    NavDecision d = decide(in, HeroPose{ 2, XZ{ 4900, 4900 }, 0 }, env, 0);
    CHECK(d.target == XZ{ 5000, 5000 });
    CHECK(d.advance);
    CHECK_FALSE(d.arrived);
}

TEST_CASE("decide without a grid, or without a path, steers straight at the destination")
{
    NavEnv env = envWith(nullptr);
    NavIntent in = walkTo(XZ{ 3000, 3000 }, 2);
    NavDecision d = decide(in, HeroPose{ 2, XZ{ 0, 0 }, 0 }, env, 0);
    CHECK(d.target == XZ{ 3000, 3000 });
    CHECK(d.advance);
}

TEST_CASE("a steer intent re-frames its bearing when the hero changes room")
{
    NavEnv env = envWith(nullptr);
    NavIntent in = walkTo(XZ{ 12000, 0 }, 2);
    in.steering = true;
    decide(in, HeroPose{ 2, XZ{ 0, 0 }, 0 }, env, 0);
    NavDecision d = decide(in, HeroPose{ 3, XZ{ 0, 0 }, 0 }, env, 20);
    CHECK(in.room == 3);
    CHECK(d.target == XZ{ 12000 - 100, 0 + 100 });
}

TEST_CASE("the stall guard abandons a far target and accepts a near one after 6 s")
{
    NavEnv env = envWith(nullptr);
    NavIntent far = walkTo(XZ{ 3000, 0 }, 2);
    HeroPose stuck{ 2, XZ{ 0, 0 }, 0 };
    CHECK(decide(far, stuck, env, 0).advance);
    CHECK(decide(far, stuck, env, kStallMs - 1).advance);
    NavDecision gaveUp = decide(far, stuck, env, kStallMs);
    CHECK(gaveUp.abandoned);
    CHECK_FALSE(gaveUp.arrived);

    NavIntent near = walkTo(XZ{ 600, 0 }, 2);
    decide(near, stuck, env, 0);
    NavDecision close = decide(near, stuck, env, kStallMs);
    CHECK(close.arrived);
    CHECK_FALSE(close.abandoned);
}

TEST_CASE("a stall close to a target through a closed door in another room abandons, never arrives")
{
    // The link midpoint is fixed at (5000, 5000) regardless of args; put the
    // hero within kGiveUpDistance of it but never let it move, and target a
    // room that is not the hero's room.
    NavEnv env = envWith(nullptr);
    NavIntent in = walkTo(XZ{ 5000, 5000 }, 7);
    HeroPose stuck{ 2, XZ{ 4900, 4900 }, 0 };
    CHECK(decide(in, stuck, env, 0).advance);
    NavDecision gaveUp = decide(in, stuck, env, kStallMs);
    CHECK(gaveUp.abandoned);
    CHECK_FALSE(gaveUp.arrived);
}

TEST_CASE("progress resets the stall clock")
{
    NavEnv env = envWith(nullptr);
    NavIntent in = walkTo(XZ{ 5000, 0 }, 2);
    decide(in, HeroPose{ 2, XZ{ 0, 0 }, 0 }, env, 0);
    decide(in, HeroPose{ 2, XZ{ 100, 0 }, 0 }, env, kStallMs - 1);
    CHECK(decide(in, HeroPose{ 2, XZ{ 100, 0 }, 0 }, env, kStallMs + 10).advance);
}
