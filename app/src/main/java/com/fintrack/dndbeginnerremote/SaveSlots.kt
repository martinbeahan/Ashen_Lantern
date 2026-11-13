package com.fintrack.dndbeginnerremote

import android.content.SharedPreferences

/**
 * v2.28 save slots: three local lanterns. See docs/SAVE_SLOTS.md.
 *
 * Storage stays in the activity prefs file. Per-slot keys live as `slot_N_<key>`; account-wide
 * keys (settings, the raid-key daily grant cap, daily quests, bookkeeping) keep their names.
 * [SlotPrefs] wraps the raw prefs so existing code that reads `save_state`, `raid_keys`, the
 * endgame snapshots, pity and bests transparently reads the ACTIVE slot. The Memory Ledger
 * already keys itself as `ledger_slot_N_*` (v2.26).
 *
 * Host / Join never bind to a slot: callers must not write a slot while online, and the helpers
 * here that write ([writeSoloSave], [armCrashGuard]) are no-ops when `online` is true.
 */
object SaveSlots {
    const val COUNT = 3

    const val KEY_ACTIVE = "active_slot"
    const val KEY_MIGRATED = "slots_migrated"
    const val KEY_MIGRATION_PENDING = "slots_migration_pending"
    /** versionCode that copied the legacy single save into slot 1; the legacy keys stay one release. */
    const val KEY_LEGACY_BACKUP_FROM = "slots_legacy_backup_from"
    const val KEY_CRASH_GUARD = "crash_guard"
    const val KEY_CRASH_GUARD_SLOT = "crash_guard_slot"

    const val SAVE_STATE = "save_state"
    const val PLAYTIME_MS = "playtime_ms"
    const val LAST_PLAYED_MS = "last_played_ms"
    const val BROKEN = "slot_broken"
    const val STORY_COMPLETE = "story_fully_complete"

    /** Endgame snapshot pairs: (active flag, pre-run snapshot). */
    val SNAPSHOTS = listOf(
        "raid_active" to "pre_raid_save_state",
        "challenge_active" to "pre_challenge_save_state",
        "arena_active" to "pre_arena_save_state",
        "endless_active" to "pre_endless_save_state",
    )

    /** Keys that belong to one lantern. Everything else is account-wide. */
    val PER_SLOT_KEYS: Set<String> = setOf(
        SAVE_STATE, "hero_name", "hero_class", "companion_class", "companion_auto_ai",
        STORY_COMPLETE,
        "raid_active", "pre_raid_save_state", "challenge_active", "pre_challenge_save_state",
        "arena_active", "pre_arena_save_state", "endless_active", "pre_endless_save_state",
        // Held raid keys (the daily GRANT cap raid_keys_date / raid_keys_granted_today is shared).
        "raid_keys",
        // Challenge Legendary pity.
        "challenge_dungeon_attempts", "challenge_dungeon_legendary_chance",
        // Arena / Endless records.
        "arena_best_score", "arena_runs_completed", "arena_best_waves",
        "endless_deep_best_depth", "endless_deep_runs_completed", "endless_deep_best_milestone",
        // Daily spoils are stamped to the slot that claimed them (the dailies themselves are shared).
        "pending_daily_gold", "pending_daily_loot",
        // New in v2.28.
        PLAYTIME_MS, LAST_PLAYED_MS, BROKEN,
    )

    /** Legacy single-save keys copied into slot 1 by [migrate] (the new v2.28 keys have no legacy copy). */
    val LEGACY_KEYS: List<String> = PER_SLOT_KEYS.filter { it != PLAYTIME_MS && it != LAST_PLAYED_MS && it != BROKEN }

    private val LEDGER_KEYS = listOf("owner", "ids", "unread", "crawl_dry", "choices", "migrated_v1")

    fun slotKey(slot: Int, key: String) = "slot_${slot}_$key"
    fun isPerSlot(key: String) = key in PER_SLOT_KEYS
    fun valid(slot: Int) = slot in 1..COUNT

    fun activeSlot(raw: SharedPreferences): Int = raw.getInt(KEY_ACTIVE, 1).takeIf { valid(it) } ?: 1
    fun setActive(raw: SharedPreferences, slot: Int) {
        if (valid(slot)) raw.edit().putInt(KEY_ACTIVE, slot).commit()
    }

