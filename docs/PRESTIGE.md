# Rekindling — prestige ranks I–X (v2.33)

Combat/progression revamp PR 5. Proposal §5 plus the approved overrides (level cap 50; prestige is
**optional**, explained in a dialog with **Rekindle / Not now**, never forced). Solo only: every bonus
counts under the solo rules (`Character::perksOn`), so Host / Join and the Keeper table are unchanged and
never touch a solo save. Code: `Perks.h` (`namespace prestige`), `GamePerks.cpp` (Rekindling section),
`Rekindling.kt` (wording), tests `tools/migration_test/prestige_test.cpp`, `RekindlingTest.kt`.

## Who can Rekindle

- The hero (party slot 0) is **level 50** and the **story is complete** (Act 4 done).
- Solo play, outside a fight, not mid-run in an endgame mode (Boss Raid, Cinder Crucible, Ember Ring,
  Ashen Deep), party not fallen, and below rank X.
- Nothing happens by itself. On first eligibility the explainer opens once per climb (the flag is saved);
  **Not now** changes nothing, and the same dialog stays on the Sheet as **Ready to Rekindle → Rekindled N**.
  **Rekindle** opens a second confirmation listing what resets, what's kept and what you gain; only that
  confirms it.

## What resets / what's kept

| Resets | Kept |
|---|---|
| Level → 1, XP 0 (hero and companion) | All gear, the bag and every upgrade |
| Attributes → the class's starting array (points are earned again) | Gold |
| Kindled Path cleared — every Ember back | Story completion and every mode it unlocked |
| Odd-CON bonus HP | The story +2 HP / +1 use padding |
| | Memory Ledger, Raid Keys, records, peak level (50) |
| | **+1 free Re-temper** after every Rekindling |

Rekindling refunds the Kindled Path; the rank's own Embers can be spent from level 1 (Steady Flames and
Wards; Lantern Arts keep their level unlocks 22 / 26 / 30 / 34).

## Ranks (cumulative)

Every rank: **+1 to every attribute** (on top, like a Steady Flame) **and +1 to every cap**, **+2% max HP**,
**+1 Ember**, **+5% XP**, **+5% gold** (loot and searches).

| Rank | Reward |
|---|---|
| I | Title *Rekindled*, amber lantern-flame badge on the slot card; unlocks Mythical gear and The Drowned Vigil (**PR 6**) |
| II | Steady Flames can reach rank **6** |
| III | A **3rd Ward** slot |
| IV | **Catch-up:** XP ×1.5 while below your peak level |
| V | A **3rd Lantern Art** slot |
| VI | **+1 class special use** |
| VII | Mythical upgrade cap +2 (**PR 6**) |
| VIII | Ember-white badge |
| IX | **+2 bonus Embers** |
| X | *Everflame*; the badge pulses gently (still with *Reduce flashing*) |

