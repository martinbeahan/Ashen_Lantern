package com.fintrack.dndbeginnerremote

/**
 * v2.36 (#91): numbers in a remote player's action (host side). Firebase hands numbers over as Long
 * (or Double); `Long.toInt()` silently wraps (4294967297 -> 1, 2147483648 -> negative), and a
 * level-up batch of "2000000000,0,0,0,0,0" used to loop two billion JNI calls on the host. Every
 * value is now read in 64 bits, then clamped to the range the game can use; anything unreadable
 * takes the default. Pure Kotlin, JVM-tested. The native side also bounds-checks every index.
 */
object RemoteActionArgs {
    /** Enemy / party / shop indices: the game never has more than a few dozen of anything. */
    const val MAX_INDEX = 63
    /** Stat points one batch can spend in total (a level-50 Rekindled hero has well under 100). */
    const val MAX_BATCH_POINTS = 120

    /** A whole number from a Long / Int / Double / numeric String, clamped to lo..hi; else def. */
    fun int(raw: Any?, lo: Int, hi: Int, def: Int): Int {
        val n: Long = when (raw) {
            is Long -> raw
            is Int -> raw.toLong()
            is Short -> raw.toLong()
            is Byte -> raw.toLong()
            is Double -> if (raw.isNaN() || raw % 1.0 != 0.0) return def else raw.coerceIn(-9.0e18, 9.0e18).toLong()
            is Float -> if (raw.isNaN() || raw % 1f != 0f) return def else raw.toDouble().coerceIn(-9.0e18, 9.0e18).toLong()
            is String -> raw.trim().toLongOrNull() ?: return def
            else -> return def
        }
        return n.coerceIn(lo.toLong(), hi.toLong()).toInt()
    }

    /** Out-of-range indices become -1 ("no such target"), never a different valid one. */
    fun index(raw: Any?, def: Int = 0): Int { val v = int(raw, Int.MIN_VALUE, Int.MAX_VALUE, def); return if (v in 0..MAX_INDEX) v else -1 }
    fun classId(raw: Any?): Int = TextSafety.validClassId(int(raw, -1, 99, 0))

    /** 0..5 (STR..CHA); out of range or unreadable -> def, never pulled onto a real stat. */
    fun statIndex(raw: Any?, def: Int): Int { val v = int(raw, Int.MIN_VALUE, Int.MAX_VALUE, def); return if (v in 0..5) v else def }

    /**
     * "a,b,c,d,e,f" stat allocations -> 6 counts, each 0.. and together at most [MAX_BATCH_POINTS]
     * (the native game still refuses points the hero doesn't have). Bad entries count as 0.
     */
    fun allocations(raw: String?): IntArray {
        val out = IntArray(6)
        if (raw.isNullOrEmpty() || raw.length > 256) return out
        var left = MAX_BATCH_POINTS
        raw.split(',').take(6).forEachIndexed { i, s ->
            val v = s.trim().toLongOrNull()?.coerceIn(0L, MAX_BATCH_POINTS.toLong())?.toInt() ?: 0
            val take = minOf(v, left)
            out[i] = take
            left -= take
        }
        return out
    }
}
