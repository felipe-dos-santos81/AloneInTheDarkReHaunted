///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Engine unit tests: the automatic counter-attack rule.
///////////////////////////////////////////////////////////////////////////////

#include "doctest.h"
#include "counterRule.h"

#include <initializer_list>

using assist::CounterCommand;
using assist::CounterFrame;
using assist::CounterPhase;
using assist::CounterRule;

namespace
{
// A free, armed hero under player control, with no input of the player's own.
CounterFrame freeHero(uint32_t nowMs)
{
    CounterFrame f;
    f.nowMs = nowMs;
    return f;
}

CounterFrame lockedHero(uint32_t nowMs)
{
    CounterFrame f = freeHero(nowMs);
    f.heroLocked = true;
    return f;
}

bool none(const CounterCommand& c)
{
    return !c.equipFists && !c.face && !c.holdStrike;
}

CounterRule pending()
{
    CounterRule rule;
    rule.noteHit(7, 0);
    return rule;
}

// Hit at t=0, hurt anim until t=900, then free: the counter is Striking at t=1000.
CounterRule striking()
{
    CounterRule rule = pending();
    rule.step(lockedHero(500));
    rule.step(freeHero(1000));
    return rule;
}

// One spoiled fact ends the counter, whether it is pending or striking. (Plain
// loop, not SUBCASE: doctest runs a subcase inside a loop only once.)
void checkCancel(void (*spoil)(CounterFrame&))
{
    for (bool fromStriking : { false, true })
    {
        CAPTURE(fromStriking);
        CounterRule rule = fromStriking ? striking() : pending();
        CounterFrame f = freeHero(1050);
        spoil(f);
        CHECK(none(rule.step(f)));
        CHECK(rule.phase() == CounterPhase::Idle);
    }
}
}

TEST_CASE("a fresh counter rule is idle and commands nothing")
{
    CounterRule rule;
    CHECK(rule.phase() == CounterPhase::Idle);
    CHECK(rule.attacker() == -1);
    CHECK(none(rule.step(freeHero(0))));
    CHECK(rule.phase() == CounterPhase::Idle);
}

TEST_CASE("a melee hit makes the counter pending against the attacker")
{
    CounterRule rule;
    rule.noteHit(7, 100);
    CHECK(rule.phase() == CounterPhase::Pending);
    CHECK(rule.attacker() == 7);
}

TEST_CASE("a negative attacker index is ignored")
{
    CounterRule rule;
    rule.noteHit(-1, 100);
    CHECK(rule.phase() == CounterPhase::Idle);
}

TEST_CASE("a pending counter waits while the hero's hurt animation is locked")
{
    CounterRule rule;
    rule.noteHit(7, 0);
    CHECK(none(rule.step(lockedHero(16))));
    CHECK(none(rule.step(lockedHero(900))));
    CHECK(rule.phase() == CounterPhase::Pending);
}

TEST_CASE("once the hero is free an armed counter faces the attacker and strikes")
{
    CounterRule rule;
    rule.noteHit(7, 0);
    rule.step(lockedHero(500));
    const CounterCommand c = rule.step(freeHero(1000));
    CHECK(c.face);
    CHECK(c.holdStrike);
    CHECK_FALSE(c.equipFists);
    CHECK(rule.phase() == CounterPhase::Striking);
    CHECK(rule.attacker() == 7);
}

TEST_CASE("the strike is held until the hero's strike arms and then resolves")
{
    CounterRule rule = striking();

    CounterFrame f = freeHero(1016); // the found-life has not armed the strike yet
    CounterCommand c = rule.step(f);
    CHECK(c.holdStrike);
    CHECK_FALSE(c.face); // facing happens once, on entering Striking

    f = freeHero(1032);
    f.strikeArmed = true; // HIT/FIRE set animActionType
    CHECK(rule.step(f).holdStrike);
    f.nowMs = 1300;
    CHECK(rule.step(f).holdStrike);

    f = freeHero(1400); // animActionType back to 0: the blow resolved
    CHECK(none(rule.step(f)));
    CHECK(rule.phase() == CounterPhase::Idle);
}

TEST_CASE("one pending counter strikes only once")
{
    CounterRule rule = striking();
    CounterFrame f = freeHero(1032);
    f.strikeArmed = true;
    rule.step(f);
    rule.step(freeHero(1400));
    REQUIRE(rule.phase() == CounterPhase::Idle);
    CHECK(none(rule.step(freeHero(1416))));
    CHECK(none(rule.step(freeHero(2000))));
}

