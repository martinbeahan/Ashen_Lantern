# Ash & Ember — Phase 1 (v2.20 / 59)

Plumbing + legal hygiene + first art slice + Act 1 rewrite. Spec: Ash & Ember art direction and
"The Kindling" plot (approved by Martin).

## Stable enemy IDs

- `Character::enemyId` (native) — set at spawn via `Game::newEnemy(id, class)`; display name comes
  from the table in `app/src/main/cpp/Bestiary.h`. Heroes have an empty id.
- Boss logic (`LootSystem::isBossId / bossTier / isEndgameBossId`), boss-seen flags
  (`markBossSeen`), raid-key / Legendary gates and the GL layer texture pick all key off the id.
  Boss tiers are unchanged (only the Act 3 Warden is tiered among story bosses, as before).
- Kotlin gets the id as the 6th field of each `E,` roster entry and looks it up in
  `MainActivity.enemySprites` — **every id has its own entry**; an unknown id shows the neutral
  `enemy_unknown` shadow (never a goblin).
- IDs are append-only. Never rename or reuse one: saves and online sync store them.

### Old saves

Enemy entries in the save gained a trailing `,enemyId` field (older readers ignore extra fields).
On load, if the field is missing/unknown, `bestiary::migrateLegacyName()` maps the old display name
(exact legacy/current name first, then the old substring rules, e.g. "Dragon" → `boss_drake`) to an
id and the foe is renamed to its current display name. Companion `Melf (NPC)` is renamed to
`Corvin (NPC)` on load. Quest/boss-seen header flags are unchanged.

## ID table

| id | Display name | Was | Sprite |
|---|---|---|---|
| `enemy_wickthief` (+ `_scout`, `_cutpurse`, `_cache`, `_river`, `_root`, `_fen`) | Wickthief (Scout / Cutpurse / Cache / River / Root / Fen) | Goblin family | `enemy_wickthief` (new) |
| `enemy_ashbound`, `enemy_ashbound_bonebound` | Ashbound, Bonebound Ashbound | Skeleton, Bonebound Skeleton | `enemy_ashbound` (new) |
| `boss_wick_king` | The Wick King | Goblin King | `boss_wick_king` (new) |
| `enemy_gloamhound` | Gloamhound | Wolf | placeholder (wolf) |
| `enemy_crier_bone`, `enemy_crier_wick` | Bone Herald, Wickthief Herald | Bone / Goblin Herald | placeholder (Ashbound / Wickthief) |
| `enemy_tallow_rat`, `enemy_brute`, `enemy_tithe_*` | Giant Rat, Ogre, Debt Enforcer, Ash Scavenger, Ashwake Scout | (unchanged names) | placeholders |
| `boss_sexton` | The Lantern Sexton | Skeleton Champion | placeholder (Ashbound) |
| `boss_collector` | Collector Sallow Venn | The Collector | placeholder (ogre) |
| `boss_warden` | The First Lamplighter | Breach Warden | placeholder (ogre) |
| `boss_herald` | The Bell-Herald of the Tithe | Ashwake Herald | placeholder (skeleton) |
| `boss_prior` | The Ossuary Prior | Skeleton King | placeholder (Ashbound) |
| `boss_drake`, `boss_hollow_crown`, `boss_ember_hydra`, `boss_nightfang` | (unchanged names) | — | placeholders |

Names can't contain `,` `|` `;` `~` (save/roster delimiters) — hence "Collector Sallow Venn".

## Art pipeline

Sources: `docs/art/ash_ember/*.svg` + `style_sheet.png`. Convert with
`python3 tools/svg2vd.py docs/art/ash_ember/wickthief.svg app/src/main/res/drawable/enemy_wickthief.xml`.
The enemy SVGs only use paths/circles, flat fills + opacity, evenodd and simple strokes, so the
conversion is lossless (no gradients/filters). `background.svg` uses gradients and is **not**
converted yet (Phase 4 stages).

Silhouettes face left as authored (not mirrored) and have no attack frame. Animation is code:
idle bob + slow sway (pivot at the feet), a forward lean during the existing lunge, and an
ash-dissolve ghost + drifting ash/ember flecks when a foe leaves the roster. **Reduce flashing**:
no idle motion, no lean, dissolve is a plain fade with no flecks.

## Device playtest checklist

1. Clean + Rebuild (native C++ changed). Install over v2.19 with an existing Continue save → it loads; foes show new names/sprites; companion reads Corvin.
2. New Story → title card + funeral hook; Wickthief Scout fight starts immediately.
3. Two identical foes (Bone Gallery Ashbound) → both selectable, labels `#1` / `#2`.
4. Kill a foe → ash-dissolve; toggle Reduce flashing → plain fade, sprites still.
5. Finish Act 1 → Wren relights the Lantern, masked watcher, Act 2 starts.
6. Crawl to a Wick King court → boss stage + boss loot; Boss Raid → endgame boss still allows Legendary.
7. Host online → menus say Lanternkeeper; Join; solo save untouched.
