# Mythical gear and The Drowned Vigil (v2.40 / 79)

Combat PR 6. A sixth item rarity, **Mythical**, above Legendary, and a new focused
endgame run, **The Drowned Vigil**. Both open only after the first Rekindle
(Prestige rank ≥ 1). Online play is unchanged; Host/Join never touch solo saves.

## Mythical items

| Rule | Legendary | Mythical |
|---|---|---|
| Rarity id | 4 | **5** |
| Upgrade cap | +25 | **+30** (+32 at rank ≥ 7) |
| Legendary stat lines active | 1 / 2 / 3 at +0 / +10 / +20 (unchanged) | same thresholds |
| Sub-effect | unlocks at upgrade | active from **+0** |
| Attribute bonus | +1 | **+2** |
| Extra roll bonus | — | +5 over the Legendary roll (depth/5 + 4 + 5) |
| Mythic effect | — | one of five, **unlocks at +25** |
| Name | — | trailing ✦, colour `#F4795A` |

Attunement is unchanged: floor(level/2) + 3 slots at rank ≥ 1. The story hero keeps
the +1 resource padding.

### Names (original)

| Class | Weapon | Armor |
|---|---|---|
| Fighter | Kindlewrath | Hearthwall Plate |
| Wizard | Last Wick Focus | Cinderweave Mantle |
| Rogue | Ashfang | Smokeveil Hide |
| Cleric | Vigil Censer | Lantern-Saint Mail |
| Bard | Bellsong Rapier | Embercourt Coat |
| Any | Firstflame Blade | Firstflame Mail |

### Mythic effects (active at +25)

| Effect | Slot | What it does |
|---|---|---|
| Rekindle | armor | Once per battle, a blow that would drop the wearer leaves them at max(1, maxHP/4). Fires before Last Light. |
| Hearth Aura | armor | +1 AC to the living party while worn (applied as a delta; never stacks or drifts). |
| Ember Echo | weapon | Every 4th hit strikes again for max(1, damage/2). |
| Cinder Brand | weapon | Hits brand the target: burn 2 rounds at max(2, damage/6). |
| Lantern's Due | weapon | A kill heals the living party 5% of max HP. |

Per-battle counters (Rekindle used, Echo count) reset at initiative and are not saved.

## Drop rates

Mythical can drop only when the encounter curve is active, the hero has rank ≥ 1,
the kill is Legendary-eligible and no Mythical has dropped yet this run.
A Mythical roll happens first; on a miss the normal Legendary roll follows.

| Source | Chance | Measured (N = 400 000) |
|---|---|---|
| The Drowned Vigil (finale) | 7% | 7.039% |
| Boss Raid | 1.5% | 1.508% |
| Ember Ring Arena | 1.5% | 1.519% |
| Endless Deep | 1.5% | 1.479% |
| Challenge (Cinder Crucible) | its Legendary rate ÷ 6 (per-mille precision) | Leg 18% → 2.993%, 12% → 1.990%, 6% → 0.993% |
| Crawl | 1% | 0.999% |
| Story mode after Act 4 (not scripted fights) | 1% | 0.988% |
| Rank 0 / anything else | 0 | 0 (tested) |

### Pity

* Each eligible kill without a Mythical adds 1 to a persisted dry streak (max 1000).
* After **50** dry kills the chance doubles.
* At **150** (streak ≥ 149 before the roll) the drop is guaranteed.
* A Mythical resets the streak and sets the per-run flag: **at most one per run**.

Simulated (Boss Raid, 2 000 000 eligible kills): one Mythical per 50.2 kills
(base 66.7), 3.01% after 50 dry, longest dry run 149.
Drowned Vigil: one per 14.0 clears (base 14.3).

A "run" is a Raid / Arena / Endless / Challenge / Vigil entry; in crawl and story it
is a session (the flag clears on load).

## The Drowned Vigil

Entered from the Boss Raid dialog (Rekindle required) for **1 Raid Key** (refunded if
start fails). Three chambers up the Vigil Spire, original text, no SRD monster names
beyond existing in-game foes:

1. *Chamber 1/3*: a bonebound warden pack (ashbound bonebound, tithe enforcer, gloamhound, bone crier).
2. *Chamber 2/3*: a brute and a tithe enforcer.
3. *Finale*: a drowned Hollow Crown, Ember Hydra or Nightfang (existing art and 3 phases) plus two acolytes.

Encounter bonus is 4 + 2 × rank. Chambers 1–2 add a small luck bump; the finale rolls
Legendary 7% and Mythical 7%. A wipe follows the Boss Raid restore rules.

## Save format (append-only)

| Where | Field | Clamp |
|---|---|---|
| header 40 | Vigil chamber | 0..3 (0 outside the Vigil, 1 if in the Vigil and 0) |
| header 47 | Mythical dry streak | 0..1000 |
| header 48 | Mythical-this-run flag | 0/1 (cleared in STORY/CRAWL) |
| mode | 6 = Vigil | ≤ 6 |
| item token field 11 | Mythic effect | 0..5, written only for Mythical items |

