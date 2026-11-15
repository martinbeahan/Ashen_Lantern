// v2.37 #90: Realtime Database rules tests (firebase/database.rules.json) on the local emulator.
// Run: tools/rules_test/run.sh  (installs firebase-tools locally, starts the database emulator with a
// "demo-" project id — nothing touches a real Firebase project).
import { readFileSync } from "node:fs";
import { initializeTestEnvironment, assertFails, assertSucceeds } from "@firebase/rules-unit-testing";
import { ref, set, get, update, push, remove, serverTimestamp } from "firebase/database";

const rules = readFileSync(new URL("../../firebase/database.rules.json", import.meta.url), "utf8");
const env = await initializeTestEnvironment({
  projectId: "demo-ashen-lantern",
  database: { rules, host: "127.0.0.1", port: 9000 },
});

const SID = "AL-7KQ2MX9D4R";
const S = `sessions/${SID}`;
const host = env.authenticatedContext("hostUid").database();
const joiner = env.authenticatedContext("joinUid").database();
const other = env.authenticatedContext("evilUid").database();
const anon = env.unauthenticatedContext().database();

let passed = 0, failed = 0;
async function check(name, p) {
  try { await p; passed++; console.log("  ok   " + name); }
  catch (e) { failed++; console.log("  FAIL " + name + " — " + (e && e.message ? e.message.split("\n")[0] : e)); }
}
const ok = (name, op) => check(name, assertSucceeds(op));
const no = (name, op) => check(name, assertFails(op));

const meta = (uid) => ({ hostUid: uid, hostName: "Keeper", role: "dm", sessionId: SID, updatedAt: serverTimestamp() });

