///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Engine unit tests: synthetic AITD1 bodies for the model pose tests.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

#include "bodyPose.h"

// Seeded integers that are the same on every standard library
// (std::uniform_int_distribution is not): a fixed sweep on every platform.
struct TestRng
{
    std::mt19937 gen;
    explicit TestRng(uint32_t seed) : gen(seed) {}
    int pick(int lo, int hi) { return lo + (int)(gen() % (uint32_t)(hi - lo + 1)); }
};

// tests/tools/model_helpers.py's chain body: a root, a spine (group 1) with
// a head (group 2), and a leg (group 3) hanging from the root; leaves first.
// Its skeleton hash is pinned on both sides (CHAIN_HASH in test_models_skeleton.py).
inline models::PoseBody chainBody()
{
    models::PoseBody b;
    b.verts = { { 0, 0, 0 },    { 0, -100, 0 }, { 50, 0, 0 },  // group 0: origin, pivot of 1, pivot of 3
                { 0, -200, 0 }, { 0, -100, 0 },                // group 1: tip, pivot of 2
                { 30, -50, 0 }, { 0, -80, 20 },                // group 2
                { 0, 100, 0 },  { 20, 100, 10 } };             // group 3
    b.groups = { { 0, 3, 0, -1, 0 }, { 3, 2, 1, 0, 1 }, { 5, 2, 4, 1, 2 }, { 7, 2, 2, 0, 3 } };
    b.order = { 2, 3, 1, 0 };
    return b;
}

// Every vertex of chainBody() at rest (the pivot chain applied).
inline std::vector<std::array<double, 3>> chainRest()
{
    return { { 0, 0, 0 },    { 0, -100, 0 }, { 50, 0, 0 },  { 0, -300, 0 }, { 0, -200, 0 },
             { 30, -250, 0 }, { 0, -280, 20 }, { 50, 100, 0 }, { 70, 100, 10 } };
}

inline models::PoseBody oneGroupBody()
{
    models::PoseBody b;
    b.verts = { { 0, 0, 0 }, { 40, -90, 10 }, { -30, -60, 25 } };
    b.groups = { { 0, 3, 0, -1, 0 } };
    b.order = { 0 };
    return b;
}

// Three bones in a line (0 -> 1 -> 2), two vertices each.
inline models::PoseBody threeBoneBody()
{
    models::PoseBody b;
    b.verts = { { 0, 0, 0 }, { 0, -120, 0 }, { 10, -60, 5 }, { 0, -110, 0 }, { -15, -50, 10 }, { 0, -90, 20 } };
    b.groups = { { 0, 2, 0, -1, 0 }, { 2, 2, 1, 0, 1 }, { 4, 2, 3, 1, 2 } };
    b.order = { 2, 1, 0 };
    return b;
}

// 18 groups with LISTBODY_011's (Carnby's) hierarchy and leaves-first order,
// depth up to 7; three made-up vertices per group, the child's pivot one of them.
inline models::PoseBody humanoidBody()
{
    const int parents[18] = { -1, 0, 1, 2, 3, 4, 2, 6, 6, 8, 9, 6, 11, 12, 2, 14, 15, 13 };
    models::PoseBody b;
    b.order = { 5, 7, 10, 16, 17, 4, 9, 13, 15, 3, 8, 12, 14, 11, 6, 2, 1, 0 };
    for (int g = 0; g < 18; ++g)
    {
        const int16_t start = (int16_t)(3 * g);
        const int16_t pivot = g ? (int16_t)(3 * parents[g] + 1 + g % 2) : 0;
        b.groups.push_back({ start, 3, pivot, (int8_t)parents[g], (int8_t)g });
        for (int k = 0; k < 3; ++k)
            b.verts.push_back({ (int16_t)(g == 0 && k == 0 ? 0 : 7 * (g % 5) - 14 + 9 * k),
                                (int16_t)(g == 0 && k == 0 ? 0 : -30 - 11 * k + 3 * (g % 4)),
                                (int16_t)(g == 0 && k == 0 ? 0 : 5 * k - 2 * (g % 3)) });
    }
    return b;
}

inline std::vector<int> groupDepths(const models::PoseBody& b)
{
    std::vector<int> depth(b.groups.size(), 0);
    for (size_t g = 1; g < b.groups.size(); ++g)
        depth[g] = depth[b.groups[g].parent] + 1;
    return depth;
}

inline std::vector<models::GroupState> restStates(const models::PoseBody& b)
{
    return std::vector<models::GroupState>(b.groups.size(), models::GroupState{ 0, 0, 0, 0 });
}

// Every vertex through its own group's matrix: what a GPU does with rigid weights.
inline std::vector<std::array<double, 3>> skinVertices(const models::PoseBody& b, const models::Affine3* m)
{
    std::vector<std::array<double, 3>> out(b.verts.size());
    for (size_t g = 0; g < b.groups.size(); ++g)
        for (int v = b.groups[g].start; v < b.groups[g].start + b.groups[g].count; ++v)
        {
            const models::Vec3 p = models::apply(m[g], { (float)b.verts[v][0], (float)b.verts[v][1], (float)b.verts[v][2] });
            out[v] = { p.x, p.y, p.z };
        }
    return out;
}

inline double maxDistance(const std::vector<std::array<double, 3>>& a, const std::vector<std::array<double, 3>>& b)
{
    double worst = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
        for (int c = 0; c < 3; ++c)
            worst = std::max(worst, std::abs(a[i][c] - b[i][c]));
    return worst;
}
