# Act 4 — Ashwake Vigil (v2.15 / 54)

Original Ashen Lantern story continuation after Act 3 *Emberdeep*. SRD-safe foes only; no WotC module text.

**v2.25:** text rewritten to *The Kindling* — the Spire is a Tithe siphon, Greyfen was a town, and at Vigil Kindled Maren Vey unmasks, takes a shard of the Lantern and descends ("Follow the ash."). Vigil Kindled now uses the `kindling_pyre` stage. See [`KINDLING_ACTS2-4.md`](KINDLING_ACTS2-4.md).

## Beats (20–25)

| Beat | Title | Shape |
|------|-------|-------|
| 20 | Vigil Watch | Soft open — Tithe Watcher; Search clue |
| 21 | Greyfen Marsh | Fen Wickthief + Tallow Rat |
| 22 | Fen Causeway | Ashbound pair (Greyfen's dead) |
| 23 | Vigil Niche | Soft Search — Vigil Ember |
| 24 | Ashwake Threshold | Boss — the Bell-Herald of the Tithe (toll hushes Wren; flavour only) |
| 25 | Vigil Kindled | Resolution — story complete / endgame unlock |

## Flow

- After Act 3 *Breach Sealed*, Onward starts Act 4 (not procedural endgame).
- Legacy post–Act 3 Continue saves: Onward from procedural → Act 4.
- `isStoryFullyComplete()` / endgame CTA → `questAct4Complete_`.
- `endgameContentAllowed()` still true for `questAct3Complete_` (grandfather).
- Serialize appends Act 4 flags (backward compatible). Solo `save_state` preserved; Host/Join isolation unchanged.
