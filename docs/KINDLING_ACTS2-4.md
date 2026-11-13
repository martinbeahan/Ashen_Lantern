# The Kindling — Acts 2–4 rewrite (v2.25 / 64)

Implements PR A of `kindling_acts2-4_proposal.md` (Martin-approved, all 12 section-12
recommendations). The Memory Ledger is PR B (v2.26) and is **not** in this release.

## What changed

| | Before | v2.25 |
|---|---|---|
| Act 2 title | Millhollow's Debt | **The Tithe** |
| Act 3 title | Emberdeep Breach | **Emberdeep** |
| Act 4 title | Ashwake Vigil | Ashwake Vigil |
| Beat 13 label | Debt Settled | **The Tithe Broken** (id `ACT2_SETTLED` = 13 unchanged) |
| Beat 25 stage | `millhollow_green` | **`kindling_pyre`** |
| Wren | one line at the shrine | **`[Lantern]`** chat lines from the Ashen Shrine on (beat enters, clears, Searches, choices) |

Room text, Lanternkeeper lines, journal lines, Search clues, act-resolution speeches and the
Onward / endgame speech for beats 8–25 come from `QuestScript.h` (`kTitheBeats`,
`kEmberdeepBeats`, `kAshwakeVigilBeats`; new optional `wrenOnEnter` / `wrenOnClear` fields) and
`Game.cpp`. Beat ids 8–25, enemy ids, spawns, HP tuning, Search flags and the 40 / 60 / 80 gold
rewards are unchanged.

- **Bell-Herald** (beat 24): "The bell tolls. The glass goes quiet." on alternate Herald turns —
  combat-log flavour only, no stun or lost turn.
- **Maren Vey** unmasks in text only at beat 25 — no new sprite, no fight.
- Mother stays "Mother".

## Choices

| Beat | Prompt | Options (value) | Effect |
|---|---|---|---|
| 13 The Tithe Broken | Sallow Venn is on his knees | **Spare him** (1) / **End it** (2) | Story text + an Act 4 epilogue line if spared (hook for Old Sallow later) |
| 19 Breach Sealed | The Well's Judgment | **Seal them in** (1) / **Let them rest** (2) | Story text + Act 4 epilogue (bright vs dimmer Lantern). No combat change |

- Native decides when a prompt is pending (`Game::pendingStoryChoice`: solo **Story** mode, on the
  beat, act complete, undecided, no foes, not a Lanternkeeper table). Kotlin only shows the
  themed two-button `AlertDialog` (app `alertDialogTheme`; no global `buttonStyle`), not cancelable.
- Return to Menu before answering → the undecided value is saved → Continue asks again.
- Pressing Onward while undecided (e.g. an older client) keeps the old script's ending (Venn ended /
  sealed in).

## Save format

The choices are header fields **35** (`vennChoice`) and **36** (`wellChoice`), written **after** the
v2.23 `E2` marker, which stays at index 34 — so v2.23 / v2.24 readers (and Kotlin's
`TextSafety.isEncodedSave`) still detect encoding and ignore the new fields. Values: 0 undecided;
Venn 1 spared / 2 ended; well 1 sealed in / 2 let rest. They travel in the solo `save_state` (and in
the pre-raid / arena / deep snapshots, which are the same string); Host/Join never writes the solo
slot. PR B (Memory Ledger) reads them via `getStoryChoices()` → `"venn,well"`.

**Older saves** (no field 35/36): `questAct2Complete` → Venn **ended**; `questAct3Complete` → well
**sealed in**; never prompted. An act still in progress stays undecided and asks on its resolution
beat.

**Room text on load:** after a solo Continue, `refreshStoryRoomAfterLoad()` replaces the stored room
description of the current scripted beat with the v2.25 text (keeping the "The foes lie still…"
suffix if the room was already cleared). Journal, chat and the last event are left as history.
`deserialize()` itself does not rewrite text (Host/Join sync is untouched).

Tests: `tools/migration_test/kindling_test.cpp` (old v2.24 and pre-v2.23 saves mid-Act 2/3/4,
post-Act-2 Venn default, post-Act-3 well default, prompt / round-trip / Onward-default, epilogue,
no D&D names), `StageMapTest` (beat 25 stage). Run `tools/migration_test/run.sh`.

## Phone checklist

1. **Old mid-act save:** with a v2.24 Continue save in Act 2, 3 or 4, install v2.25 → Continue: same
   room, new room text (no grain debt / twin brazier), old journal lines still there.
2. **Old post-Act-2 save** on The Tithe Broken or later: no Venn prompt.
3. **Beat 13:** HUD reads *The Tithe Broken*; the Venn dialog appears; try **Spare him** on one run
   and **End it** on another; back out to Menu before choosing → Continue asks again.
4. **Beat 19:** the Well dialog; **Seal them in** vs **Let them rest** → different Wren line now and a
   different Act 4 epilogue line at Vigil Kindled.
5. **Beat 25:** Kindling Pyre background (HD on a tablet); last lines end on "Follow the ash."
6. **[Lantern]** lines in chat after the shrine and through Acts 2–4 (enter, clears, Searches);
   Bell-Herald fight shows "The bell tolls. The glass goes quiet." with no lost turns.
7. Host/Join: no Venn / Well dialog on a joiner; the solo save is untouched.
