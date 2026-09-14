# Audio beds by biome / boss — v2.10 (versionCode 46)

**Base:** stacks on `feature/restrained-combat-juice` (AA step 3 / PR #56, v2.9 / 45). **Merge after #56** (and #55 / #54 if that stack is not yet on master).

**Legal:** CC0 only (Ironchest Dungeon Loops). No Mixkit, FreePD, or D&D / WotC / Critical Role / commercial OST material. Full credits in `ATTRIBUTION.md`.

## Goal

Crawl, town/menu, normal combat, and boss/raid should not share one loop. One BGM at a time (battery-friendly). Music / SFX Settings toggles unchanged — Music off mutes all beds.

## Beds → contexts

| Code (`GameAudio.Track`) | Asset | Source file | Plays when |
|---|---|---|---|
| `EXPLORE` | `bgm_explore.ogg` | Ironchest_dungeon001 | Story / crawl / cleared rooms (not merchant, not combat) |
| `TOWN` | `bgm_town.ogg` | Ironchest_dungeon005 | Main menu; merchant / shop rooms |
| `COMBAT` | `bgm_tension.ogg` | Ironchest_dungeon006 | Normal combat (non-boss) |
| `BOSS` | `bgm_boss.ogg` | Ironchest_dungeon010 | Boss encounter combat (`BOSS!` / “Boss encounter” sticky flag) **or** Boss Raid (`getSoloPlayMode() == 2`) while in combat |

Optional biome/Act accent was deferred (no extra pack) — Act 2/3 still use `EXPLORE` outside combat to avoid APK bloat. JaggedStone / RandomMind remain backup shortlist only.

## Transitions

- `MainActivity.syncAudioBedFromState()` resolves the bed each UI tick and on menu show/hide.
- `GameAudio.setBed(track)` fades out (~220 ms) then clean stop/start — never stacks two MediaPlayers.
- Lifecycle pause/resume and **Music on** checkbox still gate playback.

## Unchanged

- SFX (`sfx_attack`, `sfx_hit`, `sfx_growl`, `sfx_ui_click`)
- DM voice (on-device TTS)
- No new boss sting SFX in this pass

## Device smoke

1. Clean/Rebuild (resources pick up new `raw/` oggs).
2. Main menu → quieter town bed; Music off → silence; Music on → resumes town.
3. Start Solo story → explore bed in non-combat rooms; merchant → town; fight → combat; boss room → boss bed until combat ends.
4. Boss Raid → boss bed in combat; return to menu → town.
5. Regression: juice (#56), set-pieces (#55), Bard/raid keys (#54), SFX + DM voice toggles.
