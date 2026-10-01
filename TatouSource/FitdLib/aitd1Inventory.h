///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// AITD1 inventory facts shared by mouse gameplay and the assists. Standard
// headers only.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <iterator>

constexpr int kAitd1ActionsObject = 2;   // "Actions" (text 200): bare hands, found-life 561
constexpr int kAitd1ArmedActionVar = 90; // vars[90]: the action life 561 armed for Actions

// Found-lives that strike on Action (LISTLIFE.PAK): rifle 12, saber 49,
// sword 130, daggers 187-189, knives 354/355, revolver 365.
constexpr int kAitd1WeaponFoundLives[] = { 12, 49, 130, 187, 188, 189, 354, 355, 365 };

inline bool isAitd1WeaponFoundLife(int foundLife)
{
    for (int life : kAitd1WeaponFoundLives)
        if (life == foundLife)
            return true;
    return false;
}
