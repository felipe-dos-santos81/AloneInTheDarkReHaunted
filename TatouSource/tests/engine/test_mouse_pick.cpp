///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Engine unit tests: projection replica and floor picking.
///////////////////////////////////////////////////////////////////////////////

#include <cstdlib>

#include "doctest.h"
#include "mousePick.h"
#include "test_helpers.h"

using namespace mouse;

namespace mouse
{
// In mouse:: so doctest's comparisons find it.
bool operator==(const Box& a, const Box& b)
{
    return a.x1 == b.x1 && a.x2 == b.x2 && a.y1 == b.y1 && a.y2 == b.y2 && a.z1 == b.z1 && a.z2 == b.z2;
}
}

namespace
{
// Level camera 100 room units (1000 scaled) above floor y=0, looking along +z.
Camera levelCamera()
{
    return frameCamera(CameraData{ 0, 0, 0, /*cam*/ 0, 100, 0, /*focal*/ 1000, 300, 300 },
                       RoomOrigin{ 0, 0, 0 }, testCosTable());
}
}

TEST_CASE("frameCamera frames the camera in the picked room's coordinate space")
{
    Camera c = frameCamera(CameraData{ 1, 2, 3, 50, 60, 70, 11, 12, 13 }, RoomOrigin{ 10, 20, 30 }, testCosTable());
    CHECK(c.posX == (50 - 10) * 10);
    CHECK(c.posY == (20 - 60) * 10);
    CHECK(c.posZ == (30 - 70) * 10);
    CHECK(c.focal1 == 11);
    CHECK(c.focal2 == 12);
    CHECK(c.focal3 == 13);
}

TEST_CASE("projectPoint without rotation is the renderer's pinhole divide")
{
    Camera c = levelCamera();
    auto p = projectPoint(c, 500, 0, 3000);
    REQUIRE(p);
    CHECK(p->x == doctest::Approx(500.0 * 300.0 / 4000.0 + 160.0));
    CHECK(p->y == doctest::Approx(1000.0 * 300.0 / 4000.0 + 100.0));
}

TEST_CASE("projectPoint culls the height clamp and the near plane like the renderer")
{
    Camera c = levelCamera();
    CHECK_FALSE(projectPoint(c, 0, 10001, 3000));
    CHECK_FALSE(projectPoint(c, 0, 0, -960)); // depth = 40 <= 50
}

TEST_CASE("projectPoint applies the fixed-point beta rotation with truncation")
{
    // beta = 0x100: cs = 32767, sn = table[512] = 0.
    // x = (X*0 - Z*32767) / 0x10000 * 2 = trunc(-499.98) * 2 = -998; z = 0.
    Camera c = frameCamera(CameraData{ 0, 0x100, 0, 0, 0, 0, 1000, 300, 300 }, RoomOrigin{}, testCosTable());
    auto p = projectPoint(c, 0, 0, 1000);
    REQUIRE(p);
    CHECK(p->x == doctest::Approx(-998.0 * 300.0 / 1000.0 + 160.0));
    CHECK(p->y == doctest::Approx(100.0));
}

TEST_CASE("fitHomography reproduces a known projective map from four points")
{
    Homography truth;
    truth.m = { 0.9, 0.1, 30.0, -0.2, 1.1, 12.0, 0.0001, 0.0002, 1.0 };
    std::array<Vec2, 4> src{ Vec2{ 0, 0 }, Vec2{ 1000, 0 }, Vec2{ 1000, 800 }, Vec2{ 0, 800 } };
    std::array<Vec2, 4> dst;
    for (int i = 0; i < 4; ++i)
        dst[i] = *apply(truth, src[i].x, src[i].y);

    auto fit = fitHomography(src, dst);
    REQUIRE(fit);
    for (Vec2 probe : { Vec2{ 250, 400 }, Vec2{ 900, 100 }, Vec2{ 10, 790 } })
    {
        Vec2 a = *apply(truth, probe.x, probe.y);
        Vec2 b = *apply(*fit, probe.x, probe.y);
        CHECK(b.x == doctest::Approx(a.x).epsilon(1e-6));
        CHECK(b.y == doctest::Approx(a.y).epsilon(1e-6));
    }

    auto inverse = invert(*fit);
    REQUIRE(inverse);
    Vec2 back = *apply(*inverse, dst[2].x, dst[2].y);
    CHECK(back.x == doctest::Approx(1000.0).epsilon(1e-6));
    CHECK(back.y == doctest::Approx(800.0).epsilon(1e-6));
}

