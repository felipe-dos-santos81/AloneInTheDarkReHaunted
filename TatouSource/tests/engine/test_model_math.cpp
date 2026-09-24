///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Engine unit tests: the HD model math support (affine matrices, the
// engine's sine table read from source).
///////////////////////////////////////////////////////////////////////////////

#include "affine3.h"
#include "doctest.h"
#include "test_helpers.h"

using namespace models;

TEST_CASE("the engine's sine table is read from cosTable.cpp")
{
    const auto& t = engineCosTableEntries();
    REQUIRE(t.size() == 1024);
    CHECK(t[0] == 4); // not 0: testCosTable() would hide the engine's quirk
    CHECK(t[256] == 32767);
    CHECK(t[512] == 0);
    CHECK(t[768] == -32767);
}

TEST_CASE("compose applies its right-hand matrix first")
{
    Affine3d scale = identityAffine<double>();
    scale.m[0][0] = 2.0;
    const Affine3d m = compose(translationAffine<double>(10, 20, 30), scale);
    double p[3];
    applyAffine(m, 1.0, 1.0, 1.0, p);
    CHECK(p[0] == 12.0); // scaled, then moved
    CHECK(p[1] == 21.0);
    CHECK(p[2] == 31.0);
}

TEST_CASE("invertAffine undoes a turn, a scale and a move")
{
    Affine3d m = translationAffine<double>(5, -7, 9);
    m.m[0][0] = 0.0;
    m.m[0][1] = -3.0; // x' = -3y + 5
    m.m[1][0] = 2.0;  // y' = 2x - 7
    m.m[1][1] = 0.0;
    Affine3d inv;
    REQUIRE(invertAffine(m, &inv));
    const Affine3d round = compose(inv, m);
    const Affine3d one = identityAffine<double>();
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 4; ++j)
            CHECK(round.m[i][j] == doctest::Approx(one.m[i][j]));
}

TEST_CASE("invertAffine refuses a singular matrix and leaves the output alone")
{
    Affine3d flat = identityAffine<double>();
    flat.m[2][2] = 0.0;
    Affine3d out = translationAffine<double>(1, 2, 3);
    CHECK_FALSE(invertAffine(flat, &out));
    CHECK(out.m[0][3] == 1.0);
}

TEST_CASE("apply maps a float point through a float matrix")
{
    const Affine3 m = castAffine<float>(translationAffine<double>(1, 2, 3));
    const Vec3 p = apply(m, Vec3{ 4.0f, 5.0f, 6.0f });
    CHECK(p.x == 5.0f);
    CHECK(p.y == 7.0f);
    CHECK(p.z == 9.0f);
}
