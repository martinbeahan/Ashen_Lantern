package com.fintrack.dndbeginnerremote

import android.content.SharedPreferences
import java.util.Calendar
import kotlin.random.Random

/**
 * Local-midnight daily errands for story-complete Continue heroes.
 * Prefs are meta-only (never touch save_state / Host-Join isolation).
 */
object DailyQuests {
    data class Objective(
        val id: String,
        val title: String,
        val blurb: String,
        val metric: String, // rooms|fights|searches|damage|bosses
        val target: Int,
        var progress: Int,
        var complete: Boolean
    )

    data class State(
        val date: String,
        val objectives: List<Objective>,
        val claimed: Boolean,
        val lockedReason: String?
    ) {
        val allComplete: Boolean get() = objectives.isNotEmpty() && objectives.all { it.complete }
        val claimable: Boolean get() = allComplete && !claimed && lockedReason == null
    }

    private val pool = listOf(
        ObjDef("chamber_sweep", "Chamber Sweep", "Clear {n} chambers of foes.", "rooms", 3),
        ObjDef("stand_victorious", "Stand Victorious", "Win {n} fights (clear a chamber).", "fights", 2),
        ObjDef("curious_hands", "Curious Hands", "Search {n} times in cleared rooms.", "searches", 2),
        ObjDef("ash_and_steel", "Ash and Steel", "Deal {n} damage to foes.", "damage", 50),
        ObjDef("cut_the_apex", "Cut the Apex", "Defeat {n} boss.", "bosses", 1),
        ObjDef("deep_push", "Deep Push", "Clear {n} chambers.", "rooms", 5),
        ObjDef("heavy_hands", "Heavy Hands", "Deal {n} damage to foes.", "damage", 80),
        ObjDef("twice_the_steel", "Twice the Steel", "Win {n} fights.", "fights", 3)
    )

    private data class ObjDef(
        val id: String,
        val title: String,
        val blurbTpl: String,
        val metric: String,
        val target: Int
    ) {
        fun blurb(): String = blurbTpl.replace("{n}", target.toString())
    }

    fun calendarDay(): String {
        val cal = Calendar.getInstance()
        return String.format(
            "%04d-%02d-%02d",
            cal.get(Calendar.YEAR),
            cal.get(Calendar.MONTH) + 1,
            cal.get(Calendar.DAY_OF_MONTH)
        )
    }

    fun msUntilLocalMidnight(): Long {
        val cal = Calendar.getInstance()
        cal.add(Calendar.DAY_OF_MONTH, 1)
        cal.set(Calendar.HOUR_OF_DAY, 0)
        cal.set(Calendar.MINUTE, 0)
        cal.set(Calendar.SECOND, 0)
        cal.set(Calendar.MILLISECOND, 0)
        return (cal.timeInMillis - System.currentTimeMillis()).coerceAtLeast(0L)
    }

    /** Ensure today's board exists; rolls 1–2 objectives on day change. */
    fun ensureToday(prefs: SharedPreferences): State {
        val today = calendarDay()
        if (prefs.getString("daily_quests_date", "") != today) {
            rollNewDay(prefs, today)
        }
        return read(prefs, lockedReason = null)
    }

    fun readForUi(
        prefs: SharedPreferences,
        storyComplete: Boolean,
        hasContinueSave: Boolean
    ): State {
        ensureToday(prefs)
        val locked = when {
            !storyComplete -> "Finish the story (Acts 1–3) to unlock Daily Quests."
            !hasContinueSave -> "Keep a Continue save — Daily Quests track your story hero."
            else -> null
        }
        return read(prefs, lockedReason = locked)
    }

    fun applyDelta(
        prefs: SharedPreferences,
        rooms: Int,
        fights: Int,
        searches: Int,
        damage: Int,
        bosses: Int
    ): Boolean {
        if (rooms <= 0 && fights <= 0 && searches <= 0 && damage <= 0 && bosses <= 0) return false
        ensureToday(prefs)
        val count = prefs.getInt("daily_quests_count", 0).coerceIn(0, 2)
        if (count <= 0) return false
        val ed = prefs.edit()
        var changed = false
        for (i in 0 until count) {
            if (prefs.getBoolean("daily_quest_${i}_done", false)) continue
            val metric = prefs.getString("daily_quest_${i}_metric", "") ?: continue
            val target = prefs.getInt("daily_quest_${i}_target", 1).coerceAtLeast(1)
            val cur = prefs.getInt("daily_quest_${i}_progress", 0)
            val add = when (metric) {
                "rooms" -> rooms
                "fights" -> fights
                "searches" -> searches
                "damage" -> damage
                "bosses" -> bosses
                else -> 0
            }
            if (add <= 0) continue
            val next = (cur + add).coerceAtMost(target)
            if (next != cur) {
                ed.putInt("daily_quest_${i}_progress", next)
                if (next >= target) ed.putBoolean("daily_quest_${i}_done", true)
                changed = true
            }
        }
        if (changed) ed.apply()
        return changed
    }

    data class ClaimResult(
        val gold: Int,
        val loot: Boolean,
        val raidKey: Boolean,
        val message: String
    )

