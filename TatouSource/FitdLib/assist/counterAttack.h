///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Accessibility: automatic counter-attack — the engine's only entry points.
// With the option off (or outside AITD1) every call returns at once.
// Rule: assist/counterRule.h. Spec: the 2026-10-01 auto-counter-attack design.
///////////////////////////////////////////////////////////////////////////////
#pragma once

struct tObject;

// GereFrappe's FRAPPE_OK: a melee blow from attackerIdx landed on victimIdx.
void counterAttackNoteHit(int victimIdx, int attackerIdx);

// PlayWorld, after mouseWorldFrame and before `action` is set. playerInput:
// the player's own keyboard, gamepad, click or mouse input this frame.
void counterAttackFrame(bool playerInput, int allowSystemMenu);

// processTrack case 1: true (hero halted) while the counter holds forward to
// strike, so the manual track does not walk.
bool counterAttackSteer(tObject* actor);

// Every screen taken over from play (called by mouseWorldTakeOver): drop the
// counter and any input it wrote this frame.
void counterAttackReset();
