# Attribution & legal

## Legal / trademarks

**Ashen Lantern** is an independent tabletop fantasy adventure app by Martin Beahan.

This app is **not affiliated with, sponsored by, endorsed by, or approved by** Wizards of the Coast LLC.

**Dungeons & Dragons**, **D&D**, and related marks are trademarks of Wizards of the Coast LLC.

Game rules and class/monster concepts are **compatible with 5e SRD concepts only**, based on the
**System Reference Document 5.1** (SRD 5.1), available under the
[Creative Commons Attribution 4.0 International License](https://creativecommons.org/licenses/by/4.0/).
Original adventure text (including Solo quests such as *Ashen Lantern*) is original and not copied from
any Wizards of the Coast adventure module.

Third-party art and audio remain under their own licenses (tables below). Application source code is
MIT-licensed; see `LICENSE`.

---

# Audio attribution

All game audio assets ship under **Creative Commons Zero (CC0 / public domain)**.
Attribution is not required by the licenses, but we credit creators here and in Settings → About.

## Background music

Context beds (one loop at a time). See `docs/AUDIO_BEDS.md` for cue map.

| Asset in app | Code bed | Source title | Author | License | Link |
|---|---|---|---|---|---|
| `bgm_explore.ogg` (Ironchest_dungeon001) | `Track.EXPLORE` — dungeon / crawl / story explore | Ironchest's Dungeon Loops | Ironchest Games | [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) | https://ironchestgames.itch.io/ironchests-dungeon-music-loops |
| `bgm_town.ogg` (Ironchest_dungeon005) | `Track.TOWN` — main menu, merchant, camp | Ironchest's Dungeon Loops | Ironchest Games | [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) | https://ironchestgames.itch.io/ironchests-dungeon-music-loops |
| `bgm_tension.ogg` (Ironchest_dungeon006) | `Track.COMBAT` — normal combat | Ironchest's Dungeon Loops | Ironchest Games | [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) | https://ironchestgames.itch.io/ironchests-dungeon-music-loops |
| `bgm_boss.ogg` (Ironchest_dungeon010) | `Track.BOSS` — boss encounter / Boss Raid | Ironchest's Dungeon Loops | Ironchest Games | [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) | https://ironchestgames.itch.io/ironchests-dungeon-music-loops |

**Backup shortlist (not bundled):** JaggedStone *Loopable Dungeon Ambience* (CC0, OpenGameArt); RandomMind *Old Tower Inn* (CC0) — reserved for a later quieter town accent if needed.

## Sound effects

| Asset in app | Source title | Author | License | Link |
|---|---|---|---|---|
| `sfx_attack.ogg` (sword.1) | 20 Sword Sound Effects (Attacks and Clashes) | StarNinjas | [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) | https://opengameart.org/content/20-sword-sound-effects-attacks-and-clashes |
| `sfx_hit.ogg` (sword_clash.1) | 20 Sword Sound Effects (Attacks and Clashes) | StarNinjas | [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) | https://opengameart.org/content/20-sword-sound-effects-attacks-and-clashes |
| `sfx_growl.ogg` (trimmed from monster-snarl-attack) | Monster snarls | Darsycho | [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) | https://opengameart.org/content/monster-snarls |
| `sfx_ui_click.ogg` (from beep-02) | Interface beeps | bart | [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) | https://opengameart.org/content/interface-beeps |

## DM voice

When **DM voice** is enabled in Settings (default **off**), notable `[DM]:` narrator lines are read with the device’s built-in **Android TextToSpeech** engine. No third-party voice packs are bundled or downloaded.

## Not used

We intentionally avoid Mixkit Music, FreePD, and any D&D / Wizards of the Coast / Critical Role / film / commercial game OST material.


## 2D graphics pack (v1.6 + layer 2 / v1.9)

All bundled battle art ships under **Creative Commons Zero (CC0 / public domain)** or is original UI chrome (XML shapes). No Wizards of the Coast / D&D product art is used.

### Character sprites (LuizMelo — itch.io, CC0)

Idle frames extracted from free CC0 packs, padded to 256×256 PNGs for Android drawables. The
former CC0 monster bitmaps (`sprite_skeleton`, `sprite_wolf`, `sprite_ogre` and their `_atk`
frames — LuizMelo Monsters Creatures Fantasy / Pet Dogs Pack, Clint Bellanger Tiny Creatures)
were removed in v2.21: every enemy now uses original Ash & Ember art (below).

| Asset in app | Source pack | Author | License | Link |
|---|---|---|---|---|
| `sprite_fighter.png` | Fantasy Warrior (Idle) | LuizMelo | [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) | https://luizmelo.itch.io/fantasy-warrior |
| `sprite_wizard.png` | Wizard Pack (Idle) | LuizMelo | CC0 1.0 | https://luizmelo.itch.io/wizard-pack |
| `sprite_rogue.png` | Huntress (Idle) | LuizMelo | CC0 1.0 | https://luizmelo.itch.io/huntress |
| `sprite_cleric.png` | Medieval King Pack (Idle) | LuizMelo | CC0 1.0 | https://luizmelo.itch.io/medieval-king-pack |

### Battle backgrounds

| Asset in app | Source | Author | License | Link |
|---|---|---|---|---|
| `bg_battle_road.webp` | Dark forest path | quantumelle | CC0 1.0 | https://opengameart.org/content/dark-forest-path |
| `bg_battle_woods.webp` | Backgrounds (battleback6) | Nidhoggn | CC0 1.0 | https://opengameart.org/content/backgrounds-3 |
| `bg_battle_crypt.webp` | Backgrounds (battleback8) | Nidhoggn | CC0 1.0 | https://opengameart.org/content/backgrounds-3 |
| `bg_battle_dungeon.webp` | Backgrounds (battleback5) | Nidhoggn | CC0 1.0 | https://opengameart.org/content/backgrounds-3 |
| `bg_battle_vault.webp` | Phantasy Dungeon Entrance (skull layer) | ansimuz | CC0 1.0 | https://opengameart.org/content/phantasy-dungeon-entrance |

Runtime wiring picks road / woods / crypt / vault / dungeon stage chrome from Solo quest beat names (and room text heuristics). HUD parchment, HP bars, and dice panel strokes are original XML drawables.

### Graphics layer 2 (v1.9)

Expanded Act 2 / crawl stage chrome and combat feedback. Still **CC0 / original UI only** — no WotC art.

| Asset in app | Source | Author | License | Link |
|---|---|---|---|---|
| `bg_battle_weir.webp` | Backgrounds (battleback10) | Nidhoggn | CC0 1.0 | https://opengameart.org/content/backgrounds-3 |
| `bg_battle_hall.webp` | Backgrounds (battleback7) | Nidhoggn | CC0 1.0 | https://opengameart.org/content/backgrounds-3 |
| `bg_battle_loft.webp` | Backgrounds (battleback9) | Nidhoggn | CC0 1.0 | https://opengameart.org/content/backgrounds-3 |
| `bg_battle_cave.webp` | Backgrounds (battleback2) | Nidhoggn | CC0 1.0 | https://opengameart.org/content/backgrounds-3 |
| `bg_battle_ruins.webp` | Backgrounds (battleback3) | Nidhoggn | CC0 1.0 | https://opengameart.org/content/backgrounds-3 |
| `bg_battle_flooded.webp` | battleback8 + blue wash (Flooded Cellar) | Nidhoggn | CC0 1.0 | https://opengameart.org/content/backgrounds-3 |
| `bg_menu_backdrop.webp` | Backgrounds (battleback5, dimmed) | Nidhoggn | CC0 1.0 | https://opengameart.org/content/backgrounds-3 |
| `sprite_*_atk.png` | Attack sheet mid-frames (same LuizMelo CC0 packs as idle) | LuizMelo | CC0 1.0 | itch.io packs listed above |

Layered stage XML adds a cheap top/bottom shade (pseudo-depth) over the battlebacks. Portrait frames, menu/settings panels, screen flash, and floating damage numbers are original UI chrome.

### Richer 2D / UI polish (v2.5)

Visual-only pass — same CC0 sources, higher encode quality and cleaner chrome. No WotC art.

| Asset in app | Change | License |
|---|---|---|
| `bg_battle_*.webp` | Re-encoded Nidhoggn / quantumelle / ansimuz battlebacks at 960×540, higher WebP quality | CC0 (sources above) |
| `bg_battle_boss.webp` | New boss-room stage from Nidhoggn battleback4 | [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) — https://opengameart.org/content/backgrounds-3 |
| `sprite_*.png` / `*_atk.png` | Re-extracted LuizMelo / Tiny Creatures / Pet Dogs frames with readability outline | CC0 (packs above) |
| HUD / panels / HP / XP / selection / damage float XML | Original UI chrome (double-frame stages, turn banner, crit float) | Original |

### Ash & Ember enemy art (v2.20, completed v2.21) — original

Hand-authored for Ashen Lantern (Ash & Ember style sheet v0.1). **Original work, owned by the
project** — not CC0 third-party art. Sources live in `docs/art/ash_ember/` (SVG + style sheet +
`previews/` contact sheets); `tools/svg2vd.py` converts them to Android VectorDrawables. As of
v2.21 all 27 enemy IDs have their own silhouette (`res/drawable/<enemyId>.xml`).

| Asset in app | Source | Author | License |
|---|---|---|---|
| `enemy_wickthief.xml` | `docs/art/ash_ember/wickthief.svg` | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `enemy_ashbound.xml` | `docs/art/ash_ember/ashbound.svg` | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `boss_wick_king.xml` | `docs/art/ash_ember/wick_king.svg` | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `enemy_wickthief_scout.xml` | `docs/art/ash_ember/enemy_wickthief_scout.svg` (v2.21) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `enemy_wickthief_cutpurse.xml` | `docs/art/ash_ember/enemy_wickthief_cutpurse.svg` (v2.21) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `enemy_wickthief_cache.xml` | `docs/art/ash_ember/enemy_wickthief_cache.svg` (v2.21) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `enemy_wickthief_river.xml` | `docs/art/ash_ember/enemy_wickthief_river.svg` (v2.21) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `enemy_wickthief_root.xml` | `docs/art/ash_ember/enemy_wickthief_root.svg` (v2.21) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `enemy_wickthief_fen.xml` | `docs/art/ash_ember/enemy_wickthief_fen.svg` (v2.21) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `enemy_ashbound_bonebound.xml` | `docs/art/ash_ember/enemy_ashbound_bonebound.svg` (v2.21) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `enemy_gloamhound.xml` | `docs/art/ash_ember/enemy_gloamhound.svg` (v2.21) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `enemy_crier_bone.xml` | `docs/art/ash_ember/enemy_crier_bone.svg` (v2.21) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `enemy_crier_wick.xml` | `docs/art/ash_ember/enemy_crier_wick.svg` (v2.21) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `enemy_tallow_rat.xml` | `docs/art/ash_ember/enemy_tallow_rat.svg` (v2.21) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `enemy_brute.xml` | `docs/art/ash_ember/enemy_brute.svg` (v2.21) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `enemy_tithe_enforcer.xml` | `docs/art/ash_ember/enemy_tithe_enforcer.svg` (v2.21) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `enemy_tithe_scavenger.xml` | `docs/art/ash_ember/enemy_tithe_scavenger.svg` (v2.21) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `enemy_tithe_watcher.xml` | `docs/art/ash_ember/enemy_tithe_watcher.svg` (v2.21) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `boss_sexton.xml` | `docs/art/ash_ember/boss_sexton.svg` (v2.21) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `boss_collector.xml` | `docs/art/ash_ember/boss_collector.svg` (v2.21) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `boss_warden.xml` | `docs/art/ash_ember/boss_warden.svg` (v2.21) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `boss_herald.xml` | `docs/art/ash_ember/boss_herald.svg` (v2.21) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `boss_prior.xml` | `docs/art/ash_ember/boss_prior.svg` (v2.21) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `boss_drake.xml` | `docs/art/ash_ember/boss_drake.svg` (v2.21) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `boss_hollow_crown.xml` | `docs/art/ash_ember/boss_hollow_crown.svg` (v2.21) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `boss_ember_hydra.xml` | `docs/art/ash_ember/boss_ember_hydra.svg` (v2.21) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `boss_nightfang.xml` | `docs/art/ash_ember/boss_nightfang.svg` (v2.21) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `boss_ember_hydra_calm.xml`, `boss_warden_calm.xml`, `boss_prior_calm.xml`, `boss_nightfang_calm.xml` | Same SVGs via `tools/svg2vd.py --calm` (Reduce flashing variants, v2.21) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `enemy_unknown.xml` | Hand-written VectorDrawable (neutral shadow) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |

The LuizMelo goblin sprite (`sprite_goblin*.png`) was removed from the Android UI in v2.20; the
GL layer's legacy `assets/goblin.png` / `skeleton.png` remain. `assets/android_robot.png`
(Android template texture) was removed.

### Not used

No scraped D&D / WotC official art, Critical Role stills, or commercial game sprites.

## Rules content (SRD 5.1)

Monster names, class features, and combat math in this app are inspired by the
**System Reference Document 5.1** (SRD 5.1), available under the
[Creative Commons Attribution 4.0 International License](https://creativecommons.org/licenses/by/4.0/).

This app is **compatible with 5e SRD** concepts only. It is **not** an official
Dungeons & Dragons product. Wizards of the Coast, Dungeons & Dragons, and related
marks are trademarks of Wizards of the Coast LLC. Original adventure text in this
project (including the Solo quest *Ashen Lantern*) is original and not copied from
any Wizards of the Coast adventure module.

## Boss encounters (v2.2)

Bosses use **original names and flavor text** (Goblin King, Skeleton King, Ashen Drake). No published adventure text.

Originally these reused LuizMelo CC0 frames; since v2.21 each boss has its own original Ash & Ember silhouette:

| Boss | Sprite |
|---|---|
| The Wick King (was Goblin King) | `boss_wick_king.xml` — original Ash & Ember art (v2.20) |
| The Ossuary Prior (was Skeleton King) | `boss_prior.xml` — original Ash & Ember art (v2.21) |
| Ashen Drake | `boss_drake.xml` — original Ash & Ember art (v2.21) |



## Endgame / Act 3 / Boss Raids (v2.6)

No new third-party art or audio. Act 3 / endgame / raid encounters reuse existing CC0 battle stages listed above (monster sprites: original Ash & Ember art since v2.21) (boss stage, cave, dungeon). Original names and flavor text only — not from any WotC module.

## Temporary sprite mapping

- **Bard (v2.7):** UI battle portrait and GL player model temporarily reuse the Wizard sprite/model until dedicated Bard CC0 art is added.
- **Placeholder enemy sprites (v2.20) — retired in v2.21:** every one of the 27 enemy IDs now maps to its own same-named Ash & Ember drawable (`MainActivity.enemySprites`; checked by `tools/check_enemy_sprites.py`). No enemy borrows another's art; `enemy_unknown.xml` is only for IDs from a newer host.
