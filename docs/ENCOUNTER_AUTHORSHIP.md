# Encounter Authorship — v2.8 (versionCode 44)

**Base:** stacks on `feature/bard-cha-raid-keys` (AA step 1 / PR #54, v2.7 / 43). **Merge after #54.**

**Legal:** Original set-piece names and flavor only. SRD-compatible monster concepts (goblin, skeleton, wolf, ogre). No WotC module text, no official D&D branding. See `ATTRIBUTION.md`.

## Goal

Fewer pure-random “empty room with 1–2 trash” feels in procedural crawl / post-quest rooms. Some chambers become **authored set-pieces** with telegraph, clear composition, and a clearer reward.

## Templates

| Key | Display name | Shape | Min room |
|-----|--------------|-------|----------|
| `shadow_ambush` | Shadow Ambush | 2–3 goblin ambushers | 3 |
| `bonebound_duo` | Bonebound Duo | Elite skeleton pair | 5 |
| `relic_snatch` | Relic Snatch | Wolves + cache goblin (mini-objective) | 4 |
| `kings_herald` | King's Herald | Boss-adjacent tease (Goblin/Bone Herald; no true boss spawn) | 6 |
| `wolf_pack` | Howl Corridor | Wolf pack pressure | 4 |
| `ogre_roadblock` | Ogre Roadblock | Single ogre bruiser | 7 |

Data table: `app/src/main/cpp/EncounterAuthorship.h`.

## Hook

`Game::spawnRoomContent()` (procedural only):

1. Merchant every 4th room (unchanged).
2. `maybeSpawnBossEncounter()` (unchanged gates / Legendary rules).
3. **`trySpawnAuthoredEncounter()`** — chance rises with `roomCount_`, biased after story progress (`questComplete_` / Act 2 / Act 3 / `POST_QUEST`). Easy + early rooms stay mostly trash.
4. Else classic goblin (+ skeleton if deep).

**Preserved:** scripted Easy early story beats (`isSoloQuestScripted()` early-return), Bard, raid keys, Legendary endgame-only rules, reduce-flash, Ashen Lantern branding.

## Telegraph & rewards

- Start: `lastEvent_` = `SET-PIECE! <name> — stand ready!` (toast in UI, same path as `BOSS!`), plus DM telegraph + journal.
- Clear: combat feed uses the template clear line; **+XP / +gold**; **guaranteed Common or Uncommon** gear (never Legendary; respects drop nerfs).

## Out of scope

Juice pass, audio beds, new engine.

## Device smoke

1. Clean/Rebuild (native + Kotlin).
2. Dungeon Crawl → rooms 1–2 mostly trash; by room 3+ occasional set-piece toast.
3. Post–Acts 1–3 crawl: higher set-piece rate; clear grants Common/Uncommon + bonus gold/XP.
4. Story Act 1 Easy beats unchanged.
5. Boss / Legendary / raid-key paths unchanged.
