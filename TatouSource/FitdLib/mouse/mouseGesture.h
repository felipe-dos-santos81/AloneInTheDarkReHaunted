///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: the pointer's gesture state and its transitions.
// Engine-free: standard headers only. Port of m-aitd app/controls/pointer.py.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstdint>
#include <functional>
#include <optional>

#include "mouseTypes.h"

namespace mouse
{

enum class ClickKind : uint8_t
{
    Blocked,
    Walk,
    Steer,
    Target,
    Push,
    Attack,
    HudInventory,
    HudMap,
    HudMenu,
};

inline bool isHud(ClickKind kind)
{
    return kind == ClickKind::HudInventory || kind == ClickKind::HudMap || kind == ClickKind::HudMenu;
}

// Walk/Steer/Target/Push: destination x,z in `room`'s frame, and the clicked
// world object (-1 for walk/steer). Attack: the target actor.
struct Payload
{
    int x = 0;
    int z = 0;
    int room = -1;
    int object = -1; // world object index
    int actor = -1;  // actor index (Attack only)
};

inline bool operator==(const Payload& a, const Payload& b)
{
    return a.x == b.x && a.z == b.z && a.room == b.room && a.object == b.object && a.actor == b.actor;
}
inline bool operator!=(const Payload& a, const Payload& b) { return !(a == b); }

struct ClickResult
{
    ClickKind kind = ClickKind::Blocked;
    Payload payload;
};

using Resolver = std::function<ClickResult(Point)>;

// How far the pointer may drift between the halves of a double press and still
// resume the first half's destination.
constexpr int kResumePx = 6;
// After a camera cut the pointer must move this far on either axis before a
// held follow re-resolves against the new camera.
constexpr int kCutDeadZonePx = 6;

struct PointerState
{
    bool held = false;
    std::optional<Point> pos;
    // Last destination issued during this hold; re-issued only when the
    // resolution differs (also the one-shot latch after an arrival).
    std::optional<ClickResult> follow;
    // Pixel follow was resolved at; empty = resolve next frame regardless.
    std::optional<Point> followPos;
    // Camera slot followPos was resolved under; a mismatch means a cut.
    std::optional<int> followCamera;
    // Where the pointer was when a cut was noticed.
    std::optional<Point> settleOrigin;
    // The press resolved to HUD/attack/push: no follow on this hold.
    bool spent = false;
    // This hold began with a double press: the hero runs.
    bool run = false;
    // What the hold that just ended was heading for, for a double press.
    std::optional<ClickResult> resume;
    std::optional<Point> resumePos;
};

enum class DecisionType : uint8_t
{
    Nothing,
    Cancel,
    OpenHud,
    Attack,
    Issue,
};

struct Decision
{
    DecisionType type = DecisionType::Nothing;
    ClickKind kind = ClickKind::Blocked;
    Payload payload;
    bool run = false;
};

void resetPointer(PointerState& s);
void onPress(PointerState& s, std::optional<Point> pos);
void onMove(PointerState& s, std::optional<Point> pos);
bool settling(const PointerState& s);

// What a press means. `clicks` is SDL's consecutive-click count for this press.
Decision pressDecision(PointerState& s, Point pos, int clicks, int camera,
                       const Resolver& resolve, bool latchedPush);

// What a held pointer means this frame: re-aim, once per frame in which it moved.
Decision holdDecision(PointerState& s, std::optional<Point> pos, int camera,
                      const Resolver& resolve, bool latchedPush, bool intentAlive);

void dropDestination(PointerState& s);
// Button-up: the hold ends. A steer is never stashed for resume.
void endHold(PointerState& s, bool steering);
// A floor change: the destination indexes an unloaded floor, but the hold survives.
void rebase(PointerState& s);

} // namespace mouse
