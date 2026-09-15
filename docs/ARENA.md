# Arena / Ember Ring (v2.13)

Story-complete endgame **side mode** — the **Ember Ring**. Distinct from Boss Raid (single key-cost apex) and Challenge Dungeon / Cinder Crucible (dynamic Legendary forge). Uses the **same Continue save** as raids (no class select / no wipe of solo save). Host/Join never write into `save_state`.

## Gate
- Acts 1–4 complete (`story_fully_complete` / `questAct4Complete_`; Act 3-complete saves grandfathered) **and** a livable Continue save.
- Main menu **Arena (Ember Ring)** + Settings entry (same gate as Boss Raid / Challenge).
- **No Raid Key cost**.

## Rules
- Wave-based scored fights (default **5 waves**).
- Press **Onward** after a clear to face the next wave; after Wave 5 clear, the run ends and prior story/crawl mode is restored (spoils stay on the hero).
- Wipe Continue (Easy–Hard) or Nightmare menu restore uses `pre_arena_save_state` — same hero preserve pattern as Boss Raid / Challenge.
- Original Ashen flavor (no WotC module text). SRD-safe trash (Goblin, Wolf, Skeleton, Ogre) + Wave 5 endgame apex (Hollow Crown / Ember Hydra / Nightfang Matriarch).

## Scoring
| Event | Score |
|-------|------:|
| Clear wave *N* | **100 × N** |
| Clear Wave 5 (finale bonus) | **+200** |

Meta prefs track **best score**, **best waves cleared**, and **runs completed** (`arena_best_score`, `arena_best_waves`, `arena_runs_completed`).

## Rewards (Legendary choice — documented)
- **Gold + XP** every wave clear (standard kill/clear pipeline).
- **Waves 1–4:** loot with a luck bump toward **Rare / Epic**. **No Legendary** (`allowLegendary` false unless an endgame boss somehow appears).
- **Wave 5:** apex boss defeat sets `allowLegendary` — **Legendary at flat 7%** (same as Boss Raid / normal endgame; **not** Challenge Dungeon’s dynamic 18%± formula).
- Raid Key may still glint from the Wave 5 apex (focused-endgame key chance), like other endgame clears.

## Persist
- Best score / waves / runs: SharedPreferences (meta; survives Host/Join).
- Character/gear: same `save_state` Continue slot (pre-run snapshot `pre_arena_save_state` for wipe restore).
- Mid-run wave + score serialized in the save header (optional trailing fields; older saves ignore).

## Out of scope
Act 4. (Endless Deep shipped separately — see docs/ENDLESS_DEEP.md).
