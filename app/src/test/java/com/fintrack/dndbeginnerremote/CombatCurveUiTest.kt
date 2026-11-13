package com.fintrack.dndbeginnerremote

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

/** v2.29 combat curve HUD: the EL / Pressure badge and the surprise-boss warning. */
class CombatCurveUiTest {
    private val status = """
        Room 5 | Turn: Mira
        Quest: The Kindling — Bone Gallery
        Difficulty: Medium
        Curve: EL 7 · Pressure 2/4 · Rekindle 0
        Mira | HP: 30/30
    """.trimIndent()

    @Test
    fun parsesTheCurveLine() {
        val s = CombatCurveUi.parse(status)
        assertNotNull(s)
        assertEquals(7, s!!.encounterLevel)
        assertEquals(2, s.pressure)
        assertEquals(4, s.pressureMax)
        assertEquals(0, s.rekindleRooms)
        assertEquals("EL 7 · ◆◆◇◇", CombatCurveUi.roomBadge(s))
        assertEquals("EL 3 · ◇◇◇◇ · Rekindled",
            CombatCurveUi.roomBadge(CombatCurveUi.State(3, 0, 4, 1)))
    }

    @Test
    fun onlineStatusHasNoBadge() {
        // Keeper tables / joiners: the core never sends a Curve line, so the HUD is unchanged.
        assertNull(CombatCurveUi.parse("Room 2 | Turn: Guest\nDifficulty: Easy\nLanternkeeper: Ash\n"))
        assertNull(CombatCurveUi.parse("Curve: nonsense"))
    }

    @Test
    fun pipsClamp() {
        assertEquals("◆◆◆◆", CombatCurveUi.pressurePips(9, 4))
        assertEquals("◇◇◇◇", CombatCurveUi.pressurePips(-1, 4))
    }

    @Test
    fun warningRespectsReduceFlash() {
        assertTrue(CombatCurveUi.isWarning("WARNING! Bones knit into a mitre — the Ossuary Prior rises. It strikes next turn."))
        assertFalse(CombatCurveUi.isWarning("BOSS! The Ossuary Prior — stand ready!"))
        assertFalse(CombatCurveUi.warningAnimates(reduceFlash = true))
        assertTrue(CombatCurveUi.warningAnimates(reduceFlash = false))
    }

    /** The Kotlin parser must match what the native core actually prints. */
    @Test
    fun nativeFormatMatches() {
        val game = listOf("src/main/cpp/Game.cpp", "app/src/main/cpp/Game.cpp").map(::File).first { it.exists() }.readText()
        assertTrue(game.contains("ss << \"Curve: EL \" << el << \" · Pressure \" << pressure_ << \"/\" << curve::kPressureMaxSteps"))
        assertTrue(game.contains("<< \" · Rekindle \" << rekindleRooms_"))
        assertTrue(game.contains("lastEvent_ = \"WARNING! \" + telegraphLine(*enemy);"))
    }

    /** Save fields are append-only: the v2.29 block sits after the v2.25 story choices. */
    @Test
    fun saveFieldsAppendOnly() {
        val save = listOf("src/main/cpp/GameSave.cpp", "app/src/main/cpp/GameSave.cpp").map(::File).first { it.exists() }.readText()
        val choices = save.indexOf("<< \",\" << vennChoice_ << \",\" << wellChoice_")
        val curve = save.indexOf("<< \",\" << kCurveSchemaTag")
        assertTrue(choices in 0 until curve)
        val enemyId = save.indexOf("ss << \",\" << e->enemyId;")
        val enemyAc = save.indexOf("ss << \",\" << e->armorClass")
        assertTrue(enemyId in 0 until enemyAc)
        val inventory = save.indexOf("p->inventory[ii]->toToken()")
        val bonus = save.indexOf("<< p->prestigeRank << \",\" << p->bonusMaxHp")
        assertTrue(inventory in 0 until bonus)
    }
}
