package com.fintrack.dndbeginnerremote

import android.content.SharedPreferences

/**
 * Challenge Dungeon meta — Legendary drop chance + attempt counter in prefs.
 * Never writes Host/Join into save_state; solo Continue save is loaded like Boss Raid.
 *
 * Formula (documented in docs/CHALLENGE_DUNGEON.md):
 * - BASE 18% (normal endgame/Boss Raid is flat 7%)
 * - After each Legendary found: chance -= DECAY (3 pp), floored at FLOOR (5%)
 * - Every ATTEMPTS_PER_BOOST (50) attempts: chance += BOOST (4 pp), capped at CAP (28%)
 * - Number of Legendaries findable: uncapped
 */
object ChallengeDungeon {
    const val BASE_CHANCE = 18
    const val DECAY_PER_LEGENDARY = 3
    const val FLOOR = 5
    const val ATTEMPTS_PER_BOOST = 50
    const val BOOST = 4
    const val CAP = 28

    private const val KEY_ATTEMPTS = "challenge_dungeon_attempts"
    private const val KEY_CHANCE = "challenge_dungeon_legendary_chance"

    fun legendaryChance(prefs: SharedPreferences): Int {
        ensureInitialized(prefs)
        return prefs.getInt(KEY_CHANCE, BASE_CHANCE).coerceIn(FLOOR, CAP)
    }

    fun attempts(prefs: SharedPreferences): Int {
        ensureInitialized(prefs)
        return prefs.getInt(KEY_ATTEMPTS, 0).coerceAtLeast(0)
    }

    fun ensureInitialized(prefs: SharedPreferences) {
        if (!prefs.contains(KEY_CHANCE)) {
            prefs.edit().putInt(KEY_CHANCE, BASE_CHANCE).apply()
        }
    }

    /**
     * Call when a Challenge Dungeon run successfully starts.
     * Increments attempts; every 50th attempt boosts chance toward CAP.
     * @return new chance after any attempt boost
     */
    fun onAttemptStarted(prefs: SharedPreferences): Int {
        ensureInitialized(prefs)
        val nextAttempts = attempts(prefs) + 1
        var chance = legendaryChance(prefs)
        val ed = prefs.edit().putInt(KEY_ATTEMPTS, nextAttempts)
        if (nextAttempts % ATTEMPTS_PER_BOOST == 0) {
            chance = (chance + BOOST).coerceAtMost(CAP)
            ed.putInt(KEY_CHANCE, chance)
        }
        ed.apply()
        return chance
    }

    /** After a Legendary drop in Challenge Dungeon — noticeable decay, never below FLOOR. */
    fun onLegendaryFound(prefs: SharedPreferences): Int {
        ensureInitialized(prefs)
        val next = (legendaryChance(prefs) - DECAY_PER_LEGENDARY).coerceAtLeast(FLOOR)
        prefs.edit().putInt(KEY_CHANCE, next).apply()
        return next
    }

    fun summaryLine(prefs: SharedPreferences): String {
        val a = attempts(prefs)
        val c = legendaryChance(prefs)
        val untilBoost = ATTEMPTS_PER_BOOST - (a % ATTEMPTS_PER_BOOST)
        return "Legendary chance $c% · Attempts $a · Next forge boost in $untilBoost"
    }
}
