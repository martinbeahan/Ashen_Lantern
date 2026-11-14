# Online security (v2.37 / 76, issue #90)

Before v2.37 the Realtime Database rules were open (`sessions/$sid` readable and writable by anyone),
session ids were `AL-` + 4 hex digits (65,536 possible tables), and the host loaded any game state
written to its session. Anyone who had the database URL (it ships inside every APK) could find live
tables by trying ids, overwrite the host's game, take over meta, or act as another player's hero.

v2.37 locks this down. **Online gameplay is otherwise unchanged, and Host / Join still never read or
write a solo save.**

## What changed

| Layer | v2.37 behaviour |
|---|---|
| Sign-in | Host and Join first sign in with **Firebase Anonymous Authentication** (no account, nothing to type). Each phone gets a uid. If sign-in fails, the app says why and solo play still works |
| Session codes | A fresh code for every table: `AL-` + 10 characters from SecureRandom (Crockford base32 — digits and capitals without I, L, O, U). 32¹⁰ ≈ 1.1 × 10¹⁵ codes (50 bits) instead of 65,536. It's shown as **`AL-7KQ2M-X9D4R`** |
| Typing a code | Joining ignores dashes, spaces and case, and reads O as 0 and I / L as 1. If someone types an old 4-character code, the app tells them to ask the Lanternkeeper for a new one |
| Host is authoritative | The host never loads game state from the database. It only publishes it. The rules also refuse a joiner's write to `gameState` |
| Who you play as | Every joiner write carries its uid. The host remembers which hero name each uid joined with and ignores any action naming another player's hero. A second phone can't join under a hero name that's already taken, the host's name, or an NPC companion's name |
| Rules | `firebase/database.rules.json` (below). It closes the root, requires auth, accepts only new-format session codes, scopes each write and validates every field's type and size |

### Session-code tradeoff

A longer code is harder to read out loud. 10 characters is the shortest length that makes guessing
hopeless. At 1,000 guesses a second against a few hundred live tables, finding one would take more
than a thousand years on average. With the old 4 hex digits it took about a minute. To keep the
longer code usable:

- it's grouped 5 + 5;
- the alphabet has no look-alikes;
- typing is forgiving (see above);
- **Copy** on the share dialog, and long-pressing the top bar, put it on the clipboard so you can paste it into a message.

The rules are still the real protection. A code that leaks (for example a screenshot) only lets
someone **join** as a new hero, never overwrite the game. The Lanternkeeper can end the table and
host a new one to get a fresh code.

The online code is separate from the save's own internal session id (which the Memory Ledger keys on),
so saves and the Ledger are untouched.

### Rules summary (`firebase/database.rules.json`)

- Root `.read` / `.write` are `false`. Only `sessions/$sid` is reachable, only when signed in, and
  only when `$sid` is `AL-` + 10 alphabet characters.
- `meta`: the first writer must set `hostUid` to its own uid; after that, only that uid can change
  it. Fields: `hostUid`, `hostName` (1–64 chars), `role` (≤ 16), `sessionId` (= `$sid`), `updatedAt`.
