#include "doctest.h"
#include "mouseGesture.h"

using namespace mouse;

namespace
{
// A resolver that answers `result` and counts how often it was asked.
struct FakeResolver
{
    ClickResult result;
    int calls = 0;
    Resolver fn()
    {
        return [this](Point) { ++calls; return result; };
    }
};

const Payload kWalkA{ 1000, 2000, 3, -1 };
const Payload kWalkB{ 1500, 2500, 3, -1 };
const Payload kTarget{ 900, 900, 3, 12 };
const Payload kPush{ 700, 800, 3, 4 };
const Payload kSteer{ 12000, 0, 3, -1 };
const Point kAt{ 100, 120 };

PointerState heldAfterPress(FakeResolver& r, Point at = kAt, int clicks = 1, int camera = 0)
{
    PointerState s;
    onPress(s, at);
    pressDecision(s, at, clicks, camera, r.fn(), false);
    return s;
}
}

TEST_CASE("a walk press opens a held follow")
{
    FakeResolver r{ { ClickKind::Walk, kWalkA } };
    PointerState s;
    onPress(s, kAt);
    Decision d = pressDecision(s, kAt, 1, 2, r.fn(), false);
    CHECK(d.type == DecisionType::Issue);
    CHECK(d.kind == ClickKind::Walk);
    CHECK(d.payload == kWalkA);
    CHECK_FALSE(d.run);
    CHECK(s.followLast == kWalkA);
    CHECK(s.followPos == kAt);
    CHECK(s.followCamera == 2);
    CHECK_FALSE(s.spent);
}

TEST_CASE("a blocked press does nothing and opens no follow")
{
    FakeResolver r{ { ClickKind::Blocked, {} } };
    PointerState s;
    onPress(s, kAt);
    CHECK(pressDecision(s, kAt, 1, 0, r.fn(), false).type == DecisionType::Nothing);
    CHECK_FALSE(s.followLast.has_value());
}

TEST_CASE("HUD, attack and push presses spend the hold")
{
    SUBCASE("HUD")
    {
        FakeResolver r{ { ClickKind::HudMap, {} } };
        PointerState s;
        onPress(s, kAt);
        Decision d = pressDecision(s, kAt, 1, 0, r.fn(), false);
        CHECK(d.type == DecisionType::OpenHud);
        CHECK(d.kind == ClickKind::HudMap);
        CHECK(s.spent);
    }
    SUBCASE("attack")
    {
        FakeResolver r{ { ClickKind::Attack, Payload{ 0, 0, -1, 7 } } };
        PointerState s;
        onPress(s, kAt);
        Decision d = pressDecision(s, kAt, 1, 0, r.fn(), false);
        CHECK(d.type == DecisionType::Attack);
        CHECK(d.payload.object == 7);
        CHECK(s.spent);
    }
    SUBCASE("push")
    {
        FakeResolver r{ { ClickKind::Push, kPush } };
        PointerState s;
        onPress(s, kAt);
        Decision d = pressDecision(s, kAt, 1, 0, r.fn(), false);
        CHECK(d.type == DecisionType::Issue);
        CHECK(d.kind == ClickKind::Push);
        CHECK(s.spent);
        CHECK_FALSE(s.followLast.has_value()); // a push is latched, never re-resolved
    }
}

TEST_CASE("a press while a push is latched only answers HUD and attack")
{
    FakeResolver walk{ { ClickKind::Walk, kWalkA } };
    PointerState s;
    onPress(s, kAt);
    CHECK(pressDecision(s, kAt, 1, 0, walk.fn(), true).type == DecisionType::Nothing);
    FakeResolver hud{ { ClickKind::HudInventory, {} } };
    CHECK(pressDecision(s, kAt, 1, 0, hud.fn(), true).type == DecisionType::OpenHud);
}