Embers: 30 from levels 21–50 + 1 per rank + 2 at IX → **cap 42** (unchanged from v2.32's plan).

## Attunement

Once Rekindled (rank ≥ 1), weapon and armor bonuses count **up to floor(level / 2) + 3**, the normal gear for
the level plus 3: a +25 relic counts as +3 at level 1, +8 at level 10, +13 at level 20 and is uncapped again by
level 44–50 (cap 28). It applies to the hero **and the companion** (even when the companion's half share of
rank I rounds to 0, since it climbs from level 1 with the same gear).

- **Rank 0 (first climb): no cap at all**, exactly as v2.32.
- **Online: no cap** (classic rules, like every other Rekindling effect).
- The Sheet shows it: "Rekindled I · gear attuned up to +13 (half your level + 3)". The Encounter Level reads
  the attuned bonus.

## Companion

The companion Rekindles with the hero (level 1 too) and counts as **half the hero's rank, rounded down**
for attributes, caps, HP, Embers and milestones (so at hero rank X it has rank V's 3rd slots and flame cap 6,
but not VI's extra use). It earns the hero's XP multiplier so the two stay level. Field 37 marks it.

## Encounter Level

Prestige counts 25%: `EL = round(avg level + 0.5 × gear surplus + 0.25 × avg rank share)`
(`CombatCurve.h kPrestigeFollow`; the companion's share is half). A rank X pair adds about +2 EL.

## Nightmare

Prestige lives in the save, so it is **per save slot** (deleting a slot deletes it). On **Nightmare**:

- A story / crawl wipe already deletes the hero (the wipe dialog now says the rank goes with it).
- An endgame wipe (raid, crucible, ring, deep) restores the story hero as before **but clears its rank**
  (`Game::clearPrestigeInSave`): rank → 0, flame ranks above 5 and the 3rd slots trimmed, the Kindled
  Path refunded if it no longer fits (with a free Re-temper), attributes above the normal caps refunded as
  points. Level, gold and gear are kept. The wipe dialog names the lost title.
- Warnings: picking Nightmare on the menu, the explainer and confirmation on a Nightmare hero, and the
  entry dialog of every endgame mode for a Rekindled Nightmare hero.

## Save fields (append-only)

| Player field | Content |
|---|---|
| 27 | Rekindling rank 0–10 (written since v2.29, always 0 until now) |
| 37 | Companion half-share flag |
| 38 | The explainer was shown this climb |
| 39 | 3rd Ward slot (`-1` empty) |
| 40 | 3rd Art slot (`-1` empty) |

A v2.32 save (37 fields) loads at rank 0 with no 3rd slots. On load the "never shrink" HP check counts the
rank's +2% HP / +1 CON, and the max-uses check counts rank VI's +1, so nothing is mistaken for lost padding
(reloading twice changes nothing — `prestige_test`). `describeSaveState` appends the rank for the slot card.

## Debug-build shortcut (testing the dialog)

**Debug builds only.** Settings → **Debug: level 50 + story complete** (works in a solo adventure, outside a
fight) puts the hero and companion at level 50, marks Acts 1–4 done (post-story beat in Story mode) and clears
the "explainer shown" flag. **v2.34 rebalance:** it also builds both like a real player — every stat point
spent (half main stat, 30% CON, 20% DEX, the rest where a stat is capped), weapon and armor raised to the
normal bonus for the level (+25; the old pieces go to the bag) and every Ember spent (two Arts, two Wards,
then the main-stat flame, CON, Bright Edge, Thick Wax, DEX). Before, it left all of that unspent with
starter gear, and that hero lost every fight at level 50 even on Easy. Pressing it again on a level-50 hero
fills in anything still unspent. The Rekindle explainer opens right away (or Sheet → **Ready to Rekindle**).

- Kotlin: `app/src/debug/…/DebugShortcuts.kt` (the button + `external fun nativeJumpToRekindleReady`);
  `app/src/release/…/DebugShortcuts.kt` is a no-op with no native method.
- Native: `Game::debugJumpToRekindleReady` and the JNI entry
  `Java_…_DebugShortcuts_nativeJumpToRekindleReady` are inside `#ifndef NDEBUG` — release builds compile with
  `NDEBUG`, so neither exists in the shipped `.so`.
- Checks: `app/build.gradle.kts` task **`verifyReleaseHasNoDebugShortcuts`** runs as part of
  `assembleRelease` and fails it if the release APK's dex or native libs contain the shortcut's markers
  (`JumpToRekindleReady`, `ASHEN_DEBUG_ONLY_SHORTCUT`, its chat line). `tools/check_release_jni.sh` also
  fails on any `DebugShortcuts` JNI symbol in a release `.so` (the debug native is not counted in the 106).
  Unit tests: `testReleaseUnitTest` asserts the release `DebugShortcuts` has no shortcut; `testDebugUnitTest`
  asserts the debug one does.
- How to use: `./gradlew assembleDebug`, install `app/build/outputs/apk/debug/app-debug.apk`
  (`adb install -r …`). It is signed with the debug key, so it will not install over a release-signed copy —
  use a second device or emulator, or uninstall the release build first (that deletes its saves).

## Balance (tools/combat_sim/run.sh 600)

Fighter + Wizard, perks spent with the sim loadout (the rank's extra Embers into flames, the 3rd Ward /
Art when unlocked). Win %, party HP lost %, Encounter Level. **v2.34 rebalance:** these tables are re-run
with the sim's reference-table bug fixed (the earlier PR #85 / #86 tables past level 20 were built against
foes far tougher than the game's — COMBAT_CURVE.md) and include the new EL (+0.1 per spent Ember, so a
level-50 rank-0 hero is EL 53).

| L50, v2.32 + perks | Scenario | rank 0 | I | V | X |
|---|---|---|---|---|---|
| 50 | Medium room | 100% · 14% · EL 53 | 100% · 11% · EL 53 | 100% · 9% · EL 54 | 100% · 5% · EL 56 |
| 50 | Boss + escort (Sexton + Ashbound) | 99% · 42% · EL 53 | 100% · 35% · EL 53 | 100% · 41% · EL 54 | 100% · 34% · EL 56 |
| 50 | Nightmare (2 hounds + crier) | 97% · 50% · EL 53 | 100% · 43% · EL 53 | 100% · 47% · EL 54 | 100% · 35% · EL 56 |
| 50 | Raid: Hollow Crown | 100% · 24% · EL 55 | 100% · 19% · EL 55 | 100% · 15% · EL 56 | 100% · 11% · EL 58 |
| 50 | Raid: Ember Hydra | 99% · 26% · EL 55 | 100% · 21% · EL 55 | 100% · 16% · EL 56 | 100% · 12% · EL 58 |
| 50 | Raid: Nightfang Matriarch | 100% · 18% · EL 55 | 100% · 14% · EL 55 | 100% · 11% · EL 56 | 100% · 8% · EL 58 |

The climb after Rekindling (level-50 gear, attuned to **floor(level / 2) + 3** as shipped; the companion is
attuned too), win % · HP lost %:

| Fight | Level | first climb (rank 0) | I | V | X |
|---|---|---|---|---|---|
| Boss + escort | 1 | 100% · 0% | 93% · 5% | 98% · 1% | 100% · 0% |
| Boss + escort | 10 | 80% · 57% | 88% · 55% | 99% · 30% | 100% · 9% |
| Boss + escort | 20 | 77% · 63% | 98% · 34% | 100% · 13% | 100% · 10% |
| Boss + escort | 30 | 92% · 47% | 100% · 31% | 100% · 24% | 100% · 19% |
| Boss + escort | 40 | 92% · 53% | 100% · 34% | 100% · 34% | 100% · 28% |
| Nightmare trio | 10 | 92% · 52% | 98% · 45% | 100% · 34% | 100% · 17% |
| Nightmare trio | 20 | 90% · 54% | 99% · 33% | 100% · 16% | 100% · 16% |
| Nightmare trio | 30 | 93% · 60% | 99% · 43% | 100% · 30% | 100% · 26% |
| Nightmare trio | 40 | 91% · 63% | 98% · 45% | 100% · 33% | 100% · 27% |

Medium rooms stay 100% at every level and rank. For comparison, the first draft of this PR used **level + 3**:
rank I won the L20 boss 100% · 10%, so the climb back was trivial; Martin picked the stricter cap. At level
50 the cap (28) is above any normal gear.

Rooms cleared to reach level 50 again (room counter carrying on from room 60): first climb 113 (43 to
level 20); rank I 70 (16); rank IV 64 (14); rank X 39 (8).

**Trade-offs.** Each rank is a real power step: a built level-50 hero already wins ~97–100% on Medium,
and every rank adds to that. With the stricter attunement, rank I re-climbs easier than a first climb (boss
+8–22 points at L10–30, mostly from +1 attributes, HP and the attuned +3 over normal gear); rank V and up
re-climb very comfortably, which is the intended reward for repeated Rekindles. The fast re-climb also comes from room XP (50 + 15 × room)
carrying on from the post-story room count.

## Deviations from the proposal

- Rekindling **refunds** the Kindled Path instead of relocking it to 21: the rank's Embers can be spent from
  level 1 on Steady Flames and Wards (Arts keep their level unlocks).
- The companion's half share applies to every rank effect (attributes, caps, HP, Embers, milestones); XP
  uses the full rank so the pair stays level.
- Gold +5% per rank applies to kill loot and searches (story rewards are fixed amounts).
- Free Re-temper tokens are capped at 9.
- Mythical gear, The Drowned Vigil and the Mythical cap (+2 at VII) are PR 6; ranks I and VII show them as
  "coming next update".
