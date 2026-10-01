///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Accessibility: the automatic counter-attack's rule. After an enemy's melee
// blow the hero waits out the hurt animation, switches to fists if nothing
// in hand can strike, faces the attacker and holds the strike input until the
// blow resolves. Any input of the player's own cancels it.
// Engine-free: standard headers only. The engine adapter is counterAttack.cpp.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstdint>

namespace assist
{

enum class CounterPhase
{
    Idle,
    Pending,  // hit received; waiting for a free, armed hero
    Striking, // strike input held until the blow resolves
};

// Facts the adapter gathers each frame. The defaults describe a free, armed
// hero under player control with no input of the player's own.
struct CounterFrame
{
    bool playerInput = false;      // keyboard/gamepad/click, or the mouse drove the hero
    bool worldInputAllowed = true; // PlayWorld's allowSystemMenu
    bool heroControlled = true;    // hero present, manual track, under player control, not game over
    bool attackerValid = true;     // the attacker is still a combat target on this floor
    bool heroLocked = false;       // the hero's animation cannot be interrupted (hurt anim)
    bool armed = true;             // the object in hand can strike
    bool strikeArmed = false;      // the hero's animActionType is non-zero
    uint32_t nowMs = 0;
};

// What the adapter must do this frame.
struct CounterCommand
{
    bool equipFists = false; // perform the inventory's Actions -> Fight
    bool face = false;       // turn the hero toward attacker()
    bool holdStrike = false; // hold forward + Action and keep the hero from walking
};

constexpr uint32_t kPendingBudgetMs = 3000; // longest wait for a free hero
constexpr uint32_t kStrikeBudgetMs = 2500;  // longest strike (an empty gun never fires)

class CounterRule
{
public:
    // A melee blow from attackerIdx landed on the hero.
    void noteHit(int attackerIdx, uint32_t nowMs);
    // One frame: cancel, wait, equip, face or strike.
    CounterCommand step(const CounterFrame& f);
    void reset();

    CounterPhase phase() const { return phase_; }
    int attacker() const { return attacker_; } // -1 when Idle

private:
    CounterPhase phase_ = CounterPhase::Idle;
    int attacker_ = -1;
    uint32_t sinceMs_ = 0;    // when the current phase's budget started
    bool fistsTried_ = false; // this counter already switched to fists
    bool strikeSeen_ = false; // the strike armed since Striking began
};

} // namespace assist
