///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Engine unit tests: bone matrices as dual quaternions (HD skinning).
///////////////////////////////////////////////////////////////////////////////

#include "doctest.h"
#include "dualQuat.h"

#include <cmath>

using models::Affine3;
using models::DualQuat;
using models::Vec3;
using models::apply;

namespace
{
// A rotation about z by `deg`, then a translation.
Affine3 rotZ(float deg, float tx = 0, float ty = 0, float tz = 0)
{
    const float a = deg * 3.14159265f / 180.0f, c = std::cos(a), s = std::sin(a);
    return Affine3{ { { c, -s, 0, tx }, { s, c, 0, ty }, { 0, 0, 1, tz } } };
}

float distance(Vec3 a, Vec3 b)
{
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
}
}

TEST_CASE("a rigid bone matrix survives as a dual quaternion")
{
    const Affine3 m = rotZ(37.0f, 120.0f, -45.0f, 800.0f);
    const DualQuat q = models::dualQuat(m);
    const float one = 1.0f;
    const Vec3 p{ 300.0f, -1200.0f, 55.0f };
    CHECK(distance(models::blendApply(&q, &one, 1, p), apply(m, p)) < 0.05f);
}

TEST_CASE("blending two bones keeps a bent joint's thickness")
{
    // a point 100 units from the joint, half on a straight bone and half on one bent 90 degrees
    const Affine3 straight = rotZ(0.0f), bent = rotZ(90.0f);
    const DualQuat q[2] = { models::dualQuat(straight), models::dualQuat(bent) };
    const float w[2] = { 0.5f, 0.5f };
    const Vec3 p{ 100.0f, 0.0f, 0.0f };
    const Vec3 dq = models::blendApply(q, w, 2, p);
    // on the circle; blending the matrices would cut across it at 70.7 (29 % thinner)
    CHECK(distance(dq, { 0, 0, 0 }) == doctest::Approx(100.0f).epsilon(0.001));
}

TEST_CASE("a quaternion and its negation blend as the same rotation")
{
    DualQuat q[2] = { models::dualQuat(rotZ(30.0f)), models::dualQuat(rotZ(30.0f)) };
    for (int i = 0; i < 4; ++i)
    {
        q[1].real[i] = -q[1].real[i];
        q[1].dual[i] = -q[1].dual[i];
    }
    const float w[2] = { 0.5f, 0.5f };
    const Vec3 p{ 100.0f, 0.0f, 0.0f };
    CHECK(distance(models::blendApply(q, w, 2, p), apply(rotZ(30.0f), p)) < 0.05f);
}

TEST_CASE("a zoom of one step is a zoom; a long chain of sine-table rotations is not")
{
    Affine3 zoom = models::identityAffine<float>();
    zoom.m[0][0] = 257.0f / 256.0f; // the smallest zoom an animation can hold
    CHECK(models::zooms(zoom));
    // 13 rotations rounded the way the engine's 16-bit sine table rounds them
    Affine3 chain = models::identityAffine<float>();
    for (int i = 0; i < 13; ++i)
    {
        const int angle = 37 + 61 * i; // 10-bit angles
        const float s = std::round(std::sin(angle * 6.28318531f / 1024.0f) * 32767.0f) / 32767.0f;
        const float c = std::round(std::cos(angle * 6.28318531f / 1024.0f) * 32767.0f) / 32767.0f;
        chain = models::compose(Affine3{ { { c, -s, 0, 0 }, { s, c, 0, 0 }, { 0, 0, 1, 0 } } }, chain);
    }
    CHECK_FALSE(models::zooms(chain));
}
