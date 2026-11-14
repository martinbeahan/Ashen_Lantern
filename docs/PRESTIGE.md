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
the "explainer shown" flag. The level-up dialog appears first; tap **Later** and the Rekindle explainer opens
(or Sheet → **Ready to Rekindle**).

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
Art when unlocked). Win %, party HP lost %, Encounter Level.

| L50 | rank 0 | I | V | X |
|---|---|---|---|---|
| Medium room | 99% · 23% · EL 50 | 100% · 19% · EL 50 | 100% · 13% · EL 51 | 100% · 7% · EL 52 |
| Boss + escort (Sexton + Ashbound) | 66% · 73% · EL 50 | 82% · 63% · EL 50 | 98% · 43% · EL 51 | 100% · 33% · EL 52 |
| Nightmare (2 hounds + crier) | 80% · 71% · EL 50 | 90% · 61% · EL 50 | 99% · 48% · EL 51 | 100% · 37% · EL 52 |
| Raid: Hollow Crown | 93% · 47% · EL 52 | 97% · 38% · EL 52 | 100% · 20% · EL 53 | 100% · 15% · EL 54 |
| Raid: Ember Hydra | 70% · 57% · EL 52 | 84% · 45% · EL 52 | 99% · 23% · EL 53 | 100% · 17% · EL 54 |
| Raid: Nightfang Matriarch | 95% · 42% · EL 52 | 99% · 33% · EL 52 | 100% · 17% · EL 53 | 100% · 12% · EL 54 |

The climb after Rekindling (level-50 gear, attuned to **floor(level / 2) + 3** as shipped; the companion is
attuned too), win % · HP lost %:

| Fight | Level | first climb (rank 0) | I | V | X |
|---|---|---|---|---|---|
| Boss + escort | 1 | 100% · 0% | 93% · 5% | 98% · 1% | 100% · 0% |
| Boss + escort | 10 | 80% · 57% | 88% · 55% | 99% · 30% | 100% · 8% |
| Boss + escort | 20 | 77% · 63% | 88% · 48% | 100% · 20% | 100% · 11% |
| Boss + escort | 30 | 68% · 69% | 96% · 42% | 100% · 27% | 100% · 16% |
| Boss + escort | 40 | 80% · 65% | 96% · 41% | 100% · 31% | 100% · 25% |
| Nightmare trio | 10 | 90% · 54% | 97% · 45% | 100% · 34% | 100% · 14% |
| Nightmare trio | 20 | 90% · 56% | 93% · 48% | 100% · 22% | 100% · 17% |
| Nightmare trio | 30 | 68% · 79% | 92% · 54% | 100% · 31% | 100% · 23% |
| Nightmare trio | 40 | 85% · 68% | 97% · 46% | 100% · 32% | 100% · 27% |

Medium rooms stay 100% at every level and rank. For comparison, the first draft of this PR used **level + 3**:
rank I won the L20 boss 100% · 10% (vs 77% on a first climb), so the climb back was trivial; Martin picked the
stricter cap. At level 50 the cap (28) is above any normal gear, so the L50 grid above is unchanged.

Rooms cleared to reach level 50 again (room counter carrying on from room 60): first climb 113 (43 to
level 20); rank I 70 (16); rank IV 64 (14); rank X 39 (8).

**Trade-offs.** Each rank is a real power step: by rank V every L50 fight is ≥ 98%. With the stricter
attunement, rank I re-climbs about one tier easier than a first climb (boss +8–28 points, mostly from +1
attributes, HP and the attuned +3 over normal gear); rank V and up still re-climb very comfortably, which is
the intended reward for repeated Rekindles. The fast re-climb also comes from room XP (50 + 15 × room)
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
