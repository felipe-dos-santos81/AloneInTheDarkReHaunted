///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// HD character models: from a body's pose and the camera to the matrices the
// skinned vertex shader gets (u_model[]), and the screen box the replacement
// covers. Engine-free: standard headers only.
//
// A bone matrix maps a vertex of the .hdm (engine space, rest pose) to camera
// space: view ∘ T(actor) ∘ world(pose) ∘ world(rest)⁻¹. The inverse binds come
// from the loaded body, never from the file.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "affine3.h"
#include "bodyPose.h"
#include "hdmMesh.h"
#include "renderCamera.h"

namespace models
{

struct Box3
{
    float lo[3] = { 0, 0, 0 };
    float hi[3] = { 0, 0, 0 };
    bool empty = true;
};

struct SkinBind
{
    std::vector<Affine3> inverseBind; // one per group
    std::vector<Box3> groupBoxes;     // rest-pose box of the vertices each group moves (weight > 0)
};

// Checks the mesh belongs to `body` (group count and skeleton hash) and
// prepares its binds. On failure returns false and names the reason in *why.
bool prepareSkin(const PoseBody& body, const HdmMesh& mesh, const int16_t* sinTable, SkinBind* out, std::string* why);

// bones[g] for every group; false (nothing written) when poseGroups refuses
// the pose. (x, y, z) is the actor's position as AffObjet gets it.
bool boneMatrices(const PoseBody& body, const GroupState* states, int alpha, int beta, int gamma, int x, int y,
                  int z, const RenderCamera& cam, const SkinBind& skin, Affine3* bones);

// The 320x200 box [x0, y0, x1, y1] covering every group box's corners that lie
// in front of the engine's near clip (camera z > 50), clamped to the screen.
// False when no corner is in front.
bool screenBox(const Affine3* bones, const SkinBind& skin, const ProjParams& p, int box[4]);

// An affine as bgfx's 4x4 float matrix (column-major: element (row r, column
// c) at out[c * 4 + r]), what bgfx::setTransform uploads to u_model[].
void columnMajor(const Affine3& a, float out[16]);

} // namespace models
