package com.fintrack.dndbeginnerremote

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/** Debug variant: the test shortcut exists (and carries the marker the release check scans for). */
class DebugShortcutsDebugTest {
    @Test
    fun shortcutExistsInDebug() {
        assertTrue(DebugShortcuts.ENABLED)
        assertEquals("ASHEN_DEBUG_ONLY_SHORTCUT", DebugShortcuts.MARKER)
        assertTrue(DebugShortcuts::class.java.declaredMethods.any { it.name == "nativeJumpToRekindleReady" })
    }
}