- `gameState`: written only by the uid in `meta/hostUid`. A string of up to 524,288 chars (the #88 cap).
- `lobby/$uid`: a phone writes only its own entry. Fields: `playerName` (1–64), `classId` (0–4), `ts`.
- `actions/$aid`: created by the uid named inside it, and only once a host exists. Deleted by the
  author or the host (the host's acknowledgement). Fields: `uid`, `type` (1–32), `playerName` (≤ 64),
  `targetIndex` / `statIndex` (−1 … 1023), `classId` (0–4), `allocations` (≤ 256), `ts`.
- `chat/$cid`: write-once, by the uid inside it. Fields: `uid`, `text` (1–500, the #88 cap), `ts`.
- Any other field or child is refused (`$other: false`).

## ⚠️ Old app versions stop working online

Once these rules are published, **v2.36 and older can't Host or Join** (they don't sign in, and their
4-character codes are refused). Solo play on old versions is unaffected. **Install v2.37 on both
phones before you publish the rules.**

## Firebase console steps (Martin, in this order)

1. **Install v2.37 on both phones** (the APK from this PR, or the next release build). Optional:
   check that solo Continue still loads your hero on each.
2. **Enable Anonymous sign-in.** Firebase console → your project → **Build → Authentication** →
   *Get started* (first time only) → **Sign-in method** tab → **Add new provider** → **Anonymous**
   → turn it on → **Save**.
   - Nothing else changes. You don't need to download a new `google-services.json`; the existing one keeps working.
3. **Back up the current rules.** **Build → Realtime Database → Rules** tab. Copy the current text
   somewhere safe, in case you need to roll back.
4. **Paste the new rules.** Replace the whole editor contents with
   [`firebase/database.rules.json`](../firebase/database.rules.json), exactly as in the repo.
   **Don't publish yet.**
5. **Test with the Rules Playground** (the *Rules Playground* button above the editor; it checks the
   *unpublished* rules). Run the checklist below. Every row should match.
6. **Publish.** If the editor reports a syntax error, don't publish; re-copy the file.
7. **Try it on the phones:**
   - **Host online** on phone A. It should show a code like `AL-7KQ2M-X9D4R`.
   - **Join** on phone B with the code: typed in lower case, without dashes, or pasted.
   - Play a turn and send chat.
8. **Optional clean-up:** old sessions with 4-character ids stay in the database but can't be read or
   written any more. You can delete the `sessions` children with short ids from the **Data** tab.

**Rollback:** paste the rules you saved in step 3 and publish. Leave Anonymous sign-in on; it's harmless.

### Rules Playground checklist

In the Playground:

- set **Location** as shown, with `S` = `/sessions/AL-7KQ2MX9D4R`;
- for *Authenticated* rows, tick **Authenticated** with provider **Anonymous** and the given **UID**;
- for writes, choose **set** and paste the data.

First create the host's meta once in the **Data** tab (or run row 4 as a real write from phone A).
Set `S/meta` to `{"hostUid":"host1","hostName":"Keeper","sessionId":"AL-7KQ2MX9D4R"}`.

| # | Type | Location | Auth (uid) | Data | Expect |
|---|---|---|---|---|---|
| 1 | read | `S` | not authenticated | — | **Denied** |
| 2 | read | `/` | `host1` | — | **Denied** |
| 3 | set | `/sessions/AL-1A2B/meta` | `host1` | `{"hostUid":"host1","hostName":"K"}` | **Denied** (old-style id) |
| 4 | set | `/sessions/AL-ZZZZZZZZZZ/meta` | `host1` | `{"hostUid":"host1","hostName":"K"}` | **Allowed** (new table) |
| 5 | set | `S/meta` | `evil` | `{"hostUid":"evil","hostName":"Pwned"}` | **Denied** |
| 6 | set | `S/gameState` | `host1` | `"v2\|state"` | **Allowed** |
| 7 | set | `S/gameState` | `join1` | `"evil"` | **Denied** |
| 8 | read | `S/gameState` | `join1` | — | **Allowed** |
| 9 | set | `S/lobby/join1` | `join1` | `{"playerName":"Ada","classId":1}` | **Allowed** |
| 10 | set | `S/lobby/host1` | `join1` | `{"playerName":"Ada","classId":1}` | **Denied** (not your uid) |
| 11 | set | `S/lobby/join1` | `join1` | `{"playerName":"Ada","classId":9}` | **Denied** (class 0–4) |
| 12 | set | `S/actions/a1` | `join1` | `{"uid":"join1","type":"attack","playerName":"Ada","targetIndex":0}` | **Allowed** |
| 13 | set | `S/actions/a2` | `join1` | `{"uid":"evil","type":"attack"}` | **Denied** (uid isn't yours) |
| 14 | set | `S/actions/a3` | `join1` | `{"uid":"join1","type":"attack","gold":99999}` | **Denied** (unknown field) |
| 15 | set | `S/chat/c1` | `join1` | `{"uid":"join1","text":"[Ada]: hi"}` | **Allowed** |
| 16 | set | `S/chat/c2` | `join1` | `"hi"` | **Denied** (must be an object) |
| 17 | set | `S/chat/c3` | `join1` | `{"uid":"join1","text":"<501 or more characters>"}` | **Denied** |
| 18 | set | `S` | `join1` | `null` | **Denied** (wipe the table) |

(Rows that depend on existing data, such as "edit someone's action", are covered by the emulator test below.)

## Automated tests

- `tools/rules_test/run.sh`: **47** allow / deny cases against the local Firebase **emulator**,
  using a `demo-` project id. It never touches a real project, and needs Node 18+ and Java 11+.
  Against the pre-v2.37 open rules, 36 of the cases fail, which shows the hole.
- JVM:
  - `DatabaseRulesTest` checks the rules structure: root closed, no open rule, the session-id shape on every rule, host-only `gameState`, uid scoping, `$other: false`, and the limits matching the client;
  - `SessionIdsTest` checks the format, alphabet, uniqueness and typing forgiveness;
  - `OnlinePolicyTest` covers host-ignores-state, uid → hero ownership and the chat cap;
  - `OnlineLockdownSourceTest` guards the MainActivity wiring.