TEST_CASE("an unarmed hero switches to fists once, then strikes")
{
    CounterRule rule;
    rule.noteHit(7, 0);
    CounterFrame f = freeHero(1000);
    f.armed = false;
    CounterCommand c = rule.step(f);
    CHECK(c.equipFists);
    CHECK_FALSE(c.face);
    CHECK_FALSE(c.holdStrike);
    CHECK(rule.phase() == CounterPhase::Pending);

    c = rule.step(freeHero(1016)); // Actions/Fight now in hand
    CHECK(c.face);
    CHECK(c.holdStrike);
    CHECK_FALSE(c.equipFists);
}

TEST_CASE("switching to fists may lock the hero briefly; the counter waits")
{
    CounterRule rule;
    rule.noteHit(7, 0);
    CounterFrame f = freeHero(1000);
    f.armed = false;
    REQUIRE(rule.step(f).equipFists);
    CHECK(none(rule.step(lockedHero(1016))));
    CHECK(rule.step(freeHero(1100)).holdStrike);
}

TEST_CASE("a hero still unarmed after equipping fists gives up")
{
    CounterRule rule;
    rule.noteHit(7, 0);
    CounterFrame f = freeHero(1000);
    f.armed = false;
    REQUIRE(rule.step(f).equipFists);
    f.nowMs = 1016;
    CHECK(none(rule.step(f)));
    CHECK(rule.phase() == CounterPhase::Idle);
}

TEST_CASE("a new hit while pending retargets and restarts the pending clock")
{
    CounterRule rule;
    rule.noteHit(7, 0);
    rule.step(lockedHero(2000));
    rule.noteHit(9, 2500);
    CHECK(rule.attacker() == 9);
    CHECK(none(rule.step(lockedHero(5000)))); // 2500 ms since the latest hit
    CHECK(rule.phase() == CounterPhase::Pending);
}

TEST_CASE("a hit mid-strike puts the counter back to pending and releases the input")
{
    CounterRule rule = striking();
    rule.noteHit(9, 1100);
    CHECK(rule.phase() == CounterPhase::Pending);
    CHECK(rule.attacker() == 9);
    CHECK(none(rule.step(lockedHero(1116)))); // hurt anim again: nothing held
    const CounterCommand c = rule.step(freeHero(2100));
    CHECK(c.face);
    CHECK(c.holdStrike);
}

TEST_CASE("player input cancels the counter, pending or striking")
{
    checkCancel([](CounterFrame& f) { f.playerInput = true; });
}

TEST_CASE("world input not allowed cancels the counter (an injected click would leave PlayWorld)")
{
    checkCancel([](CounterFrame& f) { f.worldInputAllowed = false; });
}

TEST_CASE("a hero no longer under player control cancels the counter")
{
    checkCancel([](CounterFrame& f) { f.heroControlled = false; });
}

TEST_CASE("an attacker no longer valid cancels the counter")
{
    checkCancel([](CounterFrame& f) { f.attackerValid = false; });
}

TEST_CASE("the pending budget ends a counter whose hero never gets free")
{
    CounterRule rule;
    rule.noteHit(7, 0);
    CHECK(none(rule.step(lockedHero(assist::kPendingBudgetMs - 1))));
    CHECK(rule.phase() == CounterPhase::Pending);
    CHECK(none(rule.step(lockedHero(assist::kPendingBudgetMs))));
    CHECK(rule.phase() == CounterPhase::Idle);
}

TEST_CASE("the strike budget ends a strike that never resolves (an empty gun)")
{
    CounterRule rule = striking(); // Striking since t=1000
    CHECK(rule.step(freeHero(1000 + assist::kStrikeBudgetMs - 1)).holdStrike);
    CHECK(none(rule.step(freeHero(1000 + assist::kStrikeBudgetMs))));
    CHECK(rule.phase() == CounterPhase::Idle);
}

TEST_CASE("budgets survive the 32-bit tick wrap")
{
    CounterRule rule;
    rule.noteHit(5, 0xFFFFFF00u);
    CHECK(none(rule.step(lockedHero(0x00000100u)))); // 512 ms later, across the wrap
    CHECK(rule.phase() == CounterPhase::Pending);
    CHECK(rule.step(freeHero(0x00000200u)).holdStrike);
}

TEST_CASE("reset returns the counter to idle")
{
    CounterRule rule = striking();
    rule.reset();
    CHECK(rule.phase() == CounterPhase::Idle);
    CHECK(rule.attacker() == -1);
    CHECK(none(rule.step(freeHero(1100))));
}
