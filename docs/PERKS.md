# Kindled Path — level cap 50, Embers, Wards and Lantern Arts (v2.32; Rekindling slots v2.33)

Combat / progression revamp, PR 4 of 5. Code: `app/src/main/cpp/Perks.h` (all numbers),
`GamePerks.cpp` (Wards, Arts, Re-temper, the sheet strings), `GameLogic.h` (XP, levels, flames on the
hero), `GameSave.cpp` (player fields 32–36), `KindledPath.kt` (Kotlin parsing and wording).
Tests: `tools/migration_test/perks_test.cpp`, `app/src/test/.../KindledPathTest.kt`. Sims: `tools/combat_sim/ai_sim.cpp`.

**Solo only.** Every perk checks the solo stat-rules switch (`Character::perksOn()`); Host / Join and the
Keeper table keep the classic rules, show no Arts button, and the native side refuses every Kindled Path
call online. Host / Join never write the solo save.

## Levels and XP

| | v2.31 | v2.32 |
|---|---|---|
| Level cap | none (100 safety) | **50** |
| XP for the next level | 100 × L, forever | 100 × L below 20, then **2000 + 100 × (L − 20)** (2,000 at 20 … 4,900 at 49), none at 50 |
| Total XP to 20 / 30 / 40 / 50 | 19,000 / 43,500 / 78,000 / 122,500 | the same — **122,500 to the cap** |
| Stat points | +2 every level | +2 every level to 20, then **+2 at 25, 30, 35, 40, 45, 50** (50 in all) |
| Proficiency | 2 + (L − 1) / 4, grows forever | same to 20, then **frozen at +6** |
| XP per cleared solo room past level 20 | 50 + 15 × room | at least **40 + 15 × EL** (proposal 4b) |
| Stat caps | 18 + L/2, max 30, main stat 32 | unchanged — at 50 the main stat tops out at **32**, the rest at **30** |

**Why this curve.** The proposal's 40-cap curve was 2000 + 150 × (L − 20): 68,500 XP over 20 levels, 4,850 for
the last one. Rescaled to 30 levels with the same cost for the last level (4,900) and the same average per
level (3,450 vs 3,425), the slope becomes +100 — which is exactly today's 100 × L. So nobody's XP bar jumps
on update, and the extra XP past 20 comes from the room floor (40 + 15 × EL ≈ 490 a room at EL 30, about
6 rooms a level; roughly 180 endgame rooms from 20 to 50).

At the cap the XP bar stays full (`XP max · Level 50 (level cap)`), and extra XP is kept as a full bar.
A hero saved above level 50 (old saves had no cap) loads at **50 with a full bar**, keeps every point it
already earned, and the chat says so once.

## Embers

- **1 Ember per level from 21 to 50 = 30 Embers from levels.**
- Rekindling (prestige, v2.33 — [`PRESTIGE.md`](PRESTIGE.md)): **+1 Ember per rank I–X (+10)** and **+2 bonus at rank IX** → **Ember cap 42**.
  The companion counts half the hero's rank. Rank Embers can be spent from level 1 (Arts keep their unlock levels).
  (The proposal had 20 from levels + 1 per rank = 30; the cap rises by the same 10 levels plus the +2.)
