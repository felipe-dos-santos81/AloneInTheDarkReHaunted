///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: gesture rules. Engine-free.
///////////////////////////////////////////////////////////////////////////////

#include "mouseGesture.h"

#include <cstdlib>

namespace mouse
{

void resetPointer(PointerState& s)
{
    s = PointerState{};
}

void onPress(PointerState& s, std::optional<Point> pos)
{
    s.held = true;
    s.pos = pos;
}

void onMove(PointerState& s, std::optional<Point> pos)
{
    s.pos = pos;
}

void onRelease(PointerState& s)
{
    s.held = false;
    s.pos.reset();
}

bool settling(const PointerState& s)
{
    return s.settleOrigin.has_value();
}

namespace
{
// The second press of a double press is the same finger on the same spot
// saying "faster": it resumes what the first press committed to.
bool resumeDestination(const PointerState& s, Point pos, Payload* payload, ClickKind* kind)
{
    if (!s.run || !s.resumeLast || !s.resumePos)
        return false;
    if (std::abs(pos.x - s.resumePos->x) > kResumePx || std::abs(pos.y - s.resumePos->y) > kResumePx)
        return false;
    *payload = *s.resumeLast;
    *kind = s.resumeKind;
    return true;
}
}

Decision pressDecision(PointerState& s, Point pos, int clicks, int camera,
                       const Resolver& resolve, bool latchedPush)
{
    s.run = clicks >= 2;
    const ClickResult r = resolve(pos);
    Decision d;
    if (isHud(r.kind))
    {
        s.spent = true;
        d.type = DecisionType::OpenHud;
        d.kind = r.kind;
        return d;
    }
    if (r.kind == ClickKind::Attack)
    {
        s.spent = true;
        d.type = DecisionType::Attack;
        d.kind = r.kind;
        d.payload = r.payload;
        return d;
    }
    if (latchedPush || r.kind == ClickKind::Blocked)
        return d;

    Payload payload = r.payload;
    ClickKind kind = r.kind;
    const bool isPush = kind == ClickKind::Push;
    if (!isPush)
        resumeDestination(s, pos, &payload, &kind);

    if (isPush)
    {
        s.followLast.reset();
        s.followPos.reset();
        s.followCamera.reset();
    }
    else
    {
        s.followLast = payload;
        s.followKind = kind;
        s.followPos = pos;
        s.followCamera = camera;
    }
    s.settleOrigin.reset();
    s.spent = isPush;

    d.type = DecisionType::Issue;
    d.kind = kind;
    d.payload = payload;
    d.run = s.run;
    return d;
}

Decision holdDecision(PointerState& s, std::optional<Point> pos, int camera,
                      const Resolver& resolve, bool latchedPush, bool intentAlive)
{
    Decision d;
    if (!s.held || s.spent || latchedPush)
        return d;
    if (pos == s.followPos)
        return d;
    if (!pos)
        return d; // outside the window: nothing to resolve or settle against
    if (s.followCamera && *s.followCamera != camera)
    {
        if (!s.settleOrigin)
            s.settleOrigin = s.followPos ? *s.followPos : *pos;
        if (std::abs(pos->x - s.settleOrigin->x) <= kCutDeadZonePx &&
            std::abs(pos->y - s.settleOrigin->y) <= kCutDeadZonePx)
            return d;
    }
    // every path that advances followCamera closes the dead zone with it
    s.settleOrigin.reset();
    s.followPos = pos;
    s.followCamera = camera;

    const ClickResult r = resolve(*pos);
    if (r.kind == ClickKind::Walk || r.kind == ClickKind::Target || r.kind == ClickKind::Steer)
    {
        if (s.followLast && *s.followLast == r.payload)
            return d;
        s.followLast = r.payload;
        s.followKind = r.kind;
        d.type = DecisionType::Issue;
        d.kind = r.kind;
        d.payload = r.payload;
        d.run = s.run; // the run belongs to the hold, not to the destination
        return d;
    }
    if (r.kind == ClickKind::Blocked)
    {
        s.followLast.reset();
        if (intentAlive)
            d.type = DecisionType::Cancel;
        return d;
    }
    return d; // HUD, attack and push need a fresh press
}

void dropDestination(PointerState& s)
{
    s.followLast.reset();
    s.followPos.reset();
    s.followCamera.reset();
    s.settleOrigin.reset();
}

void endHold(PointerState& s, bool steering)
{
    s.spent = false;
    s.run = false;
    if (steering)
        s.resumeLast.reset();
    else
        s.resumeLast = s.followLast;
    s.resumeKind = s.followKind;
    s.resumePos = s.followPos;
    dropDestination(s);
}

void rebase(PointerState& s)
{
    s.resumeLast.reset();
    s.resumePos.reset();
    dropDestination(s);
}

} // namespace mouse
