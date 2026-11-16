package com.fintrack.dndbeginnerremote

/**
 * v2.41 online Keeper-table fairness (docs/ONLINE_FAIRNESS.md). The host's game runs every online
 * fight with the classic rules; a fresh level-1 joiner could be dropped by one hit in round 1 and
 * the table kept whatever difficulty the host's last solo run had loaded. The native side now caps
 * one hit at 45% of a hero's max HP, softens crits on heroes below level 5 and applies Easy; this
 * object only picks the table difficulty the host chose (no new remote-trusted data: joiners still
 * send only name + class, and the difficulty travels in the host's normal state sync).
 */
object OnlineFairness {
    const val EASY = 0
    const val NIGHTMARE = 3

    /** The host's preferred difficulty, clamped to the four native difficulties (bad prefs -> Easy). */
    fun tableDifficulty(preferred: Int): Int = if (preferred in EASY..NIGHTMARE) preferred else EASY

    /** Native per-hit guard on online tables (CombatCurve.h kOverkillShare). */
    const val MAX_HIT_SHARE = 0.45

    /** Largest single hit a standing hero with [maxHp] can take on an online table. */
    fun maxHit(maxHp: Int): Int = kotlin.math.floor(MAX_HIT_SHARE * maxHp.coerceAtLeast(1)).toInt().coerceAtLeast(1)
}
