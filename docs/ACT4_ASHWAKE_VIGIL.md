# Act 4 — Ashwake Vigil (v2.15 / 54)

Original Ashen Lantern story continuation after Act 3 *Emberdeep Breach*. SRD-safe foes only; no WotC module text.

## Beats (20–25)

| Beat | Title | Shape |
|------|-------|-------|
| 20 | Vigil Watch | Soft open — Ashwake Scout; Search clue |
| 21 | Greyfen Marsh | Marsh Goblin + Giant Rat |
| 22 | Fen Causeway | Skeleton pair |
| 23 | Vigil Niche | Soft Search — Vigil Ember |
| 24 | Ashwake Threshold | Boss — Ashwake Herald |
| 25 | Vigil Kindled | Resolution — story complete / endgame unlock |

## Flow

- After Act 3 *Breach Sealed*, Onward starts Act 4 (not procedural endgame).
- Legacy post–Act 3 Continue saves: Onward from procedural → Act 4.
- `isStoryFullyComplete()` / endgame CTA → `questAct4Complete_`.
- `endgameContentAllowed()` still true for `questAct3Complete_` (grandfather).
- Serialize appends Act 4 flags (backward compatible). Solo `save_state` preserved; Host/Join isolation unchanged.
