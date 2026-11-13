# Ash & Ember · Phase 3 — backgrounds, UI reskin, hero art (v2.22 / 61)

Design sources: `docs/art/ash_ember/phase3/` (backgrounds SVGs, `ui_kit_sheet.svg/png`,
`backgrounds_sheet.png`, `mockups/`, design README) and `docs/art/ash_ember/heroes/`
(10 hero SVGs, `heroes_sheet.png`, `readability_96px.png`, README).

## 1. Backgrounds

`refreshBattleBackground()` no longer guesses from room text. It calls
`stageForContext(getSoloPlayMode(), getQuestBeat(), isMerchantRoom(), rosterEnemyIds)`.
`getQuestBeat()` is a new JNI getter in `cpp/main.cpp` that returns the `SoloQuestBeat`
value from `QuestScript.h`, read under the game lock.

| Context | Stage (`bg_stage_<name>`) |
|---|---|
| ARENA (4) | `ember_ring` |
| CHALLENGE (3) | `cinder_crucible` |
| ENDLESS (5) | `ashen_deep` |
| RAID (2) | `boss_ember_hydra` → `raid_kilnmouth`, `boss_nightfang` → `raid_weftholme`, `boss_hollow_crown` or anything else → `raid_corrans_rest` |
| CRAWL (1), online / Lanternkeeper (beat 0), post-quest (beat 7) | `catacombs`, or `millhollow_green` in a merchant room |
| STORY beats 1, 6 | `kindling_pyre` |
| 2 | `thornpath_woods` |
| 3–5 | `ossuary` |
| 8, 13, 14, 20, 25 | `millhollow_green` |
| 9, 10 | `mill_weir` |
| 11, 12 | `mill_hall` |
| 15–19 | `emberdeep` |
| 21–24 | `greyfen_marsh` |
| Main menu | `bg_menu_backdrop` (+ `bg_menu_root` scrim) |
| Any roster ID starting `boss_` | the same stage plus the `bg_stage_boss_overlay` vignette |

Implementation details:
- The stage is an `ImageView` (`battleStageImage`, `scaleType=centerCrop`) under the party and enemy
  columns, and `battleArena` keeps `bg_battle_stage_shade` as the shade layer on top. The image is
  never stretched. The art's centre-safe area (x 60–660, floor in the bottom ~25%) survives the crop
  on both portrait phones and wide tablets.
- All WebPs live in **`drawable-nodpi/`** (16 × 720-wide + 16 × `_hd` 1080-wide, 368 KB total).
  `useHdBackgrounds()` picks `_hd` when `smallestScreenWidthDp ≥ 600` or the screen is wider than
  900 px. (The design README suggested `drawable-xxhdpi`. A density bucket would upscale the base
  files on xxxhdpi phones and downscale the HD ones on hdpi tablets, so a nodpi folder with a
  runtime pick is used instead.) The 1080×1350 renders were not in the hand-off; they are
  rendered from the same SVGs with cairosvg at WebP q80.
- The chosen key (`stage|hd|boss`) is cached so the image is only reset when it changes.
- Removed: 12 `bg_battle_*.webp`, the old `bg_menu_backdrop.webp`, 14 `bg_battle_stage*.xml`
  layer-lists, `bg_parchment.xml`, `bg_sprite_selected.xml`, the old `bg_hp_*` / `bg_xp_*` bars and
  `values-night/themes.xml`. (The design README says 13 + 1 old WebPs; the repo had 12 + 1.)

## 2. UI kit

