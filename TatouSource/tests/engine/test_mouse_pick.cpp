#include "doctest.h"
#include "mousePick.h"
#include "test_helpers.h"

using namespace mouse;

namespace
{
// Level camera 100 room units (1000 scaled) above floor y=0, looking along +z.
Camera levelCamera()
{
    return frameCamera(0, 0, 0, /*cam*/ 0, 100, 0, /*focal*/ 1000, 300, 300,
                       RoomOrigin{ 0, 0, 0 }, testCosTable());
}
}

TEST_CASE("frameCamera frames the camera in the picked room's coordinate space")
{
    Camera c = frameCamera(1, 2, 3, 50, 60, 70, 11, 12, 13, RoomOrigin{ 10, 20, 30 }, testCosTable());
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
    Camera c = frameCamera(0, 0x100, 0, 0, 0, 0, 1000, 300, 300, RoomOrigin{}, testCosTable());
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
