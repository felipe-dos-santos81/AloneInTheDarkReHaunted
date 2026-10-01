///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Accessibility: the enemy attack pace rule. Engine-free.
///////////////////////////////////////////////////////////////////////////////

#include "paceRule.h"

namespace assist
{

int paceMultiplier(AttackPace pace)
{
    switch (pace)
    {
    case AttackPace::Slower:
        return 2;
    case AttackPace::MuchSlower:
        return 3;
    case AttackPace::Normal:
        break;
    }
    return 1;
}

AttackPace paceFromSetting(int value)
{
    if (value == 1)
        return AttackPace::Slower;
    if (value == 2)
        return AttackPace::MuchSlower;
    return AttackPace::Normal;
}

PaceRecord startedAttack(unsigned now, unsigned cycleTicks)
{
    return PaceRecord{ true, now, cycleTicks };
}

bool mayStrike(AttackPace pace, const PaceRecord& last, unsigned now)
{
    if (pace == AttackPace::Normal || !last.active || last.cycleTicks == 0)
        return true;
    if (now < last.startTick)
        return true; // a loaded save rewound the clock
    return now - last.startTick >= (unsigned)paceMultiplier(pace) * last.cycleTicks;
}

bool holdsChase(AttackPace pace, const PaceRecord& last, unsigned now, int distanceToHero)
{
    if (pace == AttackPace::Normal || mayStrike(pace, last, now) || distanceToHero >= kHoldReach)
        return false;
    // The enemy's own attack and recovery keep their follow speed, which
    // carries the strike forward: the hold covers only the added wait.
    return now - last.startTick >= last.cycleTicks;
}

} // namespace assist
