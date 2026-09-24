///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Engine unit tests: models::viewMatrix / clipFromView against the engine's
// integer projection (mouse::projectPoint, used here only as an oracle).
///////////////////////////////////////////////////////////////////////////////

#include <cmath>

#include "bodyPose.h"
#include "bodyPoseReference.h"
#include "doctest.h"
#include "modelFixtures.h"
#include "mousePick.h"
#include "renderCamera.h"
#include "test_helpers.h"

using namespace models;

namespace
{
struct Pixel
{
    double x, y;
};

// The logical 320x200 pixel a clip-space position lands on.
Pixel screenOf(Vec4 c)
{
    return Pixel{ (c.x / c.w + 1.0) * 160.0, (1.0 - c.y / c.w) * 100.0 };
}

RenderCamera renderCameraOf(const mouse::Camera& c)
{
    RenderCamera r;
    r.alpha = c.alpha;
    r.beta = c.beta;
    r.gamma = c.gamma;
    r.posX = c.posX;
    r.posY = c.posY;
    r.posZ = c.posZ;
    r.persp = c.focal1;
    r.fovX = c.focal2;
    r.fovY = c.focal3;
    r.table = c.cosTable;
    return r;
}

Pixel project(const RenderCamera& cam, int x, int y, int z)
{
    const Vec3 pc = apply(viewMatrix(cam), Vec3{ (float)x, (float)y, (float)z });
    return screenOf(clipFromView(pc, projParams(cam, 0.0f, 0.0f)));
}
}

TEST_CASE("an unrotated camera is the renderer's pinhole divide")
{
    const mouse::Camera c = mouse::frameCamera(mouse::CameraData{ 0, 0, 0, 0, 100, 0, 1000, 300, 300 },
                                               mouse::RoomOrigin{}, engineCosTable());
    const Pixel p = project(renderCameraOf(c), 500, 0, 3000);
    const auto want = mouse::projectPoint(c, 500, 0, 3000);
    REQUIRE(want);
    CHECK(p.x == doctest::Approx(want->x));
    CHECK(p.y == doctest::Approx(want->y));
}

TEST_CASE("the float camera lands within 1 px of the engine's integer projection")
{
    TestRng rng(5);
    auto pick = [&](int lo, int hi) { return rng.pick(lo, hi); };
    double worst = 0.0;
    int compared = 0;
    for (int i = 0; i < 1000; ++i)
    {
        // Angles include 0 on an axis, and values past 1024 and below 0 (masked).
        // Floor data is in tenths of the room frame (frameCamera multiplies by 10).
        const mouse::CameraData data{ pick(0, 3) ? pick(-1500, 1500) : 0, pick(-1500, 1500), pick(0, 3) ? pick(-200, 200) : 0,
                                      pick(-300, 300), pick(0, 300), pick(-300, 300),
                                      pick(600, 2500), pick(250, 450), pick(240, 440) };
        const mouse::Camera c = mouse::frameCamera(data, mouse::RoomOrigin{ pick(-50, 50), 0, pick(-50, 50) },
                                                   engineCosTable());
        const RenderCamera cam = renderCameraOf(c);

        // A point in view, back in the room frame. At least 3,000 units deep:
        // the engine truncates each rotation step to even units, which
        // nearer the lens grows past a pixel.
        const float depth = (float)pick(3000, 20000);
        const Vec3 pc{ (float)pick(-150, 150) * depth / cam.fovX, (float)pick(-95, 95) * depth / cam.fovY, depth };
        Affine3 roomFromCamera;
        REQUIRE(invertAffine(viewMatrix(cam), &roomFromCamera));
        const Vec3 room = apply(roomFromCamera, pc);
        const int x = (int)std::lround(room.x), y = (int)std::lround(room.y), z = (int)std::lround(room.z);
        if (y > 10000 || std::abs(x) > 30000 || std::abs(y) > 30000 || std::abs(z) > 30000)
            continue; // the height clamp, and s16 room coordinates

        const auto want = mouse::projectPoint(c, x, y, z);
        REQUIRE(want);
        const Pixel got = project(cam, x, y, z);
        worst = std::max({ worst, std::abs(got.x - want->x), std::abs(got.y - want->y) });
        ++compared;
    }
    CHECK(compared > 800); // the rest fall under the height clamp or outside s16
    CHECK(worst <= 1.0);
}

