///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: shared value types. Engine-free: standard headers only.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstdint>
#include <vector>

namespace mouse
{

// The game's logical screen: every hit test and every pick works in it.
constexpr int kLogicalW = 320;
constexpr int kLogicalH = 200;

// A logical 320x200 screen position.
struct Point
{
    int x = 0;
    int y = 0;
};

inline bool operator==(Point a, Point b) { return a.x == b.x && a.y == b.y; }
inline bool operator!=(Point a, Point b) { return !(a == b); }

// A horizontal world position (room-scale units, in some room's frame).
struct XZ
{
    int x = 0;
    int z = 0;
};

inline bool operator==(XZ a, XZ b) { return a.x == b.x && a.z == b.z; }
inline bool operator!=(XZ a, XZ b) { return !(a == b); }

// Window position (SDL window points, not pixels) -> logical 320x200.
// The game view fills the whole window (bgfxGlue.cpp setViewRect), so this
// is a plain stretch with no letterbox. False when outside the window.
inline bool windowToLogical(float wx, float wy, int winW, int winH, Point* out)
{
    if (winW <= 0 || winH <= 0)
        return false;
    if (wx < 0.0f || wy < 0.0f || wx >= (float)winW || wy >= (float)winH)
        return false;
    int x = (int)(wx * (float)kLogicalW / (float)winW);
    int y = (int)(wy * (float)kLogicalH / (float)winH);
    out->x = x > kLogicalW - 1 ? kLogicalW - 1 : x;
    out->y = y > kLogicalH - 1 ? kLogicalH - 1 : y;
    return true;
}

} // namespace mouse
