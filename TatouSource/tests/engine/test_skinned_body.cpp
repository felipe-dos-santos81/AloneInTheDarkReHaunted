///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Engine unit tests: bone matrices and screen box of an HD replacement. The
// identity oracle: a mesh made of the body's own rest vertices, skinned with
// boneMatrices, must land where the engine's loops (run exactly) put them,
// seen through the same camera.
///////////////////////////////////////////////////////////////////////////////

#include <cmath>

#include "bodyPose.h"
#include "bodyPoseReference.h"
#include "doctest.h"
#include "modelFixtures.h"
#include "renderCamera.h"
#include "skinnedBody.h"
#include "test_helpers.h"

using namespace models;

namespace
{
using Points = std::vector<std::array<double, 3>>;

Points exactPose(const PoseBody& b, const std::vector<GroupState>& s, int alpha = 0, int beta = 0, int gamma = 0)
{
    return ReferencePose<true>(b, engineCosTable()).AnimNuage(alpha, beta, gamma, s);
}

// The identity delivery as import writes it: each body vertex at its rest
// position, bound with weight 255 to its own group.
HdmMesh identityMesh(const PoseBody& b)
{
    const Points rest = exactPose(b, restStates(b));
    HdmMesh m;
    m.groupCount = (uint16_t)b.groups.size();
    m.skeletonHash = skeletonHash(b);
    for (size_t g = 0; g < b.groups.size(); ++g)
        for (int v = b.groups[g].start; v < b.groups[g].start + b.groups[g].count; ++v)
        {
            HdmVertex hv{};
            for (int k = 0; k < 3; ++k)
                hv.position[k] = (float)rest[v][k];
            hv.joints[0] = (uint8_t)g;
            hv.weights[0] = 255;
            m.vertices.push_back(hv);
        }
    return m;
}

RenderCamera camera(TestRng& rng)
{
    RenderCamera c;
    c.alpha = rng.pick(-200, 200);
    c.beta = rng.pick(-1024, 1024);
    c.gamma = rng.pick(-20, 20);
    c.posX = rng.pick(-3000, 3000);
    c.posY = rng.pick(-2000, 0);
    c.posZ = rng.pick(-3000, 3000);
    c.persp = 1000;
    c.fovX = 300;
    c.fovY = 300;
    c.table = engineCosTable();
    return c;
}

Vec3 at(const Points& p, size_t v)
{
    return Vec3{ (float)p[v][0], (float)p[v][1], (float)p[v][2] };
}
}

TEST_CASE("skin: a mesh made for another body is refused")
{
    const PoseBody body = humanoidBody();
    HdmMesh mesh = identityMesh(body);
    SkinBind skin;
    std::string why;
    CHECK(prepareSkin(body, mesh, engineCosTable(), &skin, &why));
    CHECK(skin.inverseBind.size() == 18);

    mesh.skeletonHash ^= 1;
    CHECK_FALSE(prepareSkin(body, mesh, engineCosTable(), &skin, &why));
    CHECK(why == "made for another skeleton (hash differs)");

    mesh = identityMesh(body);
    mesh.groupCount = 17;
    CHECK_FALSE(prepareSkin(body, mesh, engineCosTable(), &skin, &why));
    CHECK(why == "made for 17 groups, the body has 18");
}

TEST_CASE("skin: the identity mesh lands on the engine's own vertices (200 random poses and cameras)")
{
    const PoseBody body = humanoidBody();
    const HdmMesh mesh = identityMesh(body);
    SkinBind skin;
    REQUIRE(prepareSkin(body, mesh, engineCosTable(), &skin, nullptr));
    TestRng rng(11);
    const int16_t types[] = { 0, 0, 0, 1, 2 };
    double worst = 0.0;
    for (int i = 0; i < 200; ++i)
    {
        std::vector<GroupState> s(body.groups.size());
        for (size_t g = 0; g < s.size(); ++g)
        {
            const int16_t t = g ? types[rng.pick(0, 4)] : 0;
            const int range = t == 0 ? 2048 : 100;
            s[g] = { t, (int16_t)rng.pick(-range, range), (int16_t)rng.pick(-range, range), (int16_t)rng.pick(-range, range) };
        }
        const int alpha = rng.pick(-1024, 1024), beta = rng.pick(-1024, 1024), gamma = rng.pick(-1024, 1024);
        const int x = rng.pick(-2000, 2000), y = rng.pick(-500, 500), z = rng.pick(-2000, 2000);
        const RenderCamera cam = camera(rng);
        std::vector<Affine3> bones(body.groups.size());
        REQUIRE(boneMatrices(body, s.data(), alpha, beta, gamma, x, y, z, cam, skin, bones.data()));

        const Points engine = exactPose(body, s, alpha, beta, gamma);
        const Affine3 viewActor = castAffine<float>(
            compose(castAffine<double>(viewMatrix(cam)), translationAffine<double>(x, y, z)));
        size_t v = 0;
        for (size_t g = 0; g < body.groups.size(); ++g)
            for (int k = 0; k < body.groups[g].count; ++k, ++v)
            {
                const HdmVertex& hv = mesh.vertices[v];
                const Vec3 got = apply(bones[hv.joints[0]], Vec3{ hv.position[0], hv.position[1], hv.position[2] });
                const Vec3 want = apply(viewActor, at(engine, body.groups[g].start + k));
                worst = std::max({ worst, (double)std::fabs(got.x - want.x), (double)std::fabs(got.y - want.y),
                                   (double)std::fabs(got.z - want.z) });
            }
    }
    CHECK(worst < 0.05); // engine units; positions reach several thousand, so this is float rounding
}

