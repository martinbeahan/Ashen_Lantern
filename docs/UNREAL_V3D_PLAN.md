# Ashen Lantern — Unreal Engine 3D v2 plan

**Status:** planning only  
**Date:** 2026-09-18  
**This repo (`Ashen_Lantern`):** stays the **2D Android production / tester track**  
**Proposed new repo:** **`Ashen_Lantern_UE`** (Unreal Engine 5.x desktop prototype → Android later)

This document is a **plan**, not an implementation. Do **not** start a full Unreal rewrite inside this Android repository. No `.uproject` binary dump lands here.

---

## 1. Product split

| Track | Repo | Role |
|-------|------|------|
| **2D production / testers** | `martinbeahan/Ashen_Lantern` (this repo) | Continue shipping, playtesting, and fixing the native C++ / Kotlin Android app. Source of truth for combat math, story acts, endgame modes, and SRD-safe design until the 3D slice proves itself. |
| **3D Unreal prototype** | **`Ashen_Lantern_UE`** (new) | Desktop-first vertical slice on Unreal Engine 5.x. Separate git history so UE binaries, Marketplace packs, and Android packaging do not pollute the 2D app. |

**Standing rule:** design decisions that ship in 2D remain authoritative for combat numbers and content gates until explicitly ported and re-validated in UE.

---

## 2. Proposed new repository: `Ashen_Lantern_UE`

### Name

- **GitHub:** `martinbeahan/Ashen_Lantern_UE`
- **Product branding:** still **Ashen Lantern** (same trademarks notice / SRD stance as this app)

### Who creates it

**Martin creates the empty GitHub repo**. Until that exists, all UE work stays at the “instructions only” stage below.

Suggested remote settings:

- Public or private —  (Marketplace license files may favor private until attribution is settled)
- Default branch: `main`
- README stub linking back to this plan: `Ashen_Lantern` → `docs/UNREAL_V3D_PLAN.md`
- MIT for **original code / adventure text** (match this repo); third-party Marketplace assets keep their own licenses in `ATTRIBUTION.md`

### Creation steps (Martin)

1. GitHub → **New repository** → name `Ashen_Lantern_UE` → empty (no template) or minimal README.
2. Epic Games account ready (see §7).
3. Install **Unreal Engine 5.x** via Epic Launcher (pin a specific minor, e.g. 5.4 or 5.5, once chosen — document the pin in the UE README).
4. Locally: create a blank UE project (see §8), then `git init` / push to `Ashen_Lantern_UE`.
5. Add `.gitignore` suitable for UE (ignore `Binaries/`, `DerivedDataCache/`, `Intermediate/`, `Saved/`, large caches; commit source, configs, and licensed asset packs carefully).

---

## 3. Platform order

1. **Desktop prototype first** (Windows primary; Linux/macOS optional if Martin’s hardware supports them).
2. **Android later** — only after the desktop vertical slice is playable and combat feels right. UE Android packaging, touch HUD, and device performance are a separate milestone (see §6).

Rationale: iterate camera, animation, and combat feel on a machine with GPU headroom; avoid fighting mobile packaging during the first learning curve.

---

## 4. Vertical slice definition (done = shippable demo)

**Goal:** one short, self-contained fight that proves “Ashen Lantern in 3D” without porting the full campaign.

| Element | Slice requirement |
|---------|-------------------|
| **Hero** | 1 playable class (recommend **Fighter** — simplest special: Action Surge once-per-fight). Idle + attack 3D anims. |
| **Enemy** | 1 foe (SRD-safe concept only — e.g. a generic “ashen skeleton” / “marsh brigand”; **no** WotC-unique monster art or names that imply official D&D IP). Idle + attack (or hit-react) anims. |
| **Room** | 1 enclosed combat space (stone chamber / lantern-lit ruin). Static lighting OK; no procedural dungeon yet. |
| **Animation** | Attack and idle for hero and enemy (Marketplace or free licensed skeletal meshes + anims). |
| **Combat math** | Port **basic** rules from the existing 2D design: initiative / turn or real-time-with-pause TBD in slice notes; attack roll vs AC, damage, HP, simple crit; Fighter Action Surge once per fight. Numbers should match or clearly document deltas from the Android C++ engine. |
| **HUD stubs** | HP bars (hero + foe), Attack / Special / End Turn (or equivalent) buttons, short combat log or floating damage numbers. Stubs may be UMG placeholders — polish later. |
| **Out of slice** | Full story acts, multiplayer, inventory/shop, endgame modes, companion AI, procedural crawl, audio beds parity, Android package. |

**Acceptance smoke (desktop):**

1. Boot into the room with hero + enemy visible and idling.
2. Attack resolves with anim + HP change.
3. Special (Action Surge) usable once, then disabled/greyed.
4. Defeat enemy → clear / victory stub; hero defeat → defeat stub.
5. No WotC trademarks or unofficial art in the build.

---

## 5. Legal & branding

Carry forward the same stance as this repo (`ATTRIBUTION.md`, `docs/STORE_LISTING_LEGAL.md`, README legal section):

- Product name: **Ashen Lantern** (Martin Beahan).
- **Not** affiliated with, sponsored by, endorsed by, or approved by Wizards of the Coast LLC.
- Do **not** use **Dungeons & Dragons**, **D&D**, or other WotC trademarks in the UE store page, splash, or marketing.
- Rules / class / monster **concepts** only from **5e SRD** (SRD 5.1 / CC BY 4.0) — no module text, no non-SRD proper names that imply official adventures.
- **No WotC art**, logos, or trademarked creature likenesses from official products.
- Original adventure text remains original; do not copy WotC modules into UE levels or dialogue.