    fun blob(raw: SharedPreferences, slot: Int): String? =
        raw.getString(slotKey(slot, SAVE_STATE), null)?.takeIf { looksLikeSave(it) }

    fun looksLikeSave(s: String?): Boolean = !s.isNullOrBlank() && s.contains('|')

    fun isFilled(raw: SharedPreferences, slot: Int): Boolean = blob(raw, slot) != null
    fun isBroken(raw: SharedPreferences, slot: Int): Boolean = raw.getBoolean(slotKey(slot, BROKEN), false)
    fun filledSlots(raw: SharedPreferences): List<Int> = (1..COUNT).filter { isFilled(raw, it) }
    fun firstEmpty(raw: SharedPreferences): Int? = (1..COUNT).firstOrNull { !isFilled(raw, it) }
    fun allFull(raw: SharedPreferences): Boolean = firstEmpty(raw) == null

    // ------------------------------------------------------------------------------------------
    // Migration (v2.27 and older single save -> slot 1). Idempotent.
    // ------------------------------------------------------------------------------------------

    enum class Migration { ALREADY_DONE, NO_LEGACY_SAVE, MIGRATED, PENDING_UNREADABLE, PENDING_NO_EMPTY_SLOT }

    /**
     * @param readable optional deeper check (native describeSaveState); null = shape check only.
     * @param storyCompleteFromSave derives slot 1's story flag from the save when the legacy pref is missing.
     */
    fun migrate(
        raw: SharedPreferences,
        nowMs: Long,
        versionCode: Int,
        readable: ((String) -> Boolean)? = null,
        storyCompleteFromSave: ((String) -> Boolean)? = null,
    ): Migration {
        if (raw.getBoolean(KEY_MIGRATED, false)) return Migration.ALREADY_DONE
        val legacy = raw.getString(SAVE_STATE, null)
        if (legacy.isNullOrBlank()) {
            raw.edit().putBoolean(KEY_MIGRATED, true).remove(KEY_MIGRATION_PENDING).putInt(KEY_ACTIVE, 1).commit()
            return Migration.NO_LEGACY_SAVE
        }
        if (!looksLikeSave(legacy) || (readable != null && !readable(legacy))) {
            // Never drop it: leave the legacy blob in place and try again next launch / Retry.
            raw.edit().putBoolean(KEY_MIGRATION_PENDING, true).commit()
            return Migration.PENDING_UNREADABLE
        }
        // Normally slot 1. If a previous pending run let the player start a hero meanwhile, use
        // the first empty slot; never overwrite a lantern.
        val target = if (!isFilled(raw, 1)) 1 else firstEmpty(raw)
        if (target == null) {
            raw.edit().putBoolean(KEY_MIGRATION_PENDING, true).commit()
            return Migration.PENDING_NO_EMPTY_SLOT
        }
        val ed = raw.edit()
        val all = raw.all
        for (k in LEGACY_KEYS) {
            val v = all[k] ?: continue
            putAny(ed, slotKey(target, k), v)
        }
        if (!raw.contains(STORY_COMPLETE)) {
            ed.putBoolean(slotKey(target, STORY_COMPLETE), storyCompleteFromSave?.invoke(legacy) ?: storyCompleteFromHeader(legacy))
        }
        // Daily spoils claimed before the update belonged to the one hero there was.
        ed.putLong(slotKey(target, PLAYTIME_MS), 0L)          // playtime starts at 0 (no backfill)
        ed.putLong(slotKey(target, LAST_PLAYED_MS), nowMs)    // so the card isn't blank
        ed.putInt(KEY_ACTIVE, target)
        ed.putBoolean(KEY_MIGRATED, true)
        ed.remove(KEY_MIGRATION_PENDING)
        ed.putInt(KEY_LEGACY_BACKUP_FROM, versionCode)
        // Legacy keys are left in place as the one-release backup (never read by SlotPrefs).
        ed.commit()
        return Migration.MIGRATED
    }

