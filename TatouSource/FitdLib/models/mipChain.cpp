///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// HD character models: RGBA8 mip chain. Engine-free.
///////////////////////////////////////////////////////////////////////////////

#include "mipChain.h"

#include <cstring>

namespace models
{

namespace
{
int alphaClass(int a) // 0 a hole, 1 translucent, 2 opaque
{
    return a < kTranslucentAlpha ? 0 : a < kOpaqueAlpha ? 1 : 2;
}
} // namespace

std::vector<uint8_t> rgba8MipChain(const uint8_t* rgba, int width, int height, int* levels)
{
    std::vector<uint8_t> out(rgba, rgba + (size_t)width * height * 4);
    size_t at = 0; // start of the current level in out
    int w = width, h = height, count = 1;
    while (w > 1 || h > 1)
    {
        const int nw = w > 1 ? w / 2 : 1, nh = h > 1 ? h / 2 : 1;
        std::vector<uint8_t> next((size_t)nw * nh * 4);
        for (int y = 0; y < nh; ++y)
            for (int x = 0; x < nw; ++x)
            {
                // colour: the plain average. Alpha: the class most texels have, a tie
                // to the more opaque one, then the average of that class's texels, so
                // no level makes a new class.
                int rgb[3] = { 0, 0, 0 }, count = 0, alpha[3] = { 0, 0, 0 }, inClass[3] = { 0, 0, 0 };
                for (int dy = 0; dy < 2; ++dy)
                    for (int dx = 0; dx < 2; ++dx)
                    {
                        const int sx = 2 * x + dx, sy = 2 * y + dy;
                        if (sx >= w || sy >= h)
                            continue;
                        const uint8_t* texel = &out[at + ((size_t)sy * w + sx) * 4];
                        for (int c = 0; c < 3; ++c)
                            rgb[c] += texel[c];
                        ++count;
                        const int k = alphaClass(texel[3]);
                        alpha[k] += texel[3];
                        ++inClass[k];
                    }
                int k = 0;
                for (int i = 1; i < 3; ++i)
                    if (inClass[i] >= inClass[k])
                        k = i;
                uint8_t* dst = &next[((size_t)y * nw + x) * 4];
                for (int c = 0; c < 3; ++c)
                    dst[c] = (uint8_t)((rgb[c] + count / 2) / count);
                dst[3] = (uint8_t)((alpha[k] + inClass[k] / 2) / inClass[k]);
            }
        at = out.size();
        out.insert(out.end(), next.begin(), next.end());
        w = nw;
        h = nh;
        ++count;
    }
    if (levels)
        *levels = count;
    return out;
}

} // namespace models