UE-specific extras:

- Keep a root `ATTRIBUTION.md` in `Ashen_Lantern_UE` listing every Marketplace / free pack (URL, license, what was used).
- Epic Marketplace EULA + pack licenses must allow your intended distribution (desktop demo vs commercial Android later — re-check before shipping).

---

## 6. Art constraints

**Allowed**

- Epic Marketplace assets with clear commercial / project license
- Free / CC0 / clearly permissive packs (document each)
- Original meshes, materials, and audio created for Ashen Lantern
- Placeholder engine primitives for greybox before art pass

**Not allowed**

- WotC / official D&D art or ripped assets
- Unlicensed scrapes from the web
- Packs whose license forbids redistribution or commercial use if the project will be sold / published
- Dumping huge binary packs into the **Android** `Ashen_Lantern` repo

**Practical constraints for the slice**

- Prefer one coherent fantasy pack (characters + dungeon) over mixing five styles
- Keep skeletal retargeting simple (one hero skeleton, one enemy skeleton)
- Record license text in `ATTRIBUTION.md` **before** the first public build
- Budget: free / already-owned Marketplace content for the slice unless Martin approves a purchase

---

## 7. Martin prerequisites

| Item | Notes |
|------|--------|
| **Epic Games account** | Required for Launcher, Engine download, Marketplace |
| **Unreal Engine 5.x** | Install via Epic Launcher; pin and document the exact version in `Ashen_Lantern_UE` README |
| **Hardware (desktop slice)** | Discrete GPU strongly recommended (NVIDIA/AMD). SSD for project + DDC. 32 GB RAM comfortable; 16 GB minimum for small projects. Windows 10/11 is the path of least resistance for Marketplace + packaging |
| **Disk** | Plan **80+ GB** free for Engine + one project + Derived Data Cache |
| **Optional later** | Android SDK / NDK / device for UE Android packaging (post-slice) |
| **Accounts** | GitHub `martinbeahan` already in use for the 2D app |

No Unreal work is blocked on Android Studio for the desktop slice.

---

## 8. Scaffold empty UE project — instructions only

Do **not** commit a binary Unreal project into `Ashen_Lantern`. When Martin is ready, on the **dev machine**:

1. Epic Launcher → Unreal Engine → Launch the pinned 5.x version.
2. **Games** → **Blank** (or **Third Person** if you want a free camera/character bootstrap — strip unused content later).
3. Project name: `AshenLantern` (or `Ashen_Lantern_UE`); location outside this Android clone.
4. Target: **Desktop / Console**; Maximum Quality; **No** starter content if you want a minimal tree (or include Starter Content only for lighting tests, then remove).
5. Create → open once → **File → Save All**.
6. Initialize git in that folder; add a UE-oriented `.gitignore`; commit; push to `martinbeahan/Ashen_Lantern_UE`.
7. Add README pointing at this plan and listing the pinned Engine version + first Marketplace packs.

Optional Blueprint-only slice is fine for the first vertical slice; C++ module can wait until combat math needs shared code with the Android engine (or a documented reimplementation).

---

## 9. Phased milestones after the slice

| Phase | Focus | Exit criteria |
|-------|--------|----------------|
| **M0 — Plan** | This document + repo naming + Martin prerequisites | Merged into `Ashen_Lantern` docs; Martin ACK |
| **M1 — Repo + blank project** | Create `Ashen_Lantern_UE`, blank UE project, `.gitignore`, attribution stub | Project opens on Martin’s PC |
| **M2 — Vertical slice** | 1 hero, 1 enemy, 1 room, idle/attack, basic combat math, HUD stubs | Desktop smoke in §4 passes |
| **M3 — Feel / juice** | Camera, hit react, crit feedback, reduce-flash option parity | Slice feels like Ashen Lantern, not a tech demo |
| **M4 — Content bridge** | Second enemy **or** second room; class specials beyond Fighter; loot stub | Clear path to porting Act 1 beats |
| **M5 — Systems port** | Inventory / rest / difficulty hooks aligned with 2D design docs | Design parity checklist signed off |
| **M6 — Android package** | UE Android build, touch HUD, mid-tier device smoke | Installable APK/AAB for internal testers |
| **M7 — Story / endgame (long)** | Acts, crawl, raids only after M2–M5 are stable | Not scheduled until slice ships |

The **2D app remains the tester track** through at least M2–M3. Do not freeze 2D development for UE.

---

## 10. Relationship to this Android repo

- **In scope here:** this plan, README pointer, optional future design sync notes.
- **Out of scope here:** `.uproject`, `Content/`, Engine binaries, Marketplace zips, rewriting Kotlin/C++ into UE inside this tree.
- Combat and content **docs** in `docs/` remain the shared design reference for both tracks until UE has its own design folder.

---

## 11. Open decisions (Martin)

1. Exact UE **5.x** minor version pin.
2. Turn-based (closer to current app) vs real-time-with-pause for the slice.
3. Public vs private `Ashen_Lantern_UE`.
4. Marketplace spend budget for the slice (zero vs paid pack).
5. Whether Fighter is the confirmed slice hero (recommended).

---

## See also

- [`ATTRIBUTION.md`](../ATTRIBUTION.md) — trademarks, SRD, art/audio licenses (2D track)
- [`docs/STORE_LISTING_LEGAL.md`](STORE_LISTING_LEGAL.md) — paste-ready legal blurb
- [`docs/COMBAT_JUICE.md`](COMBAT_JUICE.md) — feedback tone to preserve (restrained, reduce-flash aware)
- [`README.md`](../README.md) — 2D product overview
