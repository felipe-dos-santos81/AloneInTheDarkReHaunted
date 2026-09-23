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

// Inclusive rectangle in logical coordinates.
struct Rect
{
    int x1 = 0;
    int y1 = 0;
    int x2 = 0;
    int y2 = 0;
};

inline bool operator==(const Rect& a, const Rect& b)
{
    return a.x1 == b.x1 && a.y1 == b.y1 && a.x2 == b.x2 && a.y2 == b.y2;
}

inline bool contains(const Rect& r, Point p)
{
    return p.x >= r.x1 && p.x <= r.x2 && p.y >= r.y1 && p.y <= r.y2;
}

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

// Pad, grow to the minimum size (centred), then slide inside the 320x200 frame.
inline Rect forgivingBox(Rect box, int pad = kHitPad, int minimum = kHitMinimum)
{
    Rect r{ box.x1 - pad, box.y1 - pad, box.x2 + pad, box.y2 + pad };
    const int w = r.x2 - r.x1 + 1;
    if (w < minimum)
    {
        const int grow = minimum - w;
        r.x1 -= grow / 2;
        r.x2 += grow - grow / 2;
    }
    const int h = r.y2 - r.y1 + 1;
    if (h < minimum)
    {
        const int grow = minimum - h;
        r.y1 -= grow / 2;
        r.y2 += grow - grow / 2;
    }
    if (r.x1 < 0)
    {
        r.x2 -= r.x1;
        r.x1 = 0;
    }
    if (r.x2 > kLogicalW - 1)
    {
        r.x1 = std::max(0, r.x1 - (r.x2 - (kLogicalW - 1)));
        r.x2 = kLogicalW - 1;
    }
    if (r.y1 < 0)
    {
        r.y2 -= r.y1;
        r.y1 = 0;
    }
    if (r.y2 > kLogicalH - 1)
    {
        r.y1 = std::max(0, r.y1 - (r.y2 - (kLogicalH - 1)));
        r.y2 = kLogicalH - 1;
    }
    return r;
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
