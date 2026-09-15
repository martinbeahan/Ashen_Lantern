# Ashen Lantern

**Ashen Lantern** is an Android solo / tabletop-style fantasy RPG — story adventures, dungeon crawl, and post-story endgame — built with native C++ combat and a Kotlin UI shell.

Compatible with **5e SRD** concepts only. **Not** an official Dungeons & Dragons product.

## Features

- **Story adventure** — original scripted acts (Act 1 *Ashen Lantern*, Act 2 *Millhollow's Debt*, Act 3 *Emberdeep Breach*), with Continue mid-quest
- **Dungeon Crawl** — procedural rooms from the start (skip the scripted quest)
- **Difficulty** — Easy / Medium / Hard / Nightmare (locked for the run; wipe rules differ by tier)
- **Classes** — Fighter, Wizard, Rogue, Cleric, and **Bard** (CHA-based; Cutting Quip special)
- **Companion** — AI or player-controlled ally; change class in-run where supported
- **Inventory & loot** — rarity tiers, class-tagged gear, shop, sell, gold upgrades; early bosses (Goblin King, Skeleton King, Ashen Drake)
- **Boss Raids** — story-complete endgame mode; spends a Raid Key; focused boss fight with strong rewards (Legendary chance)
- **Daily Quests** — local-midnight errands for story-complete Continue heroes (gold / tonic / optional Raid Key)
- **Challenge Dungeon** — story-complete *Cinder Crucible* vault (no Raid Key cost; dynamic Legendary chance)
- **Arena (Ember Ring)** — story-complete 5-wave scored fights (Legendary on Wave 5 apex at 7%)
- **Endless Deep (Ashen Deep)** — story-complete endless depth-scaling rooms; soft end via death / retreat / milestone; Legendary on apex depths 10/20/… at 7%
- **Online multiplayer** — **Host online (DM)** / **Join session** via Firebase Realtime Database (host acts as DM)
- **Settings** — music on/off, sound effects on/off, reduce flashing (photosensitive-friendly), tutorial, return to main menu, About
- **Audio beds** — biome / combat / boss loops (CC0); optional DM voice via Android Text-to-Speech

## Build & run

### Requirements

- [Android Studio](https://developer.android.com/studio) (recent stable)
- **JDK 17** (bundled with Android Studio is fine)
- **SDK Platform 35**, **minSdk 30**
- **NDK** `28.2.13676358` and **CMake** (SDK Manager → SDK Tools → Show Package Details)
- Device or emulator **API 30+**

### Steps

1. Clone this repository and open it in Android Studio.
2. **File → Sync Project with Gradle Files**.
3. Install NDK / CMake if prompted (see version above).
4. **Build → Clean Project**, then **Rebuild Project** (important after native C++ changes).
5. Run on an emulator or physical device.

More detail: [`BUILD_HELP.md`](BUILD_HELP.md).

### Firebase / `google-services.json` (local secret)

Online **Host** / **Join** needs Firebase Realtime Database.

- The Google Services Gradle plugin is **enabled** in the project.
- **`app/google-services.json` is not in this repository** — it is a local secret. Download it from your own Firebase project and place it at `app/google-services.json`.
- Without that file, solo play still works; Host / Join will report that Firebase is not configured.

See [`FIREBASE_SETUP.md`](FIREBASE_SETUP.md).

## Docs & legal

| Document | Purpose |
|----------|---------|
| [`ATTRIBUTION.md`](ATTRIBUTION.md) | Trademarks notice, SRD credit, art & audio licenses |
| [`LICENSE`](LICENSE) | MIT (source code & original adventure text) |
| [`docs/STORE_LISTING_LEGAL.md`](docs/STORE_LISTING_LEGAL.md) | Paste-ready Play Store legal blurb |
| [`docs/ASHEN_LANTERN.md`](docs/ASHEN_LANTERN.md) | Act 1 playtest notes |
| [`docs/ACT2_CRAWL_DIFFICULTY.md`](docs/ACT2_CRAWL_DIFFICULTY.md) | Act 2, crawl, difficulty |
| [`docs/ENDGAME_ACT3_RAIDS.md`](docs/ENDGAME_ACT3_RAIDS.md) | Act 3, Boss Raids, Legendary rules |
| [`docs/DAILY_QUESTS.md`](docs/DAILY_QUESTS.md) | Daily Quests |
| [`docs/CHALLENGE_DUNGEON.md`](docs/CHALLENGE_DUNGEON.md) | Challenge Dungeon / Cinder Crucible |
| [`docs/ARENA.md`](docs/ARENA.md) | Arena / Ember Ring |
| [`docs/ENDLESS_DEEP.md`](docs/ENDLESS_DEEP.md) | Endless Deep / Ashen Deep |
| [`BUILD_HELP.md`](BUILD_HELP.md) | Build troubleshooting |
| [`FIREBASE_SETUP.md`](FIREBASE_SETUP.md) | Online multiplayer setup |

## Legal / trademarks

**Ashen Lantern** is an independent tabletop fantasy adventure app by Martin Beahan.

This app is **not affiliated with, sponsored by, endorsed by, or approved by** Wizards of the Coast LLC.

**Dungeons & Dragons**, **D&D**, and related marks are trademarks of Wizards of the Coast LLC.

Game rules and class/monster concepts are compatible with **5e SRD** concepts only, based on the **System Reference Document 5.1** (SRD 5.1), available under the [Creative Commons Attribution 4.0 International License](https://creativecommons.org/licenses/by/4.0/).

Original adventure text (including Solo quests such as *Ashen Lantern*) is original and not copied from any Wizards of the Coast adventure module.

Third-party art and audio remain under their own licenses — see [`ATTRIBUTION.md`](ATTRIBUTION.md). Application source code is MIT-licensed — see [`LICENSE`](LICENSE).

## Package

- Application ID: `com.fintrack.dndbeginnerremote`
- Repository folder name (`DnD_beginner_remote`) is historical; the product name is **Ashen Lantern**.
