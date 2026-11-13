# Ashen Lantern

**Ashen Lantern** is an Android solo / tabletop-style fantasy RPG — story adventures, dungeon crawl, and post-story endgame — built with native C++ combat and a Kotlin UI shell.

Compatible with **5e SRD** concepts only. **Not** an official Dungeons & Dragons product.

> **3D / Unreal:** planning only — see [`docs/UNREAL_V3D_PLAN.md`](docs/UNREAL_V3D_PLAN.md). This Android repo stays the **2D production / tester** track; proposed UE work lives in a separate repo (**Ashen_Lantern_UE**).

## What's new (v2.28 / 67) — Save slots

- **Three adventures** — keep up to three heroes at once, each in its own *lantern*. The new **Adventures** screen (main menu, or Settings) shows each hero's class, level, where they are in the story or crawl, when you last played and total playtime. The last one you played is marked **LAST**
- **Continue** on the main menu still loads the last hero you played. **New Story** and **Dungeon Crawl** now open Adventures and start in an empty lantern. They **never overwrite** a hero; if all three are lit you're asked to delete one first
- **Delete / Abandon** names the hero and asks first ("This will end Mira's adventure…"). It clears only that hero: save, gear, gold, memories, keys and records. Your other heroes and your settings stay
- **Per hero:** held Raid Keys, Cinder Crucible Legendary chance, Ember Ring / Ashen Deep bests, the Memory Ledger and the story-complete unlock. A new hero no longer inherits another hero's finished story. **Shared:** settings, the daily Raid Key grant cap (3 a day across all heroes) and Daily Quests. Daily spoils go to the hero you claimed them on
- **Safer saves** — if the game closes unexpectedly, the save is checked instead of wiped. A broken mid-raid (or Crucible / Ring / Deep) run falls back to the hero from before the run. A save that won't load is kept and marked, with **Retry** or **Delete**; nothing is deleted without asking, and the other heroes are never touched
- **Your current hero moves into lantern 1** automatically on first launch, with gear, gold, journal, keys, records and Ledger intact. Playtime starts at 0. The old single save is kept as a backup for one release
- Host / Join are unchanged and never read, write or delete any lantern. No change to the save format

## Previous (v2.27 / 66) — Gear icons

- **Every item has an icon** — inventory and shop rows now show a hand-drawn *Ash & Ember* glyph for the item in a larger 40dp socket (was 26dp). 18 glyphs cover every item in the game: sword, short blade, rapier, staff, wand, mace, warhammer, maul, leathers, robes/cloaks, mail, plate, coat, healing draught, tonic, Raid Key, gold, and unique art for the **Ashen Lantern**
- **Rarity at a glance** — the socket frame takes the rarity colour (Common ash, Uncommon, Rare, Epic, Legendary amber) and the row keeps its rarity pip (hollow dot, 1–3 diamonds, flame), so colour isn't the only cue. Legendary gear adds an amber inlay on the item and a soft static glow; with *Reduce flashing* on, the glow is dropped and the inlay stays
- **Potion button** — the combat Potion button shows the draught flask instead of an emoji, dimmed when you can't drink
- **Raid Key and gold** — the Raid Key hint on the main menu and in Settings shows a key once raids are unlocked, and the inventory header shows a gold coin
- Greater Potion shares the draught glyph (its frame shows the rarity). Item names are unchanged. Hero gear is amber, never teal. Icons are picked from the item name, so old saves show them straight away; no save change. Host/Join unchanged

## Previous (v2.26 / 65) — The Memory Ledger

- **Memory Ledger** — open it from the main menu (under Daily Quests) or from Settings during a solo run. It holds 47 Ember Memories in three ribbons, **Wren**, **Maren** and **The Lantern**, with source filters (Story / Rounds / Deep / Ring / Raids). Tap a card to read it and Wren's remark. New cards are marked **NEW** until you open them. Uncollected cards say only "A voice not yet kindled" and where they're found, so nothing is spoiled
- **Where memories come from:**
  - **Story:** 23 across Acts 1–4. Most can't be missed; a few need a Search
  - **Lamplighter's Rounds:** 8 Millhollow voices, once Wren is in the glass. A Search or a clear sometimes frees one, and one is guaranteed after 8 dry chambers
  - **Ashen Deep:** 4 of Maren's memories at depths 5, 10, 20 and 30, told in reverse
  - **Ember Ring:** 3 caged souls, from Wave 5 clears
  - **Drowned Lantern raids:** 3 per town, on its first clear
