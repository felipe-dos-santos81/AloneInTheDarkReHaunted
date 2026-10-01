///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Accessibility: enemy attack pace — the engine adapter of assist/paceRule.h,
// the only attack-pace file that touches engine globals.
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "assist/attackPace.h"
#include "assist/paceRule.h"

namespace
{
assist::PaceRecord s_records[NUM_MAX_OBJECT];

assist::AttackPace pace()
{
    return assist::paceFromSetting(g_remasterConfig.controls.enemyAttackPace);
}

bool enabled()
{
    return g_gameId == AITD1 && pace() != assist::AttackPace::Normal;
}

// Any actor but the hero (the camera's target, as in counterAttack).
bool isEnemy(int idx)
{
    return idx >= 0 && idx < NUM_MAX_OBJECT && idx != currentCameraTargetActor;
}

// One pass of an animation, in timer ticks; 0 when there is none.
unsigned animTicks(int anim)
{
    if (anim < 0)
        return 0;
    const sAnimation* a = HQR_Get(HQ_Anims, anim);
    if (!a)
        return 0;
    unsigned ticks = 0;
    for (const sFrame& frame : a->m_frames)
        ticks += frame.m_timestamp;
    return ticks;
}
} // namespace

bool attackPaceAllowsHit(int actorIdx)
{
    if (!enabled() || !isEnemy(actorIdx))
        return true;
    if (assist::mayStrike(pace(), s_records[actorIdx], timer))
        return true;
    s_records[actorIdx] = assist::refusedAttack(s_records[actorIdx], timer);
    return false;
}

bool attackPaceAllowsSample(int actorIdx)
{
    if (!enabled() || !isEnemy(actorIdx))
        return true;
    return !assist::mutesSample(pace(), s_records[actorIdx], timer);
}

void attackPaceNoteHit(int actorIdx, int anim, int nextAnim)
{
    if (!enabled() || !isEnemy(actorIdx))
        return;
    s_records[actorIdx] = assist::startedAttack(timer, animTicks(anim) + animTicks(nextAnim));
}

void attackPaceHoldChase(tObject* actor, int followedIdx, int targetX, int targetZ)
{
    if (!enabled() || !actor)
        return;
    const int idx = (int)(actor - ListObjets.data());
    if (!isEnemy(idx) || followedIdx != currentCameraTargetActor)
        return;
    const int distance = GiveDistance2D(actor->roomX, actor->roomZ, targetX, targetZ);
    if (assist::holdsChase(pace(), s_records[idx], timer, distance))
        actor->speed = 0;
}

void attackPaceForget(int actorIdx)
{
    if (actorIdx >= 0 && actorIdx < NUM_MAX_OBJECT)
        s_records[actorIdx] = assist::PaceRecord{}; // the assist's own state only
}
