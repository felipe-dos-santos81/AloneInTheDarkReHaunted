# Automatic counter-attack checklist

Manual sign-off for the "Hit back automatically when an enemy strikes you"
option (spec: `docs/superpowers/specs/2026-10-01-auto-counter-attack-design.md`,
local only). Alone in the Dark 1. Run every row for **Emily** and **Carnby**.
Mark each cell `pass`, or `fail:` with a one-line note. Build:
`make build-fitd`; run: `make run data=DIR`. Unless a row says otherwise the
option is on (F1 → Controls → Combat) and the player does not touch the
controls after the enemy's blow lands.

| # | Check | Emily | Carnby |
|---|---|---|---|
| 1 | Option off: being hit plays the hurt animation and nothing else, exactly as before | | |
| 2 | Bare hands (Actions/Fight chosen): after the hurt animation the hero turns to the attacker and punches once, then stands | | |
| 3 | Sword: one strike after the hurt animation; the hero does not walk forward | | |
| 4 | Saber: as row 3 | | |
| 5 | Knife: as row 3 | | |
| 6 | Dagger: as row 3 | | |
| 7 | Revolver with ammo: draws, fires once at the attacker, one round spent | | |
| 8 | Rifle with ammo: aims, fires once at the attacker, one round spent | | |
| 9 | Revolver or rifle without ammo: draws/aims, no shot, control returns within about 2.5 s | | |
| 10 | Lamp in hand: the hero switches to Actions/Fight (as choosing Fight in the inventory), punches once, and Actions stays in hand afterwards | | |
| 11 | Actions with Push chosen: switches to Fight, punches once; the inventory shows Fight chosen afterwards | | |
| 12 | Holding a direction key or Action when the blow lands: no counter; the player's own input works normally | | |
| 13 | Mouse gameplay on, left button held in the world when the blow lands: no counter | | |
| 14 | Gamepad input right after the blow: the waiting counter is cancelled | | |
| 15 | Two enemies hitting within the hurt animation: exactly one counter, aimed at the last attacker | | |
| 16 | Toggling the option off (F1) while a counter waits: nothing happens after closing F1; no input stays held | | |
| 17 | A screen opening during the strike (inventory key pressed, or a found-object screen): the screen gets no phantom selection, and the counter does not resume afterwards | | |
| 18 | A hit mid-counter-swing: hurt animation, then a single new counter | | |
| 19 | A killing blow: the death sequence plays normally; no counter | | |
| 20 | A thrown object or hazard hitting the hero: no counter | | |
| 21 | Cutscenes and intros with the option on: unaffected; a left click still skips | | |
| 22 | Mouse click-attack on an enemy (option on and off): faces and swings once, unchanged (mouse checklist row 13) | | |
| 23 | Keyboard-only and gamepad-only fights with the option off: unchanged from before | | |
| 24 | Save, load, map, inventory and system menu during and after combat: no counter survives them | | |
| 25 | Options window: F1 → Controls shows a "Combat" heading above "Controller behavior" with the unticked checkbox "Hit back automatically when an enemy strikes you"; hovering (?) shows the help text; ticking it, quitting and relaunching keeps it ticked (aitd_remaster.cfg has controls.autoCounterAttack = true) | | |
| 26 | The strike misses (the attacker steps out of reach before the blow): exactly one swing, then the hero stands; no second swing | | |
| 27 | A key pressed during the hurt animation (not held before the blow): the waiting counter is cancelled and the key acts normally | | |
| 28 | Mouse gameplay on, a left click in the world after the blow: the waiting counter is cancelled; the click does what it normally does | | |
| 29 | Attacker standing just across a room boundary: the hero turns toward it (not a wrong angle) and strikes once | | |
| 30 | An enemy that swings again at once (the attic monster): the counter lands between its blows and it flinches | | |

## Enemy attack pace

Manual sign-off for "Enemy attack pace" (spec:
`docs/superpowers/specs/2026-10-01-enemy-attack-pace-design.md`, local only).
Alone in the Dark 1, the bedroom window creature (floor 1, room 4) unless a
row says otherwise; stand still and let it attack. Set the pace in F1 →
Controls → Combat. With `debug.mouseNavOverlay` off.

| # | Check | Emily | Carnby |
|---|---|---|---|
| 31 | Normal: it hits about every 2 s (100 ticks), exactly as before | | |
| 32 | Slower: about twice the time between hits; Much slower: about three times | | |
| 33 | While it waits within reach it stands still facing the hero; step well away (over ~1.2 m) and it chases again | | |
| 34 | Slower/Much slower: the hero's own strikes, punches and shots are unaffected | | |
| 35 | When the wait ends it attacks normally and the blow lands as before | | |
| 36 | Two enemies at once (e.g. the dining room): each waits on its own; one's attack never delays the other | | |
| 37 | Save during the wait, load: the enemy attacks again within its normal wait, never frozen out | | |
| 38 | Options window: the combo "Enemy attack pace" shows Normal by default; choosing Much slower, quitting and relaunching keeps it (aitd_remaster.cfg has controls.enemyAttackPace = 2) | | |
| 39 | AITD2 / AITD3 with the option on Much slower: enemies behave as before | | |
| 40 | Slower/Much slower: the creature's attack sound plays once per attack, never looping during the wait | | |
