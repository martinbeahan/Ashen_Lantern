# Ashen Lantern — Ash & Ember heroes (art only, no repo changes)

| classId (GameLogic.h CharacterClass) | Idle | Attack | Silhouette hook / prop | Warm accent |
|---|---|---|---|---|
| 0 FIGHTER — the Warden | sprite_fighter.svg | sprite_fighter_atk.svg | crested great-helm, round shield with diamond sigil cut, longsword | belt lantern, lantern-lit swing arc |
| 1 WIZARD — the Ember-mage (Corvin's look) | sprite_wizard.svg | sprite_wizard_atk.svg | wide-brim low hat, high collar, long coat, scarf | caged ember on staff; attack throws an ember bolt |
| 2 ROGUE — the Wick-cutter | sprite_rogue.svg | sprite_rogue_atk.svg | low crouch, peaked hood, mask scarf, twin knives | shuttered dark-lantern slit, amber blade glints |
| 3 CLERIC — the Kindler | sprite_cleric.svg | sprite_cleric_atk.svg | deep cowl, bell robe with stole, shepherd's crook | lantern hanging from the crook; attack raises it with rays |
| 4 BARD — the Ballad-keeper (new, replaces the wizard reuse) | sprite_bard.svg | sprite_bard_atk.svg | feathered cap, short cape, pear lute pushed forward | belt lantern, amber strings; attack plays amber sound arcs |

Rules: 512 canvas, feet y≈478 (shadow to ~487), face RIGHT, amber rim light on left (lantern-side) edges, no teal anywhere (asserted in draw_heroes.py).
VectorDrawable-safe: path/circle only, flat fills + fill-opacity, evenodd, strokes; no transforms/gradients/rect/ellipse. All 10 convert with tools/svg2vd.py (6–11 KB each, 96 KB total vs 148 KB of PNGs being replaced).
Idle and attack frames share one scale/anchor (scaled ×1.22 about x370,y478 by scale_svg.py), so swapping frames doesn't jump.

Other files: heroes_sheet.png (party vs bestiary on ember_ring + idle/attack turnaround + 96px strip), readability_96px.png, draw_heroes.py (source), scale_svg.py, build_heroes_sheet.py.
