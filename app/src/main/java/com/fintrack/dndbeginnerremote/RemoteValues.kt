package com.fintrack.dndbeginnerremote

/**
 * v2.35 (#88): safe reads of values that arrive from Firebase (another player's device or any client
 * that can write the session). Firebase's `getValue(String::class.java)` throws when a node holds a
 * number, boolean or map, and those callbacks run on the main thread, so one bad write used to crash
 * every device in the session. Every listener now passes the raw `snapshot.value` through here:
 * anything that is not the expected type, or is longer than its limit, is ignored (or cut, for
 * names and chat) instead of crashing. Pure Kotlin, no Firebase types, so it is JVM-testable.
 */
object RemoteValues {
    /** gameState is the encoded native save (a full party + room is ~10–40 KB); 512 KB is far above any real one. */
    const val MAX_GAME_STATE_CHARS = 512 * 1024
    const val MAX_CHAT_CHARS = 500
    /** Raw names are capped before sanitizing; TextSafety.sanitizeName then cuts to 24. */
    const val MAX_RAW_NAME_CHARS = 64
    const val MAX_ACTION_FIELDS = 32
    const val MAX_ACTION_STRING_CHARS = 4096

    /** Lanternkeeper (host) name from sessions/{id}/meta/hostName; null when missing or not a String. */
    fun hostName(raw: Any?): String? {
        val s = (raw as? String)?.take(MAX_RAW_NAME_CHARS) ?: return null
        if (s.isBlank()) return null
        return TextSafety.sanitizeName(s, "").ifEmpty { null }
    }

    /** gameState: only a non-blank String within the size limit (a cut save would not load anyway). */
    fun gameState(raw: Any?): String? {
        val s = raw as? String ?: return null
        if (s.isBlank() || s.length > MAX_GAME_STATE_CHARS) return null
        return s
    }

    /** Lobby joiner name: the playerName String, else the node key, else null; always sanitized. */
    fun playerName(raw: Any?, key: String?): String? {
        val s = (raw as? String) ?: key ?: return null
        return TextSafety.sanitizeName(s.take(MAX_RAW_NAME_CHARS), "Hero")
    }

    /** Lobby classId: a whole number 0..4, else null (the caller joins the player as Fighter). */
    fun classId(raw: Any?): Int? {
        val n = when (raw) {
            is Long -> raw
            is Int -> raw.toLong()
            is Double -> if (raw % 1.0 == 0.0) raw.toLong() else return null
            is String -> raw.trim().toLongOrNull() ?: return null
            else -> return null
        }
        return if (n in TextSafety.CLASS_IDS.first..TextSafety.CLASS_IDS.last) n.toInt() else null
    }

    /** Chat line: a String with control characters replaced and cut to [MAX_CHAT_CHARS]; null when not a String or blank. */
    fun chat(raw: Any?): String? {
        val s = raw as? String ?: return null
        val cleaned = buildString(minOf(s.length, MAX_CHAT_CHARS)) {
            for (c in s) {
                if (length >= MAX_CHAT_CHARS) break
                append(if (c.code < 0x20 || c.code == 0x7F) ' ' else c)
            }
        }
        if (cleaned.isBlank()) return null
        // Never leave half a surrogate pair at the cut.
        return if (Character.isHighSurrogate(cleaned.last())) cleaned.dropLast(1) else cleaned
    }

    /**
     * One remote action (host only): keeps at most [MAX_ACTION_FIELDS] fields whose values are a String
     * (≤ [MAX_ACTION_STRING_CHARS]), a number or a boolean. Maps / lists / huge strings are dropped.
     */
    fun action(fields: Iterable<Pair<String?, Any?>>): Map<String, Any> {
        val out = LinkedHashMap<String, Any>()
        for ((k, v) in fields) {
            if (out.size >= MAX_ACTION_FIELDS) break
            if (k.isNullOrEmpty() || k.length > 64 || v == null) continue
            when (v) {
                is String -> if (v.length <= MAX_ACTION_STRING_CHARS) out[k] = v
                is Number, is Boolean -> out[k] = v
                else -> {}
            }
        }
        return out
    }
}
