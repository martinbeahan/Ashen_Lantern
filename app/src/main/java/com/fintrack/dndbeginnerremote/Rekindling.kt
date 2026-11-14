package com.fintrack.dndbeginnerremote

/**
 * v2.33 Rekindling (prestige, combat revamp PR 5): parsing and plain-language wording for the
 * native getRekindleState() string (GamePerks.cpp). Optional at level 50 with the story complete;
 * never forced. Solo only — online tables never Rekindle. Pure Kotlin so the JVM tests cover it;
 * MainActivity builds the dialogs. See docs/PRESTIGE.md.
 */
object Rekindling {
    const val MAX_RANK = 10
    const val ELIGIBLE_LEVEL = 50
    const val ATTUNE_ABOVE = 3
    /** Attunement cap once Rekindled: floor(level / 2) + 3 (mirrors prestige::attuneCap). */
    fun attuneCap(level: Int): Int = level.coerceAtLeast(0) / 2 + ATTUNE_ABOVE
    /** Plain wording of the attunement rule, used by the Sheet, explainer and confirm text. */
    const val ATTUNE_RULE = "half your level + $ATTUNE_ABOVE"

    data class State(
        val ready: Boolean, val canNow: Boolean, val offered: Boolean, val rank: Int, val nextRank: Int,
        val why: String, val level: Int, val difficulty: Int, val attuneCap: Int,
        val title: String, val nextTitle: String, val wardSlots: Int, val artSlots: Int,
        val perks: Map<Int, String>,
    ) {
        val nightmare: Boolean get() = difficulty == 3
        val rekindled: Boolean get() = rank > 0
    }

    fun parse(raw: String): State? {
        var head: List<String>? = null
        val perks = mutableMapOf<Int, String>()
        for (line in raw.lineSequence()) {
            val f = line.split('|')
            when (f.firstOrNull()) {
                "R" -> if (f.size >= 12) head = f
                "P" -> if (f.size >= 3) f[1].trim().toIntOrNull()?.let { perks[it] = f[2] }
            }
        }
        val f = head ?: return null
        fun i(k: Int, d: Int = 0) = f.getOrNull(k)?.trim()?.toIntOrNull() ?: d
        return State(
            ready = i(1) == 1, canNow = i(2) == 1, offered = i(3) == 1,
            rank = i(4).coerceIn(0, MAX_RANK), nextRank = i(5).coerceIn(0, MAX_RANK), why = f.getOrNull(6).orEmpty(),
            level = i(7, 1), difficulty = i(8), attuneCap = i(9, -1), title = f.getOrNull(10).orEmpty(),
            nextTitle = f.getOrNull(11).orEmpty(), wardSlots = i(12, 2), artSlots = i(13, 2), perks = perks,
        )
    }

