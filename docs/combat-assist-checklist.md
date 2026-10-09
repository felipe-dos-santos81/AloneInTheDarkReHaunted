# Automatic counter-attack checklist

Manual sign-off for the "Hit back automatically when an enemy strikes you"
option (spec: `docs/superpowers/specs/2026-10-01-auto-counter-attack-design.md`,
local only), Alone in the Dark 1. Build with `make build-fitd`, run with
`make run data=DIR`. Run every row as **Emily** and as **Carnby**. Mark each
cell `pass`, or `fail:` with a one-line note. Unless a row says otherwise, the
option is on (F1 → Controls → Combat) and you do not touch the controls after
the enemy's blow lands.

| # | Check | Emily | Carnby |
|---|---|---|---|
| 1 | Option off, get hit → the hurt animation plays and nothing else, as before | | |
| 2 | Bare hands (Actions with Fight chosen) → after the hurt animation the hero turns to the attacker, punches once and stands | | |
| 3 | Sword → one strike after the hurt animation; the hero does not walk forward | | |
| 4 | Saber → as row 3 | | |
| 5 | Knife → as row 3 | | |
| 6 | Dagger → as row 3 | | |
| 7 | Revolver with ammo → the hero draws and fires once at the attacker; one round is spent | | |
| 8 | Rifle with ammo → the hero aims and fires once at the attacker; one round is spent | | |
| 9 | Revolver or rifle without ammo → the hero draws or aims but does not fire; control returns within about 2.5 s | | |
| 10 | Lamp in hand → the hero switches to Actions with Fight (as when you choose Fight in the inventory) and punches once; Actions stays in hand | | |
| 11 | Actions with Push chosen → the hero switches to Fight and punches once; the inventory then shows Fight chosen | | |
| 12 | Hold a direction key or Action as the blow lands → no counter; your own input works normally | | |
| 13 | Mouse gameplay on, left button held in the world as the blow lands → no counter | | |
| 14 | Use the gamepad right after the blow → the waiting counter is cancelled | | |
| 15 | Two enemies hit within one hurt animation → exactly one counter, aimed at the last attacker | | |
| 16 | Untick the option in F1 while a counter waits, then close F1 → nothing happens; no input stays held | | |
| 17 | A screen opens during the strike (the inventory key, or a found-object screen) → no phantom selection on the screen; the counter does not resume afterwards | | |
| 18 | Get hit mid-counter → the hurt animation, then one new counter | | |
| 19 | A killing blow → the death sequence plays normally; no counter | | |
| 20 | A thrown object or a hazard hits the hero → no counter | | |
| 21 | Cutscenes and intros with the option on → unaffected; a left click still skips | | |
| 22 | Mouse click-attack on an enemy, option on and off → the hero faces it and swings once, as before (mouse checklist row 13) | | |
| 23 | Option off, fight by keyboard only, then by gamepad only → unchanged from before | | |
| 24 | Use save, load, the map, the inventory and the system menu during and after combat → no counter survives them | | |
| 25 | Open F1 → Controls → a "Combat" heading above "Controller behavior", with the checkbox "Hit back automatically when an enemy strikes you", unticked. Hover (?) → the help text shows. Tick it, quit and relaunch → it stays ticked, and `aitd_remaster.cfg` has `controls.autoCounterAttack = true` | | |
| 26 | The strike misses (the attacker steps out of reach first) → exactly one swing, then the hero stands; no second swing | | |
| 27 | Press a key during the hurt animation (not held before the blow) → the waiting counter is cancelled and the key acts normally | | |
| 28 | Mouse gameplay on, left-click in the world after the blow → the waiting counter is cancelled; the click does what it normally does | | |
| 29 | Attacker just across a room boundary → the hero turns towards it (not at a wrong angle) and strikes once | | |
| 30 | An enemy that swings again at once (the attic monster) → the counter lands between its blows and the enemy flinches | | |

## Enemy attack pace

Manual sign-off for the "Enemy attack pace" option (spec:
`docs/superpowers/specs/2026-10-01-enemy-attack-pace-design.md`, local only),
Alone in the Dark 1. Set the pace in F1 → Controls → Combat. Keep
`debug.mouseNavOverlay` off. Unless a row says otherwise, use the bedroom
window creature (floor 1, room 4): stand still and let it attack.

| # | Check | Emily | Carnby |
|---|---|---|---|
| 31 | Normal → it hits about every 2 s (100 ticks), as before | | |
| 32 | Slower → about twice the time between hits. Much slower → about three times | | |
| 33 | While it waits within reach → it stands still, facing the hero. Step well away (over about 1.2 m) → it chases again | | |
| 34 | Slower or Much slower → the hero's own strikes, punches and shots are unaffected | | |
| 35 | When the wait ends → it attacks normally and the blow lands as before | | |
| 36 | Two enemies at once (e.g. the dining room) → each waits on its own; one's attack never delays the other | | |
| 37 | Save during the wait, then load → the enemy attacks again within its normal wait; it is never frozen out | | |
| 38 | Open F1 → Controls → the "Enemy attack pace" combo shows Normal. Choose Much slower, quit and relaunch → it stays, and `aitd_remaster.cfg` has `controls.enemyAttackPace = 2` | | |
| 39 | AITD2 or AITD3 with the pace on Much slower → enemies behave as before | | |
| 40 | Slower or Much slower → the creature's attack sound plays once per attack and never loops during the wait | | |
