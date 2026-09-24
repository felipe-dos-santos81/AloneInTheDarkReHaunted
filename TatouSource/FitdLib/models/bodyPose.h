///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// HD character models: the pose of an AITD1 animated body as one affine
// matrix per bone group. Engine-free: standard headers only.
//
// poseGroups reproduces AnimNuage's non-INFO_OPTIMISE path (renderer.cpp,
// InitGroupeRot .. ZoomGroupe and the AnimNuage loops) without its integer
// truncation, so it equals the engine's loops run in exact arithmetic; the
// integer path drifts from both by about 9 units per level of group depth.
// tools/aitd_models/pose.py (pose_float) is the Python twin.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "affine3.h"

namespace models
{

constexpr int kMaxPoseGroups = 32; // bgfx u_model[32]
constexpr uint16_t kInfoAnim = 0x2;
constexpr uint16_t kInfoOptimise = 0x8;

// sGroup, with vertex indices (not byte offsets), as createBodyFromPtr stores them.
struct PoseGroup
{
    int16_t start;  // m_start: first vertex
    int16_t count;  // m_numVertices
    int16_t pivot;  // m_baseVertices: the vertex (in the parent group) this group hangs from
    int8_t parent;  // m_orgGroup, -1 for the root
    int8_t self;    // m_numGroup
};

struct PoseBody
{
    std::vector<std::array<int16_t, 3>> verts; // m_vertices: rest, each group in its own frame
    std::vector<PoseGroup> groups;
    std::vector<uint16_t> order;               // m_groupOrder: group indices, leaves first
    uint16_t flags = kInfoAnim;
};

// sGroupState: type 0 rotates (10-bit angles), 1 translates, 2 zooms by
// (d + 256) / 256; any other type is a no-op.
struct GroupState
{
    int16_t type;
    int16_t dx, dy, dz;
};

// The skeleton invariants the pose math relies on (all 214 AITD1 animated
// bodies hold them). On failure, *why (when given) names the first broken one.
bool validateSkeleton(const PoseBody& body, std::string* why);

// FNV-1a 64 over: u16 vertex count, every vertex as 3 x s16, u16 group count,
// per group (s16 start, s16 count, s16 pivot, s8 parent, s8 self), then the
// order as u16 each; little-endian. tools/aitd_models/skeleton.py computes the
// same value.
uint64_t skeletonHash(const PoseBody& body);

// worldFromLocal[g] maps group g's stored vertices to model space, as
// AnimNuage leaves them in pointBuffer. `states` has one entry per group;
// group 0's delta is replaced by (alpha, beta, gamma), as the engine does.
// `sinTable` is the engine's cosTable (a sine table: entry i = sin(i * 2pi / 1024)).
// Requires validateSkeleton(body). Returns false, and writes nothing, when
// group 0 translates by a non-zero delta: the engine then adds root vertex 0
// to itself, which no matrix reproduces (no AITD1 animation does this).
bool poseGroups(const PoseBody& body, const GroupState* states, int alpha, int beta, int gamma,
                const int16_t* sinTable, Affine3* worldFromLocal);

} // namespace models
