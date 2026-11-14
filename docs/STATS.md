# Universal stats (v2.31 / 70)

PR 3 of the combat / progression revamp. In **solo** play every stat helps every class. The
class's **main stat** still drives hit, damage and the class special, has a higher cap and unlocks
**Signature** bonuses. Online tables (Host / Join, the Keeper table) keep the classic rules.

Code: `app/src/main/cpp/StatRules.h` (pure numbers), `Character` helpers in `GameLogic.h`,
effects in `Game.cpp`, the save field in `GameSave.cpp`, the guide for the UI in
`Game::getStatGuide` → JNI `getStatGuide` → `StatGuide.kt`.

## Gating

`Game::universalStatsActive()` = solo curve fights (`isHost_ && !dmOnlyTable_`, the same gate as
the combat curve and enemy AI). `Game::syncStatRules()` copies it to `Character::statRules` on
every hero (runtime only, never saved). It runs on load, at the start of each fight, on every
turn, before a level-up spend and when the UI asks for the guide. With `statRules` false every
formula below is skipped, so online play is byte-for-byte the old rules.

## Formulas

`mod = floor((score − 10) / 2)`, using **effective** scores (gear included).

| Stat | Main for | Universal effect (all classes) | Formula (StatRules.h) |
|---|---|---|---|
| STR | Fighter | **Heft** extra weapon damage; **Break guard**: a hit strips Guarded (shield-wall); STR save vs **Shackle** | `+floor(STRmod / 2)` damage (min 0); break guard at mod ≥ +3 (STR 16) |
| DEX | Rogue | AC (now capped), initiative, **Evade** vs telegraphed heavies and area attacks; DEX saves (Cinder breath) | AC `+min(DEXmod, 6)`; initiative `+DEXmod`; damage `−3% × DEXmod` (max −24%) |
| CON | — | HP per level and Short Rest (as before); CON save vs Burn (as before); **death saves** | death save `+floor(CONmod / 2)` (min 0) |
| INT | Wizard | Search (as before); **Weak point** crit range; **Read intent** | crit 19+ at mod +4 (INT 18), 18+ at +8 (INT 26); exact chip numbers at mod ≥ +2 (INT 14), a 60–140% range below that |
| WIS | Cleric | **Mending** (all healing received, potions too); **Resolve** saves vs Rattled, Marked and Webs (Slowed); +1 resource on Short Rest at mod ≥ +4 | healing `+4% × WISmod` (max +40%) |
| CHA | Bard | **Barter**, **Luck of the Lantern**, **Inspire**, **Intimidate** | buy `−2% × mod` (max −20%); sell `+1% × mod` (65% cap → 75%); gold `+3% × mod` (max +30%) on kill loot and Search gold; companion to-hit `+floor(mod / 4)`; foe flee threshold `+2% × mod` (max +20%) |

Barter uses the buyer's / seller's own CHA. Luck and Intimidate use the party's best CHA. Inspire
helps companions only (not the lead hero) and uses the best CHA among the *other* heroes, so a Bard
lead inspires the companion and a Bard companion does not inspire itself.

**Class specials**

- Magic Missile darts: `1d4 + 1 + floor(INTmod / 3)` each.
- Action Surge's second swing: `+floor(STRmod / 2)` (Heft again).
- Healing Word: effective WIS (as before), then Mending on the target.
- Sneak Attack dice and Cutting Quip: unchanged, plus the Signature tiers.

## Caps

`cap = min(30, 18 + floor(level / 2)) + (main ? 2 : 0) + prestigeRank` (prestige is 0 until PR 5).
Caps apply to **base** scores; gear can go above them. The level-up screen shows the cap on each
row, greys out **+** when a stat is full, and the native `Character::increaseAttribute` refuses a
point over the cap (`playerIncreaseStat` says why). Online: no caps.

| Level | 1 | 4 | 10 | 20 | 24+ |
|---|---|---|---|---|---|
| Normal cap | 18 | 20 | 23 | 28 | 30 |
| Main-stat cap | 20 | 22 | 25 | 30 | 32 |

## Signature tiers (effective main score)