    /**
     * Full-clear claim. Caller applies gold/loot via JNI when a solo session is live,
     * or stashes pending_* prefs otherwise. Raid key is written here (meta prefs).
     */
    fun tryClaim(
        prefs: SharedPreferences,
        storyComplete: Boolean,
        hasContinueSave: Boolean,
        raidKeysHeld: Int,
        raidKeysGrantedToday: Int,
        grantRaidKey: () -> Boolean
    ): ClaimResult? {
        val state = readForUi(prefs, storyComplete, hasContinueSave)
        if (!state.claimable) return null
        val gold = 45 + (state.date.hashCode().and(0x7fffffff) % 16) // 45–60
        val giveLoot = true
        var keyGranted = false
        if (raidKeysHeld < 3 && raidKeysGrantedToday < 3) {
            // Fixed grant of 1 key when under cap (small, reliable daily perk).
            keyGranted = grantRaidKey()
        }
        prefs.edit().putBoolean("daily_quests_claimed", true).apply()
        val msg = buildString {
            append("Daily Quests cleared! +${gold}g")
            if (giveLoot) append(" · Wayfarer's Tonic")
            if (keyGranted) append(" · Raid Key")
            else if (raidKeysHeld >= 3) append(" · (keys full — no key)")
        }
        return ClaimResult(gold, giveLoot, keyGranted, msg)
    }

    fun stashPendingSpoils(prefs: SharedPreferences, gold: Int, loot: Boolean) {
        val ed = prefs.edit()
        if (gold > 0) {
            ed.putInt("pending_daily_gold", prefs.getInt("pending_daily_gold", 0) + gold)
        }
        if (loot) {
            ed.putBoolean("pending_daily_loot", true)
        }
        ed.apply()
    }

    /** Returns pending gold/loot and clears the stash flags. */
    fun takePendingSpoils(prefs: SharedPreferences): Pair<Int, Boolean> {
        val gold = prefs.getInt("pending_daily_gold", 0)
        val loot = prefs.getBoolean("pending_daily_loot", false)
        if (gold != 0 || loot) {
            prefs.edit()
                .putInt("pending_daily_gold", 0)
                .putBoolean("pending_daily_loot", false)
                .apply()
        }
        return gold to loot
    }

    fun summaryLines(state: State): String {
        if (state.lockedReason != null) {
            return "Daily Quests (locked)\n${state.lockedReason}"
        }
        if (state.objectives.isEmpty()) return "Daily Quests — none today"
        return buildString {
            append("Daily Quests")
            if (state.claimed) append(" · claimed")
            else if (state.allComplete) append(" · ready to claim!")
            append('\n')
            state.objectives.forEachIndexed { idx, o ->
                val mark = if (o.complete) "✓" else "·"
                append("$mark ${o.title}: ${o.progress}/${o.target}")
                if (idx < state.objectives.lastIndex) append('\n')
            }
        }
    }

    private fun rollNewDay(prefs: SharedPreferences, today: String) {
        val seed = today.hashCode().toLong() xor 0xA51E4L
        val rng = Random(seed)
        val count = if (rng.nextBoolean()) 2 else 1
        val picks = pool.shuffled(rng).take(count)
        val ed = prefs.edit()
            .putString("daily_quests_date", today)
            .putInt("daily_quests_count", picks.size)
            .putBoolean("daily_quests_claimed", false)
        for (i in 0 until 2) {
            ed.remove("daily_quest_${i}_id")
                .remove("daily_quest_${i}_title")
                .remove("daily_quest_${i}_blurb")
                .remove("daily_quest_${i}_metric")
                .remove("daily_quest_${i}_target")
                .remove("daily_quest_${i}_progress")
                .remove("daily_quest_${i}_done")
        }
        picks.forEachIndexed { i, def ->
            ed.putString("daily_quest_${i}_id", def.id)
                .putString("daily_quest_${i}_title", def.title)
                .putString("daily_quest_${i}_blurb", def.blurb())
                .putString("daily_quest_${i}_metric", def.metric)
                .putInt("daily_quest_${i}_target", def.target)
                .putInt("daily_quest_${i}_progress", 0)
                .putBoolean("daily_quest_${i}_done", false)
        }
        ed.apply()
    }

    private fun read(prefs: SharedPreferences, lockedReason: String?): State {
        val date = prefs.getString("daily_quests_date", calendarDay()) ?: calendarDay()
        val count = prefs.getInt("daily_quests_count", 0).coerceIn(0, 2)
        val claimed = prefs.getBoolean("daily_quests_claimed", false)
        val objs = mutableListOf<Objective>()
        for (i in 0 until count) {
            val id = prefs.getString("daily_quest_${i}_id", null) ?: continue
            val title = prefs.getString("daily_quest_${i}_title", id) ?: id
            val blurb = prefs.getString("daily_quest_${i}_blurb", "") ?: ""
            val metric = prefs.getString("daily_quest_${i}_metric", "rooms") ?: "rooms"
            val target = prefs.getInt("daily_quest_${i}_target", 1).coerceAtLeast(1)
            val progress = prefs.getInt("daily_quest_${i}_progress", 0).coerceIn(0, target)
            val done = prefs.getBoolean("daily_quest_${i}_done", false) || progress >= target
            objs.add(Objective(id, title, blurb, metric, target, progress, done))
        }
        return State(date, objs, claimed, lockedReason)
    }
}
