# Act 2 + Dungeon Crawl + Difficulty — v1.7 (versionCode 25)

**Legal:** Original plot/locations/dialogue. SRD 5.1 monster/rule concepts only. Compatible with 5e SRD — not an official D&D product. See `ATTRIBUTION.md`.

## Menu

| Button | Behavior |
|--------|----------|
| **Story adventure** | Solo scripted Act 1 → Act 2 → procedural |
| **Dungeon Crawl** | Skip quest script; procedural rooms from Room 1 |
| **Change difficulty** | Easy (default) / Medium / Hard / Nightmare — preference persisted |
| Continue | Restores mid-run (including Easy–Hard Game Over → wipe dialog) |

Difficulty is **locked for the run** when Story/Crawl starts (shown in Settings).

## Act 1 — Ashen Lantern (unchanged beats 1–6)

Same as before. Onward from Ashen Shrine now starts **Act 2** (not immediate procedural).

## Act 2 — The Tithe (beats 8–13; was *Millhollow's Debt* until v2.25)

v2.25 rewrote the text to *The Kindling* (see [`KINDLING_ACTS2-4.md`](KINDLING_ACTS2-4.md)); beat ids, foes and rewards are unchanged.

| # | Beat | Notes |
|---|------|-------|
| 8 | Millhollow Green | Grey Tithe collectors; light fight (Tithe Enforcer) |
| 9 | Millrace Weir | River goblins guarding jars of ash in funeral ribbons |
| 10 | Flooded Cellar | Ashbound + Tallow Rat; Wren names Aunt Lise |
| 11 | Ledger Loft | Soft open — Search → the soul-ledger (`questAct2LedgerFound`); your name is next |
| 12 | Collector's Hall | Collector Sallow Venn (Fighter); ends on his knees |
| 13 | The Tithe Broken (was *Debt Settled*) | Resolution, +40 gold; **Spare him / End it** choice for Venn |

**Saves:** Mid-Act 1 Continue still works. Saves already in legacy post–Act 1 procedural (`questBeat=7`) stay procedural — **start a fresh Story** to play Act 2. New header fields: `questAct2Complete`, `questAct2LedgerFound`, `soloPlayMode`, `difficulty` (backward compatible).

## Party wipe rules

| Difficulty | On entire party fallen |
|------------|------------------------|
| **Easy** | Keep stats/gear/gold; Continue → one room/beat back; **full HP** |
| **Medium** | Keep stats/gear/gold; Continue → one back; **half HP** |
| **Hard** | Keep stats; **strip gold + gear** to starters; Continue → one back |
| **Nightmare** | Complete restart — wipe save / main menu |

## Device playtest checklist

1. Clean/Rebuild (native + Kotlin changed).
2. Menu shows Story, Crawl, Difficulty (default Easy).
3. Story → Millhollow → … → Shrine → Onward → **The Tithe** on the green.
4. Act 2 loft Search → soul-ledger → Collector → The Tithe Broken (Venn choice) → Act 3.
5. Crawl → Room 1 procedural, no quest banner.
6. Easy wipe → Continue keeps gear, full HP, prior chamber.
7. Hard wipe → Continue strips gold/gear.
8. Nightmare wipe → menu, no Continue revive.
9. Settings shows locked difficulty during a run.
10. Host online still procedural (no solo script).
11. Audio, graphics pack, tap targets, Search once/room, death locks preserved.

## Out of scope

Next graphics layer.