| Main | Bonus |
|---|---|
| 20 | +1 class special use per battle (`Character::signatureUses`, reset each fight). A Fighter spends a supply for the first Action Surge as before; the Signature use is a **second** surge. Other classes spend the free use before a supply |
| 24 | Special potency +25% (damage, heal or Quip penalty 2 → 3) |
| 28 | Special crit range +1 (19–20; with Weak point it stacks, never below 17) on Action Surge, Sneak Attack and Cutting Quip |
| 32 | Lantern Arts cooldown −1 round — **no effect until Arts ship (PR 4)** |

## Save migration (append-only)

- Player field **31** = `signatureUses` (0–1). v2.30 saves have no field 31; the use is granted on
  load when the hero is already at main 20+ (generous for a mid-fight save).
- On load (and the first time the rules apply), `syncStatRules` clamps every base score above its
  cap down to the cap and **refunds the excess** into unspent stat points. HP and AC are recomputed.
  A chat line says what changed: `Stat caps (v2.31): Mira: STR 25 → 22 (3 points refunded).` It is
  idempotent: a second load refunds nothing.
- Online joiners / the Keeper table never clamp; Host / Join never read or write a solo save.
- Tests: `tools/migration_test/stats_test.cpp` (formulas, caps, refund once, a v2.30 **mid-fight**
  slot loading with caps, Signature uses, Mending / crit / Luck, Barter arbitrage over 37k
  item × CHA combos, the capped model hero, the guide, online unchanged, 25 full fights).

## Curve and balance

- The **Reference Lamplighter** (`curve::makeReferenceHero`) now respects the caps: points over a
  cap overflow to CON, then DEX, then the rest, and its AC uses the DEX +6 cap like a real hero.
  The model reads raw numbers (no Heft, crit range or Signature), so universal effects are a real
  gain for the player.
- To keep fights from collapsing, solo foes get extra HP (`curve::statsToughness`): +15% for normal
  foes, +10% for a boss with an escort, +40% for a boss alone (single targets take the whole
  Signature burst). It ramps in from EL 1 to EL 10.
- Sell prices: the CHA extra is paid on the item's value **before forge upgrades**, so an upgrade can
  never be resold at a profit; sell price always stays below the Barter buy price.

### Sim: v2.30 vs v2.31 (`tools/combat_sim/run.sh 1000`)

Fighter + Wizard party on auto, enemy AI on, 1000 seeded fights per cell. v2.30 = uncapped model hero
and main-stat-only rules; v2.31 = caps, universal effects, Signature and the toughness above.

| Level | Medium room (Wickthief, Ashbound, Watcher): v2.30 → v2.31 win, HP lost | Medium boss (Sexton + Ashbound) | Hard room | Nightmare (2 hounds + crier) |
|---|---|---|---|---|
| 1 | 100% → **100%**, 0% → 0% | 98% → **100%**, 2% → 0% | 99% → **99%**, 1% → 0% | 86% → **92%**, 11% → 6% |
| 3 | 100% → **100%**, 17% → 14% | 100% → **100%**, 35% → 25% | 100% → **100%**, 22% → 19% | 99% → **100%**, 33% → 26% |
| 5 | 100% → **100%**, 20% → 17% | 76% → **86%**, 64% → 49% | 100% → **100%**, 28% → 25% | 89% → **94%**, 52% → 43% |
| 10 | 100% → **100%**, 26% → 19% | 50% → **79%**, 83% → 58% | 98% → **100%**, 39% → 28% | 77% → **89%**, 72% → 54% |
| 15 | 100% → **100%**, 31% → 21% | 37% → **76%**, 90% → 66% | 97% → **99%**, 45% → 34% | 70% → **88%**, 78% → 59% |
| 20 | 99% → **100%**, 34% → 23% | 26% → **77%**, 94% → 63% | 95% → **98%**, 51% → 35% | 57% → **88%**, 86% → 57% |
| 25 | 100% → **100%**, 34% → 23% | 22% → **75%**, 95% → 66% | 95% → **99%**, 53% → 35% | 54% → **88%**, 88% → 59% |
| 30 | 99% → **100%**, 35% → 23% | 21% → **70%**, 96% → 71% | 94% → **99%**, 55% → 37% | 52% → **85%**, 89% → 62% |
| 35 | 100% → **100%**, 35% → 24% | 20% → **69%**, 96% → 71% | 93% → **99%**, 57% → 38% | 48% → **84%**, 91% → 64% |
| 40 | 100% → **100%**, 35% → 26% | 16% → **67%**, 97% → 74% | 93% → **98%**, 58% → 39% | 46% → **82%**, 91% → 67% |
| 45 | 100% → **100%**, 36% → 26% | 16% → **62%**, 97% → 77% | 91% → **98%**, 59% → 40% | 42% → **81%**, 92% → 68% |
| 50 | 100% → **100%**, 36% → 27% | 15% → **60%**, 97% → 79% | 92% → **98%**, 60% → 41% | 41% → **80%**, 92% → 70% |

