package com.fintrack.dndbeginnerremote

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

/**
 * v2.39: guards against merge mis-resolutions like the one after #93–#96 (version lines went back to
 * 2.35 / 74 and the #93 / #95 lobby + chat overlap lost the v2.37 half).
 */
class ReleaseConsistencyTest {
    private val root: File = generateSequence(File("").absoluteFile) { it.parentFile }
        .first { File(it, "app/build.gradle.kts").isFile }

    @Test fun versionMatchesReadmeWhatsNew() {
        val gradle = File(root, "app/build.gradle.kts").readText()
        val code = Regex("versionCode = (\\d+)").find(gradle)!!.groupValues[1]
        val name = Regex("versionName = \"([0-9.]+)\"").find(gradle)!!.groupValues[1]
        val readme = File(root, "README.md").readText()
        val top = Regex("## What's new \\(v([0-9.]+) / (\\d+)\\)").findAll(readme).toList()
        assertEquals("exactly one What's new heading", 1, top.size)
        assertEquals(name, top[0].groupValues[1])
        assertEquals(code, top[0].groupValues[2])
        // Every release since the audit keeps its entry.
        for (v in listOf("2.35 / 74", "2.36 / 75", "2.37 / 76", "2.38 / 77", "2.39 / 78")) {
            assertTrue(v, readme.contains("## Previous (v$v)"))
        }
    }

    @Test fun noConflictMarkersInSources() {
        val dirs = listOf("app/src", "tools", "firebase", "docs").map { File(root, it) }
        val bad = dirs.flatMap { d -> d.walkTopDown().filter { it.isFile && it.extension in setOf("kt", "cpp", "h", "sh", "json", "md", "kts", "mjs") }.toList() }
            .plus(listOf(File(root, "README.md"), File(root, "app/build.gradle.kts")))
            .filter { f -> f.readLines().any { it.startsWith("<<<<<<< ") || it.startsWith(">>>>>>> ") } }
        assertTrue("conflict markers in $bad", bad.isEmpty())
    }

    @Test fun lobbyAndChatKeepBothAuditFixes() {
        val mm = File(root, "app/src/main/java/com/fintrack/dndbeginnerremote/MultiplayerManager.kt").readText()
        // #88 safe reads + #90 uid-keyed lobby.
        assertTrue(mm.contains("RemoteValues.playerName("))
        assertTrue(mm.contains("onJoin(snapshot.key.orEmpty(), name, classId)"))
        // #88 safe chat read + #90 {uid, text, ts} chat objects (bare strings from old clients still read).
        assertTrue(mm.contains("RemoteValues.chat(snapshot.child(\"text\").value ?: snapshot.value)"))
        assertTrue(mm.contains("\"text\" to OnlinePolicy.outgoingChat(message)"))
    }
}