- The whole path costs **68** (flames 40 + Wards 16 + one class's Arts 12), so even a capped hero picks.
- Embers are not stored: earned = f(level, rank); spent = flame ranks + 2 × Wards + 3 × Arts. A hero at
  level 21+ on the first v2.32 load has `level − 20` Embers waiting.

## The three branches

| Branch | Cost | Slots |
|---|---|---|
| **Steady Flames** — Tempered Arm / Quickwick / Deep Tallow / Lamp-Lore / Stillflame / Silver Tongue (+1 STR / DEX / CON / INT / WIS / CHA per rank), Thick Wax (+3% max HP per rank), Bright Edge (+2% damage per rank) | 1 per rank, 5 ranks (6 from Rekindled II) | always on (like gear, a flame can pass a stat cap) |
| **Wards** — once per battle, on their own | 2 to learn | 2 equipped (3 from Rekindled III) |
| **Lantern Arts** — new class actions with a recharge, ready at battle start | 3 to learn | 2 equipped (3 from Rekindled V) |

### Wards

| Ward | Effect |
|---|---|
| Last Light | Survive a lethal hit at 1 HP |
| Cleanse | The first harmful status on you (Burn, Marked, Blinded, Slowed, Shackled, Rattled) is removed at once |
| Second Wick | Below 30% HP, heal 25% of max HP |
| Ash Veil | The first hit you take is halved |
| Kindle Rush | +50% damage on your first damaging action |
| Ember Ward | Start each battle with a shield of 10% max HP |
| Spite Spark | When an ally drops: +3 to hit and damage for 2 rounds |
| Steady Hand | Your first miss is re-rolled |

"Used this battle" is saved (player field 33), so reloading mid-fight does not re-arm a Ward.

### Lantern Arts (unlock at 22 / 26 / 30 / 34)

| Class | 22 | 26 | 30 | 34 |
|---|---|---|---|---|
| Fighter | Cleave: 2 foes at 75% (CD 3) | Iron Brace: foes must target you, +4 AC, 2 rounds (CD 4) | Sunder: hit, −3 AC for 3 rounds (CD 3) | Warcry: party +2 hit / +2 damage, 2 rounds (CD 5) |
| Wizard | Cinder Burst: all foes 2d6 + INT, DEX save halves (CD 4) | Ash Bind: stun 1 round (CON save); a boss loses its planned move (CD 5) | Wick Ward: shield the most hurt ally 10 + 2 × INT (CD 3) | Overkindle: next special ×2, then 1 round without specials or Arts (CD 6) |
| Rogue | Smoke Step: untargetable 1 round, next hit crits (CD 4) | Bleeding Cut: hit + bleed 3 rounds (CD 3) | Lift Purse: steal 10 + 3 × EL gold, cancel the foe's planned move (CD 4) | Fan of Knives: all foes at 60% (CD 4) |
| Cleric | Mending Circle: heal the party 1d6 + WIS + level (CD 3) | **Relight**: revive a downed ally at 30% HP (once per battle) | Sanctify: cleanse the party, resist new statuses 2 rounds (CD 4) | Dawnstrike: hit + 1d8, ×2 vs Ashbound and Criers (CD 3) |
| Bard | Rallying Verse: party +2 hit / +2 damage, 3 rounds (CD 4) | Discord: a foe attacks another foe on its turn (WIS save) (CD 5) | Encore: an ally takes an extra turn now (CD 6) | Ashen Lullaby: a minion or skirmisher sleeps until hit (CD 4) |

- **Signature 32** (main stat 32): every Art recharges **1 round faster** (never below 1; once-per-battle stays once).
- Foe saves vs Arts: d20 + 2 + EL/4 (+2 for bosses and elites) against 8 + proficiency + your main-stat modifier.
- A refused Art (recharging, wrong target, no downed ally, Overkindle exhaustion, Shackled) does not spend the turn.
- The companion AI uses its Arts by simple rules: Relight a downed ally, Mending Circle when 2+ are hurt or one is below a third, Sanctify against statuses, area Arts with 2–3+ foes, Iron Brace when a squishy ally is below 40%, Sunder / Bleeding Cut / Ash Bind on the boss or the biggest foe, Sneak Attack before Bleeding Cut.

## Re-temper (respec)

- Outside a fight, from Sheet → Kindled Path. Costs **100 gold × level** (3,000 at 30, 5,000 at 50), paid by
  that hero, or by you if the companion is short.
- Refunds **every attribute point above the class's starting scores** and **every Ember** (flames, Wards
  and Arts are cleared). Odd CON points take back the HP they stored; the story +2 HP / +1 resource
  padding is never touched.
- **Every hero saved before v2.32 gets one free Re-temper** (player field 35; a save without it = 1 token).
  A used token is saved, so it is never granted twice.
- Wards and Arts swap slots for free outside a fight. Learning and swapping are refused in a fight.
- Reclassing the companion keeps flames, Wards, the token and peak level; its class Arts are dropped and their Embers come back.

## Save fields (append-only)

| Player field | Content |
|---|---|
| 32 | Steady Flame ranks `r0.r1.….r7` |
| 33 | Wards `learnedMask.slot0.slot1.usedThisBattleMask` |
| 34 | Arts `learnedMask.slot0.slot1.cd0.cd1.cd2.cd3` |
| 35 | Free Re-temper tokens (missing → 1) |
| 36 | Peak level (Rekindling catch-up) |
| 37–40 | v2.33 Rekindling: companion half-share, explainer shown, 3rd Ward slot, 3rd Art slot ([`PRESTIGE.md`](PRESTIGE.md)) |

