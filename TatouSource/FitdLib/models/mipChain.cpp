///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// HD character models: RGBA8 mip chain. Engine-free.
///////////////////////////////////////////////////////////////////////////////

#include "mipChain.h"

#include <cstring>

namespace models
{

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
                for (int c = 0; c < 4; ++c)
                {
                    int sum = 0, n = 0;
                    for (int dy = 0; dy < 2; ++dy)
                        for (int dx = 0; dx < 2; ++dx)
                        {
                            const int sx = 2 * x + dx, sy = 2 * y + dy;
                            if (sx < w && sy < h)
                            {
                                sum += out[at + ((size_t)sy * w + sx) * 4 + c];
                                ++n;
                            }
                        }
                    next[((size_t)y * nw + x) * 4 + c] = (uint8_t)((sum + n / 2) / n);
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
