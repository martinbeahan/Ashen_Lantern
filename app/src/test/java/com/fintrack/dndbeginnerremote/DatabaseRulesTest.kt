package com.fintrack.dndbeginnerremote

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

/**
 * v2.37 #90: structure checks on firebase/database.rules.json (the emulator test in tools/rules_test
 * exercises the behaviour). Keeps the rules, SessionIds and the client limits in step.
 */
class DatabaseRulesTest {
    private val rulesFile: File = generateSequence(File("").absoluteFile) { it.parentFile }
        .map { File(it, "firebase/database.rules.json") }.first { it.isFile }
    private val root = JSONObject(rulesFile.readText()).getJSONObject("rules")
    private val session = root.getJSONObject("sessions").getJSONObject("\$sid")

    private fun walk(o: JSONObject, path: String, visit: (String, String, Any) -> Unit) {
        for (k in o.keys()) {
            val v = o.get(k)
            visit(path, k, v)
            if (v is JSONObject) walk(v, "$path/$k", visit)
        }
    }

    @Test fun rootIsClosed() {
        assertEquals(false, root.get(".read"))
        assertEquals(false, root.get(".write"))
    }

    @Test fun noOpenRulesAnywhere() {
        walk(root, "") { path, k, v ->
            if (k == ".read" || k == ".write") {
                assertFalse("$path/$k is open", v == true || v == "true")
                if (v is String) assertTrue("$path/$k must need auth", v.startsWith("auth != null"))
            }
        }
    }

    @Test fun everyWriteChecksTheSessionIdShape() {
        val cls = "[0-9A-HJKMNP-TV-Z]"
        val sidRe = "\$sid.matches(/^AL-" + cls.repeat(SessionIds.CODE_LENGTH) + "\$/)"
        walk(session, "") { path, k, v -> if (k == ".write" || k == ".read") assertTrue("$path/$k", (v as String).contains(sidRe)) }
        // The character class is exactly SessionIds.ALPHABET.
        val expanded = buildString { for (c in '0'..'9') append(c); for (c in 'A'..'H') append(c); append("JK"); append("MN"); for (c in 'P'..'T') append(c); for (c in 'V'..'Z') append(c) }
        assertEquals(SessionIds.ALPHABET, expanded)
    }

    @Test fun onlyTheHostWritesStateAndMeta() {
        val gs = session.getJSONObject("gameState").getString(".write")
        assertTrue(gs.contains("child('hostUid').val() === auth.uid"))
        val meta = session.getJSONObject("meta")
        assertTrue(meta.getString(".write").contains("data.child('hostUid').val() === auth.uid"))
        assertTrue(meta.getJSONObject("hostUid").getString(".validate").contains("=== auth.uid"))
    }

    @Test fun joinerEntriesAreKeyedOrStampedByUid() {
        assertTrue(session.getJSONObject("lobby").getJSONObject("\$uid").getString(".write").contains("auth.uid === \$uid"))
        for (node in listOf("actions" to "\$aid", "chat" to "\$cid")) {
            val n = session.getJSONObject(node.first).getJSONObject(node.second)
            assertTrue(node.first, n.getString(".write").contains("newData.child('uid').val() === auth.uid"))
            assertTrue(node.first, n.getJSONObject("uid").getString(".validate").contains("=== auth.uid"))
        }
        assertTrue("chat is write-once", session.getJSONObject("chat").getJSONObject("\$cid").getString(".write").contains("!data.exists()"))
    }

    @Test fun unknownFieldsAreRefused() {
        var count = 0
        walk(session, "") { _, k, v -> if (k == "\$other") { count++; assertEquals(false, (v as JSONObject).get(".validate")) } }
        assertEquals("session, meta, lobby, actions, chat", 5, count)
    }

    @Test fun limitsMatchTheClient() {
        assertTrue(session.getJSONObject("gameState").getString(".validate").contains("length <= 524288"))
        assertTrue(session.getJSONObject("chat").getJSONObject("\$cid").getJSONObject("text").getString(".validate")
            .contains("length <= ${OnlinePolicy.MAX_CHAT_CHARS}"))
        assertTrue(session.getJSONObject("lobby").getJSONObject("\$uid").getJSONObject("playerName").getString(".validate")
            .contains("length <= ${OnlinePolicy.MAX_NAME_CHARS}"))
        assertTrue(session.getJSONObject("meta").getJSONObject("hostName").getString(".validate")
            .contains("length <= ${OnlinePolicy.MAX_NAME_CHARS}"))
    }

    @Test fun everyFieldTheAppWritesIsAllowed() {
        val src = File(rulesFile.parentFile.parentFile, "app/src/main/java/com/fintrack/dndbeginnerremote/MultiplayerManager.kt").readText()
        for (f in listOf("hostUid", "hostName", "role", "sessionId", "updatedAt")) {
            assertTrue(f, session.getJSONObject("meta").has(f)); assertTrue(f, src.contains("\"$f\" to"))
        }
        for (f in listOf("playerName", "classId", "ts")) assertTrue(f, session.getJSONObject("lobby").getJSONObject("\$uid").has(f))
        val actions = session.getJSONObject("actions").getJSONObject("\$aid")
        for (f in listOf("uid", "type", "playerName", "targetIndex", "statIndex", "classId", "allocations", "ts")) assertTrue(f, actions.has(f))
        for (f in listOf("uid", "text", "ts")) assertTrue(f, session.getJSONObject("chat").getJSONObject("\$cid").has(f))
    }
}
