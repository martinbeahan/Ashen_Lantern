package com.fintrack.dndbeginnerremote

/**
 * v2.31 universal stats: the native stat guide (Game::getStatGuide) for one hero.
 * Wire format: "rules|main|caps×6|base×6|effective×6|STR line|…|CHA line|signature".
 * rules = 1 in solo (caps + every stat helps every class), 0 online (old rules, no caps).
 * Pure Kotlin so it is JVM-testable (StatGuideTest).
 */
data class StatGuide(
    val active: Boolean,
    val mainIndex: Int,
    val caps: List<Int>,
    val base: List<Int>,
    val effective: List<Int>,
    val lines: List<String>,
    val signature: String,
) {
    /** Points this stat can still take (unlimited online). */
    fun room(i: Int): Int = if (!active || caps[i] <= 0) Int.MAX_VALUE else (caps[i] - base[i]).coerceAtLeast(0)

    fun atCap(i: Int): Boolean = active && room(i) == 0

    /** Row label for the level-up screen, e.g. "STR · Strength ★ main · cap 22". */
    fun rowLabel(i: Int): String = buildString {
        append("${SHORT[i]} · ${FULL[i]}")
        if (i == mainIndex) append(" ★ main")
        if (active && caps[i] > 0) append(" · cap ${caps[i]}")
    }

    /** "Now 20 → 22 (cap 22)" with a note when the stat is full. */
    fun nowLine(i: Int, preview: Int): String = buildString {
        append("Now ${base[i]} → $preview")
        if (active && caps[i] > 0 && preview >= caps[i]) append("  (at cap)")
    }

    /** Whole-screen help: what every stat does for this hero's class right now. */
    fun helpText(heroName: String): String = buildString {
        appendLine(if (active) "What each stat does for $heroName" else "$heroName — online table (classic stat rules)")
        appendLine()
        for (i in 0..5) appendLine("• ${SHORT[i]} ${effective.getOrElse(i) { 0 }}: ${lines.getOrElse(i) { "" }}")
        if (active) {
            appendLine()
            appendLine("Caps: 18 + half your level (max 30); your main stat can go 2 higher. Gear can push past a cap.")
            if (signature.isNotBlank()) append(signature)
        }
    }.trimEnd()

    companion object {
        val SHORT = listOf("STR", "DEX", "CON", "INT", "WIS", "CHA")
        val FULL = listOf("Strength", "Dexterity", "Constitution", "Intelligence", "Wisdom", "Charisma")

        fun parse(raw: String?): StatGuide? {
            if (raw.isNullOrBlank()) return null
            val p = raw.split('|')
            if (p.size < 12) return null
            fun six(s: String): List<Int>? {
                val v = s.split(',').map { it.trim().toIntOrNull() ?: return null }
                return if (v.size == 6) v else null
            }
            val caps = six(p[2]) ?: return null
            val base = six(p[3]) ?: return null
            val eff = six(p[4]) ?: return null
            val main = p[1].trim().toIntOrNull()?.takeIf { it in 0..5 } ?: return null
            return StatGuide(
                active = p[0].trim() == "1",
                mainIndex = main,
                caps = caps,
                base = base,
                effective = eff,
                lines = p.subList(5, 11).map { it.trim() },
                signature = p.getOrElse(11) { "" }.trim(),
            )
        }
    }
}
