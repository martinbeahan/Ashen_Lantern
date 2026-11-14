package com.fintrack.dndbeginnerremote

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

/** v2.31 universal stats: the native stat guide, caps on the level-up screen, help text. */
class StatGuideTest {
    // Shape of Game::getStatGuide(): rules|main|caps|base|effective|STR..CHA lines|signature
    private val solo = "1|1|20,22,20,20,20,20|15,20,14,10,12,8|15,21,14,10,12,8|" +
        "Everyone: Heft +1 weapon damage; breaks shield-walls at STR 16; STR saves resist Shackle.|" +
        "Main for Rogue: hit, damage and Sneak Attack. Everyone: AC +5 (max +6), initiative +5|" +
        "Everyone: HP +2 per level|Everyone: Search +0|Everyone: Mending +4%|Everyone: Barter −0%|" +
        "Signature (main 21): ✓ 20 +1 Sneak Attack per battle  · 24 Sneak Attack +25%"
    private val online = "0|0|0,0,0,0,0,0|16,14,14,10,10,10|16,14,14,10,10,10|" +
        "Main for Fighter: hit, damage and Action Surge. Online: main stat only.|AC +2|HP +2 per level|Search +0|" +
        "Online: main stat only.|Online: main stat only.|"

    @Test
    fun parsesSoloGuide() {
        val g = StatGuide.parse(solo)!!
        assertTrue(g.active)
        assertEquals(1, g.mainIndex)
        assertEquals(22, g.caps[1])
        assertEquals(20, g.base[1])
        assertEquals(21, g.effective[1])
        assertEquals(6, g.lines.size)
        assertTrue(g.lines[1].startsWith("Main for Rogue"))
        assertTrue(g.signature.startsWith("Signature"))
    }

    @Test
    fun capsLimitThePlusButton() {
        val g = StatGuide.parse(solo)!!
        assertEquals(2, g.room(1))   // DEX 20 / main cap 22
        assertEquals(5, g.room(0))   // STR 15 / cap 20
        assertFalse(g.atCap(1))
        val full = StatGuide.parse(solo.replace("15,20,14,10,12,8|", "20,22,14,10,12,8|"))!!
        assertTrue(full.atCap(0) && full.atCap(1))
        assertEquals(0, full.room(0))
        assertEquals("STR · Strength · cap 20", g.rowLabel(0))
        assertEquals("DEX · Dexterity ★ main · cap 22", g.rowLabel(1))
        assertEquals("Now 20 → 22  (at cap)", g.nowLine(1, 22))
        assertEquals("Now 20 → 21", g.nowLine(1, 21))
    }

    @Test
    fun onlineHasNoCaps() {
        val g = StatGuide.parse(online)!!
        assertFalse(g.active)
        assertEquals(Int.MAX_VALUE, g.room(0))
        assertFalse(g.atCap(0))
        assertEquals("STR · Strength ★ main", g.rowLabel(0))
        assertEquals("", g.signature)
        assertTrue(g.helpText("Mira").contains("classic stat rules"))
    }

    @Test
    fun rejectsGarbage() {
        assertNull(StatGuide.parse(null))
        assertNull(StatGuide.parse(""))
        assertNull(StatGuide.parse("1|2|3"))
        assertNull(StatGuide.parse(solo.replace("20,22,20,20,20,20", "20,22,x,20,20,20")))
        assertNull(StatGuide.parse(solo.replaceFirst("1|1|", "1|9|")))
    }

    @Test
    fun helpTextShowsEveryStatForTheClass() {
        val text = StatGuide.parse(solo)!!.helpText("Mira")
        assertTrue(text.startsWith("What each stat does for Mira"))
        for (k in StatGuide.SHORT) assertTrue(k, text.contains("• $k "))
        assertTrue(text.contains("Caps: 18 + half your level"))
        assertTrue(text.contains("Signature"))
    }

    @Test
    fun beginnerGuideUsesTheNewRulesInSolo() {
        val help = BeginnerGuide.statsHelpMessage(universal = true, forHero = StatGuide.parse(solo), heroName = "Mira")
        assertTrue(help.contains("Every stat helps every class"))
        assertTrue(help.contains("What each stat does for Mira"))
        assertTrue(help.contains("Signature (main stat): 20"))
        val classic = BeginnerGuide.statsHelpMessage()
        assertTrue(classic.contains("classic rules"))
        for (c in 0..4) assertTrue(BeginnerGuide.classStatTip(c, universal = true).contains("main stat"))
        for (i in 0..5) assertTrue(BeginnerGuide.universalStatBlurb(i).contains("Everyone"))
        // No third-party tabletop branding in player-facing help.
        val all = help + classic + (0..5).joinToString { BeginnerGuide.universalStatBlurb(it) }
        for (bad in listOf("D&D", "Dungeons & Dragons", "Wizards of the Coast", "WotC")) assertFalse(bad, all.contains(bad))
    }

    @Test
    fun nativeGuideFormatMatchesThisParser() {
        // Keep the C++ producer and this parser in step: 12 '|' fields, rules first.
        val root = generateSequence(File("").absoluteFile) { it.parentFile }
            .firstOrNull { File(it, "app/src/main/cpp/Game.cpp").exists() } ?: return
        val cpp = File(root, "app/src/main/cpp/Game.cpp").readText()
        assertTrue(cpp.contains("std::string Game::getStatGuide("))
        assertTrue(cpp.contains("ss << (on ? 1 : 0) << \"|\" << main << \"|\";"))
    }
}
