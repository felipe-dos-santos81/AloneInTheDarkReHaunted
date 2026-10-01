///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Accessibility: the enemy attack pace rule. An enemy that started a melee
// attack may not start another until two or three times that attack's natural
// cycle (attack animation + the animation after it) has passed; while it
// waits within reach of the hero it holds its chase.
// Engine-free: standard headers only. The engine adapter is attackPace.cpp.
///////////////////////////////////////////////////////////////////////////////
#pragma once

namespace assist
{

enum class AttackPace
{
    Normal,     // the original game
    Slower,     // twice the natural cycle between attacks
    MuchSlower, // three times
};

int paceMultiplier(AttackPace pace);
// controls.enemyAttackPace: 1 Slower, 2 Much slower, anything else Normal.
AttackPace paceFromSetting(int value);

constexpr int kHoldReach = 1200; // room units: a waiting enemy this close holds still (spike-validated)

// The last attack an enemy started.
struct PaceRecord
{
    bool active = false;
    unsigned startTick = 0;  // game timer when hit() accepted the attack
    unsigned cycleTicks = 0; // keyframe ticks of the attack animation + the next animation
};

PaceRecord startedAttack(unsigned now, unsigned cycleTicks);
// May the enemy start an attack now?
bool mayStrike(AttackPace pace, const PaceRecord& last, unsigned now);
// Should a following enemy hold still this frame?
bool holdsChase(AttackPace pace, const PaceRecord& last, unsigned now, int distanceToHero);

} // namespace assist
