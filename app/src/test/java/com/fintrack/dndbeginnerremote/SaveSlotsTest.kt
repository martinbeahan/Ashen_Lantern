package com.fintrack.dndbeginnerremote

import android.content.SharedPreferences
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * v2.28 save slots: legacy single save -> slot 1 (idempotent, legacy backup kept, Ledger untouched),
 * per-slot vs account-wide keys through SlotPrefs, Delete / Abandon isolation, the softer
 * crash guard, Host / Join writing nothing, shared daily raid-key grant cap, and daily spoils
 * staying with the slot that claimed them.
 */
class SaveSlotsTest {

    /** In-memory SharedPreferences with real semantics for putString(null) and commit/apply. */
    private class FakePrefs : SharedPreferences {
        val map = linkedMapOf<String, Any?>()
        override fun getAll(): MutableMap<String, *> = LinkedHashMap(map)
        override fun getString(key: String?, defValue: String?) = map[key] as? String ?: defValue
        @Suppress("UNCHECKED_CAST")
        override fun getStringSet(key: String?, defValues: MutableSet<String>?) = (map[key] as? MutableSet<String>) ?: defValues
        override fun getInt(key: String?, defValue: Int) = map[key] as? Int ?: defValue
        override fun getLong(key: String?, defValue: Long) = map[key] as? Long ?: defValue
        override fun getFloat(key: String?, defValue: Float) = map[key] as? Float ?: defValue
        override fun getBoolean(key: String?, defValue: Boolean) = map[key] as? Boolean ?: defValue
        override fun contains(key: String?) = map.containsKey(key)
        override fun registerOnSharedPreferenceChangeListener(l: SharedPreferences.OnSharedPreferenceChangeListener?) {}
        override fun unregisterOnSharedPreferenceChangeListener(l: SharedPreferences.OnSharedPreferenceChangeListener?) {}
        override fun edit(): SharedPreferences.Editor = object : SharedPreferences.Editor {
            val pending = linkedMapOf<String, Any?>()
            val removed = mutableSetOf<String>()
            var clear = false
            override fun putString(k: String?, v: String?) = apply { if (v == null) removed.add(k!!) else pending[k!!] = v }
            override fun putStringSet(k: String?, v: MutableSet<String>?) = apply { pending[k!!] = v }
            override fun putInt(k: String?, v: Int) = apply { pending[k!!] = v }
            override fun putLong(k: String?, v: Long) = apply { pending[k!!] = v }
            override fun putFloat(k: String?, v: Float) = apply { pending[k!!] = v }
            override fun putBoolean(k: String?, v: Boolean) = apply { pending[k!!] = v }
            override fun remove(k: String?) = apply { removed.add(k!!) }
            override fun clear() = apply { clear = true }
            override fun commit(): Boolean {
                if (clear) map.clear()
                removed.forEach { if (it !in pending) map.remove(it) }
                map.putAll(pending)
                return true
            }
            override fun apply() { commit() }
        }
    }

    private fun header(beat: Int, act4: Boolean = false, gameOver: Boolean = false, encoded: Boolean = true): String {
        val h = MutableList(37) { "0" }
        h[0] = "AL-1F2E"; h[2] = if (gameOver) "1" else "0"; h[8] = "$beat"; h[29] = if (act4) "1" else "0"
        h[34] = "E2"; h[35] = "1"; h[36] = "2"
        return (if (encoded) h else h.take(34)).joinToString(",")
    }

    private fun save(beat: Int, act4: Boolean = false, name: String = "Mira", gameOver: Boolean = false, encoded: Boolean = true) =
        header(beat, act4, gameOver, encoded) + "|room~ev|$name,0,7,;||chat~|journal~"

