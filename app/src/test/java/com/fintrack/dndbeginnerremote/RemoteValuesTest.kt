package com.fintrack.dndbeginnerremote

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * v2.35 #88: what a Firebase listener does with values of the wrong type or size. Each case is what
 * `snapshot.value` returns for that node (Firebase gives Long / Double / Boolean / String / Map / List).
 * Before the fix `getValue(String::class.java)` threw on all the non-String cases and crashed the app.
 */
class RemoteValuesTest {
    private val wrongTypes: List<Any?> = listOf(
        null, 1L, 0L, 3.5, true, false, emptyMap<String, Any>(), mapOf("a" to "b"), listOf("x"), emptyList<Any>()
    )

    @Test
    fun gameStateIgnoresNonStringsAndOversized() {
        for (v in wrongTypes) assertNull("gameState $v", RemoteValues.gameState(v))
        assertNull(RemoteValues.gameState(""))
        assertNull(RemoteValues.gameState("   "))
        assertEquals("v2|abc", RemoteValues.gameState("v2|abc"))
        val max = "x".repeat(RemoteValues.MAX_GAME_STATE_CHARS)
        assertEquals(max, RemoteValues.gameState(max))
        assertNull(RemoteValues.gameState(max + "x"))
    }

    @Test
    fun hostNameIgnoresNonStringsAndIsCapped() {
        for (v in wrongTypes) assertNull("hostName $v", RemoteValues.hostName(v))
        assertNull(RemoteValues.hostName(" "))
        assertNull(RemoteValues.hostName(",;|~"))   // only forbidden separators -> nothing left
        assertEquals("Mira", RemoteValues.hostName("Mira"))
        val long = RemoteValues.hostName("A".repeat(10_000))!!
        assertEquals(TextSafety.MAX_NAME_CHARS, long.length)
    }

    @Test
    fun lobbyNameFallsBackToKeyAndClassIdIsValidated() {
        assertEquals("Bram", RemoteValues.playerName(5L, "Bram"))
        assertEquals("Bram", RemoteValues.playerName(mapOf("x" to 1), "Bram"))
        assertNull(RemoteValues.playerName(5L, null))
        assertEquals("Hero", RemoteValues.playerName("", "k"))
        assertEquals("Ada", RemoteValues.playerName("A,d;a", null))
        assertTrue(RemoteValues.playerName("Z".repeat(5000), null)!!.length <= TextSafety.MAX_NAME_CHARS)

        assertEquals(3, RemoteValues.classId(3L))
        assertEquals(4, RemoteValues.classId(4))
        assertEquals(2, RemoteValues.classId(2.0))
        assertEquals(1, RemoteValues.classId(" 1 "))
        for (v in listOf<Any?>(null, -1L, 5L, Long.MAX_VALUE, 2.5, "two", true, mapOf("a" to 1), listOf(1L))) {
            assertNull("classId $v", RemoteValues.classId(v))
        }
        assertEquals(0, TextSafety.validClassId(RemoteValues.classId("bad"))) // joins as Fighter
    }

    @Test
    fun chatIgnoresNonStringsCleansAndCaps() {
        for (v in wrongTypes) assertNull("chat $v", RemoteValues.chat(v))
        assertNull(RemoteValues.chat(""))
        assertNull(RemoteValues.chat("\n\t"))
        assertEquals("hi there", RemoteValues.chat("hi\nthere"))
        val long = RemoteValues.chat("y".repeat(100_000))!!
        assertEquals(RemoteValues.MAX_CHAT_CHARS, long.length)
        // A cut never leaves half an emoji (surrogate pair).
        val emoji = "a".repeat(RemoteValues.MAX_CHAT_CHARS - 1) + "\uD83D\uDD25" + "tail"
        val cut = RemoteValues.chat(emoji)!!
        assertFalse(Character.isHighSurrogate(cut.last()))
        assertTrue(cut.length <= RemoteValues.MAX_CHAT_CHARS)
    }

    @Test
    fun actionKeepsOnlyPrimitiveCappedFields() {
        val a = RemoteValues.action(
            listOf(
                "type" to "attack", "targetIndex" to 1L, "flag" to true, "ratio" to 0.5,
                "nested" to mapOf("x" to 1), "list" to listOf(1, 2), "huge" to "z".repeat(RemoteValues.MAX_ACTION_STRING_CHARS + 1),
                null to "nokey", "" to "empty", "nullValue" to null
            )
        )
        assertEquals(setOf("type", "targetIndex", "flag", "ratio"), a.keys)
        val many = RemoteValues.action((1..500).map { "k$it" to it.toLong() })
        assertEquals(RemoteValues.MAX_ACTION_FIELDS, many.size)
        assertTrue(RemoteValues.action(emptyList()).isEmpty())
    }

    @Test
    fun listenersNoLongerUseThrowingGetValue() {
        // Source guard: every listener reads through RemoteValues, never getValue(String / Long::class.java).
        val f = listOf(
            java.io.File("src/main/java/com/fintrack/dndbeginnerremote/MultiplayerManager.kt"),
            java.io.File("app/src/main/java/com/fintrack/dndbeginnerremote/MultiplayerManager.kt")
        ).first { it.exists() }
        val src = f.readText()
        assertFalse(src.contains("getValue(String::class.java)"))
        assertFalse(src.contains("getValue(Long::class.java)"))
        for (fn in listOf("hostName(", "gameState(", "playerName(", "classId(", "chat(", "action(")) {
            assertTrue("RemoteValues.$fn used", src.contains("RemoteValues.$fn"))
        }
    }
}
