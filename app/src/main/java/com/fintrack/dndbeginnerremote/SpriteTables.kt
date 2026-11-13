package com.fintrack.dndbeginnerremote

/**
 * Sprite lookup tables for the battle stage (moved out of MainActivity — Code Auditor L10).
 * Pure data + pure functions; MainActivity passes in the current Reduce-flashing setting.
 * tools/check_enemy_sprites.py parses [enemySprites] / [calmEnemySprites] from this file.
 */
object SpriteTables {
    /**
     * Enemy sprite table keyed by the stable enemy ID (native Bestiary.h). Every one of the 27
     * IDs has its own same-named Ash & Ember VectorDrawable (sources in docs/art/ash_ember/,
     * converted with tools/svg2vd.py) — nothing borrows another foe's art. All are authored
     * facing left and have no attack frame (attacks are animated in code).
     */
    val enemySprites: Map<String, Int> = mapOf(
        "enemy_wickthief" to R.drawable.enemy_wickthief,
        "enemy_wickthief_scout" to R.drawable.enemy_wickthief_scout,
        "enemy_wickthief_cutpurse" to R.drawable.enemy_wickthief_cutpurse,
        "enemy_wickthief_cache" to R.drawable.enemy_wickthief_cache,
        "enemy_wickthief_river" to R.drawable.enemy_wickthief_river,
        "enemy_wickthief_root" to R.drawable.enemy_wickthief_root,
        "enemy_wickthief_fen" to R.drawable.enemy_wickthief_fen,
        "enemy_ashbound" to R.drawable.enemy_ashbound,
        "enemy_ashbound_bonebound" to R.drawable.enemy_ashbound_bonebound,
        "enemy_gloamhound" to R.drawable.enemy_gloamhound,
        "enemy_crier_bone" to R.drawable.enemy_crier_bone,
        "enemy_crier_wick" to R.drawable.enemy_crier_wick,
        "enemy_tallow_rat" to R.drawable.enemy_tallow_rat,
        "enemy_brute" to R.drawable.enemy_brute,
        "enemy_tithe_enforcer" to R.drawable.enemy_tithe_enforcer,
        "enemy_tithe_scavenger" to R.drawable.enemy_tithe_scavenger,
        "enemy_tithe_watcher" to R.drawable.enemy_tithe_watcher,
        "boss_sexton" to R.drawable.boss_sexton,
        "boss_collector" to R.drawable.boss_collector,
        "boss_warden" to R.drawable.boss_warden,
        "boss_herald" to R.drawable.boss_herald,
        "boss_wick_king" to R.drawable.boss_wick_king,
        "boss_prior" to R.drawable.boss_prior,
        "boss_drake" to R.drawable.boss_drake,
        "boss_hollow_crown" to R.drawable.boss_hollow_crown,
        "boss_ember_hydra" to R.drawable.boss_ember_hydra,
        "boss_nightfang" to R.drawable.boss_nightfang,
        // Unknown / future IDs from a newer host: neutral shadow, never a goblin.
        "enemy_unknown" to R.drawable.enemy_unknown
    )

    /**
     * Reduce-flash variants (tools/svg2vd.py --calm): the bosses whose amber light is a large
     * glowing area — Ember Hydra's furnace door, the First Lamplighter's chest arch, the Ossuary
     * Prior's censer and the Nightfang Matriarch's caged soul — swap to a version whose amber
     * glow halos keep 30% alpha and solid amber 55%.
     */
    val calmEnemySprites: Map<String, Int> = mapOf(
        "boss_ember_hydra" to R.drawable.boss_ember_hydra_calm,
        "boss_warden" to R.drawable.boss_warden_calm,
        "boss_prior" to R.drawable.boss_prior_calm,
        "boss_nightfang" to R.drawable.boss_nightfang_calm
    )

    private val ashEmberSpriteRes: Set<Int> by lazy {
        (enemySprites.values + calmEnemySprites.values).toSet()
    }

    /** Ash & Ember vector silhouettes: authored facing left, attack is code-animated (no _atk frame). */
    fun isAshEmberSprite(res: Int): Boolean = res in ashEmberSpriteRes

    fun spriteForEnemyId(enemyId: String, calm: Boolean): Int {
        if (calm) calmEnemySprites[enemyId]?.let { return it }
        return enemySprites[enemyId] ?: enemySprites.getValue("enemy_unknown")
    }

    /**
     * Guard: all 27 Bestiary IDs must resolve to 27 distinct drawables (and the calm variants
     * must be distinct too). Returns a description of the problem, or null when the table is sound.
     */
    fun tableProblem(): String? {
        val ids = enemySprites.keys - "enemy_unknown"
        val distinct = ids.map { enemySprites.getValue(it) }.toSet().size
        val calmClash = calmEnemySprites.values.any { it in enemySprites.values }
        return if (ids.size != 27 || distinct != ids.size || calmClash)
            "ids=${ids.size} distinct=$distinct calmClash=$calmClash" else null
    }

    fun heroSprite(classId: Int): Int = when (classId) {
        0 -> R.drawable.sprite_fighter
        1 -> R.drawable.sprite_wizard
        2 -> R.drawable.sprite_rogue
        3 -> R.drawable.sprite_cleric
        4 -> R.drawable.sprite_bard
        else -> R.drawable.sprite_fighter
    }

    /**
     * Hero attack frame (Ash & Ember heroes share one scale/anchor, so swapping doesn't jump).
     * Reduce flashing uses the --calm _atk variants (amber flares — Cleric rays, Wizard ember
     * bolt, sword arc, lute arcs — toned down to 30% / 55%).
     */
    fun attackSprite(idleRes: Int, calm: Boolean): Int = when (idleRes) {
        R.drawable.sprite_fighter -> if (calm) R.drawable.sprite_fighter_atk_calm else R.drawable.sprite_fighter_atk
        R.drawable.sprite_wizard -> if (calm) R.drawable.sprite_wizard_atk_calm else R.drawable.sprite_wizard_atk
        R.drawable.sprite_rogue -> if (calm) R.drawable.sprite_rogue_atk_calm else R.drawable.sprite_rogue_atk
        R.drawable.sprite_cleric -> if (calm) R.drawable.sprite_cleric_atk_calm else R.drawable.sprite_cleric_atk
        R.drawable.sprite_bard -> if (calm) R.drawable.sprite_bard_atk_calm else R.drawable.sprite_bard_atk
        else -> idleRes
    }

    val heroSpriteRes: Set<Int> = setOf(
        R.drawable.sprite_fighter, R.drawable.sprite_wizard, R.drawable.sprite_rogue,
        R.drawable.sprite_cleric, R.drawable.sprite_bard
    )
}
