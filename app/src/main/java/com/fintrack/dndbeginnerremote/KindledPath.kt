package com.fintrack.dndbeginnerremote

/**
 * v2.32 Kindled Path (combat revamp PR 4): parsing and wording for the native perk strings
 * (GamePerks.cpp getPerkState / getArtsBar). Solo only — online heroes never get a Kindled Path.
 * Pure Kotlin so the JVM tests can cover it; MainActivity builds the dialogs.
 */
object KindledPath {
    const val LEVEL_CAP = 50
    const val EMBER_FIRST_LEVEL = 21
    const val EMBERS_FROM_LEVELS = 30
    const val EMBER_CAP = 42
    const val PATH_TOTAL = 68
    const val RETEMPER_GOLD_PER_LEVEL = 100

    const val KIND_FLAME = 0
    const val KIND_WARD = 1
    const val KIND_ART = 2

    /** Art target kinds (Perks.h ArtTarget). */
    const val TARGET_FOE = 0
    const val TARGET_ALL_FOES = 1

    data class Header(
        val name: String, val classId: Int, val level: Int, val cap: Int,
        val earned: Int, val spent: Int, val free: Int, val freeRespecs: Int, val retemperCost: Int,
        val canEdit: Boolean, val why: String, val gold: Int, val signatureTier: Int,
    )
    data class Flame(val id: Int, val name: String, val desc: String, val rank: Int, val max: Int)
    data class Ward(val id: Int, val name: String, val desc: String, val learned: Boolean, val slot: Int)
    data class Art(
        val id: Int, val name: String, val desc: String, val cooldown: Int, val unlockLevel: Int,
        val learned: Boolean, val slot: Int, val target: Int,
    )
    data class State(val header: Header, val flames: List<Flame>, val wards: List<Ward>, val arts: List<Art>)

    data class BarArt(
        val slot: Int, val id: Int, val name: String, val cooldownLeft: Int, val ready: Boolean,
        val target: Int, val baseCooldown: Int,
    )

    fun parse(raw: String): State? {
        var header: Header? = null
        val flames = mutableListOf<Flame>()
        val wards = mutableListOf<Ward>()
        val arts = mutableListOf<Art>()
        for (line in raw.lineSequence()) {
            val f = line.split('|')
            fun i(k: Int) = f.getOrNull(k)?.trim()?.toIntOrNull() ?: 0
            fun s(k: Int) = f.getOrNull(k).orEmpty()
            when (f.firstOrNull()) {
                "H" -> if (f.size >= 12) header = Header(s(1), i(2), i(3), i(4), i(5), i(6), i(7), i(8), i(9),
                    i(10) == 1, s(11), i(12), i(13))
                "F" -> if (f.size >= 6) flames += Flame(i(1), s(2), s(3), i(4), i(5))
                "W" -> if (f.size >= 6) wards += Ward(i(1), s(2), s(3), i(4) == 1, i(5))
                "A" -> if (f.size >= 9) arts += Art(i(1), s(2), s(3), i(4), i(5), i(6) == 1, i(7), i(8))
            }
        }
        return header?.let { State(it, flames, wards, arts) }
    }

    fun parseBar(raw: String): List<BarArt> = raw.split(';').mapNotNull { rec ->
        val f = rec.split('|')
        if (f.size < 7) return@mapNotNull null
        val n = f.map { it.trim().toIntOrNull() }
        BarArt(n[0] ?: return@mapNotNull null, n[1] ?: 0, f[2], n[3] ?: 0, n[4] == 1, n[5] ?: 0, n[6] ?: 1)
    }

    /** Cooldown pips, static text (no flashing): "●●○" = 2 of 3 rounds recharged; "ready" / "spent". */
    fun pips(cooldownLeft: Int, baseCooldown: Int): String {
        if (cooldownLeft <= 0) return "ready"
        if (cooldownLeft >= 99) return "spent this battle"
        val total = baseCooldown.coerceIn(1, 9)
        val left = cooldownLeft.coerceIn(0, total)
        return "●".repeat(total - left) + "○".repeat(left)
    }