    /** A v2.27 device: legacy single save + per-hero meta + v2.26 Ledger + account settings. */
    private fun legacyDevice(blob: String = save(11), storyFlag: Boolean? = null): FakePrefs {
        val p = FakePrefs()
        p.map["save_state"] = blob
        p.map["hero_name"] = "Mira"
        p.map["hero_class"] = 0
        p.map["companion_class"] = 1
        p.map["companion_auto_ai"] = true
        if (storyFlag != null) p.map["story_fully_complete"] = storyFlag
        p.map["raid_keys"] = 2
        p.map["raid_keys_date"] = "2026-11-13"
        p.map["raid_keys_granted_today"] = 1
        p.map["challenge_dungeon_legendary_chance"] = 12
        p.map["challenge_dungeon_attempts"] = 40
        p.map["arena_best_score"] = 900
        p.map["endless_deep_best_depth"] = 17
        p.map["pending_daily_gold"] = 30
        p.map["pref_reduce_flash"] = true
        p.map["pref_difficulty"] = 2
        p.map["daily_quests_date"] = "2026-11-13"
        p.map["ledger_slot_1_ids"] = "mem_kindling_night,mem_wrens_palm"
        p.map["ledger_slot_1_owner"] = "AL-1F2E"
        p.map["ledger_slot_1_migrated_v1"] = true
        return p
    }

    private fun slots(p: FakePrefs) = SlotPrefs(p) { SaveSlots.activeSlot(p) }

    @Test
    fun legacySaveLandsInSlotOneIntact() {
        val p = legacyDevice()
        val before = LinkedHashMap(p.map)
        assertEquals(SaveSlots.Migration.MIGRATED, SaveSlots.migrate(p, 1_000L, 67))
        assertEquals(before["save_state"], p.map["slot_1_save_state"])
        assertEquals("Mira", p.map["slot_1_hero_name"])
        assertEquals(1, p.map["slot_1_companion_class"])
        assertEquals(2, p.map["slot_1_raid_keys"])
        assertEquals(12, p.map["slot_1_challenge_dungeon_legendary_chance"])
        assertEquals(900, p.map["slot_1_arena_best_score"])
        assertEquals(17, p.map["slot_1_endless_deep_best_depth"])
        assertEquals(30, p.map["slot_1_pending_daily_gold"])
        assertEquals(false, p.map["slot_1_story_fully_complete"]) // derived: Act 4 not finished
        assertEquals(0L, p.map["slot_1_playtime_ms"])               // playtime starts at 0
        assertEquals(1_000L, p.map["slot_1_last_played_ms"])        // stamped so the card isn't blank
        assertEquals(1, p.map[SaveSlots.KEY_ACTIVE])
        assertEquals(67, p.map[SaveSlots.KEY_LEGACY_BACKUP_FROM])
        // Legacy backup kept; Ledger, settings and the shared daily cap untouched; slots 2–3 empty.
        for (k in before.keys) assertEquals(k, before[k], p.map[k])
        assertFalse(SaveSlots.isFilled(p, 2))
        assertFalse(SaveSlots.isFilled(p, 3))
        assertTrue(p.map.keys.none { it.startsWith("slot_2_") || it.startsWith("slot_3_") })
        // Through SlotPrefs the game reads slot 1.
        val sp = slots(p)
        assertEquals(before["save_state"], sp.getString("save_state", null))
        assertEquals(2, sp.getInt("raid_keys", 0))
        assertEquals(true, sp.getBoolean("pref_reduce_flash", false))
        assertEquals(1, sp.getInt("raid_keys_granted_today", 0))
        assertEquals(2, MemoryLedger.Store(p, 1).count())
    }

    @Test
    fun rerunningMigrationIsANoOp() {
        val p = legacyDevice()
        SaveSlots.migrate(p, 1_000L, 67)
        // Play on: slot 1 moves ahead of the legacy backup.
        slots(p).edit().putString("save_state", save(12)).putInt("raid_keys", 3).apply()
        val after = LinkedHashMap(p.map)
        assertEquals(SaveSlots.Migration.ALREADY_DONE, SaveSlots.migrate(p, 9_999L, 67))
        assertEquals(after, p.map)
        assertEquals(save(12), p.map["slot_1_save_state"])
        assertEquals(save(11), p.map["save_state"]) // backup still the old blob
    }

