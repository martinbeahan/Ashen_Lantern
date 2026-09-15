# Daily Quests (v2.11)

Local-midnight errands for story-complete Continue heroes. Meta prefs only — never writes Host/Join into `save_state`.

## Gate
- Default: **story complete** (Acts 1–3 / `story_fully_complete`) **and** a Continue save.
- Progress tracked only in **solo** sessions (story endgame / crawl / Boss Raid). Host/Join never contribute.

## Refresh
- Same local calendar-day boundary as Raid Keys (`YYYY-MM-DD` + `msUntilLocalMidnight`).
- Each day rolls **1 or 2** objectives from a small original pool (no WotC module text).

## Progress metrics (native session counters → Kotlin prefs deltas)
| Metric | Source |
|--------|--------|
| rooms | Chamber cleared (`enterClearedRoom`) |
| fights | Same clear (win-a-fight) |
| searches | Search attempt in a cleared room |
| damage | Damage dealt to foes |
| bosses | `noteBossDefeat` |

## Rewards (full clear claim)
| Reward | Amount |
|--------|--------|
| Gold | 45–60g (date-seeded) |
| Loot | Wayfarer's Tonic (Uncommon potion) |
| Raid Key | **Fixed 1 key** if held &lt; 3 and daily grant cap not reached |

If claimed from the menu with no live solo party, gold/loot stash in `pending_daily_*` and apply on next solo Continue (`activateSession` with crash guard). Keys grant immediately into meta prefs.

## UI
- Main menu: **DAILY QUESTS** summary + **Claim daily rewards** when ready.
- Settings: same summary + claim button.
- Countdown to local midnight shown beside the board.

## Non-goals (this PR)
Arena, Endless Deep, Act 4. (Challenge Dungeon: see CHALLENGE_DUNGEON.md / v2.12)