TEST_CASE("fitHomography refuses collinear points")
{
    std::array<Vec2, 4> src{ Vec2{ 0, 0 }, Vec2{ 1, 1 }, Vec2{ 2, 2 }, Vec2{ 3, 3 } };
    std::array<Vec2, 4> dst{ Vec2{ 0, 0 }, Vec2{ 1, 0 }, Vec2{ 1, 1 }, Vec2{ 0, 1 } };
    CHECK_FALSE(fitHomography(src, dst));
}

TEST_CASE("reframe uses FITD's AdjustZV signs")
{
    RoomOrigin from{ 0, 0, 0 };
    RoomOrigin to{ 1, 2, 3 };
    CHECK(reframe(XZ{ 100, 100 }, from, to) == XZ{ 100 - 10, 100 + 30 });
    CHECK(reframeY(100, from, to) == 100 + 20);
    CHECK(reframe(reframe(XZ{ 5, 7 }, from, to), to, from) == XZ{ 5, 7 });
}

namespace
{
// Cover units (room / 10): x -2000..2000, z 1000..5000 in room units.
const std::vector<XZ> kSquare{ XZ{ -200, 100 }, XZ{ 200, 100 }, XZ{ 200, 500 }, XZ{ -200, 500 } };
// A U open towards +z: arms x -2000..-1000 and 1000..2000, base z 1000..2000.
const std::vector<XZ> kU{ XZ{ -200, 100 }, XZ{ 200, 100 }, XZ{ 200, 500 }, XZ{ 100, 500 },
                          XZ{ 100, 200 },  XZ{ -100, 200 }, XZ{ -100, 500 }, XZ{ -200, 500 } };

Point pixelOf(const Camera& c, int x, int z)
{
    Vec2 v = *projectPoint(c, x, 0, z);
    return Point{ (int)v.x, (int)v.y };
}
}

TEST_CASE("pickFloor recovers the floor point under a pixel")
{
    Camera c = levelCamera();
    auto fits = fitFloor(c, { kSquare }, 0);
    REQUIRE(fits.size() == 1);
    auto hit = pickFloor(fits, pixelOf(c, 500, 3000));
    REQUIRE(hit);
    CHECK(std::abs(hit->x - 500) <= 15);
    CHECK(std::abs(hit->z - 3000) <= 15);
}

TEST_CASE("pickFloor refuses pixels whose floor point lies outside every polygon")
{
    Camera c = levelCamera();
    auto fits = fitFloor(c, { kSquare }, 0);
    CHECK_FALSE(pickFloor(fits, pixelOf(c, 3000, 3000)));
}

TEST_CASE("pickFloor takes a floor point outside every polygon that alsoFloor accepts")
{
    Camera c = levelCamera();
    auto fits = fitFloor(c, { kSquare }, 0);
    auto hit = pickFloor(fits, pixelOf(c, 3000, 3000), [](XZ p) { return p.x > 2500; });
    REQUIRE(hit);
    CHECK(std::abs(hit->x - 3000) <= 15);
    CHECK(std::abs(hit->z - 3000) <= 15);
    CHECK_FALSE(pickFloor(fits, pixelOf(c, -3000, 3000), [](XZ p) { return p.x > 2500; }));
}

TEST_CASE("fitFloor skips polygons with fewer than four usable vertices")
{
    Camera c = levelCamera();
    std::vector<XZ> triangle{ XZ{ 0, 100 }, XZ{ 100, 100 }, XZ{ 0, 200 } };
    std::vector<XZ> duplicated{ XZ{ 0, 100 }, XZ{ 0, 100 }, XZ{ 100, 100 }, XZ{ 100, 100 } };
    CHECK(fitFloor(c, { triangle, duplicated }, 0).empty());
}

