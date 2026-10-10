///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// HD character models: a bone matrix as a unit dual quaternion, for
// dual-quaternion skinning (skinned_dq_vs.sc), which keeps a joint's volume
// where blending four matrices linearly thins it. Engine-free: standard
// headers only. The engine adapter is modelReplacement.cpp.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include "affine3.h"

namespace models
{

// Rotation r and translation as (r, d), d = 0.5 (0, t) r; both (x, y, z, w).
struct DualQuat
{
    float real[4];
    float dual[4];
};

// A column length off 1 by more than this is a zoom (animation state type 2;
// its smallest step is 1/256). The engine's sine-table rotations stay far
// inside it: each is off unit length by at most 3.9e-5.
constexpr float kZoomTolerance = 2e-3f;

// The rotation and translation of a bone matrix with no zoom. The rotation is
// read from the matrix and normalised, which absorbs the sine table's error.
DualQuat dualQuat(const Affine3& m);

// Whether the matrix scales along any axis (a zooming bone): such a body
// draws with linear matrix blending instead.
bool zooms(const Affine3& m);

// The shader's blend, for tests: up to four (dual quaternion, weight) pairs,
// each sign-aligned to the first, the sum normalised, applied to a point.
Vec3 blendApply(const DualQuat* q, const float* w, int count, Vec3 p);

} // namespace models