    /** The Arts action button label: "✸\nArts" plus how many are ready. */
    fun buttonLabel(bar: List<BarArt>): String {
        val ready = bar.count { it.ready }
        return if (bar.isEmpty()) "✸\nArts" else "✸\nArts $ready/${bar.size}"
    }

    fun barLine(a: BarArt): String = "${a.name} — ${pips(a.cooldownLeft, a.baseCooldown)}"

    /** Sheet XP line; at the cap there is no "next level". */
    fun xpLabel(xp: Int, need: Int, level: Int): String =
        if (level >= LEVEL_CAP) "XP max · Level $LEVEL_CAP (level cap)"
        else "XP $xp/$need · Level $level → ${level + 1}"

    fun emberLine(h: Header): String = when {
        h.earned <= 0 && h.level < EMBER_FIRST_LEVEL ->
            "Embers start at level $EMBER_FIRST_LEVEL: one per level to $LEVEL_CAP ($EMBERS_FROM_LEVELS in all)."
        else -> "Embers: ${h.free} free · ${h.spent} spent · ${h.earned} earned (cap $EMBER_CAP)"
    }

    fun sheetButtonLabel(h: Header?): String = when {
        h == null -> "Kindled Path"
        h.free > 0 -> "Kindled Path · ${h.free} Ember${if (h.free == 1) "" else "s"} to spend"
        else -> "Kindled Path"
    }

    fun retemperPrompt(h: Header): String {
        val price = if (h.retemperCost <= 0) "free this time (your v2.32 Re-temper token)"
        else "${h.retemperCost} gold (100 × level ${h.level})"
        return "Re-temper ${h.name}? Every attribute point above the class's starting scores and every " +
            "Ember comes back to spend again. Cost: $price."
    }

    fun flameRow(f: Flame): String = "${f.name} ${f.rank}/${f.max} — ${f.desc}"
    fun wardRow(w: Ward): String = "${w.name} — ${w.desc}" + when {
        w.slot >= 0 -> " [slot ${w.slot + 1}]"
        w.learned -> " [learned]"
        else -> ""
    }
    fun artRow(a: Art, level: Int): String = "${a.name} — ${a.desc} · recharge ${if (a.cooldown >= 99) "once a battle" else "${a.cooldown} rounds"}" +
        when {
            a.slot >= 0 -> " [slot ${a.slot + 1}]"
            a.learned -> " [learned]"
            level < a.unlockLevel -> " [level ${a.unlockLevel}]"
            else -> ""
        }

    /** Can the hero learn this right now (UI enable)? The native side checks again. */
    fun canLearnFlame(s: State, f: Flame) = s.header.canEdit && f.rank < f.max && s.header.free >= 1
    fun canLearnWard(s: State, w: Ward) = s.header.canEdit && !w.learned && s.header.free >= 2
    fun canLearnArt(s: State, a: Art) = s.header.canEdit && !a.learned && s.header.free >= 3 && s.header.level >= a.unlockLevel

    val helpText: String = """
        |Kindled Path (solo): from level 21 to the level cap of 50 every level gives 1 Ember.
        |• Steady Flames — 1 Ember a rank, 5 ranks: +1 to a stat, +3% max HP or +2% damage.
        |• Wards — 2 Embers: each fires once a battle on its own (2 equipped).
        |• Lantern Arts — 3 Embers: new class actions with a recharge (2 equipped, Arts button in a fight).
        |Main stat 32 (Signature): Arts recharge 1 round faster.
        |Re-temper outside a fight: 100 gold × level refunds attribute points and Embers. Heroes from before v2.32 get one free.
        |Online tables use the classic rules (no Kindled Path).
    """.trimMargin()
}
