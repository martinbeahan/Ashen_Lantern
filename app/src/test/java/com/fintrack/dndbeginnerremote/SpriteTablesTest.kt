package com.fintrack.dndbeginnerremote

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/** SpriteTables was split out of MainActivity (Code Auditor L10); lock in its behaviour. */
class SpriteTablesTest {
    @Test
    fun enemyTableIsComplete() {
        assertNull(SpriteTables.tableProblem())
        assertEquals(R.drawable.enemy_unknown, SpriteTables.spriteForEnemyId("enemy_from_the_future", calm = false))
        assertEquals(R.drawable.boss_prior_calm, SpriteTables.spriteForEnemyId("boss_prior", calm = true))
        assertEquals(R.drawable.enemy_brute, SpriteTables.spriteForEnemyId("enemy_brute", calm = true))
        assertTrue(SpriteTables.isAshEmberSprite(R.drawable.boss_nightfang_calm))
    }

    @Test
    fun heroFrames() {
        assertEquals(R.drawable.sprite_bard, SpriteTables.heroSprite(4))
        assertEquals(R.drawable.sprite_fighter, SpriteTables.heroSprite(99))
        assertEquals(R.drawable.sprite_bard_atk, SpriteTables.attackSprite(R.drawable.sprite_bard, calm = false))
        assertEquals(R.drawable.sprite_cleric_atk_calm, SpriteTables.attackSprite(R.drawable.sprite_cleric, calm = true))
        assertEquals(R.drawable.enemy_brute, SpriteTables.attackSprite(R.drawable.enemy_brute, calm = false))
    }
}
