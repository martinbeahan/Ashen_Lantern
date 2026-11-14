package com.fintrack.dndbeginnerremote

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/** v2.33 Rekindling wording and parsing (native string from GamePerks.cpp getRekindleState). */
class RekindlingTest {
    private val perks = (1..10).joinToString("\n") { "P|$it|perk $it" }
    private fun raw(ready: Int = 1, can: Int = 1, offered: Int = 0, rank: Int = 0, why: String = "", diff: Int = 1, attune: Int = -1) =
        "R|$ready|$can|$offered|$rank|${(rank + 1).coerceAtMost(10)}|$why|50|$diff|$attune|${Rekindling.title(rank)}|" +
            "${Rekindling.title((rank + 1).coerceAtMost(10))}|2|2\n$perks"

    @Test fun parsesState() {
        val s = Rekindling.parse(raw(rank = 2, attune = 15))!!
        assertTrue(s.ready && s.canNow && !s.offered)
        assertEquals(2, s.rank); assertEquals(3, s.nextRank)
        assertEquals("Rekindled II", s.title); assertEquals("Rekindled III", s.nextTitle)
        assertEquals(15, s.attuneCap); assertEquals(10, s.perks.size)
        assertNull(Rekindling.parse("")); assertNull(Rekindling.parse("H|x"))
    }

    @Test fun titlesAndBadges() {
        assertEquals("", Rekindling.title(0)); assertEquals("Rekindled III", Rekindling.title(3)); assertEquals("Everflame", Rekindling.title(10))
        assertEquals(Rekindling.Badge.NONE, Rekindling.badge(0)); assertEquals(Rekindling.Badge.AMBER, Rekindling.badge(7))
        assertEquals(Rekindling.Badge.EMBER_WHITE, Rekindling.badge(8)); assertEquals(Rekindling.Badge.EVERFLAME, Rekindling.badge(10))
        assertTrue(Rekindling.badgeAnimates(10, reduceFlash = false))
        assertFalse(Rekindling.badgeAnimates(10, reduceFlash = true)) // reduce-flash: static badge
        assertFalse(Rekindling.badgeAnimates(5, reduceFlash = false))
        assertEquals("Lv 12 · Fighter · Rekindled III", Rekindling.slotLevelLine(12, "Fighter", 3))
        assertEquals("Lv 12 · Fighter", Rekindling.slotLevelLine(12, "Fighter", 0))
    }

    @Test fun explainerIsPlainAndComplete() {
        val s = Rekindling.parse(raw())!!
        val t = Rekindling.explainer(s, "Mira")
        for (needle in listOf("optional", "Not now", "level 1", "gold", "story", "+1 to every attribute", "+2% max HP",
                "+5% XP", "companion", "Mythical gear and The Drowned Vigil", "Re-temper", "+ 3", "Ready to Rekindle"))
            assertTrue("explainer misses '$needle'", t.contains(needle))
        assertFalse(t.contains("Nightmare"))
        val n = Rekindling.explainer(Rekindling.parse(raw(diff = 3))!!, "Mira")
        assertTrue(n.contains("Nightmare") && n.contains("lost"))
        val blocked = Rekindling.explainer(Rekindling.parse(raw(can = 0, why = "Not during a fight."))!!, "Mira")
        assertTrue(blocked.contains("Not right now: Not during a fight."))
        val c = Rekindling.confirmText(s, "Mira")
        assertTrue(c.contains("Rekindle Mira to Rekindled I?") && c.contains("can't be undone") && c.contains("Keeps"))
    }

    @Test fun sheetAndWarnings() {
        assertNull(Rekindling.sheetButtonLabel(null))
        assertNull(Rekindling.sheetButtonLabel(Rekindling.parse(raw(ready = 0))))
        assertEquals("Ready to Rekindle → Rekindled I", Rekindling.sheetButtonLabel(Rekindling.parse(raw())))
        assertNull(Rekindling.sheetLine(Rekindling.parse(raw())))
        assertTrue(Rekindling.sheetLine(Rekindling.parse(raw(rank = 3, attune = 15)))!!.contains("+15"))
        assertEquals("", Rekindling.nightmareWarning(3, 1, "Mira"))
        assertEquals("", Rekindling.nightmareWarning(0, 3, "Mira"))
        assertTrue(Rekindling.nightmareWarning(3, 3, "Mira").contains("loses Rekindled III"))
        assertEquals("", Rekindling.lostRankLine(0))
        assertTrue(Rekindling.lostRankLine(10).contains("Everflame"))
    }

    @Test fun slotSummaryCarriesRank() {
        val old = SaveSlots.Summary.parse("Mira\t0\t12\t7\tAct\tBeat\t1\t1\t0\t30\t1")
        assertNotNull(old); assertEquals(0, old!!.prestigeRank)
        val neu = SaveSlots.Summary.parse("Mira\t0\t12\t7\tAct\tBeat\t1\t1\t0\t30\t1\t4")
        assertEquals(4, neu!!.prestigeRank)
    }

    @Test fun kindledPathHeaderSlots() {
        val h = "H|Mira|0|1|50|12|0|12|1|0|1||100|0|3|3|10"
        val st = KindledPath.parse(h)!!
        assertEquals(3, st.header.wardSlots); assertEquals(3, st.header.artSlots); assertEquals(10, st.header.rekindleRank)
        val old = KindledPath.parse("H|Mira|0|30|50|10|0|10|1|0|1||100|0")!!
        assertEquals(2, old.header.wardSlots); assertEquals(0, old.header.rekindleRank)
    }
}
