package com.fintrack.dndbeginnerremote

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/** Mirrors the sanitizer cases in tools/migration_test/save_safety_test.cpp (C1 / H2). */
class TextSafetyTest {
    @Test fun sanitizeMatchesNative() {
        assertEquals("Bob Jr", TextSafety.sanitizeName("Bob, Jr"))
        assertEquals("ABCDEFG", TextSafety.sanitizeName("A;B~C|D^E:F%G"))
        assertEquals("Corvin", TextSafety.sanitizeName("Corvin (NPC)"))
        assertEquals("tab name", TextSafety.sanitizeName("  tab\tname  "))
        assertEquals("Hero", TextSafety.sanitizeName(",;|"))
        assertEquals("Hero", TextSafety.sanitizeName(null))
        assertEquals(TextSafety.MAX_NAME_CHARS, TextSafety.sanitizeName("x".repeat(200)).length)
    }

    @Test fun classIdsAreValidated() {
        assertEquals(4, TextSafety.validClassId(4))
        assertEquals(0, TextSafety.validClassId(7))
        assertEquals(0, TextSafety.validClassId(-1))
        assertEquals(0, TextSafety.validClassId(null))
    }

    @Test fun decodesNativeEncoding() {
        assertEquals("Bob, Jr", TextSafety.decode("Bob%2C Jr"))
        assertEquals("a|b~c%", TextSafety.decode("a%7Cb%7Ec%25"))
        assertEquals("100%", TextSafety.decode("100%"))
        val header = (0..33).joinToString(",") { "0" } + ",E2"
        assertTrue(TextSafety.isEncodedSave("$header|room~ev|"))
        assertFalse(TextSafety.isEncodedSave("solo,5,0,0,0,0,,0,5,0,0,1|room~ev|"))
    }
}
