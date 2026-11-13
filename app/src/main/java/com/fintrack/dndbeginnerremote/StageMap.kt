package com.fintrack.dndbeginnerremote

/** Stage background mapping (moved out of MainActivity so it can be unit-tested). */
object StageMap {
    /**
     * Stage art by play mode + story beat (Phase 3 mapping, docs/ASH_EMBER_PHASE3.md) — replaces the
     * old room-text keyword matching. Raids pick by the boss enemyId in the roster; any boss fight
     * adds bg_stage_boss_overlay. Pure function so the mapping is easy to audit.
     */
    fun stageForContext(mode: Int, beat: Int, merchant: Boolean, rosterIds: List<String>): String {
        return when (mode) {
            4 -> "ember_ring"        // ARENA
            3 -> "cinder_crucible"   // CHALLENGE
            5 -> "ashen_deep"        // ENDLESS
            2 -> when {              // RAID
                "boss_ember_hydra" in rosterIds -> "raid_kilnmouth"
                "boss_nightfang" in rosterIds -> "raid_weftholme"
                else -> "raid_corrans_rest" // boss_hollow_crown + default
            }
            1 -> if (merchant) "millhollow_green" else "catacombs" // CRAWL
            else -> when (beat) {    // STORY (0) and anything else
                // 25 Vigil Kindled: the reversed siphon flares like a funeral pyre (v2.25; was the green)
                1, 6, 25 -> "kindling_pyre"
                2 -> "thornpath_woods"
                3, 4, 5 -> "ossuary"
                8, 13, 14, 20 -> "millhollow_green"
                9, 10 -> "mill_weir"
                11, 12 -> "mill_hall"
                in 15..19 -> "emberdeep"
                in 21..24 -> "greyfen_marsh"
                // 0 = online / Lanternkeeper table, 7 = post-quest procedural
                else -> if (merchant) "millhollow_green" else "catacombs"
            }
        }
    }
}