TEST_CASE("steerPoint walks a pixel above the horizon back to a bearing on the floor")
{
    Camera c = levelCamera();
    auto fits = fitFloor(c, { kSquare }, 0);
    // (160,50) recovers behind the camera; the half-way sample (160,125) is z=11000.
    auto target = steerPoint(c, fits, 0, XZ{ 0, 2000 }, Point{ 160, 50 });
    REQUIRE(target);
    CHECK(target->x == 0);
    CHECK(target->z == 2000 + kSteerDistance);
}

TEST_CASE("steerPoint has no bearing when the hero's feet are off screen or under the pointer")
{
    Camera c = levelCamera();
    auto fits = fitFloor(c, { kSquare }, 0);
    CHECK_FALSE(steerPoint(c, fits, 0, XZ{ 0, -2000 }, Point{ 160, 150 }));
    Point feet = pixelOf(c, 0, 2000);
    CHECK_FALSE(steerPoint(c, fits, 0, XZ{ 0, 2000 }, feet));
}

TEST_CASE("pickFloor judges inside with the engine's two-ray rule, notch of a U included")
{
    Camera c = levelCamera();
    auto fits = fitFloor(c, { kU }, 0);
    REQUIRE(fits.size() == 1);
    // z = 4000 projects to whole pixels here, so the picks recover exactly.
    // Between the arms: the engine's isInPoly calls it inside (both X rays hit
    // an arm), where an even-odd test would call it outside.
    auto notch = pickFloor(fits, pixelOf(c, 0, 4000));
    REQUIRE(notch);
    CHECK(std::abs(notch->x) <= 15);
    CHECK(std::abs(notch->z - 4000) <= 15);
    CHECK(pickFloor(fits, pixelOf(c, -1500, 4000))); // on an arm
    CHECK_FALSE(pickFloor(fits, pixelOf(c, 2400, 4000))); // right of everything
}

TEST_CASE("boxSilhouetteContains is the box's outline on screen, never the floor in front of it")
{
    Camera c = levelCamera();
    // A hero-high volume standing on the floor 3000..3400 ahead.
    const Box exit{ -200, 200, -1500, 0, 3000, 3400 };
    auto middle = projectPoint(c, 0, -700, 3200);
    REQUIRE(middle);
    CHECK(boxSilhouetteContains(c, exit, Point{ (int)middle->x, (int)middle->y }));
    CHECK(boxSilhouetteContains(c, exit, pixelOf(c, 0, 3100))); // its own floor
    CHECK_FALSE(boxSilhouetteContains(c, exit, pixelOf(c, 1500, 3200))); // beside it
    CHECK_FALSE(boxSilhouetteContains(c, exit, pixelOf(c, 0, 2000)));    // floor in front of it
}

TEST_CASE("boxSilhouetteContains has no outline when a corner is culled")
{
    Camera c = levelCamera();
    const Box straddling{ -200, 200, -1500, 0, -2000, 3400 }; // reaches behind the near plane
    CHECK_FALSE(boxSilhouetteContains(c, straddling, pixelOf(c, 0, 3000)));
}

TEST_CASE("clippedBoxOutline is the silhouette when the whole box is in front")
{
    Camera c = levelCamera();
    const Box exit{ -200, 200, -1500, 0, 3000, 3400 };
    const std::vector<Vec2> outline = clippedBoxOutline(c, exit);
    auto middle = projectPoint(c, 0, -700, 3200);
    REQUIRE(middle);
    CHECK(outlineContains(outline, Point{ (int)middle->x, (int)middle->y }));
    CHECK(outlineContains(outline, pixelOf(c, 0, 3100)));
    CHECK_FALSE(outlineContains(outline, pixelOf(c, 1500, 3200)));
    CHECK_FALSE(outlineContains(outline, pixelOf(c, 0, 2000)));
}

