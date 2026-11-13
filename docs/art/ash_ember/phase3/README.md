# Ashen Lantern · Phase 3: Backgrounds + UI reskin (design only)

Everything is in `/workspace/ash_ember_style/phase3/`. Nothing in the repo has been changed.

## Files
- `backgrounds/<name>.svg|.png|.webp`: 15 stage backgrounds at 720×900 and the menu backdrop at 720×1560. The WebPs are q80 and total 111 KB.
- `backgrounds_sheet.png`: all 16 backgrounds, labelled with where each one is used and its size.
- `ui_kit_sheet.png` / `.svg`: the full UI kit.
- `mockups/mockup_{main_menu,combat,inventory}.png|svg`: 1080×2340 (360×780 dp @3x).
- `concept_heroes/{lamplighter,corvin}.svg|png`: concept-only hero silhouettes used in the mockups.
- Generators: `bg_lib.py`, `scenes.py`, `render_bgs.py`, `build_bg_sheet.py`, `ui_lib.py`, `build_ui_sheet.py`, `build_mockups.py`, `heroes.py`. Run them with `/workspace/.venv_svg/bin/python`.

## Mapping (replaces the text heuristics in refreshBattleBackground)
The mode comes from `getSoloPlayMode()`, which already exists. The story beat needs a new JNI getter, `getQuestBeat(): Int`, which returns the `SoloQuestBeat` value.

| Context | Background |
|---|---|
| ARENA (4) | ember_ring |
| CHALLENGE (3) | cinder_crucible |
| ENDLESS (5) | ashen_deep |
| RAID (2) | Chosen by the boss enemyId in the roster: boss_hollow_crown → raid_corrans_rest, boss_ember_hydra → raid_kilnmouth, boss_nightfang → raid_weftholme (default corrans) |
| CRAWL (1), online/Lanternkeeper (beat 0), POST_QUEST (7) | catacombs (merchant room → millhollow_green) |
| STORY beat 1 Millhollow Road (kindling), 6 Ashen Shrine | kindling_pyre |
| 2 Thornpath Woods | thornpath_woods |
| 3–5 Crypt Doors, Bone Gallery, Lantern Vault | ossuary |
| 8 Millhollow Green, 13 The Tithe Broken, 14 Wellside Rumor, 20 Vigil Watch | millhollow_green |
| 25 Vigil Kindled (v2.25+) | kindling_pyre |
| 9 Millrace Weir, 10 Flooded Cellar | mill_weir |
| 11 Ledger Loft, 12 Collector's Hall | mill_hall |
| 15–19 Old Well Descent → Breach Sealed | emberdeep |
| 21–24 Greyfen Marsh, Fen Causeway, Vigil Niche, Ashwake Threshold | greyfen_marsh |
| Main menu | menu_backdrop |
| Any boss fight | same background plus `bg_stage_boss_overlay` (stronger teal from the right and a heavier vignette) |

