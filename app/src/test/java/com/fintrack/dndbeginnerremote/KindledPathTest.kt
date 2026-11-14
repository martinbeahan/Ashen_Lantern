package com.fintrack.dndbeginnerremote

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

/** v2.32 Kindled Path: native string parsing, wording, and the numbers in Perks.h. */
class KindledPathTest {
    private fun src(path: String): String =
        listOf("src/main/$path", "app/src/main/$path").map(::File).first { it.exists() }.readText()

    private val sample = """
        H|Mira|0|30|50|10|7|3|1|0|1||420|4
        F|0|Tempered Arm|+1 STR per rank|2|5
        F|6|Thick Wax|+3% max HP per rank|0|5
        W|0|Last Light|Survive a lethal hit at 1 HP|1|0
        W|3|Ash Veil|The first hit you take is halved|0|-1
        A|0|Cleave|Strike 2 foes at 75% damage|2|22|1|1|0
        A|3|Warcry|Party +2 to hit and +2 damage for 2 rounds|4|34|0|-1|5
    """.trimIndent()

    @Test fun parsesPerkState() {
        val s = KindledPath.parse(sample)!!
        assertEquals("Mira", s.header.name)
        assertEquals(30, s.header.level)
        assertEquals(3, s.header.free)
        assertTrue(s.header.canEdit)
        assertEquals(1, s.header.freeRespecs)
        assertEquals(4, s.header.signatureTier)
        assertEquals(2, s.flames.size)
        assertEquals(2, s.flames[0].rank)
        assertTrue(s.wards[0].learned && s.wards[0].slot == 0)
        assertEquals(1, s.arts[0].slot)
        assertTrue(KindledPath.canLearnFlame(s, s.flames[1]))
        assertTrue(KindledPath.canLearnWard(s, s.wards[1]))
        assertFalse(KindledPath.canLearnArt(s, s.arts[1])) // unlocks at 34
        assertTrue(KindledPath.artRow(s.arts[1], 30).contains("[level 34]"))
        assertNull(KindledPath.parse(""))
    }

    @Test fun artsBarAndPips() {
        val bar = KindledPath.parseBar("0|0|Cleave|0|1|0|2;1|2|Sunder|2|0|0|3")
        assertEquals(2, bar.size)
        assertTrue(bar[0].ready)
        assertEquals("ready", KindledPath.pips(0, 3))
        assertEquals("●○○", KindledPath.pips(2, 3))
        assertEquals("spent this battle", KindledPath.pips(99, 99))
        assertEquals("✸\nArts 1/2", KindledPath.buttonLabel(bar))
        assertTrue(KindledPath.parseBar("").isEmpty())
    }

    @Test fun xpLabelAtTheCap() {
        assertEquals("XP 50/2000 · Level 20 → 21", KindledPath.xpLabel(50, 2000, 20))
        assertEquals("XP max · Level 50 (level cap)", KindledPath.xpLabel(4900, 4900, 50))
    }

    @Test fun wordingIsPlainAndUnbranded() {
        val h = KindledPath.parse(sample)!!.header
        assertTrue(KindledPath.retemperPrompt(h).contains("free"))
        assertTrue(KindledPath.retemperPrompt(h.copy(freeRespecs = 0, retemperCost = 3000)).contains("3000 gold"))
        assertTrue(KindledPath.emberLine(h.copy(level = 12, earned = 0)).contains("level 21"))
        for (t in listOf(KindledPath.helpText, BeginnerGuide.actionReference())) {
            assertFalse(t.contains("D&D") || t.contains("Dungeons & Dragons") || t.contains("WotC"))
        }
        assertTrue(KindledPath.helpText.contains("Online tables use the classic rules"))
    }

    @Test fun numbersMatchPerksH() {
        val perks = src("cpp/Perks.h")
        assertTrue(perks.contains("constexpr int kLevelCap = ${KindledPath.LEVEL_CAP};"))
        assertTrue(perks.contains("constexpr int kRetemperGoldPerLevel = ${KindledPath.RETEMPER_GOLD_PER_LEVEL};"))
        assertTrue(perks.contains("constexpr int kEmberFirstLevel = ${KindledPath.EMBER_FIRST_LEVEL};"))
        // 30 from levels + 1 per Rekindling rank I-X + 2 bonus = 42
        assertEquals(KindledPath.EMBER_CAP, KindledPath.EMBERS_FROM_LEVELS + 10 + 2)
        assertEquals(KindledPath.PATH_TOTAL, 8 * 5 + 8 * 2 + 4 * 3)
    }

    @Test fun everyNativeStatusCodeHasAGlyphAndName() {
        val ai = src("cpp/EnemyAi.h")
        val body = ai.substringAfter("inline std::string statusCodes").substringBefore("return out;")
        val codes = Regex("""add\('(\w)'""").findAll(body).map { it.groupValues[1][0] }.toList()
        assertTrue(codes.size >= 18)
        for (c in codes) {
            assertNotNull("glyph for $c", EnemyIntentUi.statusGlyph(c))
            assertNotNull("name for $c", EnemyIntentUi.statusName(c))
        }
    }

    @Test fun jniForKindledPathIsWired() {
        val main = src("cpp/main.cpp")
        val kt = src("java/com/fintrack/dndbeginnerremote/MainActivity.kt")
        for (fn in listOf("doUseArt", "getArtsBar", "getPerkState", "doLearnPerk", "doEquipPerk", "doRetemper")) {
            assertTrue(fn, main.contains("MainActivity_$fn("))
            assertTrue(fn, kt.contains("external fun $fn("))
        }
    }
}
