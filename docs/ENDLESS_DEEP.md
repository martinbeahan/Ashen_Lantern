# Endless Deep / Ashen Deep (v2.14)

Story-complete endgame **side mode** — the **Ashen Deep**. Distinct from:

- **Dungeon Crawl** (procedural story-adjacent crawl from a fresh/continue path)
- **Boss Raid** (single key-cost apex)
- **Challenge Dungeon / Cinder Crucible** (dynamic Legendary forge)
- **Arena / Ember Ring** (fixed 5 scored waves)

Uses the **same Continue save** as raids (no class select / no wipe of solo save). Host/Join never write into `save_state`.

## Gate
- Acts 1–4 complete (`story_fully_complete` / `questAct4Complete_`; Act 3-complete saves grandfathered) **and** a livable Continue save.
- Main menu **Endless Deep (Ashen Deep)** + Settings entry (same gate as Boss Raid / Challenge / Arena).
- **No Raid Key cost**.

## Depth rules
- Enter at **Depth 1**; each cleared chamber → **Onward** raises depth by 1 (no hard cap).
- **Difficulty ramps with depth**: foe HP/AC/count scale; packs get denser every few depths.
- **Milestone** (every **5** depths): elite Ogre pack (soft checkpoint — toast invites retreat).
- **Apex** (every **10** depths): endgame boss (Hollow Crown / Ember Hydra / Nightfang Matriarch) + scaling add.
- Original Ashen flavor (no WotC module text). SRD-safe trash (Goblin, Wolf, Skeleton, Ogre).

## Soft ending (no forced clear)
Leave with spoils via any of:
1. **Death** — Easy–Hard Continue ends the Deep and keeps the hero; Nightmare restores `pre_endless_save_state` (same pattern as Arena).
2. **Retreat** — Settings **Retreat from Ashen Deep** (or milestone dialog) finishes the mode, records depth, keeps gear/gold/XP on the Continue hero.
3. **Milestone** — after depth 5/10/15/… clear, you may retreat with a small gold bonus or press Onward deeper.

Mid-run **Return to main menu** can **save mid-Deep** (resume via Continue) or you can Retreat to close the run.

## Scoring / meta
Prefs track **best depth**, **best milestone**, and **runs completed** (`endless_deep_best_depth`, `endless_deep_best_milestone`, `endless_deep_runs_completed`).

## Rewards (Legendary — documented)
| Source | Reward |
|--------|--------|
| Any depth clear | Gold ≈ `8 + depth×3`; loot luck bump ≈ `4 + depth×2` (Rare/Epic lean) |
| Kill XP | Standard pipeline; tougher foes at depth |
| Depth 1–9 (and non-apex) | **No Legendary** (`allowLegendary` only from endgame apex names) |
| Depth **10, 20, 30…** apex | Endgame boss defeat sets `allowLegendary` — **Legendary at flat 7%** (same as Boss Raid / Arena Wave 5 — **not** Challenge Dungeon’s dynamic 18%± formula) |
| Raid Key | May glint from apex clears (focused-endgame key chance), like other endgame modes |

## Persist
- Best depth / milestone / runs: SharedPreferences (meta; survives Host/Join).
- Character/gear: same `save_state` Continue slot (pre-run snapshot `pre_endless_save_state` for Nightmare wipe restore).
- Mid-run depth serialized in the save header (optional trailing field after Arena fields; older saves ignore).

## Out of scope
