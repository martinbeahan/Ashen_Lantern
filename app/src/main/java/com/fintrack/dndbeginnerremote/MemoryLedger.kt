package com.fintrack.dndbeginnerremote

import android.content.SharedPreferences

/**
 * The Memory Ledger (v2.26) — per-hero store of Ember Memories in meta prefs.
 *
 * Keys are namespaced by save slot (`ledger_slot_<n>_…`). Today there is one Continue save, which
 * is slot 1; a later multi-slot PR only has to pass its slot number. The store is never written into
 * `save_state` or a Host / Join snapshot, and online sessions never call into it.
 *
 * Native code (Game.cpp + MemoryCatalog.h) decides what is earned; this only remembers it.
 * Lore only — nothing here changes stats or gear.
 */
object MemoryLedger {
    /** The single Continue save. Future save slots get 2, 3, … */
    const val SLOT_1 = 1
    const val RIBBON_WREN = 0
    const val RIBBON_MAREN = 1
    const val RIBBON_LANTERN = 2
    const val SOURCE_STORY = 0
    const val SOURCE_CRAWL = 1
    const val SOURCE_DEEP = 2
    const val SOURCE_ARENA = 3
    const val SOURCE_RAID = 4

    data class Card(
        val id: String,
        val title: String,
        val speaker: String,
        val place: String,
        val body: String,
        val wren: String,
        val source: Int,
        val ribbon: Int,
        val lockedHint: String
    )

    /** Native getMemoryCatalog(): records split by \u001e, fields by \u001f. */
    fun parseCatalog(raw: String): List<Card> {
        if (raw.isBlank()) return emptyList()
        return raw.split('\u001e').mapNotNull { rec ->
            val f = rec.split('\u001f')
            if (f.size < 9 || f[0].isBlank()) null
            else Card(f[0], f[1], f[2], f[3], f[4], f[5], f[6].toIntOrNull() ?: 0, f[7].toIntOrNull() ?: 0, f[8])
        }
    }

    /** "a|b,c" → (a, [b, c]) for native drain / progress strings. */
    fun splitIds(raw: String): Pair<String, List<String>> {
        val bar = raw.indexOf('|')
        if (bar < 0) return "" to emptyList()
        val ids = raw.substring(bar + 1).split(',').map { it.trim() }.filter { it.startsWith("mem_") }
        return raw.substring(0, bar) to ids
    }

    fun ribbonLabel(r: Int): String = when (r) {
        RIBBON_WREN -> "Wren"
        RIBBON_MAREN -> "Maren"
        else -> "The Lantern"
    }

    /** Session id of a save string (header field 0), decoded the same way the native side wrote it. */
    fun sessionIdFromSave(save: String): String {
        val field = save.substringBefore('|').substringBefore(',')
        return (if (TextSafety.isEncodedSave(save)) TextSafety.decode(field) else field).trim()
    }

    class Store(private val prefs: SharedPreferences, val slot: Int = SLOT_1) {
        private fun k(name: String) = "ledger_slot_${slot}_$name"
        private val keys = listOf("owner", "ids", "unread", "crawl_dry", "choices", "migrated_v1")

        private fun csv(name: String): List<String> =
            prefs.getString(k(name), "").orEmpty().split(',').filter { it.isNotBlank() }

        fun ids(): List<String> = csv("ids")
        fun has(id: String): Boolean = ids().contains(id)
        fun count(): Int = ids().size
        fun isEmpty(): Boolean = ids().isEmpty()
        fun unread(): Set<String> = csv("unread").toSet()
        fun owner(): String = prefs.getString(k("owner"), "").orEmpty()
        fun crawlDry(): Int = prefs.getInt(k("crawl_dry"), 0)
        fun choices(): Pair<Int, Int> {
            val p = prefs.getString(k("choices"), "0,0").orEmpty().split(',')
            return (p.getOrNull(0)?.trim()?.toIntOrNull() ?: 0) to (p.getOrNull(1)?.trim()?.toIntOrNull() ?: 0)
        }
        fun isMigrated(): Boolean = prefs.getBoolean(k("migrated_v1"), false)

        /**
         * Add ids in order; returns only the new ones (never grants twice).
         * markUnread = show the NEW tag (live grants); migration adds silently.
         */
        fun add(newIds: List<String>, markUnread: Boolean): List<String> {
            val cur = ids().toMutableList()
            val added = newIds.filter { it.startsWith("mem_") }.distinct().filter { it !in cur }
            if (added.isEmpty()) return added
            cur.addAll(added)
            val ed = prefs.edit().putString(k("ids"), cur.joinToString(","))
            if (markUnread) ed.putString(k("unread"), (csv("unread") + added).distinct().joinToString(","))
            ed.apply()
            return added
        }

        fun markRead(id: String) {
            val u = csv("unread")
            if (id in u) prefs.edit().putString(k("unread"), u.filter { it != id }.joinToString(",")).apply()
        }

        fun setCrawlDry(v: Int) { prefs.edit().putInt(k("crawl_dry"), v.coerceIn(0, 1000)).apply() }

        fun setChoices(raw: String) {
            val p = raw.split(',')
            if (p.size >= 2 && p[0].trim().toIntOrNull() != null && p[1].trim().toIntOrNull() != null) {
                prefs.edit().putString(k("choices"), "${p[0].trim()},${p[1].trim()}").apply()
            }
        }

        fun setMigrated() { prefs.edit().putBoolean(k("migrated_v1"), true).apply() }

        /** New Game / Abandon on this slot: a fresh, empty Ledger. Other slots are untouched. */
        fun clear() {
            val ed = prefs.edit()
            keys.forEach { ed.remove(k(it)) }
            ed.apply()
        }

        /** New Game on this slot: empty Ledger owned by the new hero; nothing to migrate. */
        fun startFresh(sessionId: String) {
            clear()
            prefs.edit().putString(k("owner"), sessionId).putBoolean(k("migrated_v1"), true).apply()
        }

        /**
         * The Ledger belongs to the hero whose save sits in this slot. A different hero (or a slot
         * that has never had a Ledger) starts empty and is migrated again. Returns true if reset.
         */
        fun ensureOwner(sessionId: String): Boolean {
            if (sessionId.isBlank() || owner() == sessionId) return false
            clear()
            prefs.edit().putString(k("owner"), sessionId).apply()
            return true
        }

        /**
         * One-time migration of an existing save: grant the story memories its progress has earned
         * (native previewStoryMemories / getProgressStoryMemories: "venn,well|ids"). Silent; returns
         * how many were added. Later calls only top up (safety) and keep the choices current.
         */
        fun migrateFrom(progress: String): Int {
            val (choices, ids) = splitIds(progress)
            if (progress.isBlank()) return 0
            setChoices(choices)
            val added = add(ids, markUnread = false).size
            if (!isMigrated()) setMigrated()
            return added
        }
    }
}
