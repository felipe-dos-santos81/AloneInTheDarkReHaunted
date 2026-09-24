///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: one click on an enemy faces it and swings the weapon in
// hand (port of m-aitd interaction/combat.py attack_in_hand and input.py).
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "menuMouse.h"
#include "mouseWorldInternal.h"

namespace mouseworld
{
namespace
{
constexpr uint32_t kAttackBudgetMs = 2000; // m-aitd: 100 ticks at 50 Hz

// Stop and instantly face a point without disturbing the track interpolation.
void faceToward(mouse::XZ target)
{
    tObject& h = hero();
    for (int step = 0; step < 256; ++step)
    {
        const int direction = CapObjet(h.roomX + h.stepX, h.roomZ + h.stepZ, h.beta, target.x, target.z);
        if (direction == 0)
            break;
        h.beta = (h.beta - direction * 4) & 0x3FF;
    }
    haltActor(h);
}
}

void clearAttack()
{
    g_world.attackTarget = -1;
    g_world.attackFrames = 0;
}

// Accept a click on an enemy: stop, face it, and hold Action until the swing ends.
void armAttack(int actorIdx)
{
    if (!isCombatTarget(actorIdx) || !canStrike(true))
        return;
    const tObject& h = hero();
    const tObject& t = ListObjets[actorIdx];
    mouse::XZ target{ t.roomX, t.roomZ };
    if (t.room != h.room)
        target = mouse::reframe(target, originOf(t.room), originOf(h.room));
    cancelIntent();
    faceToward(target);
    g_world.attackTarget = actorIdx;
    g_world.attackStartMs = (uint32_t)SDL_GetTicks();
    g_world.attackFrames = 0;
}

// One frame of FITD's own melee input (forward + Action) for an accepted click.
// A single frame is not enough: the hero's LIFE re-queues idle as soon as the
// action drops, so the swing would never reach its strike frame.
bool tickAttack(uint32_t now)
{
    if (g_world.attackTarget < 0)
        return false;
    if (!isCombatTarget(g_world.attackTarget) || !canStrike(false))
    {
        clearAttack();
        return false;
    }
    if (g_world.attackFrames > 0 &&
        (hero().animActionType == 0 || now - g_world.attackStartMs >= kAttackBudgetMs))
    {
        clearAttack();
        return false;
    }
    ++g_world.attackFrames;
    setJoyD(1);
    localClick = 1; // PlayWorld turns this into action = 0x2000
    return true;
}

} // namespace mouseworld
