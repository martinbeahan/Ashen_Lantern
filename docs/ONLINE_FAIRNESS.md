# Online Keeper-table fairness (v2.41 / 80)

**Report:** in online co-op a joining player on Easy died in the first round. The host suspected the
Encounter Level was scaled from the host's level / gear (possibly Mythical).

## Root cause (master 381eb63, v2.40)

1. **The host has no hero, and online foes ignore party level and gear.**
   - `Game::startDmSession` clears `players_` and sets `dmOnlyTable_ = true` (Game.cpp:205, 217).
   - `encounterCurveActive()` is `isHost_ && !dmOnlyTable_` (Game.h:359). It is false on every online
     table, so neither the EL curve nor `kGearFollow` / `curveView` runs online.
   - Online foes are the classic level-1 class foes, scaled only by the room number (for example
     `spawnRoomContent`, Game.cpp:1289). They don't depend on any hero's level or gear.
   - A joiner is always a brand-new level-1 hero with 12 HP and starter gear (`Game::addAlly`,
     Game.cpp:327).
   - So the host-gear hypothesis is **not** the cause. `online_test` checks that adding a level-50
     +32 Mythical veteran leaves every foe identical, on master and after the fix.
2. **Online tables had none of solo's safety rails.**
   - The 45% per-hit guard ran only `if (curved …)` (Game.cpp:4576).
   - Legacy crits double every die, Sneak Attack included (Game.cpp:4574).
   - A room-1 Wickthief (+5 to hit, d6+3, plus Sneak Attack d6) could deal up to 27 to a 12-HP joiner.
   - Measured on master (Easy, room 1, joiner alone, 3000 fights):
     - the joiner was down before their 2nd turn in **28.5%** of fights;
     - died in **55.4%**;
     - went from **full HP to 0 in one hit in 414 fights (13.8%)**;
     - took 2441 hits over 45% of max HP.
3. **Easy did nothing to ordinary online foes.** Trash spawns (Game.cpp:1289–1300) never read
   `difficulty_`. Only bosses and set-pieces had Easy tweaks.
4. **The table didn't use the host's difficulty.**
   - Kotlin never called `setDifficulty` (MainActivity.kt:2922 only calls `startDmSession`), and
     `startDmSession` didn't set `difficulty_`.
   - The table therefore kept whatever the native game last held, i.e. the host's last solo run (for
     example Nightmare). The joiner's own choice is never sent (Join sends only name + class).

## Fix (online tables only: `onlineTableRules()` = host && Keeper table)

| Rule | Value | Where |
|---|---|---|
| Per-hit cap on a standing hero | max 45% of max HP (`curve::overkillCap`, as solo), every difficulty | `enemyTurn` |
| Soft crit on heroes below level 5 | crit = normal damage × 1.5 instead of doubled dice (solo: below EL 5) | `enemyTurn` |
| Easy foes | HP × 0.85 at spawn, damage × 0.85, −1 to hit (solo Easy factors, `curve::difficultyInfo(EASY)`) | `applyEncounterCurve` (legacy branch), `enemyTurn` |
| Table difficulty | Easy on `startDmSession`, then the host's pick via `setDifficulty(OnlineFairness.tableDifficulty(pref))`; chat line *Table difficulty: …* | Game.cpp, MainActivity.kt |

- Medium, Hard and Nightmare tables get the cap and the soft crits, but their foe numbers are unchanged.
- Bosses that already had Easy tweaks also get the Easy ×0.85.

**Why per-target rather than a party EL:** online foes have no EL or party input to re-weight. They
follow the room number, so the fairness has to sit where the damage lands: a cap relative to the
target's own max HP, and crit softening keyed on the target's own level. That protects the weakest
hero without making the fight trivial for a veteran.

**Mythical:** online heroes start at rank 0 and have no attunement or curve, so Mythical gear can't
reach online foes. Any future online EL should build `PartyMember`s with `curve::memberOf`, which
already applies `curveView` (v2.40), so a Mythical piece counts as its +25 Legendary.

**Unchanged:**
- solo play: every existing `tools/combat_sim` table is identical to v2.40;
- clients, where `isHost_` is false;
- saves (no new fields; the difficulty already travels in the state sync);
- the online protocol and database rules (#95). Joiners still send only name + class.

## Results (fresh level-1 joiner, 5 classes, basic attacks)

**`online_test` repro** (Easy, room 1, joiner alone, 3000 fights):

| | v2.40 | v2.41 |
|---|---|---|
| Down before its 2nd turn | 28.5% | **0.0%** |
| Dead in the fight (Easy *Continue* still revives) | 55.4% | 17.2% |
| Full HP → 0 in one hit | 414 fights | **0** |
| Hits over 45% of max HP | 2441 | **0** |
| Worst single hit | 100% of max HP | 42% |

**`tools/combat_sim/run.sh 1000 online`.** Each cell is down by the 2nd turn / dead / one-shot fights:

| Party | Room | Easy v2.40 | Easy v2.41 | Medium v2.40 | Medium v2.41 |
|---|---|---|---|---|---|
| joiner alone | 1 | 29.2% / 56.7% / 139 | **0.0% / 14.8% / 0** | 30.7% / 57.0% / 154 | **0.0% / 28.6% / 0** |
| joiner alone | 3 | 28.4% / 69.6% / 99 | **1.1% / 30.5% / 0** | 30.1% / 73.5% / 105 | **3.4% / 50.4% / 0** |
| joiner alone | 5 | 27.7% / 78.9% / 108 | **0.5% / 43.4% / 0** | 41.8% / 88.9% / 105 | **10.8% / 76.1% / 0** |
| joiner alone | 10 | 17.8% / 85.2% / 85 | **0.0% / 56.6% / 0** | 61.7% / 99.9% / 130 | **24.6% / 99.7% / 0** |
| + L5 vet | 10 | 0 / 0 / 0 | 0 / 0 / 0 | 0.0% / 6.2% / 0 | 0.0% / 4.6% / 0 |
| + L10 / L20 / L50 Mythical vet | 1–10 | 0 / 0 / 0 | 0 / 0 / 0 | 0 / 0 / 0 | 0 / 0 / 0 |

## Still open

- A lone level-1 joiner on a deep table (room 10+) still loses most fights, because foes follow the
  room number.
- A later option is to cap the room-depth scaling by the weakest hero's level on online tables.
  That changes foe numbers for everyone at the table, so it needs a product call.