    @Test
    fun oldFormatsAndFlagsMigrate() {
        // v2.23/24 (35 fields) and pre-v2.23 (no E2 marker) blobs copy verbatim.
        val old = legacyDevice(save(16, encoded = false))
        assertEquals(SaveSlots.Migration.MIGRATED, SaveSlots.migrate(old, 1L, 67))
        assertEquals(save(16, encoded = false), old.map["slot_1_save_state"])
        // Finished story without the pref -> derived from header field 29; sticky pref is copied as-is.
        val done = legacyDevice(save(25, act4 = true))
        SaveSlots.migrate(done, 1L, 67)
        assertEquals(true, done.map["slot_1_story_fully_complete"])
        val sticky = legacyDevice(save(25, act4 = true), storyFlag = true)
        SaveSlots.migrate(sticky, 1L, 67)
        assertEquals(true, sticky.map["slot_1_story_fully_complete"])
        // Game Over (Easy) keeps its blob so Continue still opens the wipe dialog.
        val over = legacyDevice(save(9, gameOver = true))
        SaveSlots.migrate(over, 1L, 67)
        assertEquals(save(9, gameOver = true), over.map["slot_1_save_state"])
        // Mid-raid: the run AND the pre-raid snapshot both move.
        val raid = legacyDevice(save(25, act4 = true))
        raid.map["raid_active"] = true
        raid.map["pre_raid_save_state"] = save(25, act4 = true, name = "Mira-pre")
        SaveSlots.migrate(raid, 1L, 67)
        assertEquals(true, raid.map["slot_1_raid_active"])
        assertEquals(save(25, act4 = true, name = "Mira-pre"), raid.map["slot_1_pre_raid_save_state"])
    }

    @Test
    fun emptyInstallAndUnreadableLegacy() {
        val fresh = FakePrefs()
        assertEquals(SaveSlots.Migration.NO_LEGACY_SAVE, SaveSlots.migrate(fresh, 1L, 67))
        assertEquals((1..3).toList(), (1..3).filter { !SaveSlots.isFilled(fresh, it) })
        assertEquals(1, SaveSlots.slotForNewRun(fresh, null))

        val broken = legacyDevice("truncated-garbage")
        assertEquals(SaveSlots.Migration.PENDING_UNREADABLE, SaveSlots.migrate(broken, 1L, 67))
        assertEquals("truncated-garbage", broken.map["save_state"]) // never dropped
        assertEquals(true, broken.map[SaveSlots.KEY_MIGRATION_PENDING])
        assertFalse(broken.map.containsKey(SaveSlots.KEY_MIGRATED))
        assertFalse(SaveSlots.isFilled(broken, 1))
        // Native check says it won't load -> also pending, also kept.
        val nope = legacyDevice()
        assertEquals(SaveSlots.Migration.PENDING_UNREADABLE, SaveSlots.migrate(nope, 1L, 67, readable = { false }))
        assertEquals(save(11), nope.map["save_state"])
        // Meanwhile the player lit slot 1; a later successful retry goes to the first empty slot.
        nope.map["slot_1_save_state"] = save(2, name = "Cal")
        assertEquals(SaveSlots.Migration.MIGRATED, SaveSlots.migrate(nope, 1L, 67, readable = { true }))
        assertEquals(save(2, name = "Cal"), nope.map["slot_1_save_state"])
        assertEquals(save(11), nope.map["slot_2_save_state"])
    }

    @Test
    fun deleteAndAbandonOnlyTouchTheirSlot() {
        val p = legacyDevice()
        SaveSlots.migrate(p, 1_000L, 67)
        p.map["slot_2_save_state"] = save(3, name = "Cal"); p.map["slot_2_raid_keys"] = 1; p.map["slot_2_last_played_ms"] = 500L
        p.map["slot_3_save_state"] = save(20, name = "Ash"); p.map["slot_3_last_played_ms"] = 900L
        p.map["ledger_slot_2_ids"] = "mem_kindling_night"
        assertTrue(SaveSlots.allFull(p))
        assertNull(SaveSlots.slotForNewRun(p, 2)) // three lit: New Story refuses to overwrite
        val slot2 = p.map.filterKeys { it.startsWith("slot_2_") || it.startsWith("ledger_slot_2_") }
        val slot3 = p.map.filterKeys { it.startsWith("slot_3_") }

        SaveSlots.deleteSlot(p, 1) // active slot (Abandon path)
        assertTrue(p.map.keys.none { it.startsWith("slot_1_") || it.startsWith("ledger_slot_1_") })
        assertEquals(slot2, p.map.filterKeys { it.startsWith("slot_2_") || it.startsWith("ledger_slot_2_") })
        assertEquals(slot3, p.map.filterKeys { it.startsWith("slot_3_") })
        assertEquals(3, SaveSlots.activeSlot(p)) // most recently played remaining lantern
        // Legacy backup is not used to resurrect slot 1.
        assertEquals(save(11), p.map["save_state"])
        assertEquals(SaveSlots.Migration.ALREADY_DONE, SaveSlots.migrate(p, 2L, 67))
        assertFalse(SaveSlots.isFilled(p, 1))
        assertEquals(1, SaveSlots.slotForNewRun(p, null))
        // Settings / shared cap survive.
        assertEquals(true, p.map["pref_reduce_flash"])
        assertEquals(1, p.map["raid_keys_granted_today"])

        // Abandon on slot 2 leaves 1 and 3 alone.
        p.map["slot_1_save_state"] = save(1, name = "New")
        SaveSlots.setActive(p, 2)
        SaveSlots.deleteSlot(p, 2)
        assertTrue(SaveSlots.isFilled(p, 1) && SaveSlots.isFilled(p, 3) && !SaveSlots.isFilled(p, 2))
    }

