# Save slots (v2.28)

Three local *lanterns*. Design: the approved save-slots proposal (all 14 recommendations).
The save **format** is unchanged (header fields, `E2` marker, transactional load). This is a
prefs layout, a picker and a migration.

## Storage

Everything stays in the activity prefs file. Per-slot keys are `slot_N_<key>` (N = 1..3);
account-wide keys keep their names. `SlotPrefs` wraps the prefs so existing code that reads
`save_state`, `raid_keys`, the endgame snapshots, pity and bests reads the **active** slot
(`active_slot`, the last-used lantern). The Ledger already keys itself `ledger_slot_N_*` (v2.26).

| Per slot (`slot_N_…`) | Account-wide |
|---|---|
| `save_state` (hero, gear, gold, journal, chat, quest flags, choices, mid-run endgame state) | Settings: music, SFX, voice, Reduce flashing, coach, preferred difficulty |
| `hero_name`, `hero_class`, `companion_class`, `companion_auto_ai` | Raid Key daily grant cap: `raid_keys_date`, `raid_keys_granted_today` |
| `story_fully_complete` (no longer sticky across heroes) | Daily quests (date, objectives, claimed) |
| Endgame snapshots: `raid/challenge/arena/endless_active` + `pre_*_save_state` | `crash_guard`, `crash_guard_slot`, `active_slot`, `slots_migrated`, `slots_migration_pending`, `slots_legacy_backup_from` |
| Held `raid_keys` (max 3 per hero) | Online resume markers |
| Crucible pity: `challenge_dungeon_attempts`, `challenge_dungeon_legendary_chance` | Legacy single-save keys (backup, one release) |
| Ring / Deep records: `arena_best_*`, `arena_runs_completed`, `endless_deep_*` | |
| Daily spoils `pending_daily_gold` / `pending_daily_loot` (stamped to the claiming slot) | |
| New: `playtime_ms`, `last_played_ms`, `slot_broken` | |
| Ledger: `ledger_slot_N_*` | |

## Migration (first launch of v2.28)

`SaveSlots.migrate`, idempotent (`slots_migrated`):

- Legacy `save_state` present and readable (shape check + native `describeSaveState`): copy it and
  every legacy per-hero key into slot 1 (or the first empty slot if slot 1 was lit meanwhile).
  `story_fully_complete` is copied; if missing it is derived from header field 29 (Act 4).
  Playtime 0, last-played stamped now, active = 1. **Legacy keys are left in place** as the backup
  (`slots_legacy_backup_from = 67`); nothing reads them. The Ledger (`ledger_slot_1_*`) is untouched.
- No legacy save: three empty slots.
- Present but unreadable: **not dropped**. `slots_migration_pending`, a "could not be read. It has
  not been deleted." dialog with Retry. Retried on every launch until it works.
- Re-running is a no-op. Deleting slot 1 never resurrects it from the backup.

## Flows

- **Continue** (menu): last-used slot. **Continue** (picker): set last-used, same path.
- **New Story / Dungeon Crawl** open the picker; **Begin** on an empty card. All three lit: "Delete
  one to begin again." `beginSoloOrCrawl` only ever writes an empty slot.
- **Delete** (picker) and **Abandon** (Settings, solo) use the named confirmation and clear only that
  slot (blob, hero prefs, snapshots, keys, pity, records, playtime, spoils, Ledger). Last-used moves to
  the most recently played remaining lantern.
- **Return to menu**: unchanged, saves the active slot.
- **Nightmare wipe**: clears only the active slot's blob; Begin clears any leftovers before a new hero.
- **Failed / empty load**: the save is kept, the slot is marked, Retry / Delete offered.

## Crash guard

Armed only for a solo session, recording `crash_guard_slot`. After an unclean exit
(`SaveSlots.recoverFromCrash`): the guarded slot's save still loads → kept; else that slot's
endgame snapshot loads → restored; else the slot is marked broken (never deleted). Other slots
are never touched. Online never arms it.

## Host / Join

No slot binding. Online sessions never write a slot (`writeSoloSave` is a no-op online), never arm
the crash guard, never grant keys, memories, daily spoils, pity or records, and never set a slot's
story flag. The Host / Join name prompt may still pre-fill the last-used hero's name.

## Tests

- `SaveSlotsTest` (JVM): legacy → slot 1 intact (Ledger, settings, backup untouched), re-run no-op,
  old formats / Game Over / mid-raid, empty install, unreadable legacy kept, Delete / Abandon
  isolation, crash-guard isolation + snapshot restore, Host / Join writes nothing, per-slot keys
  with a shared daily cap, spoils stay with the claiming slot, per-slot story flag / pity / bests.
- `tools/migration_test/slots_test.cpp`: native `describeSave` for v2.23–v2.27 and pre-v2.23
  saves, Game Over / story-complete / crawl flags, broken blobs rejected, live game untouched.