## Implementation notes (Code Expert)
1. **Stage image.** Today a 16:9 webp is used as the `battleArena` *View background*, and on a portrait phone the box is close to square, so the image gets **stretched**. Replace it with an `ImageView` (`scaleType=centerCrop`) under the party and enemy columns, and keep the shade layers as a foreground. The art keeps its content inside a centre-safe area: x 60–660, and the floor sits in the bottom ~25%.
2. **APK impact.** Delete the 13 old `bg_battle_*.webp` + `bg_menu_backdrop.webp` (~392 KB) and add the 16 new ones (111 KB), for a **net saving of ~280 KB**. Optional 1080×1350 renders come to about 10–17 KB each (~250 KB total) and could go in `drawable-xxhdpi`. Their CC0 attribution lines can come out of ATTRIBUTION.md, because the new backgrounds are original.
3. **Colours.** Add `ae_*` tokens to colors.xml (the hex values are on the kit sheet). Map the old names onto them (`gold → ae_amber`, `parchment → ae_bone`, `dungeon_bg → ae_void`) so code that uses the old names keeps compiling. Remove the template `purple_*`/`teal_*` names and replace the theme's `colorPrimaryVariant` (still `@color/purple_700`). Rarity: common #A39D94, uncommon #8DBF6A, rare #78B4EA, epic #A676D6, legendary #F2A541.
4. **Fonts.** Add `res/font/cinzel.ttf` (variable, 126 KB; weights 600/700 via `fontVariationSettings` or a font-family XML) and `cormorant_garamond_medium.ttf` / `_italic.ttf`. Use the **static** Google Fonts files, not the 1 MB variable file; if possible subset them to Latin (~100–150 KB each). Replace `android:fontFamily="serif"` with `@font/cormorant` for body text and `@font/cinzel` for titles, buttons and labels. The two places that set `Typeface.BOLD` in MainActivity (~4548, ~4947) should use cinzel. Include the OFL licence text in assets/licenses.
5. **Panels** (bg_dungeon_panel, bg_hud_panel, bg_menu_panel, bg_settings_panel, bg_dice_panel, bg_portrait_frame). Use a layer-list: a soot `<shape>` (#F21C1A1E, 1dp #3A3640 stroke, 2dp corners), then an inset 4dp hairline in #2A262C, then 4 items using `@drawable/ic_corner_ornament` (a 16dp VectorDrawable: path, circle and diamond only) with `android:gravity="top|start"` etc. Mirror it either through four rotated copies or with `android:scaleX="-1"` on a `<rotate>`/`<scale>`, or ship 4 tiny vectors. minSdk is 30, so layer-list gravity and width/height work without 9-patches.
6. **Buttons** (`bg_action_btn` selector). The ActionButton style gets one of three selectors (normal, primary, danger):
   - normal: #2A262C fill, 1.2dp #4A4650 stroke, a 1dp white@6% top lip and a 2dp void@45% bottom lip
   - pressed: #141216 fill, 1.6dp amber stroke, amber@10% inner fill; text shifts 1dp
   - disabled: soot@60% fill, **dashed** 1dp ash stroke (`dashWidth=3dp dashGap=2.5dp`), Fog text at 55%
   - primary: amber stroke, Amber Core text and two 5dp diamond vectors set as compound drawables
   - danger (Sell, Quit, Flee): #8A2A2E stroke with #E3A39A text
   Corners are 2dp everywhere; the old 12–16dp rounding is dropped.
7. **HUD chips**: the same as the buttons at 22–24dp height. The active chip is #141216 with an amber stroke.
8. **Narration card.** This replaces `bg_parchment` on roomDescText: a soot panel without ornaments, a 3dp amber left bar, Cormorant Italic 13–14sp in bone, and the attribution line "— the Lanternkeeper" in amber. Light parchment surfaces are removed throughout.
9. **Wick HP bar.** Write a small custom `WickBarView` (Canvas). It draws:
   - a rounded socket track (#141216, 1dp rim)
   - wax fill #C9A98A, turning #C77A5A below 30%, with a white@22% sheen line and a few drips
   - a 3dp char cap at the fill edge
   - a 2-path amber/core flame riding the fill edge, with a 2-circle glow
   Below 30% the flame shrinks to 65%; that size change is the cue that doesn't rely on colour. The view is used for heroes on the battle stage and the sheet (`sheetHpBar`). If a custom view is not wanted, a ProgressBar layer-list with a ClipDrawable fill plus an ImageView flame repositioned on progress change works too.
10. **Enemy HP** is a thin teal soul thread (4–5dp) in a ProgressBar layer-list with a teal clip fill. The wisps are optional.
11. **Ember XP row** (`hudXpBar`, `sheetXpBar`) is 10 diamond pips. Each pip is a level-list vector with three states: unlit (ash outline), half (amber bottom half) and lit (amber plus a core diamond and a glow circle). It can be a custom View or a LinearLayout of 10 ImageViews; it replaces the horizontal bar.
12. **Target ring and selection.** `ic_target_ring.xml` is a vector made of two teal ellipses and 4 ticks, placed under the targeted enemy's feet. It pulses alpha 0.6→1 over 900 ms; when Reduce Flash is on, it stays static. The selected hero gets an amber ring with no ticks and drops the old `bg_sprite_selected` gold box. Add `bg_sprite_backlight.xml` behind every sprite slot: a radial-gradient `<shape>` in Fog at 30% for enemies and warm #8A7A6A at 30% for heroes. Without it, dark silhouettes vanish on dark floors (the mockup shows the Tallow Rat).
13. **Floats and accents.**
   - Normal hit: bone Cinzel.
   - Crit: Amber Core Cinzel with a 2dp ember underline.
   - Heal: lichen #8DBF6A.
   - Miss: fog italic.
   - Replace the hard-coded colours in MainActivity (~4546–4945, 5088–5093, 5253) with tokens. RARE_LOOT uses epic violet and SPECIAL uses gloam teal only when the effect comes from an enemy, otherwise amber.
14. **Inventory, shop and stat rows.**
   - Soot row with a 3dp rarity stripe on the left edge (layer-list with an inset item).
   - Item glyph socket with a rarity stroke.
   - Name in rarity colour in Cormorant 700, followed by a **rarity pip** to the right of the name: common hollow dot, uncommon ◆, rare ◆◆, epic ◆◆◆, legendary flame. Each is a small vector.
   - An outlined "EQUIPPED" tag.
   - Button row below: Equip/Unequip, Upgrade, Give (Transfer), Sell (danger).
   Under deuteranopia, uncommon and legendary look alike in colour, and under protanopia rare and epic do. The pips tell them apart (simulation strip on the kit sheet).
15. **Dice.** `bg_die_face` becomes a hex vector with a #2A262C fill; the crit (20) gets an amber stroke and glow, and the fumble (1) an old-blood stroke and rose text.
16. **AlertDialogs** (52 uses). Apply a `MaterialAlertDialog` theme overlay (`materialAlertDialogTheme`) with a `shapeAppearance` of 2dp, the `bg_dungeon_panel` background, cinzel titles and amber buttons. Don't set a global buttonStyle; the existing Samsung comment in themes.xml still applies.
17. **Native layer.** MainActivity is an `AppCompatActivity`, so the GameActivity/Renderer.cpp path is dead code. `android_robot.png` is already gone. `assets/{fighter,wizard,rogue,cleric,goblin,skeleton}.png` (28 KB) are only loaded by that dead Renderer and can be removed in a cleanup PR.
