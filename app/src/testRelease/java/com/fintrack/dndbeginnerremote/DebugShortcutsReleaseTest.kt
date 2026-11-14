package com.fintrack.dndbeginnerremote

import org.junit.Assert.assertFalse
import org.junit.Test

/** Release variant: the level-50 test shortcut must not exist (no flag, no native method, no marker). */
class DebugShortcutsReleaseTest {
    @Test
    fun shortcutIsAbsentInRelease() {
        assertFalse(DebugShortcuts.ENABLED)
        val names = DebugShortcuts::class.java.declaredMethods.map { it.name } +
            DebugShortcuts::class.java.declaredFields.map { it.name }
        assertFalse(names.any { it.contains("JumpToRekindleReady") || it == "MARKER" })
    }
}
