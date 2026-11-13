package com.fintrack.dndbeginnerremote

import org.junit.Assert.assertEquals
import org.junit.Test

/** Story stage mapping; v2.25 moved beat 25 (Vigil Kindled) to the kindling pyre. */
class StageMapTest {
    private fun story(beat: Int) = StageMap.stageForContext(0, beat, false, emptyList())

    @Test
    fun kindlingBeats() {
        assertEquals("kindling_pyre", story(25))
        assertEquals("kindling_pyre", story(1))
        assertEquals("kindling_pyre", story(6))
        for (b in listOf(8, 13, 14, 20)) assertEquals("millhollow_green", story(b))
        assertEquals("mill_hall", story(12))
        assertEquals("emberdeep", story(19))
        assertEquals("greyfen_marsh", story(24))
        assertEquals("catacombs", story(7))
    }

    @Test
    fun modesUnchanged() {
        assertEquals("ember_ring", StageMap.stageForContext(4, 25, false, emptyList()))
        assertEquals("raid_kilnmouth", StageMap.stageForContext(2, 0, false, listOf("boss_ember_hydra")))
        assertEquals("millhollow_green", StageMap.stageForContext(1, 0, true, emptyList()))
    }
}
