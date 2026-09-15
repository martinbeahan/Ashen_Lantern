package com.fintrack.dndbeginnerremote

import android.content.SharedPreferences

/**
 * Endless Deep (Ashen Deep) meta — best depth + run counter in prefs.
 * Never writes Host/Join into save_state; solo Continue save is loaded like Boss Raid / Arena.
 *
 * Rules (docs/ENDLESS_DEEP.md):
 * - Story-complete + Continue hero; no Raid Key.
 * - Endless depth-scaling rooms; soft end via death / retreat / milestone.
 * - Legendary only on depth-10+ apex (endgame bosses), flat 7%.
 */
object EndlessDeep {
    /** Display name for the venue (not Ember Ring / Cinder Crucible / normal Crawl). */
    const val VENUE_NAME = "Ashen Deep"
    /** Depth multiples that spawn elite packs (soft milestone). */
    const val MILESTONE_EVERY = 5
    /** Depth multiples that spawn endgame apex (Legendary-gated). */
    const val APEX_EVERY = 10

    private const val KEY_BEST_DEPTH = "endless_deep_best_depth"
    private const val KEY_RUNS = "endless_deep_runs_completed"
    private const val KEY_BEST_MILESTONE = "endless_deep_best_milestone"

    fun bestDepth(prefs: SharedPreferences): Int =
        prefs.getInt(KEY_BEST_DEPTH, 0).coerceAtLeast(0)

    fun runsCompleted(prefs: SharedPreferences): Int =
        prefs.getInt(KEY_RUNS, 0).coerceAtLeast(0)

    fun bestMilestone(prefs: SharedPreferences): Int =
        prefs.getInt(KEY_BEST_MILESTONE, 0).coerceAtLeast(0)

    /**
     * Call when an Endless Deep run ends (death, retreat, or milestone soft exit).
     * @param depthReached deepest depth entered this run (1+)
     */
    fun onRunFinished(prefs: SharedPreferences, depthReached: Int) {
        val d = depthReached.coerceAtLeast(0)
        val milestone = if (d <= 0) 0 else (d / MILESTONE_EVERY) * MILESTONE_EVERY
        val ed = prefs.edit().putInt(KEY_RUNS, runsCompleted(prefs) + 1)
        if (d > bestDepth(prefs)) ed.putInt(KEY_BEST_DEPTH, d)
        if (milestone > bestMilestone(prefs)) ed.putInt(KEY_BEST_MILESTONE, milestone)
        ed.apply()
    }

    fun summaryLine(prefs: SharedPreferences): String {
        val best = bestDepth(prefs)
        val runs = runsCompleted(prefs)
        return if (runs == 0) {
            "Endless rooms · depth scales foes · apex every $APEX_EVERY may drop Legendary (7%)"
        } else {
            "Best depth $best · Milestone ${bestMilestone(prefs)} · Runs $runs"
        }
    }

    fun isMilestone(depth: Int): Boolean =
        depth > 0 && depth % MILESTONE_EVERY == 0

    fun isApexDepth(depth: Int): Boolean =
        depth > 0 && depth % APEX_EVERY == 0
}
