package com.fintrack.dndbeginnerremote

import java.security.SecureRandom
import java.util.Random

/**
 * v2.37 (#90): online session ids. The old id was "AL-" + up to 4 hex digits (65,536 values), so live
 * tables could be found by trying them all. A new id is "AL-" + 10 characters from a 32-letter
 * alphabet (Crockford base32: digits and capitals without I, L, O, U) drawn from SecureRandom —
 * 32^10 ≈ 1.1 × 10^15 values (50 bits). Shown as "AL-7KQ2M-X9D4R" so it's easy to read out; joining
 * accepts it with or without dashes / spaces, in any case, and reads O as 0 and I / L as 1.
 *
 * The database rules (firebase/database.rules.json) only accept ids of exactly this shape, so an old
 * 4-character id can't be used online any more. Pure Kotlin, JVM-tested.
 */
object SessionIds {
    const val PREFIX = "AL-"
    const val CODE_LENGTH = 10
    /** Crockford base32. Keep in sync with the character class in firebase/database.rules.json. */
    const val ALPHABET = "0123456789ABCDEFGHJKMNPQRSTVWXYZ"

    private val secure: Random by lazy { SecureRandom() }

    /** A fresh canonical id, e.g. "AL-7KQ2MX9D4R" (the form used as the database key). */
    fun newId(random: Random = secure): String {
        val sb = StringBuilder(PREFIX.length + CODE_LENGTH).append(PREFIX)
        repeat(CODE_LENGTH) { sb.append(ALPHABET[random.nextInt(ALPHABET.length)]) }
        return sb.toString()
    }

    /** True for a canonical id ("AL-" + 10 alphabet characters, no separators). */
    fun isValid(id: String?): Boolean {
        if (id == null || id.length != PREFIX.length + CODE_LENGTH || !id.startsWith(PREFIX)) return false
        for (i in PREFIX.length until id.length) if (ALPHABET.indexOf(id[i]) < 0) return false
        return true
    }

    /**
     * What a player typed -> canonical id, or null when it can't be one. Accepts "al-7kq2m-x9d4r",
     * "7KQ2M X9D4R", "AL7KQ2MX9D4R"; O -> 0, I / L -> 1. A legacy "AL-1A2B" id returns null.
     */
    fun normalize(input: String?): String? {
        if (input == null || input.length > 40) return null
        var s = input.trim().uppercase()
        s = buildString { for (c in s) if (c != '-' && c != ' ' && c != '_' && c != '.') append(c) }
        if (s.startsWith("AL") && s.length == 2 + CODE_LENGTH) s = s.substring(2)
        if (s.length != CODE_LENGTH) return null
        val code = buildString {
            for (c in s) append(
                when (c) {
                    'O' -> '0'
                    'I', 'L' -> '1'
                    else -> c
                }
            )
        }
        val id = PREFIX + code
        return if (isValid(id)) id else null
    }

    /** Grouped for people: "AL-7KQ2M-X9D4R". Non-canonical input is returned unchanged. */
    fun display(id: String): String =
        if (isValid(id)) PREFIX + id.substring(3, 8) + "-" + id.substring(8) else id

    /** An id from before v2.37 ("AL-" + 1–4 hex digits): tells the player to ask for a new code. */
    fun looksLegacy(input: String?): Boolean =
        input != null && Regex("^AL-?[0-9A-F]{1,4}$").matches(input.trim().uppercase())
}
