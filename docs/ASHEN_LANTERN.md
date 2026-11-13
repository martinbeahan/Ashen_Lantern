# Ashen Lantern (Solo quest) — playtest notes

**Version:** 1.5 (versionCode 21)  
**Legal:** Original plot/locations/NPCs/dialogue. SRD 5.1 monster/rule concepts only. Compatible with 5e SRD — not an official D&D product. See `ATTRIBUTION.md`.

## Beats (Solo New Game) — Act 1 "The Kindling" (v2.20)

| # | Beat | Content |
|---|------|---------|
| 1 | Millhollow Road | Title card + mother's kindling; Wickthieves steal the Lantern, shadows rise, Wren gives chase; light **Wickthief Scout** (first fight immediately); Search → Wren's ribbon clue |
| 2 | Thornpath Woods | Lantern-glass whispers "Don't let them light me again."; **Gloamhound** |
| 3 | Crypt Doors | Wren's knife in the rune-lock; Search once → Ashen Rune-Key |
| 4 | Bone Gallery | Two **Ashbound** (funeral ribbons); Medium+ rare **Ossuary Prior** |
| 5 | Lantern Vault | **The Lantern Sexton**; Wren collapsed by the plinth; victory → `questLanternRecovered` + Ashen Lantern item |
| 6 | Ashen Shrine | Wren relights the Lantern with her soul (it speaks in her voice); grey-masked watcher writes in a ledger; Act 1 complete |
| 7+ | Post-quest | Existing procedural rooms; “Main quest done” banner |

## Persistence

Save header stores `questBeat`, `questLanternRecovered`, `questComplete`, `questCryptKeyFound`. **Continue** mid-quest restores the current beat.

## Online

Host (Lanternkeeper) / Join paths call `resetSoloQuestState()` — procedural rooms unchanged.

## Device playtest checklist

1. **Build → Clean Project**, then **Rebuild** (native C++ changed: `QuestScript.h`, `Game.cpp` / `Game.h`).
2. Solo **New Game** → confirm the funeral hook and Lanternkeeper lines; first fight within ~60s.
3. Clear scout → **Search** clue → **Onward** through woods → crypt.
4. At Crypt Doors, **Search** for rune-key (one attempt lock still applies).
5. Bone Gallery fight → Vault → defeat champion → lantern recovered message.
6. Shrine: Rest, journal shows quest complete; banner “Main quest done”.
7. Force-stop app mid-quest (e.g. after Thornpath) → **Continue** resumes same beat.
8. Host online table → confirm no Ashen Lantern script (procedural Room 1).

## Graphics (v1.6)

Solo beats use distinct CC0 battle backgrounds (road/woods/crypt/vault/dungeon) plus upgraded party/enemy sprites (including wolf). See `ATTRIBUTION.md`.

## Out of scope

Further 3D/engine rewrites, copying WotC modules, multiplayer campaign sync of quest beats.

## v1.7 note

Act 2 (Millhollow's Debt), Dungeon Crawl mode, and Difficulty wipe rules: see `docs/ACT2_CRAWL_DIFFICULTY.md`.
