///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Engine unit tests: models::poseGroups against a copy of the engine's loops.
///////////////////////////////////////////////////////////////////////////////

#include <cmath>
#include <string>

#include "bodyPose.h"
#include "bodyPoseReference.h"
#include "doctest.h"
#include "modelFixtures.h"
#include "test_helpers.h"

using namespace models;

namespace
{
using Points = std::vector<std::array<double, 3>>;

// Float matrices vs the exact loops: 1e-3 engine units (1e-6 m in glTF metres).
constexpr double kFloatTolerance = 1e-3;

Points exactPose(const PoseBody& b, const std::vector<GroupState>& s, int alpha = 0, int beta = 0, int gamma = 0)
{
    return ReferencePose<true>(b, engineCosTable()).AnimNuage(alpha, beta, gamma, s);
}

Points enginePose(const PoseBody& b, const std::vector<GroupState>& s, int alpha = 0, int beta = 0, int gamma = 0)
{
    return ReferencePose<false>(b, engineCosTable()).AnimNuage(alpha, beta, gamma, s);
}

Points floatPose(const PoseBody& b, const std::vector<GroupState>& s, int alpha = 0, int beta = 0, int gamma = 0)
{
    Affine3 w[kMaxPoseGroups];
    REQUIRE(poseGroups(b, s.data(), alpha, beta, gamma, engineCosTable(), w));
    return skinVertices(b, w);
}

// tests/tools/test_models_pose.py: group 1 turns 90 degrees about z; and
// group 1 translates, group 2 zooms, group 3 has junk type 7.
const std::vector<GroupState> kRotG1Z90 = { { 0, 0, 0, 0 }, { 0, 0, 0, 256 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };
const std::vector<GroupState> kMixed = { { 0, 0, 0, 0 }, { 1, 0, -10, 5 }, { 2, 256, 0, -128 }, { 7, 9, 9, 9 } };
}

TEST_CASE("the reference copy reproduces the Python twin's integer goldens")
{
    const PoseBody b = chainBody();
    CHECK(enginePose(b, restStates(b)) == chainRest());
    CHECK(enginePose(b, kRotG1Z90) == Points{ { 0, 0, 0 }, { 0, -100, 0 }, { 50, 0, 0 }, { 198, -100, 0 }, { 98, -100, 0 },
                                              { 146, -72, 0 }, { 176, -100, 20 }, { 50, 100, 0 }, { 70, 100, 10 } });
    CHECK(enginePose(b, kMixed) == Points{ { 0, 0, 0 }, { 0, -100, 0 }, { 50, 0, 0 }, { 0, -310, 5 }, { 0, -210, 5 },
                                           { 60, -260, 5 }, { 0, -290, 15 }, { 50, 100, 0 }, { 70, 100, 10 } });
    const Points exact = exactPose(b, kRotG1Z90);
    CHECK(exact[3][0] == doctest::Approx(199.994).epsilon(1e-5));
    CHECK(exact[5][1] == doctest::Approx(-70.001).epsilon(1e-5));
}

TEST_CASE("the rest pose applies the pivot chain")
{
    const PoseBody b = chainBody();
    CHECK(maxDistance(floatPose(b, restStates(b)), chainRest()) == 0.0);
}

TEST_CASE("group 0 takes the actor angles, and a full turn is not skipped")
{
    const PoseBody b = chainBody();
    std::vector<GroupState> ignored = restStates(b);
    ignored[0] = { 0, 300, 300, 300 };
    CHECK(maxDistance(floatPose(b, ignored), chainRest()) == 0.0);
    // 1024 & 0x3FF == 0, but the engine skips only a raw 0: table[0] is 4.
    CHECK(enginePose(b, restStates(b), 0, 1024, 0)[2] == std::array<double, 3>{ 48, 0, 0 });
    CHECK(floatPose(b, restStates(b), 0, 1024, 0)[2][0] == doctest::Approx(49.998).epsilon(1e-5));
}

TEST_CASE("translate, zoom and junk types follow the engine loops")
{
    const PoseBody b = chainBody();
    CHECK(maxDistance(floatPose(b, kMixed), exactPose(b, kMixed)) < kFloatTolerance);
    std::vector<GroupState> junk = restStates(b);
    for (size_t g = 1; g < junk.size(); ++g)
        junk[g] = { (int16_t)(3 + g), 40, -40, 40 };
    CHECK(maxDistance(floatPose(b, junk), chainRest()) == 0.0);
}

TEST_CASE("a translating root has no matrix form")
{
    const PoseBody b = chainBody();
    std::vector<GroupState> s = restStates(b);
    s[0].type = 1;
    Affine3 w[kMaxPoseGroups];
    w[0].m[0][0] = 7.0f;
    CHECK_FALSE(poseGroups(b, s.data(), 5, 0, 0, engineCosTable(), w));
    CHECK(w[0].m[0][0] == 7.0f); // nothing written
    CHECK(poseGroups(b, s.data(), 0, 0, 0, engineCosTable(), w)); // a zero delta is skipped, as in the engine
}

TEST_CASE("float matrices equal the exact engine loops on 1,000 random states")
{
    TestRng rng(7);
    auto pick = [&](int lo, int hi) { return rng.pick(lo, hi); };
    const PoseBody bodies[] = { oneGroupBody(), threeBoneBody(), chainBody(), humanoidBody() };
    const int16_t types[] = { 0, 0, 0, 1, 2, 5 };
    const int16_t rootTypes[] = { 0, 2, 5 }; // a translating root has no matrix form
    double worst = 0.0;
    for (int i = 0; i < 1000; ++i)
    {
        const PoseBody& b = bodies[i % 4];
        std::vector<GroupState> s(b.groups.size());
        for (size_t g = 0; g < s.size(); ++g)
        {
            const int16_t t = g ? types[pick(0, 5)] : rootTypes[pick(0, 2)];
            const int range = t == 0 ? 2048 : 128; // angles past 1024 and negative; modest moves and zooms
            s[g] = { t, (int16_t)pick(-range, range), (int16_t)pick(-range, range), (int16_t)pick(-range, range) };
            if (pick(0, 3) == 0)
                s[g].dy = 0; // an axis at exactly 0 is skipped
        }
        const int alpha = pick(-2048, 2048), beta = pick(0, 1) ? 0 : pick(-2048, 2048), gamma = pick(-2048, 2048);
        worst = std::max(worst, maxDistance(floatPose(b, s, alpha, beta, gamma), exactPose(b, s, alpha, beta, gamma)));
    }
    CHECK(worst < kFloatTolerance);
}

TEST_CASE("the integer path stays within its drift bounds")
{
    // Each RotateList axis step floors two coordinates to even values (an
    // error under 2 each), a vertex is turned by its own group and every
    // ancestor (3 steps each), and the pivot pass adds the parent's error:
    // at depth d the error is under 3 * sqrt(2) * (d + 1) * (d + 2) units.
    // The spec's 10 * (d + 1) is what real animations stay within (pytest
    // test_float_pose_matches_the_engine_on_real_animations); random
    // three-axis turns on every group reach past it at depth 7, so it is
    // pinned on the shallow chain only, as pytest does.
    TestRng rng(11);
    auto angle = [&] { return (int16_t)rng.pick(-1024, 1024); };
    for (const PoseBody& b : { chainBody(), humanoidBody() })
    {
        const bool shallow = b.groups.size() == 4;
        const std::vector<int> depth = groupDepths(b);
        for (int i = 0; i < 500; ++i)
        {
            std::vector<GroupState> s(b.groups.size());
            for (GroupState& st : s)
                st = { 0, angle(), angle(), angle() };
            const int alpha = angle(), beta = angle(), gamma = angle();
            const Points e = enginePose(b, s, alpha, beta, gamma);
            const Points x = exactPose(b, s, alpha, beta, gamma);
            for (size_t g = 0; g < b.groups.size(); ++g)
            {
                const double d = depth[g];
                const double bound = shallow ? 10.0 * (d + 1) : 3.0 * std::sqrt(2.0) * (d + 1) * (d + 2);
                for (int v = b.groups[g].start; v < b.groups[g].start + b.groups[g].count; ++v)
                {
                    const double err = std::hypot(e[v][0] - x[v][0], e[v][1] - x[v][1], e[v][2] - x[v][2]);
                    REQUIRE(err <= bound);
                }
            }
        }
    }
}

TEST_CASE("skin matrices move bind-pose vertices to their posed positions")
{
    const int16_t* t = engineCosTable();
    for (const PoseBody& b : { chainBody(), humanoidBody() })
    {
        std::vector<GroupState> pose(b.groups.size(), GroupState{ 0, 100, -200, 300 });
        pose[1] = { 2, 40, -20, 10 }; // a zoom too
        std::vector<GroupState> bindPose(b.groups.size(), GroupState{ 0, 0, 0, 0 });
        bindPose[1] = { 0, 0, 256, 0 }; // bound with group 1 turned

        const std::vector<GroupState>* binds[] = { nullptr, &bindPose }; // the rest pose, then a turned one
        for (const std::vector<GroupState>* bind : binds)
        {
            Affine3 inverseBind[kMaxPoseGroups], world[kMaxPoseGroups], skin[kMaxPoseGroups];
            REQUIRE(restBind(b, bind ? bind->data() : nullptr, t, inverseBind));
            REQUIRE(poseGroups(b, pose.data(), 0, 64, 0, t, world));
            skinMatrices(world, inverseBind, (int)b.groups.size(), skin);

            // A bind-pose model-space vertex of group g, through skin[g], lands where the engine puts it.
            const Points bound = exactPose(b, bind ? *bind : restStates(b));
            const Points posed = exactPose(b, pose, 0, 64, 0);
            double worst = 0.0;
            for (size_t g = 0; g < b.groups.size(); ++g)
                for (int v = b.groups[g].start; v < b.groups[g].start + b.groups[g].count; ++v)
                {
                    const Vec3 p = apply(skin[g], { (float)bound[v][0], (float)bound[v][1], (float)bound[v][2] });
                    worst = std::max({ worst, std::abs(p.x - posed[v][0]), std::abs(p.y - posed[v][1]),
                                       std::abs(p.z - posed[v][2]) });
                }
            CHECK(worst < kFloatTolerance);
        }
    }
}

TEST_CASE("a bind pose that zooms a group to nothing is refused")
{
    const PoseBody b = chainBody();
    std::vector<GroupState> flat = restStates(b);
    flat[2] = { 2, -256, 0, 0 }; // x scale 0
    Affine3 inverseBind[kMaxPoseGroups];
    CHECK_FALSE(restBind(b, flat.data(), engineCosTable(), inverseBind));
}
