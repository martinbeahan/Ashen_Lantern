package com.fintrack.dndbeginnerremote

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

/** v2.30 enemy AI: intent chips, INT-gated numbers, statuses, banners and Reduce flashing. */
class EnemyIntentUiTest {
    // Shape of Game::getEnemyIntents(): numbers|bannerSeq|banner|idx,intent,side,target,label,value,statuses;...
    private val native = "1|3|PHASE 2 — The Lantern Sexton|" +
        "0,2,0,0,Snuff → Mira (next turn),24,G1;" +
        "1,3,1,0,Shield-wall The Lantern Sexton,0,;" +
        "2,5,1,1,Knit bones Ashbound,9,B2.M1"

    @Test
    fun parsesTheNativeFormat() {
        val s = EnemyIntentUi.parseIntents(native)
        assertTrue(s.numbers)
        assertEquals(3, s.bannerSeq)
        assertEquals("PHASE 2 — The Lantern Sexton", s.bannerText)
        assertEquals(3, s.intents.size)
        val heavy = s.intents[0]
        assertEquals(0, heavy.foeIndex)
        assertEquals(EnemyIntentUi.HEAVY, heavy.intent)
        assertEquals(0, heavy.side)
        assertEquals(0, heavy.target)
        assertEquals(24, heavy.value)
        assertEquals("G1", heavy.statuses)
        assertEquals("", s.intents[1].statuses)
        assertEquals(EnemyIntentUi.HEAL, s.intents[2].intent)
    }

    @Test
    fun emptyAndOnlineStringsAreSafe() {
        assertEquals(EnemyIntentUi.EMPTY, EnemyIntentUi.parseIntents(""))
        assertEquals(EnemyIntentUi.EMPTY, EnemyIntentUi.parseIntents(null))
        assertEquals(EnemyIntentUi.EMPTY, EnemyIntentUi.parseIntents("garbage"))
        // Online / legacy fights: the header only, no foes.
        val s = EnemyIntentUi.parseIntents("0|0||")
        assertTrue(s.intents.isEmpty())
        assertTrue(EnemyIntentUi.parseStatuses("").isEmpty())
    }

    @Test
    fun numbersNeedPartyIntAndTheSetting() {
        val smart = EnemyIntentUi.parseIntents(native)
        val dim = EnemyIntentUi.parseIntents(native.replaceFirst("1|", "0|"))
        assertTrue(EnemyIntentUi.showNumbers(smart, prefExactNumbers = true))
        assertFalse(EnemyIntentUi.showNumbers(smart, prefExactNumbers = false))
        assertFalse(EnemyIntentUi.showNumbers(dim, prefExactNumbers = true))
        val heavy = smart.intents[0]
        assertEquals("⚒ 24", EnemyIntentUi.chipText(heavy, numbers = true))
        assertEquals("⚒", EnemyIntentUi.chipText(heavy, numbers = false))
        assertEquals("Heavy hit: Snuff → Mira (next turn), about 24", EnemyIntentUi.describe(heavy, true))
        assertEquals("Heavy hit: Snuff → Mira (next turn)", EnemyIntentUi.describe(heavy, false))
        assertEquals("🛡", EnemyIntentUi.chipText(smart.intents[1], numbers = true)) // no value -> glyph only
        assertTrue(EnemyIntentUi.describe(smart.intents[2], true).endsWith("about +9"))
    }

    @Test
    fun everyIntentHasAGlyph() {
        for (i in 1..10) assertTrue("intent $i", EnemyIntentUi.glyph(i).isNotEmpty())
        assertEquals("⚔", EnemyIntentUi.glyph(EnemyIntentUi.ATTACK))
        assertEquals("🔥", EnemyIntentUi.glyph(EnemyIntentUi.SPELL))
        assertEquals("♥", EnemyIntentUi.glyph(EnemyIntentUi.HEAL))
        assertEquals("➜", EnemyIntentUi.glyph(EnemyIntentUi.FLEE))
        assertEquals("", EnemyIntentUi.glyph(0))
    }

    @Test
    fun reduceFlashKeepsChipsAndBannersStatic() {
        val s = EnemyIntentUi.parseIntents(native)
        assertTrue(EnemyIntentUi.chipPulses(s.intents[0], reduceFlash = false))
        assertFalse(EnemyIntentUi.chipPulses(s.intents[0], reduceFlash = true))
        assertFalse(EnemyIntentUi.chipPulses(s.intents[1], reduceFlash = false)) // only heavies pulse
        assertTrue(EnemyIntentUi.bannerAnimates(false))
        assertFalse(EnemyIntentUi.bannerAnimates(true))
    }

    @Test
    fun bannersShowOncePerSequence() {
        val s = EnemyIntentUi.parseIntents(native)
        assertEquals("PHASE 2 — The Lantern Sexton", EnemyIntentUi.newBanner(s, 2))
        assertNull(EnemyIntentUi.newBanner(s, 3))
        assertNull(EnemyIntentUi.newBanner(EnemyIntentUi.parseIntents("0|4||"), 3)) // no text
    }

    @Test
    fun heroStatuses() {
        val m = EnemyIntentUi.parseStatuses("0,B2.M1;1,K2.S1.R1")
        assertEquals("B2.M1", m[0])
        assertEquals("🔥 ◉", EnemyIntentUi.statusBadge(m[0]!!))
        assertEquals("Burning (2), Marked (1)", EnemyIntentUi.statusNames(m[0]!!))
        assertEquals("⛓ ⌛ ≋", EnemyIntentUi.statusBadge(m[1]!!))
        assertEquals("", EnemyIntentUi.statusBadge("Z9"))
    }

    /** The Kotlin parser and the native writer must agree on the field order and letters. */
    @Test
    fun matchesTheNativeWriter() {
        val root = generateSequence(File("").absoluteFile) { it.parentFile }
            .firstOrNull { File(it, "app/src/main/cpp/Game.cpp").exists() } ?: return
        val game = File(root, "app/src/main/cpp/Game.cpp").readText()
        assertTrue(game.contains("""ss << i << "," << intent << "," << side << "," << tgt << "," << label << "," << value << ","""))
        assertTrue(game.contains("""ss << (heroHasIntBonus() ? 1 : 0) << "|" << bannerSeq_ << "|""""))
        val ai = File(root, "app/src/main/cpp/EnemyAi.h").readText()
        for (c in listOf('B', 'M', 'G', 'L', 'S', 'K', 'I', 'R')) {
            assertTrue("status $c", ai.contains("add('$c'") || ai.contains("add('$c',"))
            assertTrue("glyph $c", EnemyIntentUi.statusGlyph(c) != null)
        }
        assertTrue(ai.contains("NONE = 0") && ai.contains("HEX = 10"))
    }
}
