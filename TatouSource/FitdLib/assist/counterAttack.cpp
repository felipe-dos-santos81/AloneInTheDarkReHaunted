///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Accessibility: automatic counter-attack — the engine adapter. The only
// assist file that touches engine globals. It only produces what a player
// would: the inventory's Actions -> Fight, and forward + Action held; the
// in-hand object's own LIFE swings or fires.
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "actorFacing.h"
#include "aitd1Inventory.h"
#include "counterAttack.h"
#include "counterRule.h"

#include <SDL.h>

namespace
{
assist::CounterRule s_rule;
bool s_wroteInput = false; // localJoyD/localClick written by the counter this frame
int s_attackerWorldIdx = -1; // the attacker's world object, to tell it from a reused ListObjets slot

constexpr int kActionsObject = kAitd1ActionsObject;
constexpr int kFightActionBit = 1 << 4; // inventory text 27 "Fight" (= 23 + 4)
constexpr int kFightModeVar = kAitd1ArmedActionVar;
// The stick held with Action picks the strike. Left is the quickest melee blow
// for fists and every weapon (strike frame after 20-25 anim units, against
// 30-40 for the others: LISTANIM/LISTANI2 anims 37/39/40/41, 262/263/265);
// guns fire on up alone.
constexpr int kJoyLeft = 4;
constexpr int kJoyUp = 1;
constexpr int kGunFoundLives[] = { 12, 365 }; // rifle, revolver

bool enabled()
{
    return g_remasterConfig.controls.autoCounterAttack && g_gameId == AITD1;
}

bool heroPresent()
{
    return currentCameraTargetActor >= 0 && currentCameraTargetActor < NUM_MAX_OBJECT &&
           ListObjets[currentCameraTargetActor].indexInWorld >= 0;
}

const tWorldObject* worldObjectAt(int idx)
{
    if (idx < 0 || idx >= (int)ListWorldObjets.size())
        return nullptr;
    return &ListWorldObjets[idx];
}

// The object in hand can strike: a weapon, or Actions with Fight chosen.
bool armed()
{
    if (currentInventory < 0 || currentInventory >= NUM_MAX_INVENTORY)
        return false;
    const int inHand = inHandTable[currentInventory];
    if (inHand == kActionsObject)
        return vars[kFightModeVar] == kFightActionBit;
    const tWorldObject* w = worldObjectAt(inHand);
    return w && isAitd1WeaponFoundLife(w->foundLife);
}

bool gunInHand()
{
    if (currentInventory < 0 || currentInventory >= NUM_MAX_INVENTORY)
        return false;
    const tWorldObject* w = worldObjectAt(inHandTable[currentInventory]);
    if (!w)
        return false;
    for (int life : kGunFoundLives)
        if (w->foundLife == life)
            return true;
    return false;
}

// Actions is carried and offers Fight.
bool canEquipFists()
{
    const tWorldObject* w = worldObjectAt(kActionsObject);
    if (!w || !(w->foundFlag & kFightActionBit))
        return false;
    if (currentInventory < 0 || currentInventory >= NUM_MAX_INVENTORY)
        return false;
    for (int i = 0; i < numObjInInventoryTable[currentInventory]; ++i)
        if (inventoryTable[currentInventory][i] == kActionsObject)
            return true;
    return false;
}

// Exactly the inventory's Actions -> Fight (inventory.cpp confirmAction, then
// executeFoundLife): its LIFE sets the bare-hand body, IN_HAND 2 and vars[90].
// When it cannot, the hero stays unarmed and the rule gives up next frame.
void equipFists()
{
    if (!canEquipFists())
        return;
    action = kFightActionBit;
    executeFoundLife(kActionsObject);
}

bool attackerValid(int idx)
{
    if (idx < 0 || idx >= NUM_MAX_OBJECT || idx == currentCameraTargetActor)
        return false;
    const tObject& a = ListObjets[idx];
    const tWorldObject* w = worldObjectAt(a.indexInWorld);
    return w && a.indexInWorld == s_attackerWorldIdx && (a.objectType & AF_ANIMATED) && w->stage == g_currentFloor;
}

// InitAnim's own AITD1 test (anim.cpp): it refuses new animations now.
bool heroLocked(const tObject& h)
{
    return (h.animType & ANIM_UNINTERRUPTABLE) || (h.newAnimType & ANIM_UNINTERRUPTABLE);
}
}

void counterAttackNoteHit(int victimIdx, int attackerIdx)
{
    if (!enabled() || victimIdx != currentCameraTargetActor)
        return;
    if (attackerIdx < 0 || attackerIdx >= NUM_MAX_OBJECT)
        return;
    s_attackerWorldIdx = ListObjets[attackerIdx].indexInWorld;
    s_rule.noteHit(attackerIdx, (uint32_t)SDL_GetTicks());
}

void counterAttackFrame(bool playerInput, int allowSystemMenu)
{
    s_wroteInput = false;
    if (!enabled())
    {
        s_rule.reset(); // switched off mid-counter: nothing survives
        return;
    }
    if (s_rule.phase() == assist::CounterPhase::Idle)
        return;

    const bool present = heroPresent();
    tObject* h = present ? &ListObjets[currentCameraTargetActor] : nullptr;

    assist::CounterFrame f;
    f.playerInput = playerInput;
    f.worldInputAllowed = allowSystemMenu != 0;
    f.heroControlled = present && h->trackMode == 1 && vars[0] == 1 && !FlagGameOver;
    f.attackerValid = attackerValid(s_rule.attacker());
    f.heroLocked = present && heroLocked(*h);
    f.armed = armed();
    f.strikeArmed = present && h->animActionType != 0;
    f.nowMs = (uint32_t)SDL_GetTicks();

    const assist::CounterCommand cmd = s_rule.step(f);
    if (cmd.equipFists)
        equipFists();
    if (cmd.face && present)
        faceActorToward(*h, ListObjets[s_rule.attacker()]);
    if (cmd.holdStrike)
    {
        localJoyD = gunInHand() ? kJoyUp : kJoyLeft;
        localClick = 1; // PlayWorld turns this into action = 0x2000
        s_wroteInput = true;
    }
}

bool counterAttackSteer(tObject* actor)
{
    if (!s_wroteInput || !heroPresent() || actor != &ListObjets[currentCameraTargetActor])
        return false;
    haltActor(*actor); // forward is held for the in-hand LIFE's strike, not to walk
    return true;
}

void counterAttackReset()
{
    s_rule.reset();
    if (s_wroteInput)
    {
        // A screen opened mid-frame (e.g. by a script) must not inherit the
        // stick and Action the counter wrote for this frame.
        localJoyD = 0;
        localClick = 0;
        s_wroteInput = false;
    }
}
