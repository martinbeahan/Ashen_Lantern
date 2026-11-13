package com.fintrack.dndbeginnerremote

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

/**
 * v2.27 gear glyphs. The native game is the source of truth for item names, so this test
 * scans the C++ sources for every item it can create (Item::make literals and the loot
 * NamedItem / generic tables) and requires an explicit glyph for each one — a new item
 * without art fails the build instead of silently showing the type fallback.
 */
class GearIconsTest {

    private fun cppDir(): File {
        val candidates = listOf(File("src/main/cpp"), File("app/src/main/cpp"))
        return candidates.firstOrNull { it.isDirectory } ?: error("C++ sources not found from ${File(".").absolutePath}")
    }

    /** Item names the native game can create. */
    private fun nativeItemNames(): Set<String> {
        val names = linkedSetOf<String>()
        val make = Regex("""Item::make\(\s*"([^"]+)"\s*,\s*ItemType::""")
        val namedItem = Regex("""\{\s*"([^"]+)"\s*,\s*-?\d+\s*\}""")
        val tableLine = Regex("""(?:NamedItem\s+k\w+(?:\[\w*\])?|const char\*\s+k\w+\[\w*\])\s*=""")
        cppDir().walkTopDown().filter { it.isFile && (it.extension == "h" || it.extension == "cpp") }.forEach { f ->
            val text = f.readText()
            make.findAll(text).forEach { names += it.groupValues[1] }
            // Loot tables: `static constexpr NamedItem kX[...] = { {"Name", n}, ... };`
            // and `static constexpr const char* kGenericWeapons[...] = { "A", "B" };`
            var idx = 0
            while (true) {
                val m = tableLine.find(text, idx) ?: break
                val end = text.indexOf(';', m.range.last)
                val body = text.substring(m.range.last + 1, end)
                if (m.value.startsWith("NamedItem")) {
                    namedItem.findAll(body).forEach { names += it.groupValues[1] }
                } else {
                    Regex(""""([^"]+)"""").findAll(body).forEach { names += it.groupValues[1] }
                }
                idx = end
            }
        }
        return names
    }

    @Test
    fun everyNativeItemHasAnExplicitGlyph() {
        val names = nativeItemNames()
        // Sanity: the scan really found the tables (starter gear, shop, legendaries, generic loot).
        for (known in listOf("Longsword", "Healing Draught", "Crownbreaker", "Arcane Wand", "War Maul",
                "Troubadour Coat", "Wayfarer's Tonic", "Ashen Lantern", "Emberdeep Relic Mail")) {
            assertTrue("scan missed $known", known in names)
        }
        assertTrue("expected 50+ native item names, got ${names.size}", names.size >= 50)
        assertFalse("non-item string leaked into the scan", "E2" in names)
        val missing = names.filterNot { GearIcons.hasExplicitGlyph(it) }
        assertTrue("items without a gear glyph (add them to GearIcons.BY_NAME): $missing", missing.isEmpty())
        val stale = GearIcons.BY_NAME.keys - names
        assertTrue("GearIcons maps names the game never creates: $stale", stale.isEmpty())
    }

    @Test
    fun everyGlyphAndVariantHasADrawable() {
        assertEquals(18, GearIcons.ALL_GLYPHS.size)
        for (g in GearIcons.ALL_GLYPHS) {
            assertTrue(g, GearIcons.hasDrawable("ic_gear_$g"))
            val leg = g in GearIcons.LEGENDARY_GLYPHS
            assertEquals("$g legendary", leg, GearIcons.hasDrawable("ic_gear_${g}_legendary"))
            assertEquals("$g legendary calm", leg, GearIcons.hasDrawable("ic_gear_${g}_legendary_calm"))
        }
        for (g in GearIcons.BY_NAME.values) assertTrue(g, g in GearIcons.ALL_GLYPHS)
    }

    @Test
    fun lookupFallbackAndCalm() {
        assertEquals(GearIcons.MACE, GearIcons.glyphFor("Dawnward Mace", "Weapon"))
        assertEquals(GearIcons.COAT, GearIcons.glyphFor(" Troubadour Coat ", "Armor"))
        assertEquals(GearIcons.POTION, GearIcons.glyphFor("Greater Potion", "Potion"))
        assertFalse(GearIcons.hasExplicitGlyph("Blade of Tomorrow"))
        assertEquals(GearIcons.SWORD, GearIcons.glyphFor("Blade of Tomorrow", "Weapon"))
        assertEquals(GearIcons.MAIL, GearIcons.glyphFor("Coat of Tomorrow", "Armor"))
        assertEquals(GearIcons.POTION, GearIcons.glyphFor("Draught of Tomorrow", "Potion"))
        assertEquals(GearIcons.SWORD, GearIcons.glyphFor("Legacy shop line", "Item"))

        assertEquals("ic_gear_mace", GearIcons.stemFor(GearIcons.MACE, "Epic", calm = true))
        assertEquals("ic_gear_mace_legendary", GearIcons.stemFor(GearIcons.MACE, "Legendary", calm = false))
        assertEquals("ic_gear_mace_legendary_calm", GearIcons.stemFor(GearIcons.MACE, "legendary", calm = true))
        // Never-legendary glyphs keep their base art even if a Legendary label shows up.
        assertEquals("ic_gear_ashen_lantern", GearIcons.stemFor(GearIcons.ASHEN_LANTERN, "Legendary", calm = false))
        assertEquals(R.drawable.ic_gear_sword_legendary_calm,
            GearIcons.forItem("Crownbreaker", "Weapon", "Legendary", calm = true))
        assertEquals(R.drawable.ic_gear_plate, GearIcons.forItem("Knight Plate", "Armor", "Rare", calm = false))
        assertEquals(R.drawable.bg_gear_socket_legendary, GearIcons.socketFor("Legendary"))
        assertEquals(R.drawable.bg_gear_socket_common, GearIcons.socketFor("weird"))
    }
}
