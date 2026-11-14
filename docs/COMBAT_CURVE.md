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

## v2.34 rebalance — the EL follows the party's real power

Playtest: "too hard at level 50, even on Easy". Cause, found in code and confirmed in `tools/combat_sim`:

1. **The EL assumed a model hero.** Foes are built from the Reference Lamplighter at the EL (stat points
   spent 50/30/20, gear ⌊L/2⌋ per slot), and the EL only fell 2 below the level however weak the party
   really was (`kElBelowLevel`, gear surplus clamped at −4). A level-50 hero with points and Embers unspent
   and starter gear met full level-48 foes: **0% wins on every difficulty** (to-hit +9 against AC 50+).
2. **The debug shortcut made exactly that hero** (levels only: every point, every Ember and the gear left
   as they were), so testing at 50 met the worst case.
3. **The sims hid it.** `curve::reference()` cached its table on first use, and the first sim column ran
   under `perks::legacyLevelRulesForSim()` (v2.31 proficiency growing to +14), so every v2.32 / v2.33 table
   past level 20 was built against much tougher foes than the game ships (the app never sets the flag).
   The table is now cached per flag combination; numbers below are the real game's.
4. **A Nightmare stall:** a brute that telegraphed "finish the fallen" on a hero who then died ended its
   turn with no new plan, forever, while the last hero lay stable — the fight never ended.

What changed (solo only — online Keeper tables keep the v2.32 level + gear EL):

```
gap_off = levels below its level the party's offense really sits   (to-hit x damage vs the model's)
gap_def = same for defense (HP / chance to be hit), less 15% of the gap (level-based class features)
EL      = round(avg + 0.5·clamp(surplus, −4, 8) + 0.25·prestige + 0.10·Embers spent), clamped as before
offEl   = gap_off < 0.5 ? EL : min(EL, round(avg − gap_off + 0.25·prestige + 0.10·Embers))   // foe HP, AC
defEl   = gap_def < 0.5 ? EL : min(EL, round(avg − gap_def + …))                             // foe damage, to-hit
shown EL = round((offEl + defEl) / 2) + mode bonus        (saved per foe, pays the XP)
```

- **Two axes** (`curve::encounterLevels`, `foeStatsSplit`): foe HP and AC follow what the party can
  actually hit; foe damage and to-hit follow what it can actually take. A party built like the model has
  `offEl = defEl = EL`, so its numbers are unchanged. Measured on the party's *average* numbers against the
  Reference Lamplighter (Fighter / Wizard), with a per-class damage factor from the sims (Fighter 1.4,
  Wizard 0.6, Cleric 0.45, Rogue 2.0, Bard 0.8) — so a Cleric + Wizard party no longer drags bosses out to
  20+ rounds. Never raises the EL; Wards and Arts aren't measured.
- **Embers count a little:** +0.1 EL per spent Ember (30 from levels = +3), well under what they buy, so a
  built level-30+ hero is no longer far ahead of the curve.
- **Nightmare** foe HP ×1.25 → ×1.15 (boss fights ran 15–17 rounds), damage ×1.15 → ×1.22 to keep the risk.
- **Easy (Sloppy) AI** gangs up less: 45% random target (was 30%), half the pull toward a wounded hero and
  no "finish the weak one" bonus (ENEMY_AI.md).
- **Stall fix:** the brute strikes another fallen hero instead (and any foe that loses its target re-plans).
- **Debug shortcut** (debug builds only) now builds the hero like a player — see PRESTIGE.md.
- Solo foe toughness (+15% others / +10% boss with escort / +40% boss alone) re-checked and kept: raids stay
  at 6–8 rounds and boss + escort at 9–15.

`tools/combat_sim/run.sh 600` (Fighter + Wizard, Medium unless named). **built** = model stats + gear +
the sim's Kindled Path loadout; **unspent** = class base stats with every point unspent, starter gear, no
Embers. Each cell: win %, party HP lost %, rounds, EL shown.