    /** Header field 29 = Act 4 complete (fields 10 / 12 / 19 = Acts 1–3). */
    fun storyCompleteFromHeader(save: String): Boolean {
        val f = save.substringBefore('|').split(',')
        return f.getOrNull(29)?.trim() == "1"
    }

    private fun putAny(ed: SharedPreferences.Editor, key: String, v: Any) {
        when (v) {
            is String -> ed.putString(key, v)
            is Int -> ed.putInt(key, v)
            is Boolean -> ed.putBoolean(key, v)
            is Long -> ed.putLong(key, v)
            is Float -> ed.putFloat(key, v)
            is Set<*> -> ed.putStringSet(key, v.filterIsInstance<String>().toSet())
        }
    }

    // ------------------------------------------------------------------------------------------
    // Delete / Abandon / Begin
    // ------------------------------------------------------------------------------------------

    /**
     * Clear one lantern: blob, hero prefs, snapshots, keys, pity, records, playtime, broken mark,
     * pending spoils and its Ledger. Other slots, account settings, the shared daily cap and the
     * legacy backup are untouched. If it was the active slot, active moves to the most recently
     * played remaining lantern (or stays put when none is left).
     */
    fun deleteSlot(raw: SharedPreferences, slot: Int) {
        if (!valid(slot)) return
        val ed = raw.edit()
        PER_SLOT_KEYS.forEach { ed.remove(slotKey(slot, it)) }
        LEDGER_KEYS.forEach { ed.remove("ledger_slot_${slot}_$it") }
        if (activeSlot(raw) == slot) {
            val next = (1..COUNT).filter { it != slot && isFilled(raw, it) }
                .maxByOrNull { raw.getLong(slotKey(it, LAST_PLAYED_MS), 0L) }
            if (next != null) ed.putInt(KEY_ACTIVE, next)
        }
        if (raw.getBoolean(KEY_CRASH_GUARD, false) && raw.getInt(KEY_CRASH_GUARD_SLOT, -1) == slot) {
            ed.putBoolean(KEY_CRASH_GUARD, false).remove(KEY_CRASH_GUARD_SLOT)
        }
        ed.commit()
    }

    /** Pick an empty slot for a new Story / Crawl; null = all three lit (never overwrite). */
    fun slotForNewRun(raw: SharedPreferences, requested: Int?): Int? {
        if (requested != null && valid(requested) && !isFilled(raw, requested)) return requested
        return firstEmpty(raw)
    }

    // ------------------------------------------------------------------------------------------
    // Solo writes and the crash guard (no-ops online).
    // ------------------------------------------------------------------------------------------

    /** Write the active slot's save + hero prefs. Online (Host / Join): writes nothing, returns false. */
    fun writeSoloSave(raw: SharedPreferences, online: Boolean, data: String, heroName: String, heroClass: Int, nowMs: Long, commit: Boolean = false): Boolean {
        if (online || !looksLikeSave(data)) return false
        val slot = activeSlot(raw)
        val ed = raw.edit()
            .putString(slotKey(slot, SAVE_STATE), data)
            .putString(slotKey(slot, "hero_name"), heroName)
            .putInt(slotKey(slot, "hero_class"), heroClass)
            .putLong(slotKey(slot, LAST_PLAYED_MS), nowMs)
            .remove(slotKey(slot, BROKEN))
            .putBoolean(KEY_CRASH_GUARD, false)
            .remove(KEY_CRASH_GUARD_SLOT)
            .putBoolean("was_online_host", false)
            .putBoolean("was_online_client", false)
            .remove("online_session_id")
        if (commit) ed.commit() else ed.apply()
        return true
    }

    fun addPlaytime(raw: SharedPreferences, slot: Int, deltaMs: Long) {
        if (!valid(slot) || deltaMs <= 0) return
        val k = slotKey(slot, PLAYTIME_MS)
        raw.edit().putLong(k, raw.getLong(k, 0L) + deltaMs).apply()
    }

