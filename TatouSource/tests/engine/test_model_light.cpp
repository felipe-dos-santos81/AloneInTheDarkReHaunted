///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Engine unit tests: HD model lights in camera space.
///////////////////////////////////////////////////////////////////////////////

#include <cmath>

#include "doctest.h"
#include "modelLight.h"
#include "test_helpers.h"

using namespace models;

namespace
{
RenderCamera turnedCamera(int beta)
{
    RenderCamera c;
    c.beta = beta;
    c.posX = 100;
    c.posY = -200;
    c.posZ = 300;
    c.persp = 1000;
    c.table = engineCosTable();
    return c;
}
}

TEST_CASE("light: an unturned camera keeps directions, normalised")
{
    const Vec3 d = cameraDirection(turnedCamera(0), Vec3{ 0.0f, -2.0f, 0.0f });
    CHECK(d.x == doctest::Approx(0.0f));
    CHECK(d.y == doctest::Approx(-1.0f));
    CHECK(d.z == doctest::Approx(0.0f));
}

TEST_CASE("light: directions turn with the camera but do not move with it")
{
    const RenderCamera cam = turnedCamera(256); // a quarter turn about the vertical axis
    const Vec3 d = cameraDirection(cam, Vec3{ 1.0f, 0.0f, 0.0f });
    const Affine3 v = viewMatrix(cam);
    const Vec3 origin = apply(v, Vec3{ 0, 0, 0 }), tip = apply(v, Vec3{ 1, 0, 0 });
    CHECK(d.x == doctest::Approx(tip.x - origin.x).epsilon(1e-3));
    CHECK(d.y == doctest::Approx(tip.y - origin.y).epsilon(1e-3));
    CHECK(d.z == doctest::Approx(tip.z - origin.z).epsilon(1e-3));
    CHECK(std::fabs(d.x) < 1e-3f); // x turned into z
}

TEST_CASE("light: points go through the full view matrix")
{
    const RenderCamera cam = turnedCamera(100);
    const Vec3 p = cameraPoint(cam, Vec3{ 500.0f, -50.0f, 1200.0f });
    const Vec3 want = apply(viewMatrix(cam), Vec3{ 500.0f, -50.0f, 1200.0f });
    CHECK(p.x == want.x);
    CHECK(p.y == want.y);
    CHECK(p.z == want.z);
}

TEST_CASE("light: a zero direction stays zero")
{
    const Vec3 d = cameraDirection(turnedCamera(300), Vec3{ 0.0f, 0.0f, 0.0f });
    CHECK(d.x == 0.0f);
    CHECK(d.y == 0.0f);
    CHECK(d.z == 0.0f);
}