    @Test
    fun crashGuardIsolationAndSofterRecovery() {
        val p = FakePrefs()
        SaveSlots.migrate(p, 1L, 67)
        p.map["slot_1_save_state"] = save(5, name = "Mira")
        p.map["slot_2_save_state"] = "broken-after-crash|"
        p.map["slot_3_save_state"] = save(20, name = "Ash")
        SaveSlots.setActive(p, 2)
        SaveSlots.armCrashGuard(p, online = false)
        assertEquals(2, p.map[SaveSlots.KEY_CRASH_GUARD_SLOT])
        val others = p.map.filterKeys { it.startsWith("slot_1_") || it.startsWith("slot_3_") }
        val loads: (String) -> Boolean = { it.contains("Mira") || it.contains("Ash") || it.contains("Cal") }

        // No snapshot -> marked broken, NOT deleted; other slots untouched.
        assertEquals(SaveSlots.CrashOutcome.MARKED_BROKEN to 2, SaveSlots.recoverFromCrash(p, loads))
        assertEquals("broken-after-crash|", p.map["slot_2_save_state"])
        assertTrue(SaveSlots.isBroken(p, 2))
        assertEquals(false, p.map["crash_guard"])
        assertEquals(others, p.map.filterKeys { it.startsWith("slot_1_") || it.startsWith("slot_3_") })

        // Mid-raid crash with a broken run -> pre-raid snapshot restored.
        p.map["slot_2_raid_active"] = true
        p.map["slot_2_pre_raid_save_state"] = save(25, act4 = true, name = "Cal")
        SaveSlots.armCrashGuard(p, online = false)
        assertEquals(SaveSlots.CrashOutcome.RESTORED_SNAPSHOT to 2, SaveSlots.recoverFromCrash(p, loads))
        assertEquals(save(25, act4 = true, name = "Cal"), p.map["slot_2_save_state"])
        assertFalse(p.map.containsKey("slot_2_raid_active"))
        assertFalse(SaveSlots.isBroken(p, 2))

        // A save that still loads is simply kept.
        SaveSlots.armCrashGuard(p, online = false)
        assertEquals(SaveSlots.CrashOutcome.KEPT to 2, SaveSlots.recoverFromCrash(p, loads))
        assertEquals(others, p.map.filterKeys { it.startsWith("slot_1_") || it.startsWith("slot_3_") })
        // No guard -> nothing.
        assertEquals(SaveSlots.CrashOutcome.NONE, SaveSlots.recoverFromCrash(p, loads).first)
    }

    @Test
    fun hostJoinWritesNoSlot() {
        val p = legacyDevice()
        SaveSlots.migrate(p, 1L, 67)
        p.map["slot_2_save_state"] = save(3, name = "Cal")
        val before = LinkedHashMap(p.map)
        // Online: a sync / pause write and session start change nothing slot-related.
        assertFalse(SaveSlots.writeSoloSave(p, online = true, data = save(99, name = "Joiner"), heroName = "Joiner",
            heroClass = 2, nowMs = 5L))
        SaveSlots.armCrashGuard(p, online = true)
        assertEquals(false, p.map["crash_guard"])
        assertEquals(before.filterKeys { it.startsWith("slot_") || it.startsWith("ledger_slot_") || it == "save_state" },
            p.map.filterKeys { it.startsWith("slot_") || it.startsWith("ledger_slot_") || it == "save_state" })
        // Solo: writes only the active slot.
        SaveSlots.setActive(p, 2)
        assertTrue(SaveSlots.writeSoloSave(p, online = false, data = save(4, name = "Cal"), heroName = "Cal",
            heroClass = 1, nowMs = 7L))
        assertEquals(save(4, name = "Cal"), p.map["slot_2_save_state"])
        assertEquals(before["slot_1_save_state"], p.map["slot_1_save_state"])
        assertEquals(before["save_state"], p.map["save_state"])
    }

