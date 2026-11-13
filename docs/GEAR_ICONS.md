# Gear icons (v2.27)

Original *Ash & Ember* item glyphs. No third-party art; hero gear is amber / ember / bone / iron, never teal.

## How an item gets its icon

Items carry no art id in the save, so the **item name** is the identity
(`GearIcons.BY_NAME`, 57 names → 18 glyphs). Unknown names fall back by type:
Weapon → sword, Armor → mail, Potion → draught. No save-format change.

| Glyph | Items |
|---|---|
| sword | Longsword, Champion's Longsword, Fighter's Arming Sword, Steel Blade, Runed Blade, Traveler Blade, Traveler's Blade, Crownbreaker, Emberdeep Relic Blade |
| short_blade | Shortsword, Silent Shortsword, Rogue's Stiletto, Shadow Dirk, Nightwhisper |
| rapier | Rapier, Stage Rapier, Bard's Stage Rapier, Songsteel Rapier |
| staff | Quarterstaff, Focus Staff, Wizard's Focus Rod, Starfall Focus |
| wand | Arcane Wand |
| mace | Mace, Hallowed Mace, Dawnward Mace |
| hammer | Cleric's Warhammer |
| maul | War Maul |
| ashen_lantern | Ashen Lantern (unique art) |
| leather | Leather Armor, Padded Vest, Shadow Leathers, Rogue's Leathers, Performer's Leathers, Veil of Shadows, Veilwalker Hide |
| cloth | Traveler Clothes, Scholar Robes, Wizard's Robe, Temple Vestments, Archsage Mantle, Sturdy Cloak |
| mail | Chain Shirt, Scale Mail, Reinforced Mail, Bulwark Mail, Fighter's Mail, Reliquary Mail, Emberdeep Relic Mail, Cleric's Scale |
| plate | Knight Plate, Sanctum Plate, Aegis of Millhollow |
| coat | Troubadour Coat |
| potion | Healing Draught, Greater Potion (told apart by the rarity frame) |
| tonic | Wayfarer's Tonic |
| raid_key | Raid Key hints (main menu, Settings) |
| gold | Inventory gold header |

`GearIconsTest` scans the C++ sources (`Item::make` literals and the loot tables in
`GameLogic.h`) and fails if any item the game can create has no explicit glyph, or if
the map names an item the game never creates.

## Rarity

- **Frame:** `bg_gear_socket_<rarity>` — far-soot plate, rarity-colour stroke, rim hairline.
  The row keeps its `ic_rarity_*` pip (hollow dot / 1–3 diamonds / flame), so rarity is never colour-only.
- **Legendary:** `ic_gear_<glyph>_legendary` adds an amber inlay on the object (Crownbreaker's
  crown guard and amber fuller for swords) plus static glow circles.
- **Reduce flashing:** `ic_gear_<glyph>_legendary_calm` drops the glow and keeps the inlay.
  These come from their own no-glow SVGs rather than `svg2vd.py --calm`, because `--calm` would
  also dim the inlay.
- The Ashen Lantern, potions, tonic, Raid Key and gold have no Legendary variant.

## Where icons show

Inventory and shop rows (40dp socket, was 26dp), the combat Potion button (dims when disabled),
the Raid Key hint on the main menu and in Settings (once raids are unlocked), and the inventory
gold header.

## Regenerating

```
python3 tools/gear_icons/build_gear_icons.py     # SVGs -> docs/art/ash_ember/gear, VDs -> res/drawable
python3 tools/gear_icons/preview.py out.png      # contact sheet (needs cairosvg + Pillow)
```

Glyph code: `tools/gear_icons/gear_glyphs.py` (new glyphs + Legendary inlays) on top of
`tools/gear_icons/draw_gear.py` (the approved proposal samples) and `ae_lib.py`.
