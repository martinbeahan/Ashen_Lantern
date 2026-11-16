package com.fintrack.dndbeginnerremote

import org.junit.Assert.assertEquals
import org.junit.Test

class OnlineFairnessTest {
    @Test
    fun tableDifficultyKeepsTheHostChoice() {
        for (d in 0..3) assertEquals(d, OnlineFairness.tableDifficulty(d))
    }

    @Test
    fun badPrefsFallBackToEasy() {
        assertEquals(0, OnlineFairness.tableDifficulty(-1))
        assertEquals(0, OnlineFairness.tableDifficulty(4))
        assertEquals(0, OnlineFairness.tableDifficulty(Int.MAX_VALUE))
    }

    @Test
    fun aLevelOneHeroNeedsThreeHitsToDrop() {
        // 12 HP (the level-1 floor): at most 5 per hit, so no hit takes a fresh joiner from full to 0.
        assertEquals(5, OnlineFairness.maxHit(12))
        assertEquals(1, OnlineFairness.maxHit(0))
        assertEquals(45, OnlineFairness.maxHit(100))
    }
}
