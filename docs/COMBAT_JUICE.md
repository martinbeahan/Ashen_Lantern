# Restrained Combat Juice — v2.9 (versionCode 45)

**Base:** stacks on `feature/encounter-authorship` (AA step 2 / PR #55, v2.8 / 44). **Merge after #55** (and #54 if that stack is not yet on master).

**Legal:** Original polish only — no copyrighted assets, no WotC IP. Soft UI motion + original combat-log copy. See `ATTRIBUTION.md`.

## Goal

Hits, crits, kills, specials, level-ups, and Rare+ loot should feel clearer **without** arcade spam or strobing. Easy / photosensitive-safe via `pref_reduce_flash`.

## What shipped

| Beat | Feedback | `pref_reduce_flash` ON |
|------|----------|------------------------|
| Hit | Target tint + floating damage; brief arena shake | Soft tint only; **no** screen flash; **no** shake |
| Crit | Stronger tint, gold float, screen wash, stronger shake | Soft gold tint only; flash + shake skipped |
| Kill | `Felled <foe>! …` combat-log + short toast + soft amber wash | Toast + log only (no wash) |
| Special (Cutting Quip, Action Surge, Sneak Attack, Magic Missile, Healing Word) | Combat-log (engine) + one-shot Special button scale pulse + soft blue wash | Button pulse only (no wash) |
| Level-up | Existing toast / dialog / Sheet highlight + one Sheet scale pulse + soft gold wash | Toast / dialog / soft Sheet highlight; **no** wash; no opacity strobe on Sheet |
| Rare / Epic / Legendary loot | `lastEvent_` carries `[Rarity]`; short toast + soft violet wash | Toast only |

## Key hooks

- **Kotlin:** `MainActivity` — `shakeBattleArena`, `pulseAccent`, `maybeKillPop`, `maybeRareLootAccent`, `maybeSpecialAbilityJuice`, expanded `maybeAnimateFromEvent` (Cutting Quip / psychic sting).
- **Existing (kept):** `flashHit`, `flashScreen`, floating damage, dice overlay — already gated where needed.
- **C++:** `grantKillLoot` → `Felled <foe>! …`; `enterClearedRoom` preserves kill beat and surfaces Rare+ into `lastEvent_`.

## Out of scope

Audio beds (AA step 4), new encounters, new classes, new assets.

## Device smoke

1. Clean/Rebuild (native + Kotlin).
2. Hit / crit: float + tint; shake only with reduce-flash **off**.
3. Toggle **Reduce flashing** — no full-screen flashes, no arena shake, no Sheet opacity strobe.
4. Kill foe → short “Felled …” toast + feed line.
5. Bard Cutting Quip → feed line + Special button pulse.
6. Level-up / Rare+ drop → one restrained accent (not fireworks).
7. Regression: set-pieces (#55), Bard / raid keys (#54), Easy early story.
