///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// HD character models: the engine's camera and 320x200 projection in float.
// Engine-free: standard headers only.
//
// viewMatrix is the float twin of the actor-vertex path in renderer.cpp
// (room point - camera position, height clamp aside, then transformPoint)
// plus cameraPerspective on z; clipFromView is the CPU twin of the skinned
// vertex shader. After the divide, x and y land where the classic renderer
// puts the vertex and depth is z / 40960, flat_vs.sc's encoding.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstdint>

#include "affine3.h"

namespace models
{

// What SetAngleCamera, SetPosCamera and SetProjection store (main.cpp).
struct RenderCamera
{
    int alpha = 0, beta = 0, gamma = 0; // camera angles, 10-bit
    int posX = 0, posY = 0, posZ = 0;   // translateX/Y/Z: camera position in the room frame
    int persp = 0;                      // cameraPerspective
    int fovX = 0, fovY = 0;             // cameraFovX / cameraFovY
    int centerX = 160, centerY = 100;   // cameraCenterX / cameraCenterY
    const int16_t* table = nullptr;     // the engine's cosTable (a sine table)
};

// clip = (x px + pz Z, -y py + pw Z, Z^2 / 40960, Z) for a camera-space point.
struct ProjParams
{
    float px, py, pz, pw;
};

struct Vec4
{
    float x, y, z, w;
};

constexpr float kDepthScale = 40960.0f; // flat_vs.sc: a_position.z / 40960

// Camera space (cameraPerspective already added to z) from the room frame:
// T(0, 0, persp) ∘ R ∘ T(-pos), R = Rz Rx Ry from SetAngleCamera. The camera
// masks each angle before testing it for zero, so 1024 is no rotation (the
// bone path tests the raw angle). An actor at (x, y, z) is
// compose(viewMatrix(cam), translationAffine<float>(x, y, z)).
Affine3 viewMatrix(const RenderCamera& cam);

// The screen shake (g_shakeOffsetX/Y) moves every vertex by whole logical pixels.
ProjParams projParams(const RenderCamera& cam, float shakeX, float shakeY);

Vec4 clipFromView(Vec3 pc, const ProjParams& p);

} // namespace models
