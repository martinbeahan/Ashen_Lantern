# Combat curve (v2.29 / 68) — PR 1 of the combat & progression revamp

Native code: `app/src/main/cpp/CombatCurve.h` (namespace `dnd::curve`), wired in `Game.cpp`
(`applyEncounterCurve`, `noteRoomOutcomeForCurve`, `enemyTurn`), saved in `GameSave.cpp`.
UI: `CombatCurveUi.kt` (status badge, surprise warning). Tests: `tools/migration_test/combat_test.cpp`,
`CombatCurveUiTest.kt`. Simulator: `tools/combat_sim/run.sh` (Monte Carlo against the shipped headers).

## When it applies

Only in solo play: `isHost_ && !dmOnlyTable_`. The online Keeper table and joiners keep the
old rules, and Host / Join never read or write solo saves.

## Encounter Level

```
avg     = mean party level
surplus = (party gear bonus − expected gear 2·⌊L/2⌋) / 2      // gear counts 50%
EL      = round(avg + 0.5·clamp(surplus, −4, 8) + 0.25·prestige)   // prestige = 0 until PR 5
EL      = clamp(EL, avg − 2, avg + 6) + mode bonus
```

Mode bonus: Boss Raid +2, Challenge +3, Ember Ring wave 5 +2, Ashen Deep +depth/5. **Deviation:**
the mode bonus raises foe HP and damage only. AC and to-hit use the party's base EL (`accuracyEl`),
because a full +2 EL gave a 16% raid win rate in the sim. Tables cover levels 1–50 (`kTableMaxLevel`);
EL is capped at 64.

## Roles

A **Reference Lamplighter** (an average of a real Fighter and Wizard at that level, with a 50/30/20
stat split and +⌊L/2⌋ gear per slot) sets the baseline. Each role is described by how many party hits
it takes to fell, how many of its hits drop the reference hero, a to-hit/AC offset and a threat cost.

| Role | id | Used by |
|---|---|---|
| Minion | 1 | rats |
| Skirmisher | 2 | wickthieves, hounds, scavengers (default) |
| Caster | 3 | watchers |
| Support | 4 | criers |
| Brute | 5 | ashbound, brutes, enforcers |
| Elite | 6 | surprise bosses |
| Boss | 7 | every `boss_*` |

## Budgets

Difficulty multiplier ×: Easy 0.85, Medium 1.0, Hard 1.12, Nightmare 1.25 (max 4 foes; 5 on Nightmare).
Room type ×: soft open 0.6 (crawl rooms 1–2), normal 1.0, set piece 1.3, Deep milestone 1.6.
Variance ±15%, never over budget twice in a row. Boss rooms: the boss gets the room, adds share 0.5× budget.
Surprise bosses (Ossuary Prior at the Bone Gallery, Wick King at the Weir) come in as Elites capped at
1.5× budget, with a one-turn telegraph (`WARNING! …`). With *Reduce flashing* on, the warning is a toast only.

## Dice

- Hit chance clamped to 15–90% (the log notes "needs N+ — hit clamp").
- Foe damage = average × U(0.75, 1.25); crits ×2 (×1.5 below EL 5); a Rogue foe's sneak ×1.3.
- Overkill guard: one hit takes at most 45% of a hero's max HP. There is a hook to exempt future
  telegraphed heavy attacks (PR 2); for now it applies to bosses too.

## Comeback / Pressure

- Party wipe → Rekindle: next room (3 on Hard) at 0.8× budget, story bosses floor 0.9×. Pressure resets.
- A room cleared with everyone above 70% HP and nobody down → Pressure +1 (max 4). Each pip adds +5%
  threat and +5% XP and gold.
- A hero dropping resets Pressure. Two rooms in a row ending below 25% HP → one Rekindle room.

## Bug fixes (with tests)

- **a** Foe AC is saved (enemy field 19), so a mid-fight reload keeps it.
- **b** Max HP = formula + `bonusMaxHp` (odd CON points, the story +2), so level-ups keep it.
- **c** `addXpLevels` loops, so one award can give several levels.
- **d** HP, initiative, Search and Healing Word use effective (gear-included) stats.
- Also from §1d: a level-1 hero has at least 12 HP.

## Save format (append-only)

| Section | Field | Meaning |
|---|---|---|
| Header | 37 | `S3` schema tag |
| Header | 38 / 39 | Pressure / Rekindle rooms |
| Header | 40 | reserved (`0`, Vigil) |
| Header | 41 / 42 / 43 | low-HP streak / last room over budget / hero dropped this room |
| Player | 27 | prestige rank (0) |
| Player | 28 | `bonusMaxHp`. On older saves it is derived as saved max − formula; max HP never shrinks on load |
| Enemy | 19–26 | AC, role, EL, to-hit, avg dmg ×10, phase (0), warn turns, surprise |

Note: the proposal planned player fields 28–32 for later PRs; those shift to 29+.
A v2.28 mid-fight foe has no curve fields and keeps the old rules until that fight ends.

## Not in this PR

Abilities / intents / AI and statuses (PR 2), XP formula (PR 4), level cap 50 and optional prestige
with the Rekindle / Not now dialog (later PRs). The story +1 resource padding is still lost at level-up
(noticed, not fixed here).