| Difficulty | Level | room: built | room: unspent | boss + escort: built | boss + escort: unspent |
|---|---|---|---|---|---|
| Easy | 1 | 100% · 0% · 2.2 r · EL 1 | 100% · 0% · 2.2 r · EL 1 | 100% · 0% · 4.4 r · EL 1 | 100% · 0% · 4.4 r · EL 1 |
| Easy | 10 | 100% · 10% · 2.5 r · EL 10 | 100% · 10% · 2.1 r · EL 5 | 99% · 29% · 6.5 r · EL 10 | 100% · 16% · 4.6 r · EL 5 |
| Easy | 20 | 100% · 10% · 2.6 r · EL 20 | 100% · 12% · 2.1 r · EL 7 | 99% · 29% · 6.5 r · EL 20 | 100% · 19% · 4.5 r · EL 7 |
| Easy | 30 | 100% · 7% · 2.7 r · EL 31 | 100% · 13% · 2.1 r · EL 9 | 100% · 20% · 6.0 r · EL 31 | 100% · 21% · 4.6 r · EL 9 |
| Easy | 40 | 100% · 8% · 2.8 r · EL 42 | 100% · 13% · 2.1 r · EL 10 | 100% · 26% · 6.7 r · EL 42 | 100% · 22% · 4.6 r · EL 10 |
| Easy | 50 | 100% · 6% · 2.8 r · EL 53 | 100% · 13% · 2.1 r · EL 12 | 100% · 21% · 6.5 r · EL 53 | 100% · 22% · 4.7 r · EL 12 |
| Medium | 1 | 100% · 0% · 2.5 r · EL 1 | 100% · 0% · 2.5 r · EL 1 | 100% · 0% · 6.1 r · EL 1 | 100% · 0% · 6.1 r · EL 1 |
| Medium | 10 | 100% · 19% · 3.8 r · EL 10 | 100% · 16% · 2.8 r · EL 5 | 80% · 57% · 10.9 r · EL 10 | 98% · 28% · 6.7 r · EL 5 |
| Medium | 20 | 100% · 23% · 4.2 r · EL 20 | 100% · 21% · 3.0 r · EL 7 | 77% · 63% · 11.2 r · EL 20 | 95% · 39% · 7.2 r · EL 7 |
| Medium | 30 | 100% · 15% · 3.6 r · EL 31 | 100% · 23% · 3.1 r · EL 9 | 92% · 47% · 10.1 r · EL 31 | 91% · 43% · 7.6 r · EL 9 |
| Medium | 40 | 100% · 19% · 3.9 r · EL 42 | 100% · 24% · 3.3 r · EL 10 | 92% · 53% · 10.5 r · EL 42 | 90% · 46% · 8.0 r · EL 10 |
| Medium | 50 | 100% · 14% · 3.7 r · EL 53 | 100% · 25% · 3.3 r · EL 12 | 99% · 42% · 9.3 r · EL 53 | 89% · 47% · 8.1 r · EL 12 |
| Hard | 1 | 100% · 0% · 3.2 r · EL 1 | 100% · 0% · 3.2 r · EL 1 | 96% · 3% · 7.8 r · EL 1 | 96% · 3% · 7.8 r · EL 1 |
| Hard | 10 | 100% · 29% · 5.3 r · EL 10 | 100% · 25% · 4.1 r · EL 5 | 46% · 82% · 12.7 r · EL 10 | 93% · 43% · 9.5 r · EL 5 |
| Hard | 20 | 99% · 34% · 5.8 r · EL 20 | 100% · 33% · 4.5 r · EL 7 | 43% · 83% · 12.9 r · EL 20 | 80% · 57% · 9.6 r · EL 7 |
| Hard | 30 | 99% · 33% · 5.6 r · EL 31 | 98% · 38% · 4.9 r · EL 9 | 61% · 76% · 14.8 r · EL 31 | 73% · 63% · 10.2 r · EL 9 |
| Hard | 40 | 99% · 41% · 6.4 r · EL 42 | 97% · 41% · 5.1 r · EL 10 | 60% · 81% · 14.7 r · EL 42 | 67% · 68% · 10.5 r · EL 10 |
| Hard | 50 | 100% · 32% · 5.6 r · EL 53 | 97% · 41% · 5.2 r · EL 12 | 83% · 68% · 13.0 r · EL 53 | 65% · 69% · 10.6 r · EL 12 |
| Nightmare | 1 | 99% · 1% · 3.9 r · EL 1 | 99% · 1% · 3.9 r · EL 1 | 96% · 2% · 8.2 r · EL 1 | 96% · 2% · 8.2 r · EL 1 |
| Nightmare | 10 | 97% · 39% · 6.9 r · EL 10 | 99% · 36% · 5.5 r · EL 5 | 36% · 87% · 12.4 r · EL 10 | 75% · 59% · 11.1 r · EL 5 |
| Nightmare | 20 | 97% · 44% · 7.3 r · EL 20 | 94% · 51% · 6.3 r · EL 7 | 35% · 88% · 12.6 r · EL 20 | 58% · 73% · 10.9 r · EL 7 |
| Nightmare | 30 | 98% · 44% · 7.2 r · EL 31 | 90% · 55% · 6.7 r · EL 9 | 53% · 82% · 15.3 r · EL 31 | 46% · 80% · 11.4 r · EL 9 |
| Nightmare | 40 | 96% · 54% · 8.0 r · EL 42 | 88% · 58% · 6.8 r · EL 10 | 46% · 89% · 15.3 r · EL 42 | 39% · 84% · 11.6 r · EL 10 |
| Nightmare | 50 | 99% · 41% · 6.7 r · EL 53 | 88% · 59% · 6.9 r · EL 12 | 78% · 76% · 14.2 r · EL 53 | 37% · 85% · 11.6 r · EL 12 |

