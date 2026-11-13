# Enemy AI (v2.30 / versionCode 69)

Solo foes now plan a move, show it as an **intent chip**, play their role, and bosses change
**phase** at 66% and 33% HP. This is PR 2 of the combat revamp; it sits on the v2.29 combat
curve (`docs/COMBAT_CURVE.md`). Code: `app/src/main/cpp/EnemyAi.h` (pure rules, tables, save
codecs) and the AI block at the end of `Game.cpp`; UI: `EnemyIntentUi.kt` (JVM-tested) and
`MainActivity.refreshIntentChips()`.

## Where it runs

`Game::enemyAiActive() = encounterCurveActive() && enemyAiEnabled_`, i.e. **solo play only**
(host, not the online Keeper table). Foes without a curve EL (legacy / online) keep the old
`enemyTurn` path. Online Host / Join are unchanged and never touch solo saves; joiners get no
intents or statuses (`getEnemyIntents()` returns only its header).

## Turn model

- A foe **plans at the end of its own turn** (and every foe plans at fight start), so the chip
  always shows its *next* move. If the planned target is gone at execution it re-targets.
- Statuses tick at the end of the bearer's own turn. `advanceTurn()` runs that and counts
  rounds (`combatRound_`).
- Saving throw: `d20 + stat mod >= 10 + EL/2`.

## Tiers (by difficulty)

| Difficulty | Tier | Rules |
|---|---|---|
| Easy | Sloppy | 30% random target, ±30% score noise, never weights healers, tricks only half the time they are ready, cooldowns +1, never flees |
| Medium | Tactical | Role rules, ±15% noise, heals allies below 40%, flees when allowed |
| Hard | Coordinated | No noise; foes share a focus target (+25 per other foe on the same hero, up to 2); casters rattle the healer |
| Nightmare | Ruthless | As Hard, heals below 50%, cooldowns −1, a brute may **finish a downed hero** — only after a `WARNING!` banner a turn ahead |

## Target score

`40·missing% + 30·[Cleric/Bard or healed an ally recently] + 20·[below 35%] + 2·(15 − AC) + 15·[Marked] + 25·[hit this foe since its last turn]`
plus the focus share (Coordinated+) and behaviour terms: Finisher +80·missing%, Backstab +6·(15 − AC),
Pack +20·focus +20·missing%. Standing heroes always come first; downed heroes are only hit
when nobody stands (as in v2.29) or by the telegraphed Nightmare finisher. The Fighter
"Iron Brace" term (−60) waits for the Arts PR.

## Behaviours per enemy ID

| Enemy IDs | Role | Behaviour |
|---|---|---|
| `enemy_wickthief`, `_scout`, `_river`, `_root`, `_fen` | Skirmisher | **Snatch** 5% of the target's gold on hit (returned when it dies); at <25% HP shows ➜ and **slips away** next turn (no loot, half XP) |
| `enemy_wickthief_cutpurse`, `_cache` | Skirmisher | **Backstab:** prefers the lowest AC; can flee |
| `enemy_gloamhound` | Skirmisher | **Pack:** piles onto the hero others already target; can flee |
| `enemy_ashbound`, `enemy_ashbound_bonebound` | Brute (undead) | **Shield-wall:** +3 AC for a round on an ally below 50% (one wall per ally, cooldown 3) |
| `enemy_crier_bone` | Support (undead) | **Knit bones:** heals an undead ally 20% |
| `enemy_crier_wick` | Support | **Rally cry:** allies +2 to hit for 2 rounds |
| `enemy_tallow_rat` | Minion | **Swarm:** never targets downed heroes |
| `enemy_brute` | Brute | **Wind-up → Millstone Slam** (telegraphed, 2×); 20% of its max HP taken in between staggers it and breaks the Slam |
| `enemy_tithe_enforcer` | Brute | **Shackle:** WIS save or the hero can't use their Special for 2 rounds |
| `enemy_tithe_scavenger` | Skirmisher | **Finisher:** lowest HP% |
| `enemy_tithe_watcher` | Caster | **Mark** (+2 to hit against the hero for everyone) and **Cinder bolt**; on Hard+ rattles a Cleric / recent healer |
| bosses (`boss_*`) | Boss | Phase scripts below; never flee |

Undead (never flee, Knit bones heals them): both Ashbound, the Bone Crier, the Sexton, the
First Lamplighter, the Ossuary Prior and the Hollow Crown.

## Boss phases (66% / 33%)

Each change shows a banner `PHASE N — name` and a Keeper line; the boss re-plans at once.

