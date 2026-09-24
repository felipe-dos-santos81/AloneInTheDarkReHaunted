///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: the engine's cover-zone polygon test, shared by the walk grid
// and floor picking so both agree with the engine on what is inside.
// Engine-free: standard headers only.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <vector>

#include "mouseTypes.h"

namespace mouse
{

constexpr int kCoverScale = 10; // cover-zone unit -> room units

// Replica of the engine's testCrossProduct (main.cpp).
int testCrossProduct(int x1, int z1, int x2, int z2, int x3, int z3, int x4, int z4);
// The engine's isInPoly for one polygon and a point (cover units): inside when
// both the -X and +X 10000-unit rays hit an edge.
bool insideTwoRay(int x, int z, const std::vector<XZ>& poly);
// A room-unit point against a cover-unit polygon, by insideTwoRay.
bool insideCoverZone(int x, int z, const std::vector<XZ>& coverPoly);
// Floor division (Python //) for negative coordinates.
int floorDiv(int a, int b);

} // namespace mouse