| Scenario | EL | v2.30 stats: win · HP lost · rounds | v2.31 universal stats: win · HP lost · rounds |
|---|---|---|---|
| L2 Bone Gallery surprise: Ossuary Prior (Elite, warned) | 2 | 100.0% · 0% · 4.5 | **100.0%** · 0% · 3.6 |
| L3 Lantern Sexton | 3 | 100.0% · 20% · 5.7 | **100.0%** · 15% · 4.9 |
| L5 Wick King | 5 | 99.8% · 24% · 7.7 | **99.9%** · 21% · 7.2 |
| L6 Collector Sallow Venn | 6 | 99.9% · 20% · 7.0 | **99.9%** · 19% · 7.0 |
| L8 First Lamplighter (Warden) | 8 | 99.6% · 32% · 7.3 | **99.7%** · 30% · 6.8 |
| L9 Bell-Herald | 9 | 100.0% · 17% · 7.0 | **100.0%** · 13% · 5.7 |
| L10 Ashen Drake | 10 | 98.7% · 38% · 8.2 | **99.3%** · 30% · 6.8 |
| L10 Millstone Brute + Enforcer + Watcher (Hard) | 10 | 92.5% · 53% · 8.0 | **94.6%** · 41% · 6.4 |
| L10 4 Wickthieves (flee + snatch) | 10 | 90.4% · 55% · 8.1 | **93.5%** · 46% · 7.0 |
| L10 Bone Crier + 2 Ashbound (heals) | 10 | 99.4% · 30% · 5.7 | **99.7%** · 20% · 3.9 |
| Raid L12 Ember Hydra (+2 EL) | 14 | 88.8% · 55% · 11.2 | **84.0%** · 52% · 9.6 |
| Raid L20 Hollow Crown | 22 | 87.7% · 64% · 12.1 | **98.4%** · 38% · 7.2 |
| Raid L20 Nightfang Matriarch | 22 | 93.3% · 54% · 13.4 | **99.6%** · 32% · 8.4 |
| Raid L30 +20/+20 gear, Ember Hydra | 36 | 100.0% · 15% · 5.7 | **100.0%** · 7% · 3.7 |
| Ashen Deep depth 14, L14, 6 foes trimmed to 4 | 16 | 81.2% · 68% · 8.8 | **84.7%** · 58% · 7.6 |
| Raid L50 Hollow Crown | 52 | 83.9% · 70% · 14.1 | **96.3%** · 48% · 10.0 |
| Nightmare L20 Brute + 2 Ashbound (finish the fallen) | 20 | 77.3% · 71% · 13.0 | **92.3%** · 49% · 15.1 |

## Deviations from the proposal

- **Over-cap stats are clamped and refunded**, not grandfathered with a free Re-temper: the PR 3
  brief asked for clamp + refund. A full respec (Re-temper) comes with perks in PR 4.
- **Signature 32** (Arts cooldown −1) is shown but does nothing until Lantern Arts exist (PR 4).
- **Signature 28** crit applies to Action Surge, Sneak Attack and Cutting Quip, not Magic Missile
  (auto-hit, no roll) or Healing Word (no attack roll).
- **Death save bonus** never goes negative (`max(0, floor(CONmod / 2))`).
- **Shackle** is now resisted with STR in solo (was CON); online keeps CON. Grit covers Burn only:
  Bleed, Stun and knock-down do not exist yet.
- **Barter sell bonus** is paid on the pre-upgrade value (see above), to rule out farming.
- **Foe HP toughness** (+10–40%) is new: without it the universal effects made most fights trivial
  (Raid bosses fell in ~4.5 rounds instead of 12).
- **Luck** applies to kill loot and Search gold, not to fixed story / set-piece rewards.
- Everything is **solo-only**; online tables keep the v2.30 rules.
