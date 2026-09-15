# Challenge Dungeon (v2.12)

Story-complete endgame mode — **Cinder Crucible** vault. Uses the **same Continue save** as Boss Raid (no class select / no wipe of solo save). Host/Join never write into `save_state`.

## Gate
- Acts 1–3 complete (`story_fully_complete` / `questAct3Complete_`) **and** a livable Continue save.
- Main menu **Challenge Dungeon** + Settings entry (same gate as Boss Raid).
- **No Raid Key cost** (keys still drop from the apex foe like other endgame clears).

## Encounter
- Original vault flavor (no WotC module text).
- One endgame apex (Hollow Crown / Ember Hydra / Nightfang Matriarch) + SRD-safe adds (Ogre; Wolf on Medium+).
- Onward after clear / Continue after wipe → restore prior story/crawl mode; spoils stay on the hero.

## Legendary drop formula
Normal endgame / Boss Raid Legendary chance on a successful drop remains **flat 7%**.

Challenge Dungeon uses a **dynamic** chance stored in meta prefs (`challenge_dungeon_legendary_chance`, `challenge_dungeon_attempts`):

| Parameter | Value |
|-----------|-------|
| **BASE** (start / default) | **18%** |
| **Per-Legendary decay** | **−3** percentage points after each Legendary found |
| **FLOOR** | **5%** (never below) |
| **Every 50 attempts** | **+4** percentage points |
| **Hard CAP** | **28%** |
| Legendaries findable | **Uncapped** |

Attempt counter increments when a run **successfully starts**. The +4 boost applies on attempts 50, 100, 150, … (toward CAP).

## Persist
- Chance + attempts: SharedPreferences (meta; survives Host/Join).
- Character/gear: same `save_state` Continue slot as solo (pre-run snapshot `pre_challenge_save_state` for wipe restore).

## Out of scope
Endless Deep Crawl, Act 4. (Arena shipped separately — see docs/ARENA.md).