TEST_CASE("the camera masks an angle before testing it for zero")
{
    RenderCamera a;
    a.persp = 1000;
    a.table = engineCosTable();
    RenderCamera b = a;
    b.beta = 1024; // & 0x3FF == 0: no rotation, unlike a bone angle of 1024
    const Affine3 va = viewMatrix(a), vb = viewMatrix(b);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 4; ++j)
            CHECK(va.m[i][j] == vb.m[i][j]);
}

TEST_CASE("clip depth after the divide is the classic z / 40960")
{
    const ProjParams p = projParams(RenderCamera{ 0, 0, 0, 0, 0, 0, 1000, 300, 300, 160, 100, nullptr }, 0.0f, 0.0f);
    const Vec4 c = clipFromView(Vec3{ 120.0f, -80.0f, 4096.0f }, p);
    CHECK(c.w == 4096.0f);
    CHECK(c.z / c.w == doctest::Approx(4096.0 / 40960.0));
}

TEST_CASE("screen shake and the projection centre move the image in logical pixels")
{
    RenderCamera cam{ 0, 0, 0, 0, 0, 0, 1000, 300, 300, 160, 100, nullptr };
    const Vec3 pc{ 120.0f, -80.0f, 3000.0f };
    const Pixel still = screenOf(clipFromView(pc, projParams(cam, 0.0f, 0.0f)));
    const Pixel shaken = screenOf(clipFromView(pc, projParams(cam, 3.0f, -2.0f)));
    CHECK(shaken.x - still.x == doctest::Approx(3.0));
    CHECK(shaken.y - still.y == doctest::Approx(-2.0));
    cam.centerX = 200; // the inventory's off-centre projection
    cam.centerY = 90;
    const Pixel moved = screenOf(clipFromView(pc, projParams(cam, 0.0f, 0.0f)));
    CHECK(moved.x - still.x == doctest::Approx(40.0));
    CHECK(moved.y - still.y == doctest::Approx(-10.0));
}

TEST_CASE("a posed actor lands within 1 px of the engine's pixels")
{
    const mouse::Camera c = mouse::frameCamera(mouse::CameraData{ 60, 200, 0, 300, 180, -400, 1200, 320, 310 },
                                               mouse::RoomOrigin{ 50, 0, 20 }, engineCosTable());
    const RenderCamera cam = renderCameraOf(c);
    const PoseBody b = humanoidBody();
    std::vector<GroupState> s(b.groups.size(), GroupState{ 0, 40, -90, 25 });
    const int beta = 300;
    // Stand the actor 5,000 units in front of the camera, a little right of and below centre.
    Affine3 roomFromCamera;
    REQUIRE(invertAffine(viewMatrix(cam), &roomFromCamera));
    const Vec3 at = apply(roomFromCamera, Vec3{ 400.0f, 300.0f, 5000.0f });
    const int actorX = (int)std::lround(at.x), actorY = (int)std::lround(at.y), actorZ = (int)std::lround(at.z);

    Affine3 world[kMaxPoseGroups];
    REQUIRE(poseGroups(b, s.data(), 0, beta, 0, engineCosTable(), world));
    const Affine3 modelView = compose(viewMatrix(cam), translationAffine<float>(actorX, actorY, actorZ));
    const ProjParams p = projParams(cam, 0.0f, 0.0f);
    // The engine's own model-space points, in exact arithmetic (its integer
    // drift is §4.1's business, not the camera's).
    const auto exact = ReferencePose<true>(b, engineCosTable()).AnimNuage(0, beta, 0, s);

    double worst = 0.0;
    for (size_t g = 0; g < b.groups.size(); ++g)
        for (int v = b.groups[g].start; v < b.groups[g].start + b.groups[g].count; ++v)
        {
            const Vec3 local{ (float)b.verts[v][0], (float)b.verts[v][1], (float)b.verts[v][2] };
            const Pixel got = screenOf(clipFromView(apply(compose(modelView, world[g]), local), p));
            const auto want = mouse::projectPoint(c, actorX + (int)std::lround(exact[v][0]),
                                                  actorY + (int)std::lround(exact[v][1]),
                                                  actorZ + (int)std::lround(exact[v][2]));
            REQUIRE(want);
            worst = std::max({ worst, std::abs(got.x - want->x), std::abs(got.y - want->y) });
        }
    CHECK(worst <= 1.0);
}