| Piece | Where |
|---|---|
| Colour tokens `ae_*` + rarity colours; legacy names (`gold`, `parchment`, `ink`, `dungeon_bg`, …) alias onto them; `purple_*` / `teal_*` template colours removed | `values/colors.xml` |
| Soot panels (soot fill, 1dp rim, 4dp inset hairline, four 16dp corner ornaments) | `bg_dungeon_panel`, `bg_hud_panel`, `bg_menu_panel`, `bg_settings_panel`, `bg_dice_panel`, `bg_portrait_frame`, `ic_corner_ornament_{tl,tr,bl,br}` |
| Buttons: normal / **Primary** (amber stroke, Amber Core text, diamond compound drawables) / **Danger** (Sell, Quit, Abandon) with pressed (amber stroke + 1dp shift via `ae_btn_press` animator), disabled (dashed ash stroke, Fog 55% text) and activated states; 2dp corners | `bg_action_btn*`, `Widget.DnD.ActionButton{,.Primary,.Danger}`, `res/color/ae_btn_text*` |
| HUD chips (drawn ~24dp tall inside a 36dp touch target; active = far fill + amber stroke) | `bg_hud_chip`, `Widget.DnD.HudChip` |
| Dialogs — one theme attribute: `alertDialogTheme` → `ThemeOverlay.Ashen.Dialog` (panel window background, Cinzel amber title, Ashen dialog buttons). `materialAlertDialogTheme` is set to a matching overlay with a 2dp shape. **No global `buttonStyle`** (Samsung comment kept) | `values/themes.xml`, `bg_dialog_window` |
| Narration card (soot, 3dp amber left bar, Cormorant italic in bone, “— the Lanternkeeper” in amber) | `narrationCard` in `activity_main.xml`, `bg_narration_card` |
| Fonts: Cinzel (titles, buttons, labels) and Cormorant Garamond (body) | `res/font/`, OFL text in `assets/licenses/` |
| Rarity rows: 3dp stripe, socket, name in rarity colour (Cormorant 700), pip (hollow dot, ◆, ◆◆, ◆◆◆, flame), outlined EQUIPPED tag | `bg_item_row_*`, `bg_item_socket_*`, `ic_rarity_*`, `bg_equipped_tag`, `bindRarityRow()` |
| Target ring (teal, 4 ticks) under the targeted foe; amber ring under the selected hero; backlights behind every sprite | `ic_target_ring`, `ic_select_ring`, `bg_sprite_backlight_{enemy,hero}` |
| Hero HP: `WickBarView` (candle, drips, char cap, flame that shrinks below 30%) | `WickBarView.kt` (HUD columns + character sheet) |
| Enemy HP: 5dp teal soul thread | `bg_hp_bar_enemy` |
| XP: 10 diamond embers (unlit / half / lit level-list), 20 half-steps | `EmberXpView.kt`, `ic_ember_pip*` (HUD + sheet) |
| Floats: hit bone Cinzel, crit Amber Core Cinzel + ember underline, heal lichen “+N”, miss fog italic | `spawnFloatingText(FloatKind)` |
| Dice faces: hex; crit amber stroke + glow, fumble old-blood stroke + rose text | `bg_die_face{,_crit,_fumble}` |
| Hard-coded colours in MainActivity routed to tokens (no `Color.parseColor` left); RARE_LOOT = epic violet; SPECIAL = teal only when the line starts with a roster foe's name, otherwise amber | `MainActivity.kt` |

## 3. Heroes

- The 10 hero SVGs were converted with `tools/svg2vd.py`, plus `--calm` `_atk` variants for all
  five classes (`sprite_*_atk_calm.xml`). The old LuizMelo `sprite_*.png` files are deleted.
- `spriteFor()` maps class 4 (Bard) to `sprite_bard`; `attackSpriteFor()` adds
  `sprite_bard → sprite_bard_atk` and returns the `_atk_calm` frame when Reduce flashing is on.
- Heroes face right (towards the foes) and are never flipped. The attack lean is **+6°** for heroes,
  −6° for Ash & Ember foes, and none under Reduce flashing.

## 4. Reduce flashing

- The target-ring alpha pulse (0.6→1 over 900 ms) is skipped. Rings and backlights are static and
  nothing in the new UI flickers.
- Calm boss art (v2.21) and calm hero attack frames are used.
- `WickBarView` and `EmberXpView` never animate.

## 5. Deferred

- Design note 17: `assets/{fighter,wizard,rogue,cleric,goblin,skeleton}.png` are loaded only by the
  dead GameActivity/Renderer path. They are left for a cleanup PR, as the note suggests.

## Device checklist

Run each item on a **small phone** (≤ 360dp wide, e.g. 720×1280), a **tablet** (≥ 600dp) and a
**Samsung** One UI device. Tick both Reduce flashing off and on where marked.