| Boss | Phase 2 | Phase 3 |
|---|---|---|
| The Lantern Sexton | Calls an Ashbound | **Snuff** the lantern-bearer (hero 1), telegraphed heavy 1.4× |
| Collector Sallow Venn | **Tally:** marks the richest hero (3 rounds) | Calls a Tithe Enforcer |
| The First Lamplighter | **Ember wound:** party Burn 2 rounds (CON save halves) | Alternates Guard and **Twin strike** |
| The Bell-Herald | **Toll** every 3rd turn (WIS save or Rattled) | **The bell cracks:** +2 to hit, −2 AC (once) |
| The Wick King | Calls 2 Wickthieves | **Crown flare:** blinds (−4 to hit) |
| The Ossuary Prior | **Reliquary:** heals 15% once | Raises an Ashbound |
| Ashen Drake | **Cinder breath** (all heroes, DEX save halves, telegraphed heavy) | Takes flight (guard), then **Dive** (telegraphed heavy 1.6×) |
| Hollow Crown | Never strikes the same hero twice in a row | **Tyrant's due** (telegraphed heavy 1.6×) |
| Ember Hydra | Phase 1 already casts (fire spit / bite). **Heads:** 3 attacks at 0.35× | Regrows 5% a round unless a crit or a spell seared it that round |
| Nightfang Matriarch | **Webs:** slows a hero (drops to the end of the initiative) | Calls 2 Gloamhounds |

Telegraphed heavies (Snuff, Dive, Tyrant's due, Cinder breath) use the PR 1 hook
(`curve::overkillCap(..., exempt = true)`) and are the only hits that skip the 45% guard; they
never crit (the telegraph is the big hit). Every other boss hit keeps the guard.

## Statuses

Burn (damage at end of turn), Marked (+2 to hit against), Guarded (+3 AC), Blinded (−4 to hit),
Slowed (moved to the end of initiative, restored after), Shackled (no Special), Inspired (+2 to
hit), Rattled (−2 on the next attack, reuses `nextAttackPenalty`), and a hidden "healed recently"
flag. Codes in the UI string: `B M G L S K I R` plus rounds left, e.g. `B2.M1`.

## Intent chips (UI)

`getEnemyIntents()` → `numbers|bannerSeq|bannerText|idx,intent,side,target,label,value,statuses;...`
(intent 0 none, 1 attack, 2 heavy, 3 guard, 4 spell, 5 heal, 6 mark, 7 flee, 8 buff, 9 summon,
10 hex; side 0 hero, 1 foe, 2 self, 3 everyone). `getStatusEffects()` → `heroIdx,codes;...`.

- Chip glyphs: ⚔ attack · ⚒ heavy · 🛡 guard · 🔥 spell · ♥ heal · ◉ mark · ➜ flee · ✚ buff · ✦ summon · ✖ hex.
  Long-press for the full line; the same text is the content description.
- **Numbers** show only when the party's best INT is 14+ (`numbers = 1`) **and** Settings →
  Combat → *Exact intent numbers* is on. *Foe intent chips* hides chips entirely.
- **Reduce flashing:** chips, banners and warnings are static. With it off, only HEAVY chips pulse
  gently (alpha 1 → 0.65, 1.1 s) and the banner fades in / out.
- A new `bannerSeq` shows the phase / `WARNING!` banner once (not replayed on resume).

## Balance notes

Smarter foes concentrate damage, so the tuning keeps the v2.29 budgets: bosses hit for
**70%** of the curve's boss damage (`kBossDamageScale`), summoned foes have **20%** HP and **25%**
damage of a normal foe of their role, shield-walls last one round, and telegraphed heavies don't
crit. See the sim table in the PR / `tools/combat_sim/run.sh`.

## Save fields (append-only)

| Where | Field | Meaning |
|---|---|---|
| Header | 44 / 45 / 46 | `combatRound_`, `roomFoeTotal_`, `fledFoes_` |
| Player | 29 | `bonusMaxResources` (story +1; older saves derive it as saved max − formula; max never shrinks) |
| Player | 30 | statuses (`EnemyAi.h encodeStatuses`) |
| Enemy | 24 | boss phase (was unused) |
| Enemy | 27 / 28 | statuses / the FoeAi blob (move, target, cooldown, flags, stolen gold, …) |

A v2.29 save (including one saved mid-fight) loads cleanly: missing fields default, the boss
phase is set silently from HP on its first turn, and foes plan on their turn. Tests:
`tools/migration_test/ai_test.cpp` (via `tools/migration_test/run.sh`).

## Sim

`tools/combat_sim/run.sh [fights]` builds `combat_sim` (v2.29 curve tables) and `ai_sim`, which
plays real `Game` fights with the party on auto (Fighter + Wizard companion) and compares the
v2.29 foe rules (AI off) with v2.30 (AI on). Results are in the PR description.

## Deviations from the proposal

- Intents are solo-only; online play is unchanged (no intent sync).
- Stunned, Bleed and Shielded are not implemented — no ability uses them yet.
- Heroes have no Burn yet, so the Hydra's regrowth is stopped by a crit **or a spell** (Magic Missile / Cutting Quip).
- Slowed moves the hero to the end of initiative and restores it when it ends.
- "Rattle the healer" uses Shackled-style lockout on Hard+ Watchers.
- The companion AI is a light update (prefers casters / supports, interrupts heavies and heals; no Arts yet).
- Enemy save field numbers differ from the proposal (24 phase, 25/26 kept from PR 1, 27, 28); player 29–30.
- Flee is Medium and up only; the 45% guard still applies to non-telegraphed boss hits.
- Balance: boss damage ×0.7, summons 20% HP / 25% damage, heavies at 1.4–1.6× (not 2×), Hydra heads 0.35× and regrowth 5% (not 0.5× / 10%), shield-wall 1 round — tuned with the sim so boss win rates stay near v2.29.
