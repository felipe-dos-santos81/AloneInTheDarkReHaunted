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
