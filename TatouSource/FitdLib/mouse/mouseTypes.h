///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: shared value types. Engine-free: standard headers only.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstdint>
#include <optional>
#include <utility>
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
// is a plain stretch with no letterbox. Nothing when outside the window.
inline std::optional<Point> windowToLogical(float wx, float wy, int winW, int winH)
{
    if (winW <= 0 || winH <= 0)
        return std::nullopt;
    if (wx < 0.0f || wy < 0.0f || wx >= (float)winW || wy >= (float)winH)
        return std::nullopt;
    const int x = (int)(wx * (float)kLogicalW / (float)winW);
    const int y = (int)(wy * (float)kLogicalH / (float)winH);
    return Point{ x > kLogicalW - 1 ? kLogicalW - 1 : x, y > kLogicalH - 1 ? kLogicalH - 1 : y };
}

// One left-button / pointer event, already mapped to logical coordinates.
enum class EventType : uint8_t
{
    Motion,
    Down,
    Up,
    FocusLost,
};

struct Event
{
    EventType type = EventType::Motion;
    std::optional<Point> pos; // Motion/Down, inside the window only
    int clicks = 0;           // SDL consecutive-click count (Down only)
};

// Everything the game thread learns about the mouse in one frame.
struct Frame
{
    std::vector<Event> events;
    std::optional<Point> pos; // current pointer, inside the window only
    bool leftDown = false;
    bool blocked = false; // F1 dialog open or ImGui wants the mouse
};

// Main thread pushes events and publishes once per frame; the game thread takes
// at most once per frame. The engine's render semaphores serialize the two
// sides, so no lock is needed. A frame nobody took is replaced (screens that do
// not take frames have no use for world events).
class FrameQueue
{
public:
    void push(const Event& e) { pending_.push_back(e); }

    void publish(std::optional<Point> pos, bool leftDown, bool blocked)
    {
        ready_.events.swap(pending_);
        pending_.clear();
        ready_.pos = pos;
        ready_.leftDown = leftDown;
        ready_.blocked = blocked;
        hasReady_ = true;
    }

    bool take(Frame* out)
    {
        if (!hasReady_)
            return false;
        *out = std::move(ready_);
        ready_.events.clear();
        hasReady_ = false;
        return true;
    }

private:
    std::vector<Event> pending_;
    Frame ready_;
    bool hasReady_ = false;
};

// OS cursor shapes (SDL3 system cursors).
enum class CursorShape : uint8_t
{
    Default,
    Pointer,
    Crosshair,
    Move,
    NotAllowed,
    Text,       // ImGui windows only (see mouseInputEndMainFrame)
    ResizeNS,
    ResizeEW,
    ResizeNESW,
    ResizeNWSE,
    Count,
};

} // namespace mouse
