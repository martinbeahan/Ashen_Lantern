package com.fintrack.dndbeginnerremote

/**
 * v2.40 The Drowned Vigil (combat revamp PR 6, docs/MYTHICAL.md): a three-chamber endgame run for
 * Rekindled heroes (rank I+) on the Continue hero — an elite pack, an elite pair, then a Drowned
 * Lantern boss with its acolytes. It costs one Raid Key (the same keys as Boss Raid) and its
 * finale has a 7% Mythical chance. Pure Kotlin so the JVM tests cover the gate and the key cost;
 * MainActivity builds the dialog and reuses the Boss Raid session flags (raid_active /
 * pre_raid_save_state) with native mode 6. Solo only: Host / Join never offer it.
 */
object DrownedVigil {
    /** Native SoloPlayMode::VIGIL. */
    const val MODE = 6
    const val KEY_COST = 1
    const val MIN_RANK = 1
    const val MYTHICAL_PERCENT = 7
    const val CHAMBERS = 3

    fun unlocked(rank: Int): Boolean = rank >= MIN_RANK

    /** Rekindled and holding a key (the Raid Key cap is 3). */
    fun canEnter(rank: Int, keysHeld: Int): Boolean = unlocked(rank) && keysHeld >= KEY_COST

    /** Keys left after paying for an entry; never below 0. */
    fun keysAfterEntry(keysHeld: Int): Int = (keysHeld - KEY_COST).coerceAtLeast(0)

    /** Keys after a refund (the run failed to start); capped like every key grant. */
    fun keysAfterRefund(keysHeld: Int, cap: Int = 3): Int = (keysHeld + KEY_COST).coerceAtMost(cap)

    const val LOCKED_LINE = "The Drowned Vigil: Rekindle to open the Drowned Vigil."

    /** The extra line in the Boss Raid entry dialog. */
    fun entryLine(rank: Int): String = if (unlocked(rank)) {
        "Or keep The Drowned Vigil: $CHAMBERS chambers up the Vigil Spire, $KEY_COST Raid Key, " +
            "Mythical chance $MYTHICAL_PERCENT% on the Drowned finale."
    } else LOCKED_LINE
}