v2.39 saves load with all new fields at 0; tokens with rarity > 5 still clamp to 4.
`tools/migration_test/mythic_test.cpp` checks three canonical v2.39 saves
(`v239_fixtures.h`) byte-for-byte, round-trips, clamps, rates, pity, the per-run
cap, rank 0 and the Vigil key cost.

## Art

Hand-written SVG vector drawables (`tools/gear_icons/gear_glyphs.py`): ember frame,
bone-white core, ringed-flame pip; 13 icons with calm variants (26 drawables),
`ic_rarity_mythical`, socket and row backgrounds. The "Mythical loot!" accent wash is
skipped under reduce-flash.

## Balance

**Martin's decision (#99): Mythical gear must not make fights harder**, including through the
gear-surplus rule. There is no Mythical encounter-level term, and for the curve only a Mythical
piece counts as the max-tier Legendary it would be:

* rarity Legendary, upgrade **min(upgrade, +25)** (the Legendary cap);
* bonus **minus the Mythical extra** (kMythicalExtraBonus 5 − kLegendaryExtraBonus 2 = **+3**) and
  minus every upgrade past +25 (each upgrade is +1 bonus);
* **no Mythic effect**, Hearth Aura off, so its stat lines give +1 (not +2) and follow the Legendary
  thresholds.

`curve::curveView` builds that view (CombatCurve.h); it feeds both the gear surplus (`kGearFollow`)
and the measured power (offEl / defEl), with AC and max HP moved by the difference only. The real
hero keeps every Mythical number in the fight. Heroes without a Mythical piece take the unchanged
v2.39 path. `mythic_test` checks Mythical +10…+32 == Legendary +min(u, 25) (same surplus, measured
stats, EL, offEl, defEl) over 2160 cases, and that a 19 200-case non-Mythical sweep hashes to
master's value.

Win % for Legendary +20 → **Mythical Echo/Rekindle** → Mythical Brand/Hearth, L50 Fighter + Wizard,
1000 fights; then HP lost; then EL. The EL is now the same in every column.

| Scenario | Rank I | Rank V | Rank X |
|---|---|---|---|
| Boss + escort (Medium) | 100 → **100** → 100 · EL 53 | 100 → **100** → 100 · EL 54 | 100 → **100** → 100 · EL 56 |
| Boss + escort (Hard) | 95 → **100** → 100 · 54/27/24% · EL 53 | 96 → **100** → 100 · EL 54 | 100 → **100** → 100 · EL 56 |
| Boss + escort (Nightmare) | 91 → **100** → 100 · 62/31/28% · EL 53 | 94 → **100** → 100 · EL 54 | 99 → **100** → 100 · EL 56 |
| Raid: Hollow Crown (Medium) | 100 → **100** → 100 · EL 55 | 100 · EL 56 | 100 · EL 58 |
| Raid: Ember Hydra (Hard) | 99 → **100** → 100 · EL 55 | 100 · EL 56 | 100 · EL 58 |
| Vigil 1/3 (Hard) | 96 → **100** → 100 · EL 59 | 98 → **100** → 100 · EL 64 | 100 · EL 64 |
| Vigil 1/3 (Nightmare) | 87 → **99** → 99 · EL 59 | 92 → **99** → 100 · EL 64 | 98 → **100** → 100 · EL 64 |
| Vigil finale + 2 (Medium) | 98 → **100** → 100 · EL 59 | 98 → **100** → 100 · EL 64 | 100 · EL 64 |
| Vigil finale + 2 (Hard) | 76 → **100** → 100 · 76/36/32% · EL 59 | 70 → **100** → 100 · EL 64 | 91 → **100** → 100 · EL 64 |
| Vigil finale + 2 (Nightmare) | 68 → **99** → 99 · 81/41/36% · EL 59 | 67 → **99** → 100 · EL 64 | 87 → **100** → 100 · EL 64 |

The EL saturates at kMaxEL 64 in the Vigil from rank V. All pre-existing sim tables
(`run.sh 150`, `run.sh 200 grid`) are identical to v2.39. Reproduce with
`bash tools/combat_sim/run.sh 1000 mythic`.

## Deviations from the proposal

* Pity applies to every eligible source (not Vigil-only +1pp per 10).
* Endless Deep is a flat 1.5% (not 2% from depth 30).
* No separate Vigil tile: entry is in the Boss Raid dialog and reuses the raid flags, so wipe dialogs say "Raid".
* Drowned bosses reuse existing boss ids/art/phases, plus two acolytes.
* State lives in the save header, not per-slot prefs; a raid-wipe restore loses that run's streak increments.
* The proposal's +10 accuracy cap does not exist in code, so it is N/A.
* The Vigil boss can drop Raid Keys (existing 55% focused-run rule).
* On Mythical-eligible kills the Legendary rate falls by a factor of about (1 − M).
