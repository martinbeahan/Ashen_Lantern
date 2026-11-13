package com.fintrack.dndbeginnerremote

import android.content.SharedPreferences
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * v2.26 Memory Ledger meta-pref store: slot-1 keys (ready for later save slots), one-time
 * migration from a save's progress string, New Game / Abandon clears only that slot, and nothing
 * here touches save_state or the Host / Join prefs.
 */
class MemoryLedgerStoreTest {

    /** Minimal in-memory SharedPreferences (apply == commit). */
    private class FakePrefs : SharedPreferences {
        val map = linkedMapOf<String, Any?>()
        override fun getAll(): MutableMap<String, *> = map
        override fun getString(key: String, defValue: String?) = map[key] as? String ?: defValue
        override fun getStringSet(key: String, defValues: MutableSet<String>?) = defValues
        override fun getInt(key: String, defValue: Int) = map[key] as? Int ?: defValue
        override fun getLong(key: String, defValue: Long) = map[key] as? Long ?: defValue
        override fun getFloat(key: String, defValue: Float) = map[key] as? Float ?: defValue
        override fun getBoolean(key: String, defValue: Boolean) = map[key] as? Boolean ?: defValue
        override fun contains(key: String) = map.containsKey(key)
        override fun registerOnSharedPreferenceChangeListener(l: SharedPreferences.OnSharedPreferenceChangeListener?) {}
        override fun unregisterOnSharedPreferenceChangeListener(l: SharedPreferences.OnSharedPreferenceChangeListener?) {}
        override fun edit(): SharedPreferences.Editor = object : SharedPreferences.Editor {
            val pending = linkedMapOf<String, Any?>()
            val removed = mutableSetOf<String>()
            var clear = false
            override fun putString(k: String, v: String?) = apply { pending[k] = v }
            override fun putStringSet(k: String, v: MutableSet<String>?) = apply { pending[k] = v }
            override fun putInt(k: String, v: Int) = apply { pending[k] = v }
            override fun putLong(k: String, v: Long) = apply { pending[k] = v }
            override fun putFloat(k: String, v: Float) = apply { pending[k] = v }
            override fun putBoolean(k: String, v: Boolean) = apply { pending[k] = v }
            override fun remove(k: String) = apply { removed.add(k) }
            override fun clear() = apply { clear = true }
            override fun commit(): Boolean {
                if (clear) map.clear()
                removed.forEach { map.remove(it) }
                map.putAll(pending)
                return true
            }
            override fun apply() { commit() }
        }
    }

    private val act1 = "mem_kindling_night,mem_mothers_whisper,mem_wrens_knife,mem_funeral_ribbon,mem_sextons_watch,mem_wrens_palm"

    @Test
    fun slotOneKeysOnly() {
        val p = FakePrefs()
        val s = MemoryLedger.Store(p)
        assertEquals(MemoryLedger.SLOT_1, s.slot)
        s.ensureOwner("AL-1F2E")
        s.migrateFrom("2,1|$act1,mem_lises_ribbon")
        s.add(listOf("mem_tamsin_patience"), markUnread = true)
        s.setCrawlDry(3)
        assertTrue(p.map.isNotEmpty())
        assertTrue(p.map.keys.all { it.startsWith("ledger_slot_1_") })
        assertEquals("AL-1F2E", p.getString("ledger_slot_1_owner", null))
        assertEquals("$act1,mem_lises_ribbon,mem_tamsin_patience", p.getString("ledger_slot_1_ids", null))
        assertEquals(3, p.getInt("ledger_slot_1_crawl_dry", 0))
        assertEquals(2 to 1, s.choices())
        // No global / unslotted keys, and never the solo save or online prefs.
        assertFalse(p.map.keys.any { it == "save_state" || it.startsWith("online") || it.startsWith("was_online") })
    }