    private val ROMAN = arrayOf("", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X")
    fun roman(rank: Int): String = ROMAN[rank.coerceIn(0, MAX_RANK)]

    /** Slot card / sheet title: "Rekindled III", "Everflame" at X, "" at rank 0. */
    fun title(rank: Int): String = when {
        rank <= 0 -> ""
        rank >= MAX_RANK -> "Everflame"
        else -> "Rekindled ${roman(rank)}"
    }

    /** Badge tint: amber lantern flame I-VII, ember-white VIII-IX, Everflame X. */
    enum class Badge { NONE, AMBER, EMBER_WHITE, EVERFLAME }
    fun badge(rank: Int): Badge = when {
        rank <= 0 -> Badge.NONE
        rank >= MAX_RANK -> Badge.EVERFLAME
        rank >= 8 -> Badge.EMBER_WHITE
        else -> Badge.AMBER
    }
    /** Everflame's badge pulses gently; with reduce-flash on it stays still. */
    fun badgeAnimates(rank: Int, reduceFlash: Boolean): Boolean = badge(rank) == Badge.EVERFLAME && !reduceFlash

    /** "Lv 12 · Fighter · Rekindled III" */
    fun slotLevelLine(level: Int, className: String, rank: Int): String =
        "Lv $level · $className" + if (rank > 0) " · ${title(rank)}" else ""

    /** Sheet button: shown when level 50 + story are done (even if a fight blocks it right now). */
    fun sheetButtonLabel(s: State?): String? = when {
        s == null || !s.ready -> null
        else -> "Ready to Rekindle → ${s.nextTitle}"
    }

    /** Sheet line for a Rekindled hero: title and attunement. */
    fun sheetLine(s: State?): String? {
        if (s == null || !s.rekindled) return null
        val attune = if (s.attuneCap >= 0) " · gear attuned up to +${s.attuneCap} ($ATTUNE_RULE)" else ""
        return "${s.title}$attune"
    }

    /** The first-eligibility explainer (also from the Sheet). Plain language, no jargon. */
    fun explainer(s: State, heroName: String): String = buildString {
        append("$heroName has reached level $ELIGIBLE_LEVEL and finished the story. You can Rekindle — ")
        append("start the climb again from level 1, stronger for good. It's optional: \"Not now\" changes nothing, ")
        append("and you can do it later from the Sheet (\"Ready to Rekindle\").\n\n")
        append("What resets\n")
        append("• Level back to 1 (you and your companion).\n")
        append("• Attribute points go back to your class's starting scores; you earn them again as you level.\n")
        append("• Embers come back — your Kindled Path is cleared, plus a free Re-temper.\n\n")
        append("What you keep\n")
        append("• All gear and the bag, every upgrade, and your gold. Gear is attuned: it counts up to $ATTUNE_RULE, ")
        append("rounded down (a +25 relic counts as +${attuneCap(1)} at level 1 and +${attuneCap(20)} at level 20), and grows back as you level.\n")
        append("• The story stays complete, with every mode it unlocked, plus the Memory Ledger, Raid Keys and records.\n\n")
        append("Every rank, for good\n")
        append("• +1 to every attribute (and +1 to every cap), +2% max HP, +1 Ember, +5% XP and +5% gold.\n")
        append("• Your companion Rekindles with you and gets half (rounded down).\n")
        append("• Foes notice: each rank adds a quarter of a level to encounters.\n\n")
        append("Rank rewards\n")
        for (r in 1..MAX_RANK) {
            val text = s.perks[r] ?: continue
            val mark = when {
                r <= s.rank -> "✓"
                r == s.nextRank -> "→"
                else -> "·"
            }
            append("$mark ${roman(r)}: $text\n")
        }
        append("\nRank I also unlocks Mythical gear and The Drowned Vigil — coming in the next update.\n")
        if (s.nightmare) {
            append("\n⚠ This hero is on Nightmare: if the party falls, the run ends and the Rekindling rank is lost with it.\n")
        }
        if (!s.canNow && s.why.isNotBlank()) append("\nNot right now: ${s.why}\n")
    }

    /** Second step: the confirmation, listing exactly what happens. */
    fun confirmText(s: State, heroName: String): String = buildString {
        append("Rekindle $heroName to ${s.nextTitle}?\n\n")
        append("Resets: level 1, attribute points, Embers (Kindled Path cleared).\n")
        append("Keeps: gear and upgrades (attuned to $ATTUNE_RULE), gold, the story and every mode, Ledger, Raid Keys.\n")
        append("Gains: ${s.nextTitle} — ${s.perks[s.nextRank] ?: "rank bonuses"}; +1 all attributes, +2% HP, +1 Ember, +5% XP and gold; a free Re-temper.\n")
        if (s.nightmare) append("\n⚠ Nightmare: a party wipe ends the run and clears the rank.\n")
        append("\nThis can't be undone.")
    }

    /** Warning shown before a Nightmare endgame run with a Rekindled hero ("" when it doesn't apply). */
    fun nightmareWarning(rank: Int, difficulty: Int, heroName: String): String =
        if (rank > 0 && difficulty == 3)
            "⚠ Nightmare: if the party falls here, $heroName loses ${title(rank)} (the Rekindling rank is cleared).\n\n"
        else ""

    /** Line added to the Nightmare wipe dialog when a rank was cleared. */
    fun lostRankLine(rank: Int): String =
        if (rank > 0) "Nightmare cleared ${title(rank)} from this hero — level, gear and gold are kept.\n\n" else ""
}
