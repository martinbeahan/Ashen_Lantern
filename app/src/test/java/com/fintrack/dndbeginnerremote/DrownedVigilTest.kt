package com.fintrack.dndbeginnerremote

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

/** v2.40 The Drowned Vigil: the Rekindled I gate and its one-Raid-Key cost (docs/MYTHICAL.md). */
class DrownedVigilTest {
    private val root: File = generateSequence(File("").absoluteFile) { it.parentFile }
        .first { File(it, "app/build.gradle.kts").isFile }

    @Test fun rankGate() {
        assertFalse(DrownedVigil.unlocked(0))
        assertTrue(DrownedVigil.unlocked(1))
        assertTrue(DrownedVigil.unlocked(10))
        assertTrue(DrownedVigil.entryLine(0).contains("Rekindle to open the Drowned Vigil"))
        assertTrue(DrownedVigil.entryLine(1).contains("1 Raid Key"))
        assertTrue(DrownedVigil.entryLine(1).contains("7%"))
    }

    @Test fun keyCost() {
        assertEquals(1, DrownedVigil.KEY_COST)
        assertFalse("no key", DrownedVigil.canEnter(3, 0))
        assertFalse("rank 0", DrownedVigil.canEnter(0, 3))
        assertTrue(DrownedVigil.canEnter(1, 1))
        assertEquals(2, DrownedVigil.keysAfterEntry(3))
        assertEquals(0, DrownedVigil.keysAfterEntry(1))
        assertEquals(0, DrownedVigil.keysAfterEntry(0))
        // A failed start refunds the key, capped at 3 like every key grant.
        assertEquals(3, DrownedVigil.keysAfterRefund(DrownedVigil.keysAfterEntry(3)))
        assertEquals(3, DrownedVigil.keysAfterRefund(3))
    }

    @Test fun modeMatchesNativeAndEntrySpendsAKey() {
        val quest = File(root, "app/src/main/cpp/QuestScript.h").readText()
        assertTrue(quest.contains("VIGIL = ${DrownedVigil.MODE}"))
        val main = File(root, "app/src/main/java/com/fintrack/dndbeginnerremote/MainActivity.kt").readText()
        // The Vigil button spends a key before it starts, shares the raid session flags, and only shows when Rekindled.
        val button = main.substringAfter("setNeutralButton(\"Drowned Vigil\")").substringBefore("dialog.show()")
        assertTrue(button.contains("spendRaidKey()"))
        assertTrue(button.indexOf("spendRaidKey()") < button.indexOf("startBossRaidFromSave(vigil = true)"))
        assertTrue(main.contains("if (DrownedVigil.canEnter(rank, keys))"))
        assertTrue(main.contains("mode != 2 && mode != DrownedVigil.MODE"))
    }
}