TEST_CASE("clippedBoxOutline keeps the part of a box in front of the near plane")
{
    Camera c = levelCamera();
    // A door standing open beside the camera, reaching behind it: a thin slab
    // off to the left, from behind the camera to 4000 ahead. Its vertices in
    // front span the screen's left strip (0,0)-(106,199) once clamped, but it
    // covers only a band of it, narrowing toward x = 106.
    const Box slab{ -1000, -900, -2000, 0, -2000, 4000 };
    const std::vector<Vec2> outline = clippedBoxOutline(c, slab);
    CHECK(outlineContains(outline, Point{ 30, 100 }));
    CHECK(outlineContains(outline, Point{ 104, 100 }));
    CHECK_FALSE(outlineContains(outline, Point{ 104, 10 }));  // above its far end
    CHECK_FALSE(outlineContains(outline, Point{ 104, 190 })); // below it
    CHECK_FALSE(outlineContains(outline, Point{ 160, 100 })); // right of it
}

TEST_CASE("clippedBoxOutline is empty for a box wholly behind the camera")
{
    Camera c = levelCamera();
    CHECK(clippedBoxOutline(c, Box{ -200, 200, -1500, 0, -3000, -2000 }).empty());
    CHECK_FALSE(outlineContains({}, Point{ 160, 100 }, 6.0));
}

TEST_CASE("outlineContains forgives a pixel within its slack of the outline")
{
    const std::vector<Vec2> square{ Vec2{ 100, 100 }, Vec2{ 120, 100 }, Vec2{ 120, 120 }, Vec2{ 100, 120 } };
    CHECK(outlineContains(square, Point{ 110, 110 }));
    CHECK_FALSE(outlineContains(square, Point{ 124, 110 }));
    CHECK(outlineContains(square, Point{ 124, 110 }, 6.0));
    CHECK_FALSE(outlineContains(square, Point{ 130, 110 }, 6.0));
    // An edge-on box collapses to a segment: only the slack can reach it.
    const std::vector<Vec2> segment{ Vec2{ 100, 100 }, Vec2{ 100, 150 } };
    CHECK_FALSE(outlineContains(segment, Point{ 100, 120 }));
    CHECK(outlineContains(segment, Point{ 103, 120 }, 6.0));
}

TEST_CASE("posedBox moves an unturned body's box to where it stands")
{
    const Box body{ -990, 0, -2475, 0, 0, 99 };
    CHECK(posedBox(body, 0, 0, 0, -2690, 0, -2080, testCosTable()) == Box{ -3680, -2690, -2475, 0, -2080, -1981 });
}

TEST_CASE("posedBox turns the body's corners as the renderer turns its vertices")
{
    // beta = 0x100: X' = -Z*32767/32768, Z' = X*32767/32768 (RotateNuage's Y
    // rotation), widened to whole units: a door swung a quarter turn.
    const Box body{ -990, 0, -2475, 0, 0, 99 };
    CHECK(posedBox(body, 0, 0x100, 0, -2690, 0, -2080, testCosTable()) ==
          Box{ -2690 - 99, -2690, -2475, 0, -2080 - 990, -2080 });
}

TEST_CASE("nearerThanBox: floor drawn in front of a box, raised floor included, is not the box")
{
    Camera c = levelCamera();
    const Box exit{ -200, 200, -1500, 0, 3000, 3400 };
    CHECK(nearerThanBox(c, exit, 0, 0, 2000));        // floor in front of it
    CHECK_FALSE(nearerThanBox(c, exit, 0, 0, 3200));  // its own floor
    CHECK_FALSE(nearerThanBox(c, exit, 0, 0, 5000));  // floor behind it
    // A raised floor in front of the box shows inside its outline: the outline
    // alone would call it the box; its depth says it is in front.
    auto raised = projectPoint(c, 0, -700, 1500);
    REQUIRE(raised);
    REQUIRE(boxSilhouetteContains(c, exit, Point{ (int)raised->x, (int)raised->y }));
    CHECK(nearerThanBox(c, exit, 0, -700, 1500));
}