TEST_CASE("run comes only from the press being decided having clicks >= 2")
{
    FakeResolver r{ { ClickKind::Walk, kWalkA } };
    PointerState s;
    onPress(s, kAt);
    CHECK(pressDecision(s, kAt, 2, 0, r.fn(), false).run);
    CHECK(s.run);
    onRelease(s);
    endHold(s, false);
    onPress(s, kAt);
    CHECK_FALSE(pressDecision(s, kAt, 1, 0, r.fn(), false).run);
}

TEST_CASE("a double press resumes the first press's destination within the resume radius")
{
    FakeResolver first{ { ClickKind::Walk, kWalkA } };
    PointerState s = heldAfterPress(first);
    onRelease(s);
    endHold(s, false);

    FakeResolver second{ { ClickKind::Walk, kWalkB } };
    Point near{ kAt.x + kResumePx, kAt.y - kResumePx };
    onPress(s, near);
    Decision d = pressDecision(s, near, 2, 0, second.fn(), false);
    CHECK(d.run);
    CHECK(d.payload == kWalkA);
    CHECK(d.kind == ClickKind::Walk);
}

TEST_CASE("a double press beyond the resume radius, or a single press, picks afresh")
{
    FakeResolver first{ { ClickKind::Walk, kWalkA } };
    FakeResolver second{ { ClickKind::Walk, kWalkB } };

    PointerState far = heldAfterPress(first);
    onRelease(far);
    endHold(far, false);
    Point away{ kAt.x + kResumePx + 1, kAt.y };
    onPress(far, away);
    CHECK(pressDecision(far, away, 2, 0, second.fn(), false).payload == kWalkB);

    PointerState single = heldAfterPress(first);
    onRelease(single);
    endHold(single, false);
    onPress(single, kAt);
    CHECK(pressDecision(single, kAt, 1, 0, second.fn(), false).payload == kWalkB);
}

TEST_CASE("a steer is never stashed for resume")
{
    FakeResolver steer{ { ClickKind::Steer, kSteer } };
    PointerState s = heldAfterPress(steer);
    onRelease(s);
    endHold(s, true);
    CHECK_FALSE(s.resumeLast.has_value());
}

TEST_CASE("a still pointer is never re-resolved, even across a camera cut")
{
    FakeResolver r{ { ClickKind::Walk, kWalkA } };
    PointerState s = heldAfterPress(r);
    r.calls = 0;
    CHECK(holdDecision(s, kAt, 0, r.fn(), false, true).type == DecisionType::Nothing);
    CHECK(holdDecision(s, kAt, 5, r.fn(), false, true).type == DecisionType::Nothing);
    CHECK(r.calls == 0);
}

TEST_CASE("a moved pointer re-issues only when the resolution changes, keeping the hold's run")
{
    FakeResolver r{ { ClickKind::Walk, kWalkA } };
    PointerState s = heldAfterPress(r, kAt, 2);
    Point moved{ kAt.x + 1, kAt.y };
    onMove(s, moved);
    CHECK(holdDecision(s, moved, 0, r.fn(), false, true).type == DecisionType::Nothing);

    r.result = { ClickKind::Target, kTarget };
    Point moved2{ kAt.x + 2, kAt.y };
    onMove(s, moved2);
    Decision d = holdDecision(s, moved2, 0, r.fn(), false, true);
    CHECK(d.type == DecisionType::Issue);
    CHECK(d.kind == ClickKind::Target);
    CHECK(d.payload == kTarget);
    CHECK(d.run);
}

TEST_CASE("a blocked resolution while held cancels a live intent once")
{
    FakeResolver r{ { ClickKind::Walk, kWalkA } };
    PointerState s = heldAfterPress(r);
    r.result = { ClickKind::Blocked, {} };
    Point m1{ kAt.x + 3, kAt.y };
    CHECK(holdDecision(s, m1, 0, r.fn(), false, true).type == DecisionType::Cancel);
    Point m2{ kAt.x + 4, kAt.y };
    CHECK(holdDecision(s, m2, 0, r.fn(), false, false).type == DecisionType::Nothing);
}