### Backgrounds
- [ ] Main menu shows the new backdrop, centre-cropped (no stretching) with the scrim; tablet uses `_hd`
- [ ] Story: Millhollow Road / Ashen Shrine → Kindling Pyre; Thornpath Woods; Crypt Doors / Bone Gallery / Lantern Vault → Ossuary
- [ ] Story: Millhollow Green, Debt Settled, Wellside Rumor, Vigil Watch, Vigil Kindled → Millhollow Green
- [ ] Story: Millrace Weir / Flooded Cellar → Mill Weir; Ledger Loft / Collector's Hall → Mill Hall
- [ ] Story: Old Well Descent … Breach Sealed → Emberdeep; Greyfen Marsh … Ashwake Threshold → Greyfen Marsh
- [ ] Post-quest procedural rooms and Dungeon Crawl → Catacombs; a merchant room → Millhollow Green
- [ ] Online (Host / Join) → Catacombs
- [ ] Arena → Ember Ring; Challenge → Cinder Crucible; Endless Deep → Ashen Deep
- [ ] Boss Raids: Hollow Crown → Corran's Rest, Ember Hydra → Kilnmouth, Nightfang → Weftholme
- [ ] Every boss fight (story bosses, raids) adds the boss vignette; it goes away in the next normal fight
- [ ] On a small phone the floor is visible below the sprites and nothing important is cropped; on a tablet nothing looks soft or blurry
- [ ] Continue a v2.21 save mid-story: the right stage appears on load

### UI
- [ ] Panels show four amber corners; no light parchment surfaces left anywhere
- [ ] Buttons: normal, Primary (Attack, Continue) and Danger (Sell, Quit, Abandon) look right; pressed shows the amber stroke + 1dp shift; disabled shows a dashed stroke and dim text (Special at 0 resources, Short Rest after resting, Long Rest on cooldown)
- [ ] HUD chips are easy to tap on a small phone; the Sheet chip shows the active state while the sheet is open
- [ ] **Dialogs** (rest chooser, level up, class change, inventory, shop, sheet, settings confirmations, Host/Join): dark panel window, amber Cinzel title, readable buttons. **Samsung:** buttons are not tinted/filled by One UI and there are no double panels
- [ ] Narration card: italic bone text with the amber “— the Lanternkeeper” line; long room text wraps on a small phone
- [ ] Fonts: Cinzel titles/buttons, Cormorant body text; accented names render (no tofu)
- [ ] Inventory / shop: rarity stripe, socket, coloured name and pip for every rarity; EQUIPPED tag on equipped weapon/armor; Sell is the danger button; real labels (Equip/Unequip, Upgrade, Give to ally, Sell)
- [ ] Hero HP is the wick bar (low colour + small flame under 30%); enemy HP is the thin teal thread
- [ ] XP is 10 embers on the HUD and the sheet; half embers appear; the row resets on level up
- [ ] Floats: hit, crit (amber + underline), heal “+N” (potion, Healing Word), miss
- [ ] Dice: 20 shows the amber crit face, 1 the blood fumble face, others the plain hex
- [ ] Duplicate foes are still selected by slot (`#2`); the teal ring follows the selected slot

### Heroes
- [ ] All five classes (Fighter, Wizard, Rogue, Cleric, **Bard**) show their new sprite facing right (towards the foes); none are flipped
- [ ] The Bard uses its own sprite and attack frame (not the Wizard's)
- [ ] Attack: the frame swaps to `_atk` and back without a jump or resize; heroes lean forward (+6°), foes still lean the other way
- [ ] Backlight is visible behind dark heroes on dark floors (Catacombs, Ashen Deep); the amber ring sits under the selected hero
- [ ] The companion and the player both show the correct class sprite after a class change

### Reduce flashing (Settings → on)
- [ ] The target ring is static (no pulse); rings and backlights never flicker
- [ ] Calm boss art still appears (Ember Hydra, First Lamplighter, Ossuary Prior, Nightfang); toggling mid-fight swaps it
- [ ] Hero attacks use the calm `_atk` frames (Cleric and Wizard flares are dimmer) with no lean
- [ ] No screen flash or pulse from the new UI anywhere

### Regression
- [ ] Long Rest every 5 rooms; Short Rest once per clear
- [ ] Old saves (pre-v2.20, v2.20, v2.21) load fine; `tools/migration_test/run.sh` and `tools/check_enemy_sprites.py` pass
- [ ] Host/Join never touches the solo save
