///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Engine unit tests: the enemy attack pace rule.
///////////////////////////////////////////////////////////////////////////////

#include "doctest.h"
#include "paceRule.h"

using assist::AttackPace;
using assist::PaceRecord;

namespace
{
// The bedroom window creature: attack 52 (30 ticks) + recovery 49 (70 ticks).
const PaceRecord kWindowCreatureAt1000 = assist::startedAttack(1000, 100);
}

TEST_CASE("paceFromSetting maps 1 and 2, and anything else to Normal")
{
    CHECK(assist::paceFromSetting(0) == AttackPace::Normal);
    CHECK(assist::paceFromSetting(1) == AttackPace::Slower);
    CHECK(assist::paceFromSetting(2) == AttackPace::MuchSlower);
    CHECK(assist::paceFromSetting(3) == AttackPace::Normal);
    CHECK(assist::paceFromSetting(-1) == AttackPace::Normal);
    CHECK(assist::paceFromSetting(7) == AttackPace::Normal);
}

TEST_CASE("paceMultiplier is 1, 2 and 3")
{
    CHECK(assist::paceMultiplier(AttackPace::Normal) == 1);
    CHECK(assist::paceMultiplier(AttackPace::Slower) == 2);
    CHECK(assist::paceMultiplier(AttackPace::MuchSlower) == 3);
}

TEST_CASE("Normal always lets an enemy strike")
{
    CHECK(assist::mayStrike(AttackPace::Normal, kWindowCreatureAt1000, 1000));
    CHECK(assist::mayStrike(AttackPace::Normal, kWindowCreatureAt1000, 1001));
}

TEST_CASE("an enemy's first attack is never delayed")
{
    CHECK(assist::mayStrike(AttackPace::MuchSlower, PaceRecord{}, 5));
}

TEST_CASE("Slower waits twice the natural cycle, boundary included")
{
    CHECK_FALSE(assist::mayStrike(AttackPace::Slower, kWindowCreatureAt1000, 1100));
    CHECK_FALSE(assist::mayStrike(AttackPace::Slower, kWindowCreatureAt1000, 1199));
    CHECK(assist::mayStrike(AttackPace::Slower, kWindowCreatureAt1000, 1200));
}

TEST_CASE("Much slower waits three times the natural cycle, boundary included")
{
    CHECK_FALSE(assist::mayStrike(AttackPace::MuchSlower, kWindowCreatureAt1000, 1299));
    CHECK(assist::mayStrike(AttackPace::MuchSlower, kWindowCreatureAt1000, 1300));
}

TEST_CASE("a clock rewound by a loaded save never blocks")
{
    CHECK(assist::mayStrike(AttackPace::MuchSlower, kWindowCreatureAt1000, 400));
}

TEST_CASE("an attack with no measurable cycle never blocks")
{
    CHECK(assist::mayStrike(AttackPace::MuchSlower, assist::startedAttack(1000, 0), 1000));
}

TEST_CASE("mutesSample silences the sound a script plays right after a refused attack")
{
    // Life 74: "HIT 52 ... 49; SAMPLE 28" runs every frame of the wait.
    const PaceRecord refused = assist::refusedAttack(kWindowCreatureAt1000, 1150);
    CHECK(assist::mutesSample(AttackPace::MuchSlower, refused, 1150));
    CHECK_FALSE(assist::mutesSample(AttackPace::MuchSlower, refused, 1151)); // a later frame's sample
    CHECK_FALSE(assist::mutesSample(AttackPace::Normal, refused, 1150));
    CHECK_FALSE(assist::mutesSample(AttackPace::MuchSlower, kWindowCreatureAt1000, 1000)); // an accepted attack keeps its sound
    CHECK_FALSE(assist::mutesSample(AttackPace::MuchSlower, PaceRecord{}, 0));
    // A refusal is forgotten once the next attack starts.
    CHECK_FALSE(assist::mutesSample(AttackPace::MuchSlower, assist::startedAttack(1150, 100), 1150));
}

TEST_CASE("holdsChase leaves the enemy's own attack and recovery alone")
{
    // During the natural cycle the enemy keeps its follow speed, which carries
    // its strike forward; the hold starts only once the cycle is over.
    CHECK_FALSE(assist::holdsChase(AttackPace::MuchSlower, kWindowCreatureAt1000, 1000, 600));
    CHECK_FALSE(assist::holdsChase(AttackPace::MuchSlower, kWindowCreatureAt1000, 1099, 600));
    CHECK(assist::holdsChase(AttackPace::MuchSlower, kWindowCreatureAt1000, 1100, 600));
}

TEST_CASE("holdsChase only while waiting, within reach, and never on Normal")
{
    CHECK(assist::holdsChase(AttackPace::Slower, kWindowCreatureAt1000, 1150, 600));
    CHECK(assist::holdsChase(AttackPace::Slower, kWindowCreatureAt1000, 1150, assist::kHoldReach - 1));
    CHECK_FALSE(assist::holdsChase(AttackPace::Slower, kWindowCreatureAt1000, 1150, assist::kHoldReach));
    CHECK_FALSE(assist::holdsChase(AttackPace::Slower, kWindowCreatureAt1000, 1200, 600)); // wait over
    CHECK_FALSE(assist::holdsChase(AttackPace::Normal, kWindowCreatureAt1000, 1150, 600));
    CHECK_FALSE(assist::holdsChase(AttackPace::Slower, PaceRecord{}, 1150, 600));            // never attacked
}
