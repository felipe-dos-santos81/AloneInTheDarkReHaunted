///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: HUD icon layout and forgiving hit boxes (320x200 space).
// Engine-free: standard headers only.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>

#include "mouseTypes.h"

namespace mouse
{

enum class HudIcon : uint8_t
{
    Inventory = 0,
    Map = 1,
    Menu = 2,
};

constexpr int kHudIconCount = 3;
constexpr int kHudIconW = 20;
constexpr int kHudIconH = 16;
constexpr int kHudIconY = 4;
constexpr int kHudIconX[kHudIconCount] = { 4, 28, 52 };
constexpr int kHitPad = 2;
constexpr int kHitMinimum = 12;

inline std::array<Rect, kHudIconCount> hudIconRects()
{
    std::array<Rect, kHudIconCount> r{};
    for (int i = 0; i < kHudIconCount; ++i)
        r[i] = Rect{ kHudIconX[i], kHudIconY, kHudIconX[i] + kHudIconW - 1, kHudIconY + kHudIconH - 1 };
    return r;
}

// One axis of forgivingBox: pad, grow to the minimum size (centred), then
// slide inside [0, last].
inline void forgivingSpan(int& lo, int& hi, int last)
{
    lo -= kHitPad;
    hi += kHitPad;
    const int size = hi - lo + 1;
    if (size < kHitMinimum)
    {
        const int grow = kHitMinimum - size;
        lo -= grow / 2;
        hi += grow - grow / 2;
    }
    if (lo < 0)
    {
        hi -= lo;
        lo = 0;
    }
    if (hi > last)
    {
        lo = std::max(0, lo - (hi - last));
        hi = last;
    }
}

// Pad, grow to the minimum size (centred), then slide inside the 320x200 frame.
inline Rect forgivingBox(Rect box)
{
    forgivingSpan(box.x1, box.x2, kLogicalW - 1);
    forgivingSpan(box.y1, box.y2, kLogicalH - 1);
    return box;
}

// Forgiving HUD hit rects; overlaps between neighbours split at the midpoint.
inline std::array<Rect, kHudIconCount> hudIconHitRects()
{
    const auto visible = hudIconRects();
    std::array<Rect, kHudIconCount> hit{};
    for (int i = 0; i < kHudIconCount; ++i)
        hit[i] = forgivingBox(visible[i]);
    for (int i = 0; i + 1 < kHudIconCount; ++i)
    {
        if (hit[i].x2 >= hit[i + 1].x1)
        {
            const int mid = (visible[i].x2 + visible[i + 1].x1) / 2;
            hit[i].x2 = mid;
            hit[i + 1].x1 = mid + 1;
        }
    }
    return hit;
}

inline std::optional<HudIcon> hudIconAt(Point p)
{
    const auto hit = hudIconHitRects();
    for (int i = 0; i < kHudIconCount; ++i)
        if (contains(hit[i], p))
            return (HudIcon)i;
    return std::nullopt;
}

} // namespace mouse
