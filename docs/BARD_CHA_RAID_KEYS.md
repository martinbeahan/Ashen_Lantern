# Bard + CHA + Raid Keys 3 — v2.7 (versionCode 43)

**Base:** master `2.6.5` / versionCode 42. **Independent of #53** (Ashen Lantern launcher icon / v2.6.6-43) — if both merge, bump this PR's versionCode past the icon or resolve in merge.

**Legal:** SRD-safe class feel; original flavor text (no PHB dumps). Sprite temporarily reuses Wizard art (2D + GL) until Bard CC0 art.

## 1) Bard class (ordinal 4, append-only)

- Added after Cleric so existing saves (class 0–3) and classTag gear stay valid — no ordinal remapping.
- **CHA** primary for weapon attacks/damage (like Wizard uses INT).
- **Special — Cutting Quip** (Vicious Mockery–lite): CHA attack vs AC; on hit deals psychic sting (`1d4 + CHA mod`, crit adds another d4) and rattles the foe (**−2 on their next attack roll**). Original name/flavor.
- Starter: Rapier + Leather (class-tagged). Hit die d8. Resources `2 + level`.
- Companion: **Jory (NPC)** in pick lists / Ally change-class.
- Loot/shop: Bard-tagged weapons/armor (Stage Rapier, Performer's Leathers, Songsteel Rapier, Troubadour Coat, etc.). `-1` Any gear still works.

## 2) CHA in UI/copy

- BeginnerGuide stat blurbs/hints no longer call CHA "rarely used".
- Class tip + Help "What do stats do?" + tutorial Special page include Bard / Cutting Quip.

## 3) Raid keys max 2 → 3

- Held, daily grant, UI ` /3 keys`, toast, and `docs/ENDGAME_ACT3_RAIDS.md` updated.

## Preserve

Story/crawl/raid gates, Legendary rules, reduce-flash, Ashen Lantern branding, online DM.