- **Your choices show** — Venn's and the Well's cards (and Old Sallow's in the Ember Ring) follow what you chose at The Tithe Broken and Breach Sealed
- **Existing heroes** — on the first Continue (or menu visit) your save is credited with the six Act 1 memories plus every story memory your progress has already earned. Saves that finished Act 2 or Act 3 before v2.25 count as Venn ended / sealed in. Rounds, Deep, Ring and Raid memories are earned from now on
- **Per hero** — New Game or Abandon starts an empty Ledger. Return to Menu, Raids, the Ring and the Deep keep it. The Ledger lives in app settings for the save slot (today's one save is slot 1, ready for save slots later), never inside the save or a Host/Join session. Host/Join never reads, grants or wipes it
- Lore only: no stats, gear or set bonus. The grant toast is static when *Reduce flashing* is on

## Previous (v2.25 / 64) — The Kindling, Acts 2–4

- **Acts 2–4 rewritten** — the rest of the story now tells *The Kindling*. Same rooms, foes and rewards; new story:
  - **Act 2 — The Tithe** (was *Millhollow's Debt*): the grey collectors were never after grain. The village owes souls, the elders have been choosing who, and your name is next in the ledger. Beat 13 is now called **The Tithe Broken**
  - **Act 3 — Emberdeep** (was *Emberdeep Breach*): under the old well lie the village's own dead. The Breach Warden is the First Lamplighter, your ancestor, and he names **Maren Vey**
  - **Act 4 — Ashwake Vigil**: the Vigil Spire is a siphon, Greyfen was a town, and at the end the Chandler unmasks, takes a piece of the Lantern, and walks down into the Ashen Deep. *Follow the ash.* Vigil Kindled now plays on the kindling pyre stage
- **Wren speaks** — from the Ashen Shrine on, Wren comments from inside the Lantern in chat, tagged **[Lantern]**. The Bell-Herald's toll hushes her (flavour only)
- **Two choices** — at The Tithe Broken: **Spare him** or **End it** (Sallow Venn). At Breach Sealed: **Seal them in** (the Lantern stays bright) or **Let them rest** (a dimmer Lantern). They change story text and the Act 4 epilogue, not combat. Back out to the menu before choosing and Continue asks again
- **Old saves keep their place** — a mid-Act 2, 3 or 4 Continue stays on the same room and shows the new room text. Your journal and chat stay as they were. Saves that already finished Act 2 count Venn as ended, and saves that already finished Act 3 count the ash-dead as sealed in. Neither is asked again
- Unchanged: Long Rest every 5 rooms, Short Rest once per clear, duplicate foes selectable by slot (`#2`), Host/Join never touches the solo save. No new art. The Memory Ledger comes in the next update

## Previous (v2.24 / 63) — Android 16 & play readiness

- **Ready for Android 16** — the app now targets Android 16 (API 36) with the current build tools (Android Gradle Plugin 8.13, Gradle 8.13), so it meets Google Play's upcoming target-API requirement. Details: [`docs/ANDROID16.md`](docs/ANDROID16.md)
- **Full-screen that behaves** — the game hides the status and navigation bars with the modern Android API; swipe from an edge to peek at them. On phones with a camera notch or punch-hole, and when the keyboard is open for chat, the screen now stays clear of them
- **Smaller, tidier release build** — release APKs are now code-shrunk (R8), and the unused legacy textures from the old graphics layer were removed
- **Ability modifiers fixed** — odd scores below 10 now round down like the tabletop rules (9 → −1, 7 → −2); previously they rounded toward zero. Starting heroes use even scores, so most characters see no change
- **Stability** — safer graphics shutdown, no crash on a zero-height window during resize, a capped animation queue, clearer shader error logs, and damaged loot entries are logged instead of silently ignored
- **Under the hood** — the native code now builds with no compiler warnings, save/sync code has its own file with one shared reader/writer for heroes and foes, loot tables are named data instead of magic numbers (same drop rates), the graphics layer looks up its shader settings once instead of every frame, and the sprite tables moved out of the main screen code. No gameplay change
- Unchanged: Long Rest every 5 rooms, Short Rest once per clear, duplicate foes selectable by slot (`#2`), old saves load fine, Host/Join never touches the solo save

## Previous (v2.23 / 62) — save & online safety fixes

- **Names with commas no longer break your save** — a hero called *Bob, Jr* (or any name or chat line with `, ; | ~`) used to wipe the party on Continue or corrupt an online table. Save and sync text is now encoded safely, and old saves still load
- **Cleaner names** — hero, Lanternkeeper and joining-player names skip the few separator characters (`, ; | ~ ^ : %`) and are capped at 24 characters
- **Chat can't erase the adventure log** — a `|` in chat no longer overwrites the journal
- **Broken saves fail safely** — a damaged save is now rejected as a whole (you get the "Save looked broken" message) instead of loading half a game; a bad online update is ignored and the table keeps its last good state
- **Online joins are checked** — a joiner with an invalid class joins as a Fighter instead of getting garbage stats
- **Fairer dice** — combat, loot and saving-throw rolls use a properly seeded random generator, so each launch no longer replays the same roll sequence
- **Stability** — the native layer checks its inputs and no longer crashes on graphics-setup failures or unexpected errors
- Unchanged: Long Rest every 5 rooms, Short Rest once per clear, duplicate foes selectable by slot (`#2`), Host/Join never touches the solo save. **Online play:** host and joiners should all update to v2.23 together (older versions show encoded names)

## Previous (v2.22 / 61) — Ash & Ember, phase 3

- **New original backgrounds** — 16 hand-authored *Ash & Ember* stages replace the old CC0 battlebacks. The stage now follows the mode and story beat instead of guessing from room text: Kindling Pyre, Thornpath Woods, the Ossuary, Millhollow Green, the Millrace Weir, the Mill Hall, Emberdeep, Greyfen Marsh, the Catacombs (Crawl), Ember Ring (Arena), Cinder Crucible (Challenge), Ashen Deep (Endless) and a stage per Boss Raid. Boss fights add a heavier vignette with a teal glow. Art is centre-cropped (never stretched), and tablets and large phones get sharper 1080×1350 versions
- **Original hero art** — the Fighter, Wizard, Rogue, Cleric and **Bard** have new hand-drawn silhouettes with idle and attack frames, facing the foes. The Bard finally has their own sprite instead of borrowing the Wizard's. With *Reduce flashing* on, the attack frames use calmer glows
- **UI reskin** — soot panels with amber corner ornaments, new buttons (primary, normal and danger, with pressed and disabled states), dark Lanternkeeper narration card, matching dialogs, and *Cinzel* / *Cormorant Garamond* type
- **Wick HP bar** — your HP is a burning candle that melts down and gutters below 30%. Enemy HP is a thin teal soul thread
- **Ember XP** — XP is a row of 10 embers that light up (in half steps) as you earn XP toward the next level
- **Loot rarity markers** — each item row has a rarity stripe, socket and pip (hollow dot, ◆, ◆◆, ◆◆◆, flame), so rarities are easy to tell apart without relying on colour alone. Equipped gear shows an *Equipped* tag
- **Targeting** — the targeted foe stands on a teal ring and your selected hero on an amber ring, and every sprite has a soft backlight so dark silhouettes stay visible on dark floors. Damage floats are styled (crit, heal and miss)
- *Reduce flashing* keeps the new UI still: the rings and backlights don't pulse, and the calm boss and hero art is used
- Unchanged: Long Rest every 5 rooms, Short Rest once per clear, duplicate foes selectable by slot (`#2`), old saves load fine, Host/Join never touches the solo save

## Previous (v2.21 / 60) — Ash & Ember, phase 2

- **Every foe has its own art** — 24 new hand-authored *Ash & Ember* silhouettes complete the bestiary: all six Wickthief variants, Bonebound Ashbound, Gloamhound, Tallow Rat, Millstone Brute, the Grey Tithe (Enforcer, Scavenger, Watcher), both Criers, every story boss (Lantern Sexton, Collector Sallow Venn, First Lamplighter, Bell-Herald, Ossuary Prior), Ashen Drake and the Three Drowned Lanterns (Hollow Crown, Ember Hydra, Nightfang Matriarch). All 27 enemy IDs now resolve to 27 distinct drawables — nothing borrows another foe's sprite, and the old CC0 wolf/ogre/skeleton placeholders are gone
- **Renamed foes** (names only — saves and IDs unchanged): Giant Rat → **Tallow Rat**, Ogre → **Millstone Brute** (set-piece: *Millstone Roadblock*), Debt Enforcer → **Tithe Enforcer**, Ashwake Scout → **Tithe Watcher**, Bone Herald → **Bone Crier**. Old saves (both pre-v2.20 and v2.20) pick up the new names automatically
- **Reduce flashing covers the new bosses** — with *Reduce flashing* on, Ember Hydra's furnace door, the First Lamplighter's chest arch, the Ossuary Prior's censer and the Nightfang Matriarch's caged soul switch to calmer art (amber glow halos at 30%, solid amber at 55%); toggling the setting mid-fight swaps them immediately
- Unchanged: Long Rest every 5 rooms, Short Rest once per clear, duplicate foes selectable by slot (`#2`), Host/Join never touches the solo save

## Previous (v2.20 / 59) — Ash & Ember, phase 1

- **Act 1 rewritten: *The Kindling*** — opens on your mother's funeral ("In Millhollow, we do not bury our dead. We burn them into the light."). Wickthieves steal the Lantern mid-ceremony, the shadows stand up, and your sister **Wren** gives chase; first fight within a minute. Act 1 ends with Wren relighting the Lantern with her own soul, and a masked watcher writing in a ledger
- **New original enemy art** — hand-authored *Ash & Ember* silhouettes (VectorDrawables) for the **Wickthief** (was goblins), **Ashbound** (was skeletons) and **The Wick King** (was Goblin King), animated in code: idle bob/sway, lunge on attack, ash-dissolve on death (Reduce flashing keeps them still and uses a plain fade)
- **Bestiary names** — story bosses are now **The Lantern Sexton** (Act 1), **Collector Sallow Venn** (Act 2), **The First Lamplighter** (Act 3 Breach Warden) and **The Bell-Herald of the Tithe** (Act 4); also **Gloamhound** (was wolf) and **The Ossuary Prior** (was Skeleton King)
- **Every enemy has its own sprite entry** — no more "everything unknown looks like a goblin"; foes without new art yet use a placeholder through their own entry
- **Stable enemy IDs** — sprites, boss tiers, boss-seen flags and saves now key off an ID instead of the enemy's name. **Old Continue saves load fine** (names are migrated to IDs on load)
- **Narrator renamed** — the narrator is now **the Lanternkeeper** (Host online: you play the Lanternkeeper); companion **Melf** is now **Corvin** (existing saves are renamed automatically)
- Unchanged: Long Rest every 5 rooms, Short Rest once per clear, duplicate foes selectable by slot (`#2`), Host/Join never touches the solo save

## Previous (v2.19 / 58)

- **Duplicate foe targeting** — identical enemies are selectable by slot (not name-only); labels show `#2` when names match
- **Gear stat flavor** — inventory, shop, and hero sheet explain Weapon/Armor bonuses (attack & damage vs Armor Class) for beginners; long-press item for the full tip
- **Long Rest in all modes** — 5-room Long Rest cooldown now ticks correctly in Story, Arena, Endless Deep, Boss Raid, and Challenge (not just Crawl); Short Rest still once per clear
- **Readable combat log** — taller scrollable log, slightly smaller type, more lines kept so action text isn't clipped on phones
- **Dice / action pop-ups** — results stay on screen longer (~3s, crits longer); tap the overlay to dismiss early

## Previous (v2.18 / 57)

- **Action Surge fix** — Fighter Action Surge is once per fight (still spends a resource) and can no longer be spammed when out of uses; Special button greys out at 0 resources for all classes (Cutting Quip, Sneak Attack, Magic Missile, Healing Word)
- **Long Rest cooldown** — Long Rest only every **5 rooms** (Short Rest still once per clear); chooser greys the option with remaining rooms + toast when on cooldown
- **Combat anim pass** — LuizMelo idle/attack frames: wind-up → attack lunge → recover, plus gentle idle bob; respects Reduce flashing
- Host/Join still isolated from solo Continue saves

## Previous (v2.17 / 56)

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

- **Story adventure — *The Kindling*** — four original scripted acts (Act 1 *The Kindling*, Act 2 *The Tithe*, Act 3 *Emberdeep*, Act 4 *Ashwake Vigil*), with Continue mid-quest. You are the Lamplighter's child; after Act 1 your sister Wren lives in the Ashen Lantern and speaks in chat as **[Lantern]**. Two story choices — Sallow Venn's fate and what the Ember Seal does to the ash-dead — colour the ending
- **Memory Ledger** — a book of 47 Ember Memories: short testimonies from the souls in the Lantern, with Wren's remarks, sorted into three ribbons (*Wren*, *Maren*, *The Lantern*). They come from story beats, Lamplighter's Rounds, the Ashen Deep, the Ember Ring and the Drowned Lantern raids. Lore only, no stat effects. Each hero has their own Ledger; uncollected cards show only where they are found
- **Ash & Ember art** — original hand-authored silhouettes for all 27 foes (one per stable enemy ID) and all five hero classes, with code-driven idle sway / attack lunge / ash-dissolve animation; 16 original stage backgrounds chosen by mode and story beat; *Reduce flashing* stills them and calms the bright amber boss and hero glows
- **Ash & Ember UI** — soot-and-amber panels, buttons and dialogs, Cinzel / Cormorant Garamond type, a candle-wick HP bar, a 10-ember XP row and rarity pips on loot
- **Gear icons** — 18 original item glyphs (one per kind of gear, unique art for the Ashen Lantern) in rarity-framed 40dp sockets across inventory and shop, plus the Potion button, Raid Key hints and a gold coin. Legendary gear carries an amber inlay; *Reduce flashing* drops its glow
- **Save slots** — three adventures (lanterns) side by side on an *Adventures* screen; Continue loads the last one played, New Story / Crawl never overwrite a hero, and Delete / Abandon clear only that hero. Keys, Crucible pity, Ring / Deep bests, the Ledger and the story unlock are per hero; settings, Daily Quests and the daily Raid Key cap are shared
- **Dungeon Crawl** — procedural rooms from the start (skip the scripted quest)
- **Difficulty** — Easy / Medium / Hard / Nightmare (locked for the run; wipe rules differ by tier)
- **Classes** — Fighter, Wizard, Rogue, Cleric, and **Bard** (CHA-based; Cutting Quip special)
- **Companion** — AI or player-controlled ally; change class in-run where supported
- **Inventory & loot** — rarity tiers, class-tagged gear, shop, sell, gold upgrades; early bosses (The Wick King, The Ossuary Prior, Ashen Drake)
- **Boss Raids** — story-complete endgame mode; spends a Raid Key; focused boss fight with strong rewards (Legendary chance)
- **Daily Quests** — local-midnight errands for story-complete Continue heroes (gold / tonic / optional Raid Key)
- **Challenge Dungeon** — story-complete *Cinder Crucible* vault (no Raid Key cost; dynamic Legendary chance)
- **Arena (Ember Ring)** — see Endgame modes below
- **Endless Deep (Ashen Deep)** — see Endgame modes below
- **Online multiplayer** — **Host online (Lanternkeeper)** / **Join session** via Firebase Realtime Database (the host narrates as the Lanternkeeper)
- **Settings** — music on/off, sound effects on/off, reduce flashing (photosensitive-friendly), tutorial, Adventures (save slots), return to main menu, About
- **Audio beds** — biome / combat / boss loops (CC0); optional Lanternkeeper (narrator) voice via Android Text-to-Speech

## Endgame modes (story-complete)

Unlocked per hero after that hero finishes Acts 1–4, with a livable **Continue** save (the last-used lantern). Same hero, gear, and gold — Host/Join never overwrite any save slot. **No D&D / WotC module text**; SRD-safe monsters only.

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
- **SDK Platform 36** (Android 16), **minSdk 30** (Android **11+** only — older phones get `INSTALL_FAILED_OLDER_SDK`)
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
| [`docs/art/ash_ember/`](docs/art/ash_ember/) | Ash & Ember style sheet + original enemy SVG sources, one per enemy ID, plus `phase3/` (backgrounds + UI kit) and `heroes/` (`tools/svg2vd.py` converts them to VectorDrawables; `--calm` makes the Reduce-flashing variants) |
| [`docs/ASH_EMBER_PHASE2.md`](docs/ASH_EMBER_PHASE2.md) | Phase 2: art import, renames + save migration, Reduce-flashing variants, checks (`tools/check_enemy_sprites.py`, `tools/migration_test/run.sh`) |
| [`docs/ASH_EMBER_PHASE3.md`](docs/ASH_EMBER_PHASE3.md) | Phase 3: backgrounds (mode/beat mapping), UI kit, fonts, hero art, and the device checklist. Design sources live in `docs/art/ash_ember/phase3/` and `docs/art/ash_ember/heroes/` |
| [`assets/licenses/`](app/src/main/assets/licenses/) | SIL Open Font License text for the bundled Cinzel and Cormorant Garamond fonts |
| [`LICENSE`](LICENSE) | MIT (source code & original adventure text) |
| [`docs/STORE_LISTING_LEGAL.md`](docs/STORE_LISTING_LEGAL.md) | Paste-ready Play Store legal blurb |
| [`docs/UNREAL_V3D_PLAN.md`](docs/UNREAL_V3D_PLAN.md) | **3D v2 plan** (Unreal Engine) — desktop slice; 2D app remains production/tester track |
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
