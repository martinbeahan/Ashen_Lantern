# Ash & Ember — Phase 2 (v2.21 / 60)

Phase 1 (v2.20) introduced stable enemy IDs and the first three silhouettes. Phase 2 finishes
the bestiary art, renames five foes and extends *Reduce flashing* to the new bosses.

## 1. Art — one drawable per ID

- 24 new original SVGs (`docs/art/ash_ember/<enemyId>.svg`, previews in `docs/art/ash_ember/previews/`)
  converted with `tools/svg2vd.py` to `app/src/main/res/drawable/<enemyId>.xml`.
- All 24 converted as-is: only paths/circles, flat fills + fill/stroke-opacity, evenodd and
  simple strokes — no gradients, filters, masks, transforms or dashes, so **nothing had to be simplified**.
- `MainActivity.enemySprites` maps each of the 27 IDs to its same-named drawable; no entry borrows
  art (Hollow Crown, Sexton, Prior and the former CC0 placeholders included). The unused CC0
  `sprite_skeleton` / `sprite_wolf` / `sprite_ogre` (+ `_atk`) bitmaps were removed.
- All enemy art is treated as Ash & Ember (faces left, no mirror, no `_atk` swap, sway + lean).
- Check: `python3 tools/check_enemy_sprites.py` — 27 IDs → 27 distinct drawables (by name and
  content), calm variants distinct too. `MainActivity.verifyEnemySpriteTable()` logs an error at
  start-up if the table ever regresses.

## 2. Renames (display names only)

| ID | v2.20 name | v2.21 name | pre-v2.20 name |
|---|---|---|---|
| `enemy_tallow_rat` | Giant Rat | **Tallow Rat** | Giant Rat |
| `enemy_brute` | Ogre | **Millstone Brute** | Ogre |
| `enemy_tithe_enforcer` | Debt Enforcer | **Tithe Enforcer** | Debt Enforcer |
| `enemy_tithe_watcher` | Ashwake Scout | **Tithe Watcher** | Ashwake Scout |
| `enemy_crier_bone` | Bone Herald | **Bone Crier** | Bone Herald |

- IDs and the save format are unchanged. v2.20 saves carry the ID and are re-labelled with the
  current name on load; pre-v2.20 saves migrate by name via `Bestiary.h`: `legacyName`, the new
  `aliases()` table (former display names) and substring fallbacks (`Giant Rat 2`, `Elite Ogre`, …).
- The *Ogre Roadblock* set-piece is now *Millstone Roadblock* (template key `ogre_roadblock` unchanged).
- Test: `tools/migration_test/run.sh` (host build of `Game.cpp` with the NDK clang) covers
  pre-v2.20 saves, v2.20 saves with Phase-1 names, every legacy/display/alias name and boss tiers.

## 3. Reduce flashing

`tools/svg2vd.py --calm` emits `<id>_calm.xml` with Lantern Amber / Amber Core (`#F2A541` /
`#FFD27A`) toned down: translucent glow halos keep 30% of their alpha, solid amber 55%.
Shipped for the four bosses whose glow is a large lit area:

| Boss | Glow |
|---|---|
| `boss_ember_hydra` | furnace door (+ coal-head embers) |
| `boss_warden` | light through the chest arch |
| `boss_prior` | censer |
| `boss_nightfang` | caged soul in the abdomen |

With *Reduce flashing* on, `spriteForEnemyId` returns the calm variant; toggling the setting
rebuilds the battle arena so it applies mid-fight. These sprites have no animated glow pulse —
the existing reduce-flash rules (no idle sway, no lunge lean, plain fade on death) still apply.
