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
    bool refused = false;    // an attack was refused at refusedTick
    unsigned refusedTick = 0;
};

PaceRecord startedAttack(unsigned now, unsigned cycleTicks);
// May the enemy start an attack now?
bool mayStrike(AttackPace pace, const PaceRecord& last, unsigned now);
// Should a following enemy hold still this frame? Only during the added wait,
// never during its own attack and recovery.
bool holdsChase(AttackPace pace, const PaceRecord& last, unsigned now, int distanceToHero);
// The record after mayStrike refused an attack at `now`.
PaceRecord refusedAttack(PaceRecord last, unsigned now);
// Should a sample the enemy's script plays now be dropped? Only in the frame of
// a refused attack: scripts play an attack's sound right after HIT ("HIT ...;
// SAMPLE 28"), which would otherwise loop through the whole wait.
bool mutesSample(AttackPace pace, const PaceRecord& last, unsigned now);

} // namespace assist
