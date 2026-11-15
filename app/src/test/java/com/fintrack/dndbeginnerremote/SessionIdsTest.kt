package com.fintrack.dndbeginnerremote

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.util.Random

/** v2.37 #90: online session ids are long, unguessable and still easy to type. */
class SessionIdsTest {
    @Test fun newIdHasCanonicalShape() {
        repeat(2000) {
            val id = SessionIds.newId()
            assertEquals(13, id.length)
            assertTrue(id, id.startsWith("AL-"))
            assertTrue(id, SessionIds.isValid(id))
            assertTrue(id, Regex("^AL-[0-9A-HJKMNP-TV-Z]{10}$").matches(id))
        }
    }

    @Test fun alphabetIsCrockfordWithoutLookalikes() {
        assertEquals(32, SessionIds.ALPHABET.length)
        assertEquals(32, SessionIds.ALPHABET.toSet().size)
        for (c in "ILOU") assertFalse("$c", c in SessionIds.ALPHABET)
        // 32^10 ≈ 1.1e15 — at least 50 bits, versus 16 bits for the old "AL-1A2B".
        assertTrue(Math.pow(32.0, 10.0) >= Math.pow(2.0, 50.0))
    }

    @Test fun idsDoNotRepeatAndUseTheWholeAlphabet() {
        val ids = HashSet<String>()
        val seen = HashSet<Char>()
        repeat(20000) { val id = SessionIds.newId(); ids += id; id.substring(3).forEach { seen += it } }
        assertEquals(20000, ids.size)
        assertEquals(SessionIds.ALPHABET.toSet(), seen)
    }

    @Test fun seededRandomIsDeterministic() {
        assertEquals(SessionIds.newId(Random(7)), SessionIds.newId(Random(7)))
    }

    @Test fun normalizeAcceptsHowPeopleTypeIt() {
        val id = "AL-7KQ2MX9D4R"
        for (typed in listOf("AL-7KQ2MX9D4R", "AL-7KQ2M-X9D4R", "al-7kq2m-x9d4r", " 7KQ2M X9D4R ",
            "7kq2mx9d4r", "AL7KQ2MX9D4R", "AL 7KQ2M.X9D4R", "AL_7KQ2M_X9D4R")) {
            assertEquals(typed, id, SessionIds.normalize(typed))
        }
        assertEquals("AL-0000011111", SessionIds.normalize("OOOOO-ILIL1"))
    }

    @Test fun normalizeRejectsWrongInput() {
        for (bad in listOf(null, "", "AL-", "AL-1A2B", "1A2B", "AL-7KQ2MX9D4", "AL-7KQ2MX9D4RR",
            "AL-7KQ2MX9D4U", "AL-7KQ2M!X9D4", "../../sessions", "x".repeat(41), "AL-7KQ2MX9D4R/meta")) {
            assertNull("$bad", SessionIds.normalize(bad))
        }
    }

    @Test fun isValidOnlyAcceptsCanonical() {
        assertTrue(SessionIds.isValid("AL-0123456789"))
        assertFalse(SessionIds.isValid("AL-7KQ2M-X9D4R"))
        assertFalse(SessionIds.isValid("al-7kq2mx9d4r"))
        assertFalse(SessionIds.isValid("AL-A1B2"))
        assertFalse(SessionIds.isValid(null))
        assertFalse(SessionIds.isValid("XX-7KQ2MX9D4R"))
    }

    @Test fun displayGroupsTheCode() {
        assertEquals("AL-7KQ2M-X9D4R", SessionIds.display("AL-7KQ2MX9D4R"))
        assertEquals("AL-7KQ2MX9D4R", SessionIds.normalize(SessionIds.display("AL-7KQ2MX9D4R")))
        assertEquals("AL-A1B2", SessionIds.display("AL-A1B2"))
    }

    @Test fun legacyCodesAreRecognised() {
        for (old in listOf("AL-A1B2", "al-1f", "AL3C", "AL-0")) assertTrue(old, SessionIds.looksLegacy(old))
        assertFalse(SessionIds.looksLegacy("AL-7KQ2M-X9D4R"))
        assertFalse(SessionIds.looksLegacy(null))
    }
}
