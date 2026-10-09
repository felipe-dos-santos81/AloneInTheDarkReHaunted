///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// HD character models: an RGBA8 mip chain for a base-colour texture, as
// bgfx::createTexture2D(hasMips = true) wants it. Engine-free: standard
// headers only.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstdint>
#include <vector>

namespace models
{

// A texture no larger than this on both sides is a palette swatch (the
// identity delivery's 16x16): it gets no mips and point sampling, so every
// texel keeps its exact colour at any distance.
constexpr int kSwatchSide = 64;

// Every level from width x height down to 1x1, concatenated; each level is the
// 2x2 box average of the one above (an odd edge averages the texels it has).
// *levels gets the level count.
std::vector<uint8_t> rgba8MipChain(const uint8_t* rgba, int width, int height, int* levels);

// A texel's alpha class: below kTranslucentAlpha a hole, up to kOpaqueAlpha - 1
// translucent (the engine's transparent material 2, blended at 50 %, drawn in a
// second pass), from kOpaqueAlpha opaque. model_ps.sc and tools/aitd_models/hdm.py
// use the same values (tests/tools/test_models_hdm.py checks).
constexpr uint8_t kTranslucentAlpha = 128;
constexpr uint8_t kOpaqueAlpha = 253;

} // namespace models