    @Test
    fun migrationIsSilentOnceAndNeverGrantsTwice() {
        val s = MemoryLedger.Store(FakePrefs())
        s.ensureOwner("AL-0001")
        assertFalse(s.isMigrated())
        assertEquals(7, s.migrateFrom("2,0|$act1,mem_lises_ribbon"))
        assertTrue(s.isMigrated())
        assertTrue(s.unread().isEmpty()) // migrated cards are not NEW
        assertEquals(0, s.migrateFrom("2,0|$act1,mem_lises_ribbon")) // top-up is idempotent
        assertEquals(listOf("mem_your_name"), s.add(listOf("mem_your_name", "mem_lises_ribbon"), markUnread = true))
        assertEquals(setOf("mem_your_name"), s.unread())
        s.markRead("mem_your_name")
        assertTrue(s.unread().isEmpty())
        assertEquals(0, s.migrateFrom("")) // broken / empty preview
        assertEquals(8, s.count())
    }

    @Test
    fun newGameAndAbandonClearOnlyThisSlot() {
        val p = FakePrefs()
        p.edit().putString("save_state", "AL-9,1|solo").putString("ledger_slot_2_ids", "mem_maren_pact").apply()
        val s = MemoryLedger.Store(p)
        s.ensureOwner("AL-9")
        s.migrateFrom("0,0|$act1")
        s.startFresh("AL-9") // New Game on slot 1
        assertTrue(s.isEmpty() && s.isMigrated())
        assertEquals("AL-9", s.owner())
        s.add(listOf("mem_kindling_night"), markUnread = true)
        s.clear() // Abandon
        assertTrue(s.isEmpty())
        assertFalse(p.map.keys.any { it.startsWith("ledger_slot_1_") })
        assertEquals("mem_maren_pact", p.getString("ledger_slot_2_ids", null)) // other slots untouched
        assertEquals("AL-9,1|solo", p.getString("save_state", null))
        assertEquals(listOf("mem_maren_pact"), MemoryLedger.Store(p, 2).ids())
    }

    @Test
    fun anotherHeroInTheSlotStartsAFreshLedger() {
        val s = MemoryLedger.Store(FakePrefs())
        s.ensureOwner("AL-AAAA")
        s.migrateFrom("0,0|$act1")
        assertFalse(s.ensureOwner("AL-AAAA"))
        assertFalse(s.ensureOwner("")) // unknown id never wipes
        assertTrue(s.ensureOwner("AL-BBBB"))
        assertTrue(s.isEmpty() && !s.isMigrated())
    }

    @Test
    fun hostAndJoinLeaveTheStoreAlone() {
        // Online play never constructs the store; prove the online prefs the app writes do not
        // collide with ledger keys, and that the store reads back unchanged around them.
        val p = FakePrefs()
        val s = MemoryLedger.Store(p)
        s.ensureOwner("AL-1")
        s.migrateFrom("2,1|$act1")
        val before = p.map.filterKeys { it.startsWith("ledger_slot_1_") }.toMap()
        p.edit().putBoolean("was_online_host", true).putString("online_session_id", "AL-HOST").apply()
        p.edit().remove("was_online_host").remove("online_session_id").apply()
        assertEquals(before, p.map.filterKeys { it.startsWith("ledger_slot_1_") })
    }

    @Test
    fun parsesCatalogDrainAndSaveIds() {
        val raw = listOf(
            listOf("mem_your_name", "The Next Entry", "The soul-ledger", "Ledger Loft", "Body", "Surplus.", "0", "0", "Found in story · The Tithe"),
            listOf("mem_maren_pact", "The Pact", "Maren Vey", "Ashen Deep · depth 30", "B", "", "2", "1", "Found in the Ashen Deep")
        ).joinToString("\u001e") { it.joinToString("\u001f") }
        val cards = MemoryLedger.parseCatalog(raw)
        assertEquals(2, cards.size)
        assertEquals("The Next Entry", cards[0].title)
        assertEquals(MemoryLedger.RIBBON_MAREN, cards[1].ribbon)
        assertEquals(MemoryLedger.SOURCE_DEEP, cards[1].source)
        assertEquals("3" to listOf("mem_a", "mem_b"), MemoryLedger.splitIds("3|mem_a,mem_b,"))
        assertEquals("0" to emptyList<String>(), MemoryLedger.splitIds("0|"))
        assertEquals("AL-1F", MemoryLedger.sessionIdFromSave("AL-1F,3,0|x"))
    }
}
