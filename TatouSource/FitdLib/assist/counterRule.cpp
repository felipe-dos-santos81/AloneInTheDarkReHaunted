///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Accessibility: the automatic counter-attack's rule (see counterRule.h).
///////////////////////////////////////////////////////////////////////////////

#include "counterRule.h"

namespace assist
{

void CounterRule::noteHit(int attackerIdx, uint32_t nowMs)
{
    if (attackerIdx < 0)
        return;
    if (phase_ == CounterPhase::Idle)
        fistsTried_ = false;
    // A hit while Striking means the hurt animation replaced the swing: the
    // counter did not land, so it is pending again rather than a second one.
    phase_ = CounterPhase::Pending;
    attacker_ = attackerIdx;
    sinceMs_ = nowMs;
    strikeSeen_ = false;
}

CounterCommand CounterRule::step(const CounterFrame& f)
{
    if (phase_ == CounterPhase::Idle)
        return {};

    const uint32_t budget = phase_ == CounterPhase::Pending ? kPendingBudgetMs : kStrikeBudgetMs;
    const uint32_t elapsed = f.nowMs - sinceMs_; // unsigned: survives the tick wrap
    if (f.playerInput || !f.worldInputAllowed || !f.heroControlled || !f.attackerValid || elapsed >= budget)
    {
        reset();
        return {};
    }

    CounterCommand cmd;
    if (phase_ == CounterPhase::Pending)
    {
        if (f.heroLocked)
            return cmd;
        if (!f.armed)
        {
            if (fistsTried_)
            {
                reset();
                return {};
            }
            fistsTried_ = true;
            cmd.equipFists = true;
            return cmd;
        }
        phase_ = CounterPhase::Striking;
        sinceMs_ = f.nowMs;
        strikeSeen_ = false;
        cmd.face = true;
        cmd.holdStrike = true;
        return cmd;
    }

    // Striking: hold until the strike has armed and then resolved.
    if (f.strikeArmed)
        strikeSeen_ = true;
    else if (strikeSeen_)
    {
        reset();
        return {};
    }
    cmd.holdStrike = true;
    return cmd;
}

void CounterRule::reset()
{
    phase_ = CounterPhase::Idle;
    attacker_ = -1;
    sinceMs_ = 0;
    fistsTried_ = false;
    strikeSeen_ = false;
}

} // namespace assist
