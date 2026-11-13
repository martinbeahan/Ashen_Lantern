# The Memory Ledger (v2.26 / 65)

PR B of `kindling_acts2-4_proposal.md` (sections 6, 7 and 10). These are Martin's approved answers:
- The Ledger is **per hero** and clears on New Game / Abandon.
- It is **lore only**: no set bonus and no stat effect.
- Existing saves get the six Act 1 memories plus every story memory their progress has already earned.
- The store lives in **meta prefs** and never in a save or a Host/Join snapshot.
- Uncollected cards spoil nothing.

## Pieces

| Where | What |
|---|---|
| `cpp/MemoryCatalog.h` | The 47 memories: id (sacred), title, speaker, place, body, Wren remark, source, ribbon, grant beat/trigger, and Venn/Well body variants |
| `cpp/Game.cpp` | Grant rules: `memoryOn*` hooks, `progressStoryMemories()`, static `previewStoryMemories(save)`, plus a grant queue that is never saved |
| `cpp/main.cpp` | JNI: `setMemoryLedgerContext`, `needsMemoryContext`, `drainMemoryGrants`, `getProgressStoryMemories`, `previewStoryMemories`, `getMemoryCatalog` |
| `MemoryLedger.kt` | `MemoryLedger.Store(prefs, slot)`: the per-slot meta-pref store, plus catalogue and id parsing |
| `MemoryLedgerScreen.kt`, `dialog_memory_ledger.xml` | The overlay: ribbon tabs, source chips, collected and locked cards, footer |
| `ic_memory_ember.xml`, `ic_memory_unkindled.xml` | The lit flame and the hollow flame (paths only) |

## Store keys (slot-ready)

There is one Continue save today, and it is **slot 1**. Every key is `ledger_slot_1_<name>`:

| Key | Holds |
|---|---|
| `owner` | The save's session id. A different hero in the slot means a fresh Ledger and a new migration |
| `ids` | Collected ids, in the order they were granted |
| `unread` | Ids that still show **NEW** |
| `crawl_dry` | Lamplighter's Rounds pity counter |
| `choices` | `venn,well` from `getStoryChoices()`. It picks the choice-coloured bodies, even at the main menu |
| `migrated_v1` | Set once the save's existing progress has been credited |

A later save-slot PR builds `MemoryLedger.Store(prefs, n)` for slot *n*, with no Ledger migration. New Game / Abandon clears only its own slot's keys.

## Grants

Native code decides what is earned; Kotlin drains the queue on every UI tick.
- **Solo:** new ids are added to the store and toasted once.
- **Online or a Lanternkeeper table:** the queue is drained and dropped, and nothing is written.

Native code skips grants on a DM table. Host (`startDmSession`), Join (`prepareClientJoin`) and every load (`deserialize`) also reset the grant queue and the "owned" context, so a joined snapshot never grants anything.

**Story (23).** Granted when entering the beat, clearing its fight, or Searching, as listed in proposal §6.6:
- Act-resolution safety covers The Next Entry (beat 13), The First Oath (beat 19) and The Siphon Prayer (beat 25).
- The Tithe Sermon (beat 8) and Down the Well (beat 14) are Search-only, so they can be missed.
- The Well's Judgment is granted when the well choice is made, or defaulted when Onward is pressed without one, so its body always matches the choice.

**Lamplighter's Rounds (8).**
- Eligible in procedural chambers once Act 1 is done: post-story rooms and Crawl, but not raids, Arena, Deep or Challenge.
- Odds are 18% on a Search and 8% on a clear.
- After 8 dry cleared chambers, the next Search or clear is guaranteed a memory.
- Once the pool is empty, a Search only gives a flavour line.

**Ashen Deep (4).** Reaching depth 5, 10, 20 or 30 grants Maren's memories in reverse order.

**Ember Ring (3).** Each Wave 5 clear frees one uncollected caged soul.

**Drowned Lantern raids (9).** A raid boss's first defeat grants that town's three memories. Repeat clears don't toast again.

Pool grants (Rounds and Ring) wait until Kotlin has pushed the hero's collected ids, so nothing is picked twice.

## Migration

The app first credits a save at the main menu, using `previewStoryMemories(save)`, which parses into a scratch game so the live game is untouched. On a solo Continue it credits again with `getProgressStoryMemories()`. Both are silent, and both only add memories.

A save is credited with:
- every guaranteed memory **behind** the current beat or in a completed act;
- the current beat's enter memory, and its clear memory if the room is clear;
- Search memories whose item flag is set (rune-key, ledger, seal, ember);
- the Act 1 knife whenever Act 1 is done, per the approved "six Act 1 memories";
- The Well's Judgment once a well choice exists.

Saves that finished Act 2 or Act 3 before v2.25 default to Venn **ended** and well **sealed in** (from v2.25), so their cards read that way. Rounds, Deep, Ring and Raid memories are not granted retroactively. Journal and chat are left alone.

Tests:
- `tools/migration_test/memory_test.cpp`: pre-v2.23, v2.23, v2.24 and v2.25 saves at beats 7, 11, 13, 16 (mid-fight and cleared), 20 and 25; a crawl hero; choice bodies; live grants; no double grants; the save format unchanged; Rounds pity; Host/Join never grant.
- `MemoryLedgerStoreTest`: slot-1 keys only; silent one-time migration; New Game / Abandon clear only slot 1; another hero in the slot; Host/Join prefs don't collide.

## UI

The entry is a **Memory Ledger** button under Daily Quests on the main menu, with "N of 47 kindled" under it.
- It is locked with "Finish the Kindling to open the glass." until the hero has a memory. Beat 1 grants one, and a migrated Act 1 save opens straight away.
- In-run it is in Settings, for solo play only.

The overlay is a soot panel (`bg_menu_panel`) on the scrim, with Cinzel titles and Cormorant italic body text, and no Gloam teal. Cards:
- **Collected:** amber bar, lit flame, title, and speaker · place. Tap to expand into the body and "Wren, from the glass".
- **NEW** shows until a card is opened.
- **Locked:** dashed ash outline and hollow flame, with only "A voice not yet kindled" and the category.

The grant toast fades in, or appears with no animation when *Reduce flashing* is on. The overlay itself has no animation.

## Phone checklist

1. **Old save:** with a v2.25 save mid-Act 2 or later, open the main menu. The Ledger button is unlocked and shows the right count. In the overlay, Act 1 has six cards; Aunt Lise appears if you're past the cellar; the missable Search memories (The Tithe Sermon, Down the Well) are not credited retroactively.
2. **Old Act 2/3-complete save:** The Exile's Father ends "…did not leave again", and The Well's Judgment reads "burns bright".
3. **New Game:** the Ledger is locked. Clear beat 1 and "A soul settles in the glass — The Kindling Night" toasts. Tap the card, and NEW disappears.
4. **Choices:** spare Venn, then Let them rest. The cards follow the choices, and Old Sallow mentions sparing him (Ember Ring).
5. **Locked cards** show only "A voice not yet kindled" and a category, never a title.
6. **Abandon** empties the Ledger. **Return to Menu → Continue** keeps it.
7. **Host and Join:** no grants and no Settings button. Afterwards, the solo Continue and its Ledger are unchanged.
8. **Reduce flashing on:** the grant toast appears with no fade.
9. **Endless Deep depth 5:** "I Will Not Go Out". **Raid first clear:** the town's three cards, with no re-toast on a repeat.