    @Test
    fun keysPerSlotDailyCapSharedSpoilsStayWithClaimingSlot() {
        val p = FakePrefs()
        SaveSlots.migrate(p, 1L, 67)
        p.map["slot_1_save_state"] = save(25, act4 = true)
        p.map["slot_2_save_state"] = save(3, name = "Cal")
        val sp = slots(p)
        // Slot 1 earns two keys (shared grant counter goes to 2).
        SaveSlots.setActive(p, 1)
        sp.edit().putInt("raid_keys", 2).putInt("raid_keys_granted_today", 2).apply()
        // Slot 2 sees its own (zero) held keys but the SAME daily grant count.
        SaveSlots.setActive(p, 2)
        assertEquals(0, sp.getInt("raid_keys", 0))
        assertEquals(2, sp.getInt("raid_keys_granted_today", 0))
        sp.edit().putInt("raid_keys", 1).putInt("raid_keys_granted_today", 3).apply()
        SaveSlots.setActive(p, 1)
        assertEquals(2, sp.getInt("raid_keys", 0))
        assertEquals(3, sp.getInt("raid_keys_granted_today", 0)) // cap reached for every lantern

        // Per-slot story flag: slot 1 finished, slot 2 (mid-Act 1) stays locked.
        sp.edit().putBoolean("story_fully_complete", true).apply()
        SaveSlots.setActive(p, 2)
        assertFalse(sp.getBoolean("story_fully_complete", false))

        // Dailies claimed on slot 1 stay with slot 1 after switching to slot 2.
        SaveSlots.setActive(p, 1)
        DailyQuests.stashPendingSpoils(sp, 40, true)
        SaveSlots.setActive(p, 2)
        assertEquals(0 to false, DailyQuests.takePendingSpoils(sp))
        SaveSlots.setActive(p, 1)
        assertEquals(40 to true, DailyQuests.takePendingSpoils(sp))

        // Pity and bests are per slot too.
        ChallengeDungeon.onAttemptStarted(sp)
        SaveSlots.setActive(p, 2)
        assertEquals(0, ChallengeDungeon.attempts(sp))
        Arena.onRunFinished(sp, 500, 3)
        SaveSlots.setActive(p, 1)
        assertEquals(0, Arena.bestScore(sp))
        // Settings are account-wide; clear() from a slot view never wipes the file.
        sp.edit().putBoolean("pref_reduce_flash", true).apply()
        SaveSlots.setActive(p, 2)
        assertTrue(sp.getBoolean("pref_reduce_flash", false))
        sp.edit().clear().commit()
        assertNotNull(p.map["slot_1_save_state"])
    }

    @Test
    fun cardText() {
        val s = SaveSlots.Summary.parse("Mira\t0\t7\t11\tThe Tithe\tLedger Loft\t0\t1\t0\t11\t0")!!
        assertEquals("The Tithe · Ledger Loft", SaveSlots.progressLine(s))
        assertEquals("Game Over · Easy", SaveSlots.progressLine(s.copy(gameOver = true, difficulty = 0)))
        assertEquals("Lamplighter's Rounds · Room 14", SaveSlots.progressLine(s.copy(mode = 1, roomCount = 14, questBeat = 0)))
        assertEquals("Ashwake Vigil · Vigil Kindled · In Boss Raid",
            SaveSlots.progressLine(s.copy(mode = 2, questBeat = 7, storyComplete = true)))
        assertNull(SaveSlots.Summary.parse(""))
        assertEquals("4h 12m", SaveSlots.playtimeLabel((4 * 60 + 12) * 60_000L))
        assertEquals("0m", SaveSlots.playtimeLabel(0))
        val day: (Long) -> Long = { it / 100 }
        assertEquals("today", SaveSlots.lastPlayedLabel(250, 260, day))
        assertEquals("yesterday", SaveSlots.lastPlayedLabel(150, 260, day))
        assertEquals("unknown", SaveSlots.lastPlayedLabel(0, 260, day))
    }
}