    /** Arm only for a real solo session; online never arms (and disarms). */
    fun armCrashGuard(raw: SharedPreferences, online: Boolean) {
        if (online) {
            raw.edit().putBoolean(KEY_CRASH_GUARD, false).remove(KEY_CRASH_GUARD_SLOT).apply()
        } else {
            raw.edit().putBoolean(KEY_CRASH_GUARD, true).putInt(KEY_CRASH_GUARD_SLOT, activeSlot(raw)).apply()
        }
    }

    enum class CrashOutcome { NONE, KEPT, RESTORED_SNAPSHOT, MARKED_BROKEN }

    /**
     * Softer crash guard. After an unclean solo exit: if the guarded slot's save still loads, keep
     * it; else restore that slot's endgame snapshot if one loads; else mark the slot broken (never
     * delete). Only the guarded slot is ever touched. [loads] is a non-adopting check
     * (native describeSaveState); null = shape check only.
     */
    fun recoverFromCrash(raw: SharedPreferences, loads: ((String) -> Boolean)?): Pair<CrashOutcome, Int> {
        if (!raw.getBoolean(KEY_CRASH_GUARD, false)) return CrashOutcome.NONE to 0
        val slot = raw.getInt(KEY_CRASH_GUARD_SLOT, activeSlot(raw)).takeIf { valid(it) } ?: activeSlot(raw)
        val check: (String?) -> Boolean = { s -> looksLikeSave(s) && (loads == null || loads(s!!)) }
        val ed = raw.edit()
            .putBoolean(KEY_CRASH_GUARD, false)
            .remove(KEY_CRASH_GUARD_SLOT)
            .remove("was_online_host")
            .remove("was_online_client")
            .remove("online_session_id")
        val blob = raw.getString(slotKey(slot, SAVE_STATE), null)
        val outcome = when {
            blob.isNullOrBlank() -> CrashOutcome.NONE
            check(blob) -> CrashOutcome.KEPT
            else -> {
                val snap = SNAPSHOTS.firstOrNull { (flag, pre) ->
                    raw.getBoolean(slotKey(slot, flag), false) && check(raw.getString(slotKey(slot, pre), null))
                }
                if (snap != null) {
                    ed.putString(slotKey(slot, SAVE_STATE), raw.getString(slotKey(slot, snap.second), null))
                    SNAPSHOTS.forEach { (flag, pre) -> ed.remove(slotKey(slot, flag)).remove(slotKey(slot, pre)) }
                    ed.remove(slotKey(slot, BROKEN))
                    CrashOutcome.RESTORED_SNAPSHOT
                } else {
                    ed.putBoolean(slotKey(slot, BROKEN), true)
                    CrashOutcome.MARKED_BROKEN
                }
            }
        }
        ed.commit()
        return outcome to slot
    }

    // ------------------------------------------------------------------------------------------
    // Card text
    // ------------------------------------------------------------------------------------------

    /** Parsed native describeSaveState() line. */
    data class Summary(
        val name: String, val classId: Int, val level: Int, val questBeat: Int,
        val actTitle: String, val beatName: String, val mode: Int, val difficulty: Int,
        val gameOver: Boolean, val roomCount: Int, val storyComplete: Boolean,
    ) {
        companion object {
            fun parse(line: String): Summary? {
                if (line.isBlank()) return null
                val p = line.split('\t')
                if (p.size < 11) return null
                return Summary(
                    p[0].ifBlank { "Hero" }, p[1].toIntOrNull() ?: 0, p[2].toIntOrNull() ?: 1,
                    p[3].toIntOrNull() ?: 0, p[4], p[5], p[6].toIntOrNull() ?: 0, p[7].toIntOrNull() ?: 0,
                    p[8] == "1", p[9].toIntOrNull() ?: 0, p[10] == "1",
                )
            }
        }
    }

    fun className(id: Int) = when (id) {
        1 -> "Wizard"; 2 -> "Rogue"; 3 -> "Cleric"; 4 -> "Bard"; else -> "Fighter"
    }

    private fun difficultyName(d: Int) = when (d) { 1 -> "Medium"; 2 -> "Hard"; 3 -> "Nightmare"; else -> "Easy" }

