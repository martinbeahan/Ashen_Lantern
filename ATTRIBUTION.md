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


## 2D graphics

As of v2.22 **all bundled battle art, backgrounds and UI chrome are original** to Ashen Lantern;
the remaining CC0 third-party art (LuizMelo hero sprites, Nidhoggn / quantumelle / ansimuz
battlebacks and the dimmed menu backdrop) was removed. No Wizards of the Coast / D&D product art is used.

**Retired third-party art (history):** v1.6–v2.21 used LuizMelo CC0 hero sprites (Fantasy Warrior,
Wizard Pack, Huntress, Medieval King Pack; `sprite_*.png` / `sprite_*_atk.png`) and CC0
battlebacks by Nidhoggn (*Backgrounds*), quantumelle (*Dark forest path*) and ansimuz
(*Phantasy Dungeon Entrance*) as `bg_battle_*.webp` / `bg_menu_backdrop.webp`. All of those
files are gone from the app in v2.22; we thank their authors.

### Ash & Ember hero art (v2.22) — original

Hand-authored for Ashen Lantern. **Original work, owned by the project** — not CC0 third-party art.
Sources live in `docs/art/ash_ember/heroes/` (SVG + contact sheets); `tools/svg2vd.py` converts them
to VectorDrawables and `--calm` makes the Reduce-flashing attack frames.

| Asset in app | Source | Author | License |
|---|---|---|---|
| `sprite_fighter.xml`, `sprite_fighter_atk.xml` | `docs/art/ash_ember/heroes/sprite_fighter.svg`, `sprite_fighter_atk.svg` | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `sprite_wizard.xml`, `sprite_wizard_atk.xml` | `docs/art/ash_ember/heroes/sprite_wizard.svg`, `sprite_wizard_atk.svg` | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `sprite_rogue.xml`, `sprite_rogue_atk.xml` | `docs/art/ash_ember/heroes/sprite_rogue.svg`, `sprite_rogue_atk.svg` | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `sprite_cleric.xml`, `sprite_cleric_atk.xml` | `docs/art/ash_ember/heroes/sprite_cleric.svg`, `sprite_cleric_atk.svg` | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `sprite_bard.xml`, `sprite_bard_atk.xml` | `docs/art/ash_ember/heroes/sprite_bard.svg`, `sprite_bard_atk.svg` | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `sprite_*_atk_calm.xml` (all five classes) | Same `_atk` SVGs via `tools/svg2vd.py --calm` (Reduce flashing) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |

### Ash & Ember backgrounds and UI kit (v2.22) — original

| Asset in app | Source | Author | License |
|---|---|---|---|
| `drawable-nodpi/bg_stage_*.webp` (15 stages) and their `_hd` 1080×1350 renders; `drawable/bg_stage_boss_overlay.xml` (boss vignette) | `docs/art/ash_ember/phase3/backgrounds/*.svg` (rendered to WebP) | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| `drawable-nodpi/bg_menu_backdrop.webp` / `_hd` | `docs/art/ash_ember/phase3/backgrounds/menu_backdrop.svg` | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |
| Panels, corner ornaments, buttons, HUD chips, narration card, dialog window, rarity rows / sockets / pips, target and selection rings, sprite backlights, ember XP pips, die faces, damage floats (XML drawables), `WickBarView`, `EmberXpView` | `docs/art/ash_ember/phase3/ui_kit_sheet.svg` | Ashen Lantern (original) | Original, project-owned (see `LICENSE`) |

### Fonts (v2.22) — SIL Open Font License 1.1