// --- no auth: nothing
await no("unauthenticated read", get(ref(anon, S)));
await no("unauthenticated meta write", set(ref(anon, `${S}/meta`), meta("x")));
await no("root read", get(ref(host, "/")));
// --- session id shape
await no("short legacy id refused", set(ref(host, "sessions/AL-1A2B/meta"), { ...meta("hostUid"), sessionId: "AL-1A2B" }));
await no("lowercase id refused", set(ref(host, "sessions/al-7kq2mx9d4r/meta"), { ...meta("hostUid"), sessionId: "al-7kq2mx9d4r" }));
await no("joiner can't join a session with no host", set(ref(joiner, `${S}/lobby/joinUid`), { playerName: "Ada", classId: 1 }));
// --- host claims the table
await no("meta hostUid must be the writer", set(ref(other, `${S}/meta`), meta("hostUid")));
await ok("host creates meta", update(ref(host, `${S}/meta`), meta("hostUid")));
await no("another uid can't take over meta", update(ref(other, `${S}/meta`), meta("evilUid")));
await no("another uid can't rename the host", update(ref(other, `${S}/meta`), { hostName: "Pwned" }));
await ok("host updates meta", update(ref(host, `${S}/meta`), { hostName: "Keeper 2", updatedAt: serverTimestamp() }));
await no("meta rejects unknown fields", update(ref(host, `${S}/meta`), { admin: true }));
await no("meta hostName must be a string", update(ref(host, `${S}/meta`), { hostName: 5 }));
await ok("joiner reads meta", get(ref(joiner, `${S}/meta`)));
// --- gameState: host only, string, ≤ 512 KB
await ok("host writes gameState", set(ref(host, `${S}/gameState`), "v2|state"));
await no("joiner can't write gameState", set(ref(joiner, `${S}/gameState`), "evil"));
await no("other can't wipe gameState", remove(ref(other, `${S}/gameState`)));
await no("gameState must be a string", set(ref(host, `${S}/gameState`), 1));
await no("gameState over 512 KB", set(ref(host, `${S}/gameState`), "x".repeat(524289)));
await ok("joiner reads gameState", get(ref(joiner, `${S}/gameState`)));
// --- lobby: own uid only
await ok("joiner registers in lobby/<own uid>", set(ref(joiner, `${S}/lobby/joinUid`), { playerName: "Ada", classId: 1, ts: serverTimestamp() }));
await no("joiner can't write someone else's lobby entry", set(ref(joiner, `${S}/lobby/evilUid`), { playerName: "Ada", classId: 1 }));
await no("other can't overwrite the joiner's lobby entry", set(ref(other, `${S}/lobby/joinUid`), { playerName: "Bob", classId: 0 }));
await no("lobby classId 0..4", set(ref(joiner, `${S}/lobby/joinUid`), { playerName: "Ada", classId: 9 }));
await no("lobby playerName must be a string", set(ref(joiner, `${S}/lobby/joinUid`), { playerName: 5, classId: 1 }));
await no("lobby playerName ≤ 64", set(ref(joiner, `${S}/lobby/joinUid`), { playerName: "a".repeat(65), classId: 1 }));
await no("lobby rejects extra fields", set(ref(joiner, `${S}/lobby/joinUid`), { playerName: "Ada", classId: 1, gold: 1e9 }));
// --- actions: created by the uid inside, deleted by author or host
const a1 = push(ref(joiner, `${S}/actions`));
await ok("joiner pushes an action as itself", set(a1, { uid: "joinUid", type: "attack", playerName: "Ada", targetIndex: 0, statIndex: 0, classId: 1, ts: serverTimestamp() }));
await no("joiner can't push an action as another uid", set(push(ref(joiner, `${S}/actions`)), { uid: "evilUid", type: "attack" }));
await no("other can't edit the joiner's action", set(ref(other, `${S}/actions/${a1.key}`), { uid: "evilUid", type: "heal" }));
await no("other can't delete the joiner's action", remove(ref(other, `${S}/actions/${a1.key}`)));
await no("action targetIndex in range", set(push(ref(joiner, `${S}/actions`)), { uid: "joinUid", type: "attack", targetIndex: 4294967297 }));
await no("action type must be a string", set(push(ref(joiner, `${S}/actions`)), { uid: "joinUid", type: 7 }));
await no("action rejects nested maps", set(push(ref(joiner, `${S}/actions`)), { uid: "joinUid", type: "attack", extra: { a: 1 } }));
await no("action allocations ≤ 256", set(push(ref(joiner, `${S}/actions`)), { uid: "joinUid", type: "levelupBatch", allocations: "1,".repeat(200) }));
await ok("joiner sends a level-up batch", set(push(ref(joiner, `${S}/actions`)), { uid: "joinUid", type: "levelupBatch", allocations: "1,0,0,0,0,1", playerName: "Ada", targetIndex: 0, statIndex: 0, classId: 1 }));
await ok("host acks (deletes) an action", remove(ref(host, `${S}/actions/${a1.key}`)));
// --- chat: {uid, text}, write once
const c1 = push(ref(joiner, `${S}/chat`));
await ok("joiner chats as itself", set(c1, { uid: "joinUid", text: "[Ada]: hi", ts: serverTimestamp() }));
await no("bare string chat refused", set(push(ref(joiner, `${S}/chat`)), "hi"));
await no("number chat refused", set(push(ref(joiner, `${S}/chat`)), 5));
await no("chat as another uid refused", set(push(ref(joiner, `${S}/chat`)), { uid: "hostUid", text: "spoof" }));
await no("chat over 500 chars refused", set(push(ref(joiner, `${S}/chat`)), { uid: "joinUid", text: "y".repeat(501) }));
await no("chat lines can't be edited", set(ref(joiner, `${S}/chat/${c1.key}`), { uid: "joinUid", text: "edited" }));
await no("chat lines can't be deleted by others", remove(ref(other, `${S}/chat/${c1.key}`)));
// --- nothing else
await no("unknown session child refused", set(ref(host, `${S}/admin`), true));
await no("whole-session overwrite by a joiner refused", set(ref(joiner, S), { gameState: "x" }));
await no("whole-session wipe by another uid refused", remove(ref(other, S)));

await env.cleanup();
console.log(`\nRULES: ${passed} passed, ${failed} failed`);
process.exit(failed ? 1 : 0);
