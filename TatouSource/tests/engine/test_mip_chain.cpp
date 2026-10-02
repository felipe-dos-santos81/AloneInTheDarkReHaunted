///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Engine unit tests: the RGBA8 mip chain for HD model textures.
///////////////////////////////////////////////////////////////////////////////

#include "doctest.h"
#include "mipChain.h"

using namespace models;

TEST_CASE("mip chain: each level is the 2x2 average of the one above, down to 1x1")
{
    // 4x2, one channel varying: texel (x, y) has red = 10 * (x + 4 * y).
    std::vector<uint8_t> base(4 * 2 * 4, 255);
    for (int i = 0; i < 8; ++i)
        base[i * 4] = (uint8_t)(10 * i);
    int levels = 0;
    const std::vector<uint8_t> chain = rgba8MipChain(base.data(), 4, 2, &levels);
    CHECK(levels == 3); // 4x2, 2x1, 1x1
    REQUIRE(chain.size() == (8 + 2 + 1) * 4u);
    CHECK(chain[32] == 25); // (0 + 10 + 40 + 50) / 4
    CHECK(chain[36] == 45); // (20 + 30 + 60 + 70) / 4
    CHECK(chain[40] == 35); // (25 + 45) / 2
    CHECK(chain[43] == 255); // alpha stays
}

TEST_CASE("mip chain: odd sizes halve down like bgfx (max(1, n >> 1))")
{
    std::vector<uint8_t> base(5 * 3 * 4, 100);
    int levels = 0;
    const std::vector<uint8_t> chain = rgba8MipChain(base.data(), 5, 3, &levels);
    CHECK(levels == 3); // 5x3, 2x1, 1x1
    CHECK(chain.size() == (15 + 2 + 1) * 4u);
    CHECK(chain.back() == 100);
}

TEST_CASE("mip chain: a 1x1 texture is its own only level")
{
    const uint8_t texel[4] = { 1, 2, 3, 4 };
    int levels = 0;
    const std::vector<uint8_t> chain = rgba8MipChain(texel, 1, 1, &levels);
    CHECK(levels == 1);
    CHECK(chain == std::vector<uint8_t>{ 1, 2, 3, 4 });
}
