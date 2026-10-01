///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Accessibility: automatic counter-attack — the engine adapter. The only
// assist file that touches engine globals. It only produces what a player
// would: the inventory's Actions -> Fight, and forward + Action held; the
// in-hand object's own LIFE swings or fires.
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "actorFacing.h"
#include "counterAttack.h"
#include "counterRule.h"

#include <SDL.h>

namespace
{
assist::CounterRule s_rule;
bool s_wroteInput = false; // localJoyD/localClick written by the counter this frame

constexpr int kActionsObject = 2;       // AITD1 "Actions" (text 200): bare hands, found-life 561
constexpr int kFightActionBit = 1 << 4; // inventory text 27 "Fight" (= 23 + 4)
constexpr int kFightModeVar = 90;       // vars[90]: the action chosen for Actions
constexpr int kJoyForward = 1;          // forward: weapon strike, bare-hand kick, the only gun-firing direction

// AITD1 found-lives that strike on Action (LISTLIFE.PAK): rifle 12, saber 49,
// sword 130, daggers 187-189, knives 354/355, revolver 365.
constexpr int kWeaponFoundLives[] = { 12, 49, 130, 187, 188, 189, 354, 355, 365 };

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
    if (!w)
        return false;
    for (int life : kWeaponFoundLives)
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
    return w && (a.objectType & AF_ANIMATED) && w->stage == g_currentFloor;
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
        localJoyD = kJoyForward;
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