New statuses are appended to the status codec (shield, braced, rallied, spite, smoke, next crit, resist,
overkindle, exhausted, sundered, stunned, asleep, bleed / bleed damage, discord); older saves decode them as 0.
Chips: ◈ shielded, ▣ braced, ▲ rallied, ☁ hidden, ✧ sanctified, … exhausted, ⚒ sundered, ✶ stunned, z asleep, ♦ bleeding, ⁇ discord.

## Balance (tools/combat_sim/run.sh 1000)

v2.31 = the old level rules (2 points a level, growing proficiency). "No perks" = a v2.32 hero who never
spends an Ember. "+ perks" = the sim loadout: the class's first Art, two Wards, the second Art, then the
main-stat flame, Deep Tallow, Bright Edge, Thick Wax. Foes and the Reference Lamplighter use the v2.32
level rules. Cells: win % · HP lost % · rounds.

Fighter + Wizard party, 1000 fights per cell. Each cell: v2.31 rules → v2.32 without perks → **v2.32 with the sim loadout** (win %, HP lost %, rounds).

| Level | Scenario | EL | v2.31 | v2.32, no perks | v2.32 + perks |
|---|---|---|---|---|---|
| 20 | Medium room | 20 | 100% · 23% · 4.3 | 100% · 23% · 4.3 | **100%** · 23% · 4.3 |
| 20 | Boss + escort (Sexton + Ashbound) | 20 | 77% · 63% · 11.1 | 77% · 63% · 11.1 | **77%** · 63% · 11.1 |
| 20 | Nightmare (2 hounds + crier) | 20 | 88% · 57% · 8.8 | 88% · 57% · 8.8 | **88%** · 57% · 8.8 |
| 20 | Raid: Hollow Crown | 22 | 98% · 38% · 7.2 | 98% · 38% · 7.2 | **98%** · 38% · 7.2 |
| 20 | Raid: Ember Hydra | 22 | 91% · 41% · 8.0 | 91% · 41% · 8.0 | **91%** · 41% · 8.0 |
| 20 | Raid: Nightfang Matriarch | 22 | 100% · 32% · 8.4 | 100% · 32% · 8.4 | **100%** · 32% · 8.4 |
| 30 | Medium room | 30 | 100% · 23% · 4.6 | 99% · 32% · 5.7 | **100%** · 22% · 4.4 |
| 30 | Boss + escort (Sexton + Ashbound) | 30 | 70% · 71% · 12.6 | 32% · 89% · 14.0 | **68%** · 70% · 14.1 |
| 30 | Nightmare (2 hounds + crier) | 30 | 85% · 62% · 9.8 | 48% · 83% · 12.7 | **67%** · 79% · 11.9 |
| 30 | Raid: Hollow Crown | 32 | 98% · 40% · 8.4 | 83% · 61% · 12.1 | **97%** · 41% · 9.4 |
| 30 | Raid: Ember Hydra | 32 | 87% · 45% · 9.2 | 56% · 69% · 13.4 | **79%** · 49% · 10.6 |
| 30 | Raid: Nightfang Matriarch | 32 | 98% · 39% · 9.8 | 85% · 58% · 13.8 | **98%** · 31% · 9.1 |
| 40 | Medium room | 40 | 100% · 26% · 4.9 | 97% · 38% · 7.3 | **100%** · 23% · 4.8 |
| 40 | Boss + escort (Sexton + Ashbound) | 40 | 67% · 74% · 13.1 | 13% · 96% · 15.8 | **80%** · 66% · 15.0 |
| 40 | Nightmare (2 hounds + crier) | 40 | 82% · 67% · 10.6 | 30% · 91% · 14.7 | **85%** · 68% · 11.7 |
| 40 | Raid: Hollow Crown | 42 | 97% · 44% · 9.2 | 64% · 74% · 17.3 | **98%** · 40% · 10.8 |
| 40 | Raid: Ember Hydra | 42 | 85% · 50% · 10.2 | 34% · 83% · 17.7 | **82%** · 49% · 12.7 |
| 40 | Raid: Nightfang Matriarch | 42 | 98% · 44% · 10.6 | 61% · 75% · 18.8 | **99%** · 32% · 10.4 |
| 50 | Medium room | 50 | 100% · 27% · 5.2 | 82% · 56% · 11.6 | **99%** · 23% · 5.6 |
| 50 | Boss + escort (Sexton + Ashbound) | 50 | 60% · 79% · 13.8 | 1% · 100% · 15.7 | **64%** · 74% · 20.0 |
| 50 | Nightmare (2 hounds + crier) | 50 | 80% · 70% · 11.0 | 5% · 99% · 14.0 | **79%** · 71% · 14.5 |
| 50 | Raid: Hollow Crown | 52 | 96% · 48% · 10.0 | 18% · 94% · 25.2 | **93%** · 47% · 14.5 |
| 50 | Raid: Ember Hydra | 52 | 80% · 55% · 11.4 | 5% · 98% · 21.7 | **68%** · 59% · 17.5 |
| 50 | Raid: Nightfang Matriarch | 52 | 98% · 47% · 11.4 | 14% · 95% · 25.4 | **95%** · 43% · 14.7 |