    fun progressLine(s: Summary): String {
        if (s.gameOver) return "Game Over · ${difficultyName(s.difficulty)}"
        val endgame = when (s.mode) {
            2 -> "In Boss Raid"; 3 -> "In Cinder Crucible"; 4 -> "In the Ember Ring"; 5 -> "In the Ashen Deep"; else -> null
        }
        val base = when {
            s.mode == 1 -> "Lamplighter's Rounds · Room ${s.roomCount}"
            // Beat 7 = post-story procedural rooms (QuestScript POST_QUEST).
            s.storyComplete && (s.questBeat == 7 || s.questBeat !in 1..25) -> "Ashwake Vigil · Vigil Kindled"
            s.questBeat in 1..25 && s.questBeat != 7 && s.actTitle.isNotBlank() -> "${s.actTitle} · ${s.beatName}"
            else -> "Room ${s.roomCount}"
        }
        return if (endgame != null) "$base · $endgame" else base
    }

    fun playtimeLabel(ms: Long): String {
        val mins = (ms / 60_000L).coerceAtLeast(0)
        return if (mins < 60) "${mins}m" else "${mins / 60}h ${mins % 60}m"
    }

    /** "today" / "yesterday" / "N days ago" / "unknown". */
    fun lastPlayedLabel(lastMs: Long, nowMs: Long, dayIndex: (Long) -> Long): String {
        if (lastMs <= 0L) return "unknown"
        val d = dayIndex(nowMs) - dayIndex(lastMs)
        return when {
            d <= 0L -> "today"
            d == 1L -> "yesterday"
            else -> "$d days ago"
        }
    }
}

/**
 * SharedPreferences view where [SaveSlots.PER_SLOT_KEYS] resolve to the active slot
 * (`slot_N_<key>`) and every other key passes straight through. [slot] is re-read on every call,
 * so switching lanterns takes effect immediately.
 */
class SlotPrefs(val raw: SharedPreferences, private val slot: () -> Int) : SharedPreferences {
    private fun map(key: String?): String? =
        if (key != null && SaveSlots.isPerSlot(key)) SaveSlots.slotKey(slot(), key) else key

    override fun getAll(): MutableMap<String, *> = raw.all
    override fun getString(key: String?, defValue: String?): String? = raw.getString(map(key), defValue)
    override fun getStringSet(key: String?, defValues: MutableSet<String>?): MutableSet<String>? = raw.getStringSet(map(key), defValues)
    override fun getInt(key: String?, defValue: Int): Int = raw.getInt(map(key), defValue)
    override fun getLong(key: String?, defValue: Long): Long = raw.getLong(map(key), defValue)
    override fun getFloat(key: String?, defValue: Float): Float = raw.getFloat(map(key), defValue)
    override fun getBoolean(key: String?, defValue: Boolean): Boolean = raw.getBoolean(map(key), defValue)
    override fun contains(key: String?): Boolean = raw.contains(map(key))
    override fun edit(): SharedPreferences.Editor = Editor(raw.edit())
    override fun registerOnSharedPreferenceChangeListener(l: SharedPreferences.OnSharedPreferenceChangeListener?) =
        raw.registerOnSharedPreferenceChangeListener(l)
    override fun unregisterOnSharedPreferenceChangeListener(l: SharedPreferences.OnSharedPreferenceChangeListener?) =
        raw.unregisterOnSharedPreferenceChangeListener(l)

    private inner class Editor(private val e: SharedPreferences.Editor) : SharedPreferences.Editor {
        override fun putString(key: String?, value: String?) = apply { e.putString(map(key), value) }
        override fun putStringSet(key: String?, values: MutableSet<String>?) = apply { e.putStringSet(map(key), values) }
        override fun putInt(key: String?, value: Int) = apply { e.putInt(map(key), value) }
        override fun putLong(key: String?, value: Long) = apply { e.putLong(map(key), value) }
        override fun putFloat(key: String?, value: Float) = apply { e.putFloat(map(key), value) }
        override fun putBoolean(key: String?, value: Boolean) = apply { e.putBoolean(map(key), value) }
        override fun remove(key: String?) = apply { e.remove(map(key)) }
        /** Never clears the whole file from a slot view (would wipe other slots). */
        override fun clear() = this
        override fun commit(): Boolean = e.commit()
        override fun apply() = e.apply()
    }
}