TEST_CASE("after a camera cut the pointer must leave a dead zone before re-resolving")
{
    FakeResolver r{ { ClickKind::Walk, kWalkA } };
    PointerState s = heldAfterPress(r, kAt, 1, 0);
    r.result = { ClickKind::Walk, kWalkB };

    Point inside{ kAt.x + kCutDeadZonePx, kAt.y + kCutDeadZonePx };
    CHECK(holdDecision(s, inside, 1, r.fn(), false, true).type == DecisionType::Nothing);
    CHECK(settling(s));

    Point outside{ kAt.x + kCutDeadZonePx + 1, kAt.y };
    Decision d = holdDecision(s, outside, 1, r.fn(), false, true);
    CHECK(d.type == DecisionType::Issue);
    CHECK(d.payload == kWalkB);
    CHECK_FALSE(settling(s));
    CHECK(s.followCamera == 1);
}

TEST_CASE("a pointer outside the window is ignored while held")
{
    FakeResolver r{ { ClickKind::Walk, kWalkA } };
    PointerState s = heldAfterPress(r);
    CHECK(holdDecision(s, std::nullopt, 0, r.fn(), false, true).type == DecisionType::Nothing);
    CHECK(holdDecision(s, std::nullopt, 3, r.fn(), false, true).type == DecisionType::Nothing);
}

TEST_CASE("a spent, released or push-latched hold never follows")
{
    FakeResolver r{ { ClickKind::Walk, kWalkB } };
    Point moved{ kAt.x + 20, kAt.y };

    FakeResolver hud{ { ClickKind::HudInventory, {} } };
    PointerState spent = heldAfterPress(hud);
    CHECK(holdDecision(spent, moved, 0, r.fn(), false, false).type == DecisionType::Nothing);

    FakeResolver walk{ { ClickKind::Walk, kWalkA } };
    PointerState released = heldAfterPress(walk);
    onRelease(released);
    CHECK(holdDecision(released, moved, 0, r.fn(), false, false).type == DecisionType::Nothing);

    PointerState latched = heldAfterPress(walk);
    CHECK(holdDecision(latched, moved, 0, r.fn(), true, true).type == DecisionType::Nothing);
}

TEST_CASE("HUD, attack and push under a held pointer need a fresh press")
{
    FakeResolver r{ { ClickKind::Walk, kWalkA } };
    PointerState s = heldAfterPress(r);
    r.result = { ClickKind::Attack, Payload{ 0, 0, -1, 9 } };
    Point moved{ kAt.x + 10, kAt.y };
    CHECK(holdDecision(s, moved, 0, r.fn(), false, true).type == DecisionType::Nothing);
}

TEST_CASE("endHold clears the hold, rebase keeps it, resetPointer clears everything")
{
    FakeResolver r{ { ClickKind::Walk, kWalkA } };
    PointerState ended = heldAfterPress(r, kAt, 2);
    onRelease(ended);
    endHold(ended, false);
    CHECK_FALSE(ended.run);
    CHECK_FALSE(ended.spent);
    CHECK_FALSE(ended.followLast.has_value());
    CHECK(ended.resumeLast == kWalkA);
    CHECK(ended.resumePos == kAt);

    PointerState rebased = heldAfterPress(r, kAt, 2);
    rebase(rebased);
    CHECK(rebased.held);
    CHECK(rebased.run);
    CHECK_FALSE(rebased.followPos.has_value());
    CHECK_FALSE(rebased.resumeLast.has_value());

    PointerState reset = heldAfterPress(r, kAt, 2);
    resetPointer(reset);
    CHECK_FALSE(reset.held);
    CHECK_FALSE(reset.run);
    CHECK_FALSE(reset.pos.has_value());
    CHECK_FALSE(reset.followLast.has_value());
}