TEST_CASE("skin: a refused pose writes no bones")
{
    const PoseBody body = humanoidBody();
    SkinBind skin;
    REQUIRE(prepareSkin(body, identityMesh(body), engineCosTable(), &skin, nullptr));
    std::vector<GroupState> s = restStates(body);
    s[0] = { 1, 0, 0, 0 }; // group 0 translates by the actor angles: no matrix form
    TestRng rng(3);
    std::vector<Affine3> bones(body.groups.size());
    CHECK_FALSE(boneMatrices(body, s.data(), 0, 64, 0, 0, 0, 0, camera(rng), skin, bones.data()));
}

TEST_CASE("skin: the screen box holds every projected vertex")
{
    const PoseBody body = humanoidBody();
    const HdmMesh mesh = identityMesh(body);
    SkinBind skin;
    REQUIRE(prepareSkin(body, mesh, engineCosTable(), &skin, nullptr));
    RenderCamera cam;
    cam.persp = 1000;
    cam.fovX = 300;
    cam.fovY = 300;
    cam.posZ = -3000; // the body 3000 units in front of the camera
    cam.table = engineCosTable();
    std::vector<GroupState> s = restStates(body);
    s[6] = { 0, 0, 0, 200 }; // swing a limb
    std::vector<Affine3> bones(body.groups.size());
    REQUIRE(boneMatrices(body, s.data(), 0, 100, 0, 0, 0, 0, cam, skin, bones.data()));
    const ProjParams p = projParams(cam, 0.0f, 0.0f);
    int box[4];
    REQUIRE(screenBox(bones.data(), skin, p, box));
    for (const HdmVertex& v : mesh.vertices)
    {
        const Vec4 c = clipFromView(apply(bones[v.joints[0]], Vec3{ v.position[0], v.position[1], v.position[2] }), p);
        const float sx = (c.x / c.w + 1.0f) * 160.0f, sy = (1.0f - c.y / c.w) * 100.0f;
        CHECK(sx >= box[0]);
        CHECK(sx <= box[2] + 1);
        CHECK(sy >= box[1]);
        CHECK(sy <= box[3] + 1);
    }
    CHECK(box[2] > box[0]);
    CHECK(box[3] > box[1]);
}

TEST_CASE("skin: nothing in front of the near clip has no screen box")
{
    const PoseBody body = humanoidBody();
    SkinBind skin;
    REQUIRE(prepareSkin(body, identityMesh(body), engineCosTable(), &skin, nullptr));
    RenderCamera cam;
    cam.persp = 0;
    cam.posZ = 5000; // the body far behind the camera
    cam.fovX = cam.fovY = 300;
    cam.table = engineCosTable();
    std::vector<Affine3> bones(body.groups.size());
    REQUIRE(boneMatrices(body, restStates(body).data(), 0, 0, 0, 0, 0, 0, cam, skin, bones.data()));
    int box[4];
    CHECK_FALSE(screenBox(bones.data(), skin, projParams(cam, 0.0f, 0.0f), box));
}

TEST_CASE("skin: bgfx gets the matrix column-major with the translation in 12..14")
{
    Affine3 a{ { { 1, 2, 3, 4 }, { 5, 6, 7, 8 }, { 9, 10, 11, 12 } } };
    float m[16];
    columnMajor(a, m);
    const float want[16] = { 1, 5, 9, 0, 2, 6, 10, 0, 3, 7, 11, 0, 4, 8, 12, 1 };
    for (int i = 0; i < 16; ++i)
        CHECK(m[i] == want[i]);
}
