package com.fintrack.dndbeginnerremote

import android.content.SharedPreferences

/**
 * Ember Ring Arena meta — best score + run counter in prefs.
 * Never writes Host/Join into save_state; solo Continue save is loaded like Boss Raid / Challenge.
 *
 * Rules (docs/ARENA.md):
 * - Story-complete + Continue hero; no Raid Key.
 * - 5 scored waves; Rare/Epic on clears; Legendary only on Wave 5 apex (flat 7%, same as Boss Raid).
 */
object Arena {
    const val MAX_WAVES = 5
    /** Display name for the venue (not Challenge Dungeon / Cinder Crucible). */
    const val VENUE_NAME = "Ember Ring"

    private const val KEY_BEST_SCORE = "arena_best_score"
    private const val KEY_RUNS = "arena_runs_completed"
    private const val KEY_BEST_WAVES = "arena_best_waves"

    fun bestScore(prefs: SharedPreferences): Int =
        prefs.getInt(KEY_BEST_SCORE, 0).coerceAtLeast(0)

    fun runsCompleted(prefs: SharedPreferences): Int =
        prefs.getInt(KEY_RUNS, 0).coerceAtLeast(0)

    fun bestWaves(prefs: SharedPreferences): Int =
        prefs.getInt(KEY_BEST_WAVES, 0).coerceIn(0, MAX_WAVES)

    /**
     * Call when an Arena run ends (clear or wipe that leaves the mode).
     * @param score final score from native
     * @param wavesCleared how many waves were fully cleared (0..MAX_WAVES)
     */
    fun onRunFinished(prefs: SharedPreferences, score: Int, wavesCleared: Int) {
        val s = score.coerceAtLeast(0)
        val w = wavesCleared.coerceIn(0, MAX_WAVES)
        val ed = prefs.edit().putInt(KEY_RUNS, runsCompleted(prefs) + 1)
        if (s > bestScore(prefs)) ed.putInt(KEY_BEST_SCORE, s)
        if (w > bestWaves(prefs)) ed.putInt(KEY_BEST_WAVES, w)
        ed.apply()
    }

    fun summaryLine(prefs: SharedPreferences): String {
        val best = bestScore(prefs)
        val runs = runsCompleted(prefs)
        val waves = bestWaves(prefs)
        return if (runs == 0) {
            "5 waves · score for gold & loot · Wave 5 may drop Legendary (7%)"
        } else {
            "Best $best · Waves $waves/$MAX_WAVES · Runs $runs"
        }
    }
}
