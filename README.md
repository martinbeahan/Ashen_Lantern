# Ashen Lantern

**Ashen Lantern** is an Android solo / tabletop-style fantasy RPG — story adventures, dungeon crawl, and post-story endgame — built with native C++ combat and a Kotlin UI shell.

Compatible with **5e SRD** concepts only. **Not** an official Dungeons & Dragons product.

## What's new (v2.17 / 56)

- **Short Rest gate** — after a clear, Short Rest works **once per rest opportunity** (between encounters). Button disables / shows "Rested" with a toast if you try again
- **Long Rest** — new option beside Short Rest (Rest chooser): full HP restore and full special/supply resources; same once-per-clear gate
- **Wayfarer's Tonic** (daily-quest potion) — inventory shows a plain-English effect and a **Use** button that heals and refreshes one special/supply, then consumes the tonic
- Host/Join still isolated from solo Continue saves

## Previous (v2.16 / 55)

- **Sideload / install docs** — do **not** share Android Studio **debug** APKs (`android:testOnly=true` → `INSTALL_FAILED_TEST_ONLY`); build a **signed release** APK instead
- ProGuard/R8 keep rules ready for Game Activity, JNI, Firebase, Kotlin (minify still **off**)
- Release packaging: universal APK (ABI/density splits off); no `abiFilters` stripping native libs
- Reminder: **minSdk 30** (Android 11+); uninstall old signature before reinstall; capture `adb install` error text

## Previous (v2.15 / 54)

- **Act 4 — Ashwake Vigil** — original story after Emberdeep: Greyfen Marsh, Vigil Ember, Ashwake Herald climax; story-complete / endgame gate moves to Vigil Kindled
- Continue saves preserved (no wipe); Host/Join still isolated from solo `save_state`
- Builds atop Endless Deep / Ashen Deep (v2.14) when that lands — **Merge after #65**

## Features

