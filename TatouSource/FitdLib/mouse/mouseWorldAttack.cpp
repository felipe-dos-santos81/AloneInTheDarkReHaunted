///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: one click on an enemy faces it and swings the weapon in
// hand (port of m-aitd interaction/combat.py attack_in_hand and input.py).
// With fists one click punches and a double click kicks.
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "menuMouse.h"
#include "mouseWorldInternal.h"

namespace mouseworld
{
namespace
{
constexpr uint32_t kAttackBudgetMs = 2000; // m-aitd: 100 ticks at 50 Hz
}

void clearAttack()
{
    g_world.attackTarget = -1;
    g_world.attackFrames = 0;
}

// Accept a click on an enemy: stop, face it, and hold Action until the swing ends.
void armAttack(int actorIdx, bool doublePress)
{
    if (!isCombatTarget(actorIdx) || !canStrike(true))
        return;
    cancelIntent();
    faceActorToward(hero(), ListObjets[actorIdx]);
    g_world.attackTarget = actorIdx;
    g_world.attackStick = mouse::strikeStick(armedAction(), doublePress);
    g_world.attackStartMs = (uint32_t)SDL_GetTicks();
    g_world.attackFrames = 0;
}

// The first press of a double press already started the punch; the second
// swaps the held direction, and the swing's budget starts again.
void kickTakesOver(int clicks, uint32_t now)
{
    if (clicks < 2 || g_world.attackTarget < 0 || g_world.attackStick != mouse::kStickLeft)
        return;
    g_world.attackStick = mouse::kStickUp;
    g_world.attackStartMs = now;
    g_world.attackFrames = 0;
}

// One frame of FITD's own melee input (a direction + Action) for an accepted click.
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
    setJoyD(g_world.attackStick);
    localClick = 1; // PlayWorld turns this into action = 0x2000
    return true;
}

} // namespace mouseworld