| Party (boss + escort, Medium) | Level | v2.31 | v2.32 + perks |
|---|---|---|---|
| Rogue + Cleric | 30 | 86% · 35% · 8.9 | **89%** · 32% · 7.9 |
| Rogue + Cleric | 50 | 90% · 28% · 7.4 | **88%** · 32% · 10.1 |
| Bard + Fighter | 30 | 69% · 74% · 13.8 | **60%** · 79% · 15.4 |
| Bard + Fighter | 50 | 61% · 81% · 14.5 | **84%** · 66% · 16.4 |
| Cleric + Wizard | 30 | 31% · 80% · 19.8 | **34%** · 81% · 23.7 |
| Cleric + Wizard | 50 | 15% · 91% · 20.8 | **41%** · 74% · 30.6 |

| Level 1-20 check (Medium room) | v2.31 | v2.32 |
|---|---|---|
| L1 | 100% · 0% | 100% · 0% |
| L5 | 100% · 17% | 100% · 17% |
| L10 | 100% · 19% | 100% · 19% |
| L15 | 100% · 21% | 100% · 21% |
| L20 | 100% · 23% | 100% · 23% |

**Trade-offs**
- Past 20 a hero's power now comes from the Kindled Path. **A hero who never spends Embers falls far
  behind** (Medium boss + escort at 50: 1% wins). The Sheet button says "N Embers to spend" and the
  level-up dialog points there from level 21.
- **Levels 21–30 are the soft spot** (only ≤10 Embers): Nightmare at 30 drops from 85% to 67%, Ember
  Hydra at 30 from 87% to 79%, Bard + Fighter boss at 30 from 69% to 60%.
- **40–50 with perks is at or above v2.31** for bosses (boss + escort 40: 67 → 80%; Bard + Fighter 50:
  61 → 84%; Cleric + Wizard 50: 15 → 41%) but Ember Hydra at 50 is lower (80 → 68%).
- **Fights are longer** past 30 (e.g. boss + escort at 50: 13.8 → 20.0 rounds), because Arts and Wards
  trade burst for control and the frozen proficiency means more misses.
- Levels 1–20 are identical (same seeds, same results).

## Deviations from the proposal

- Level cap **50** (approved override). XP slope **+100 per level past 20** (proposal: +150 for a 40 cap):
  the same last-level cost and average spread over 30 levels; it equals the current 100 × L curve.
- Ember cap **42** of a 68-Ember path (proposal: 30 of 58), see above.
- Cleric "Rekindle" Art is named **Relight** (Rekindle / Rekindling are the comeback rule and prestige).
- Mending Circle heals 1d6 + WIS **+ level** (1d6 + WIS alone is ~3% of a level-30 hero's HP);
  Dawnstrike adds 1d8 on a normal foe.
- Discord can finish the foe it hits; Ashen Lullaby has no save but works only on minions and skirmishers.
- The XP floor 40 + 15 × EL applies past level 20 only (below 20 nothing changes).
- Encore's extra turn is not saved mid-turn (a reload resumes the normal order).
- 3rd Ward / Art slots at Rekindled III / V, Steady Flame rank 6 at II, "Ready to Rekindle" and the prestige
  dialog shipped in v2.33 ([`PRESTIGE.md`](PRESTIGE.md)). Mythical-gear Ward overlap (§6) waits for Mythical gear.