- **Story adventure** — original scripted acts (Act 1 *Ashen Lantern*, Act 2 *Millhollow's Debt*, Act 3 *Emberdeep Breach*, Act 4 *Ashwake Vigil*), with Continue mid-quest
- **Dungeon Crawl** — procedural rooms from the start (skip the scripted quest)
- **Difficulty** — Easy / Medium / Hard / Nightmare (locked for the run; wipe rules differ by tier)
- **Classes** — Fighter, Wizard, Rogue, Cleric, and **Bard** (CHA-based; Cutting Quip special)
- **Companion** — AI or player-controlled ally; change class in-run where supported
- **Inventory & loot** — rarity tiers, class-tagged gear, shop, sell, gold upgrades; early bosses (Goblin King, Skeleton King, Ashen Drake)
- **Boss Raids** — story-complete endgame mode; spends a Raid Key; focused boss fight with strong rewards (Legendary chance)
- **Daily Quests** — local-midnight errands for story-complete Continue heroes (gold / tonic / optional Raid Key)
- **Challenge Dungeon** — story-complete *Cinder Crucible* vault (no Raid Key cost; dynamic Legendary chance)
- **Arena (Ember Ring)** — see Endgame modes below
- **Endless Deep (Ashen Deep)** — see Endgame modes below
- **Online multiplayer** — **Host online (DM)** / **Join session** via Firebase Realtime Database (host acts as DM)
- **Settings** — music on/off, sound effects on/off, reduce flashing (photosensitive-friendly), tutorial, return to main menu, About
- **Audio beds** — biome / combat / boss loops (CC0); optional DM voice via Android Text-to-Speech

## Endgame modes (story-complete)

Unlocked after Acts 1–4 with a livable **Continue** save. Same hero, gear, and gold — Host/Join never overwrite the solo save. **No D&D / WotC module text**; SRD-safe monsters only.

| Mode | Venue | Shape | Key cost | Legendary |
|------|-------|-------|----------|-----------|
| **Boss Raid** | Apex hunt | One focused boss fight | Yes (Raid Key) | Flat **7%** on clear |
| **Challenge Dungeon** | *Cinder Crucible* | One vault apex + adds | No | **Dynamic** 18%± (floor 5%, cap 28%) |
| **Arena** | *Ember Ring* | **5 scored waves**; Onward between waves | No | Waves 1–4 Rare/Epic only; **Wave 5** apex flat **7%** |
| **Endless Deep** | *Ashen Deep* | **Endless** depth-scaling rooms | No | Apex every **10** depths, flat **7%** |

### Arena — Ember Ring
Wave-based scored fights on your Continue hero. Clear a wave → **Onward** for the next; Wave 5 is an endgame apex. Soft end on wipe (Easy–Hard Continue restores the hero; Nightmare restores the pre-Arena snapshot). Meta prefs track **best score**, **best waves**, and **runs**. Details: [`docs/ARENA.md`](docs/ARENA.md).

### Endless Deep — Ashen Deep
Distinct from normal Dungeon Crawl: depth rises forever; foe density/HP scale with depth. Soft end via **death**, Settings **Retreat**, or **milestone** (every 5 depths) — spoils stay on the Continue hero. Rewards (gold/XP/loot luck) scale with depth. Meta prefs track **best depth**. Details: [`docs/ENDLESS_DEEP.md`](docs/ENDLESS_DEEP.md).

## Build & run

### Requirements

- [Android Studio](https://developer.android.com/studio) (recent stable)
- **JDK 17** (bundled with Android Studio is fine)
- **SDK Platform 35**, **minSdk 30** (Android **11+** only — older phones get `INSTALL_FAILED_OLDER_SDK`)
- **NDK** `28.2.13676358` and **CMake** (SDK Manager → SDK Tools → Show Package Details)
- Device or emulator **API 30+**

### Steps

1. Clone this repository and open it in Android Studio.
2. **File → Sync Project with Gradle Files**.
3. Install NDK / CMake if prompted (see version above).
4. **Build → Clean Project**, then **Rebuild Project** (important after native C++ changes).
5. Run on an emulator or physical device.

More detail: [`BUILD_HELP.md`](BUILD_HELP.md).

### Share a sideloadable APK (testers)

**Primary install-fail cause:** Android Studio **Run/Debug** packages a **debug** APK with `android:testOnly="true"`. Sharing that file (often from `app/build/outputs/apk/debug/`) makes normal sideload / “Open APK” install fail with **`INSTALL_FAILED_TEST_ONLY`**. That is **not** a ProGuard issue — release `isMinifyEnabled` is currently **false**.

**Do this instead — build a release APK:**

1. **Android Studio:** **Build → Generate Signed Bundle / APK…** → choose **APK** (not App Bundle) → create or pick a keystore → **release** → finish.
2. **Or CLI** (from the project root, after a successful sync):
   ```bash
   ./gradlew assembleRelease
   ```
   Output: `app/build/outputs/apk/release/`. Sign it if your local release build is unsigned (Studio’s **Generate Signed APK** wizard is the easiest path for testers).
3. Send the **release** `.apk` (one **universal** APK — ABI/density splits are off so phones get all native ABIs).

**Before the tester installs:**

- Phone must be **Android 11+** (`minSdk 30`).
- If an older Ashen Lantern build was installed with a **different signing key** (e.g. Studio debug vs your release keystore), uninstall the old app first — otherwise you get `INSTALL_FAILED_UPDATE_INCOMPATIBLE`.
- Prefer the release APK above; do **not** hand out Studio debug APKs.

**If install still fails — collect the error string:**

```bash
adb install -r path/to/ashen-lantern-release.apk
```

Copy the full failure line (e.g. `INSTALL_FAILED_TEST_ONLY`, `INSTALL_FAILED_OLDER_SDK`, `INSTALL_FAILED_UPDATE_INCOMPATIBLE`, `INSTALL_FAILED_NO_MATCHING_ABIS`) and send it back. That string is the fastest way to diagnose.

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
| [`docs/ACT4_ASHWAKE_VIGIL.md`](docs/ACT4_ASHWAKE_VIGIL.md) | Act 4 Ashwake Vigil |
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
