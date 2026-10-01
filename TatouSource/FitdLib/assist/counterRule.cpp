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
    strikePrev_ = false;
    hurtSeen_ = false; // this blow's own hurt animation is still to come
    freeFrames_ = 0;
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
        // The hero's LIFE plays the hurt animation a frame after the blow; a
        // strike started before it would be replaced by it. Wait to see the
        // hero locked, or a few free frames when the blow brings no lock.
        if (f.heroLocked)
        {
            hurtSeen_ = true;
            return cmd;
        }
        if (!hurtSeen_ && ++freeFrames_ < kNoHurtFrames)
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
        strikePrev_ = f.strikeArmed; // a flag already set now is stale, not our strike
        cmd.face = true;
        cmd.holdStrike = true;
        return cmd;
    }

    // Striking: hold until the strike has armed (a false -> true change) and
    // then resolved.
    if (f.strikeArmed && !strikePrev_)
        strikeSeen_ = true;
    const bool resolved = strikeSeen_ && !f.strikeArmed;
    strikePrev_ = f.strikeArmed;
    if (resolved)
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
    strikePrev_ = false;
    hurtSeen_ = false;
    freeFrames_ = 0;
}

} // namespace assist
