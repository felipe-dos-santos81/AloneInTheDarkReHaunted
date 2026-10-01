///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Stop an actor and turn it toward another at once. Shared by mouse gameplay
// (a clicked enemy) and the automatic counter-attack (the attacker).
// Include after common.h.
///////////////////////////////////////////////////////////////////////////////
#pragma once

// Stop an actor dead: no step, no turn in progress.
void haltActor(tObject& a);

// Stop and instantly face `target`, without disturbing the track
// interpolation. A target in another room is re-framed into actor's room.
void faceActorToward(tObject& actor, const tObject& target);
