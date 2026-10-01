///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Accessibility: enemy attack pace — the engine's only entry points. On
// Normal (or outside AITD1) every call returns at once and touches no game
// state. Rule: assist/paceRule.h. Spec: the 2026-10-01 enemy-attack-pace design.
///////////////////////////////////////////////////////////////////////////////
#pragma once

struct tObject;

// hit(), first line: false = this enemy may not start an attack yet.
bool attackPaceAllowsHit(int actorIdx);

// hit(), after InitAnim accepted the attack: remember when it started and how
// long its attack + next animation last.
void attackPaceNoteHit(int actorIdx, int anim, int nextAnim);

// processTrack case 2, after speed = 4: a waiting enemy following the hero
// within reach stands still (speed 0); the facing turn above still runs.
void attackPaceHoldChase(tObject* actor, int followedIdx, int targetX, int targetZ);

// LM_SAMPLE / life_Sample: false = drop this sample. Only in the frame an
// enemy's attack was refused, so the sound its script plays right after HIT
// does not loop through the wait.
bool attackPaceAllowsSample(int actorIdx);

// InitObjet: a (re)initialised actor slot starts with no record.
void attackPaceForget(int actorIdx);
