///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// HD character models: the light a replacement mesh is shaded with, in the
// engine camera's space (what model_ps.sc works in). Engine-free: standard
// headers only.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include "affine3.h"
#include "renderCamera.h"

namespace models
{

// Restrained shading that keeps a lit body near its texture's own colours, as
// the unlit classic body shows its palette (checked in docs/hd-models-checklist.md).
constexpr float kAmbientGround = 0.85f; // hemisphere ambient facing the floor
constexpr float kAmbientSky = 1.10f;    // ... facing up
constexpr float kKeyStrength = 0.30f;   // Lambert key light, from the planar-shadow direction
constexpr float kSpecular = 0.15f;      // mild Blinn highlight
constexpr float kShininess = 24.0f;
constexpr float kLanternReach = 2500.0f; // engine units (about 2.5 m) a held lantern lights a model to

// A world direction in camera space (rotation only), unit length; (0, 0, 0)
// stays (0, 0, 0).
Vec3 cameraDirection(const RenderCamera& cam, Vec3 worldDir);

// A world point in camera space (models::viewMatrix).
Vec3 cameraPoint(const RenderCamera& cam, Vec3 worldPoint);

} // namespace models
