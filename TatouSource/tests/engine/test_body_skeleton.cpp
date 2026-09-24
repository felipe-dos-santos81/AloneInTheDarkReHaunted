///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Engine unit tests: models::validateSkeleton and models::skeletonHash.
///////////////////////////////////////////////////////////////////////////////

#include <string>

#include "bodyPose.h"
#include "doctest.h"
#include "modelFixtures.h"

using namespace models;

TEST_CASE("every fixture body is a valid skeleton")
{
    for (const PoseBody& b : { oneGroupBody(), threeBoneBody(), chainBody(), humanoidBody() })
    {
        std::string why;
        CHECK_MESSAGE(validateSkeleton(b, &why), why);
    }
}

TEST_CASE("validateSkeleton names each broken invariant")
{
    struct Case
    {
        const char* expect;
        void (*breakIt)(PoseBody&);
    };
    const Case cases[] = {
        { "not animated", [](PoseBody& b) { b.flags = 0; } },
        { "INFO_OPTIMISE", [](PoseBody& b) { b.flags |= kInfoOptimise; } },
        { "33 groups", [](PoseBody& b) { b.groups.resize(33, b.groups[3]); } },
        { "not a permutation", [](PoseBody& b) { b.order[0] = 3; } },
        { "m_numGroup is 5", [](PoseBody& b) { b.groups[2].self = 5; } },
        { "root group has parent", [](PoseBody& b) { b.groups[0].parent = 1; } },
        { "is not an earlier group", [](PoseBody& b) { b.groups[1].parent = 2; } },
        { "outside 0..9", [](PoseBody& b) { b.groups[3].count = 5; } },
        { "exactly once", [](PoseBody& b) { b.groups[3].start = 6; } },
        { "pivot 12 out of range", [](PoseBody& b) { b.groups[3].pivot = 12; } },
        { "is not in parent group", [](PoseBody& b) { b.groups[3].pivot = 3; } },
        { "root pivot", [](PoseBody& b) { b.verts[0] = { 1, 0, 0 }; } },
        { "processed before one of its descendants", [](PoseBody& b) { b.order = { 1, 2, 3, 0 }; } },
        // The root pivot pass adds vertex 0 to every root vertex: it must be a root vertex.
        { "vertex 0 is not in the root group", [](PoseBody& b) {
             b.verts = { { 0, 0, 0 }, { 10, -50, 0 }, { 0, -100, 0 }, { 5, -20, 3 } };
             b.groups = { { 1, 2, 0, -1, 0 }, { 0, 1, 2, 0, 1 }, { 3, 1, 2, 0, 2 } };
             b.order = { 1, 2, 0 };
         } },
        { "body has no vertices", [](PoseBody& b) {
             b.verts.clear();
             b.groups = { { 0, 0, 0, -1, 0 } };
             b.order = { 0 };
         } },
    };
    for (const Case& c : cases)
    {
        PoseBody b = chainBody();
        c.breakIt(b);
        std::string why;
        CHECK_FALSE(validateSkeleton(b, &why));
        CHECK_MESSAGE(why.find(c.expect) != std::string::npos, why);
    }
    PoseBody b = chainBody();
    b.flags = 0;
    CHECK_FALSE(validateSkeleton(b, nullptr)); // the reason is optional
}

TEST_CASE("the skeleton hash matches the Python pipeline")
{
    PoseBody b = chainBody();
    CHECK(skeletonHash(b) == 0x22a03bb7f51b6bf4ull); // CHAIN_HASH in tests/tools/test_models_skeleton.py
    b.verts[8][2] = 11;
    CHECK(skeletonHash(b) != 0x22a03bb7f51b6bf4ull);
}