| Debug shortcut (fresh hero → level 50) | room | boss + escort |
|---|---|---|
| Easy | 100% · 3% · 2.8 r · EL 53 | 100% · 12% · 6.6 r · EL 53 |
| Medium | 100% · 8% · 3.8 r · EL 53 | 100% · 25% · 8.2 r · EL 53 |
| Hard | 100% · 19% · 5.0 r · EL 53 | 99% · 42% · 10.9 r · EL 53 |
| Nightmare | 100% · 25% · 5.7 r · EL 53 | 98% · 50% · 12.2 r · EL 53 |

Before this change (same seeds, sim cache fixed): the unspent hero lost **100% of fights from level 20 up on
every difficulty** (Easy boss L10: 4%), and so did the debug-shortcut hero; built heroes: Medium boss
81 / 76 / 95 / 98 / 100% at L10–50, Hard 45 / 44 / 65 / 86 / 96%, Nightmare 41 / 39 / 56 / 79 / 95%
(boss fights 15–17 rounds on Nightmare at L30–40, one Nightmare cell 70 rounds from the stall).

| Party (boss + escort, Medium) | Level | v2.31 | v2.32 + perks |
|---|---|---|---|
| Rogue + Cleric | 30 | 87% · 35% · 8.7 | **99%** · 18% · 5.0 |
| Rogue + Cleric | 50 | 90% · 27% · 7.1 | **100%** · 13% · 4.2 |
| Bard + Fighter | 30 | 70% · 74% · 13.7 | **91%** · 55% · 11.1 |
| Bard + Fighter | 50 | 62% · 80% · 14.5 | **100%** · 40% · 8.3 |
| Cleric + Wizard | 30 | 91% · 30% · 9.5 | **84%** · 40% · 11.1 |
| Cleric + Wizard | 50 | 70% · 47% · 15.2 | **88%** · 36% · 11.4 |

Trade-offs: past level 30 a built hero on Medium still wins the boss + escort 92–99% (target 76–85%);
pushing it lower needs a larger Ember weight, which also drops Hard / Nightmare well below their old
numbers (tried 0.15: Hard L30 32%, Nightmare 20%). An unspent hero is easier than a built one on Medium
(90–98%) but earns less XP (room XP follows the EL) and is told to spend.

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