| Asset in app | Font | Copyright | License |
|---|---|---|---|
| `res/font/cinzel_semibold.ttf`, `cinzel_bold.ttf` | Cinzel | Copyright 2020 The Cinzel Project Authors (https://github.com/NDISCOVER/Cinzel); Copyright © 2012 Natanael Gama (www.ndiscovered.com), with Reserved Font Name “Cinzel Decorative” | [SIL OFL 1.1](https://openfontlicense.org) — full text in `app/src/main/assets/licenses/OFL-Cinzel.txt` and below |
| `res/font/cormorant_garamond_medium.ttf`, `cormorant_garamond_medium_italic.ttf`, `cormorant_garamond_bold.ttf` | Cormorant Garamond | Copyright 2015 the Cormorant Project Authors (github.com/CatharsisFonts/Cormorant) | [SIL OFL 1.1](https://openfontlicense.org) — full text in `app/src/main/assets/licenses/OFL-CormorantGaramond.txt` and below |

The bundled files are **modified versions** under the OFL: subset to Latin and unhinted to save
APK size, and `cinzel_semibold.ttf` is a static weight-600 instance made from the upstream Cinzel
variable font. They are not sold on their own and keep their original family names; neither uses the
Reserved Font Name “Cinzel Decorative”.

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

No new third-party art or audio. Act 3 / endgame / raid encounters originally reused the CC0 battle stages; since v2.22 every mode uses an original Ash & Ember background (Ember Ring, Cinder Crucible, Ashen Deep and one stage per Boss Raid), and monster sprites are original Ash & Ember art since v2.21. Original names and flavor text only — not from any WotC module.

## Temporary sprite mapping

- **Bard (v2.7) — retired in v2.22 for the Android UI:** the Bard now has its own original `sprite_bard` / `sprite_bard_atk` art. The legacy GL layer (dead code, see `docs/ASH_EMBER_PHASE3.md`) still reuses the Wizard model.
- **Placeholder enemy sprites (v2.20) — retired in v2.21:** every one of the 27 enemy IDs now maps to its own same-named Ash & Ember drawable (`MainActivity.enemySprites`; checked by `tools/check_enemy_sprites.py`). No enemy borrows another's art; `enemy_unknown.xml` is only for IDs from a newer host.

## SIL Open Font License, Version 1.1 (Cinzel, Cormorant Garamond)

```
-----------------------------------------------------------
SIL OPEN FONT LICENSE Version 1.1 - 26 February 2007
-----------------------------------------------------------

PREAMBLE
The goals of the Open Font License (OFL) are to stimulate worldwide
development of collaborative font projects, to support the font creation
efforts of academic and linguistic communities, and to provide a free and
open framework in which fonts may be shared and improved in partnership
with others.

The OFL allows the licensed fonts to be used, studied, modified and
redistributed freely as long as they are not sold by themselves. The
fonts, including any derivative works, can be bundled, embedded, 
redistributed and/or sold with any software provided that any reserved
names are not used by derivative works. The fonts and derivatives,
however, cannot be released under any other type of license. The
requirement for fonts to remain under this license does not apply
to any document created using the fonts or their derivatives.

DEFINITIONS
"Font Software" refers to the set of files released by the Copyright
Holder(s) under this license and clearly marked as such. This may
include source files, build scripts and documentation.

"Reserved Font Name" refers to any names specified as such after the
copyright statement(s).

"Original Version" refers to the collection of Font Software components as
distributed by the Copyright Holder(s).

"Modified Version" refers to any derivative made by adding to, deleting,
or substituting -- in part or in whole -- any of the components of the
Original Version, by changing formats or by porting the Font Software to a
new environment.

"Author" refers to any designer, engineer, programmer, technical
writer or other person who contributed to the Font Software.

PERMISSION & CONDITIONS
Permission is hereby granted, free of charge, to any person obtaining
a copy of the Font Software, to use, study, copy, merge, embed, modify,
redistribute, and sell modified and unmodified copies of the Font
Software, subject to the following conditions:

1) Neither the Font Software nor any of its individual components,
in Original or Modified Versions, may be sold by itself.

2) Original or Modified Versions of the Font Software may be bundled,
redistributed and/or sold with any software, provided that each copy
contains the above copyright notice and this license. These can be
included either as stand-alone text files, human-readable headers or
in the appropriate machine-readable metadata fields within text or
binary files as long as those fields can be easily viewed by the user.

3) No Modified Version of the Font Software may use the Reserved Font
Name(s) unless explicit written permission is granted by the corresponding
Copyright Holder. This restriction only applies to the primary font name as
presented to the users.

4) The name(s) of the Copyright Holder(s) or the Author(s) of the Font
Software shall not be used to promote, endorse or advertise any
Modified Version, except to acknowledge the contribution(s) of the
Copyright Holder(s) and the Author(s) or with their explicit written
permission.

5) The Font Software, modified or unmodified, in part or in whole,
must be distributed entirely under this license, and must not be
distributed under any other license. The requirement for fonts to
remain under this license does not apply to any document created
using the Font Software.

TERMINATION
This license becomes null and void if any of the above conditions are
not met.

DISCLAIMER
THE FONT SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO ANY WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
OF COPYRIGHT, PATENT, TRADEMARK, OR OTHER RIGHT. IN NO EVENT SHALL THE
COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
INCLUDING ANY GENERAL, SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL
DAMAGES, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF THE USE OR INABILITY TO USE THE FONT SOFTWARE OR FROM
OTHER DEALINGS IN THE FONT SOFTWARE.
```
