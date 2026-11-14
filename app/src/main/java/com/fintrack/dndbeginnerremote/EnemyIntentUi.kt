package com.fintrack.dndbeginnerremote

/**
 * v2.30 enemy AI — intent chips and hero status badges (pure, JVM-tested).
 *
 * Native format (Game::getEnemyIntents, solo curve fights only; online tables send no foes):
 *   "numbers|bannerSeq|bannerText|idx,intent,side,target,label,value,statuses;..."
 *  - numbers: 1 when the party's best INT is 14+ (exact numbers may show)
 *  - bannerSeq / bannerText: a phase change ("PHASE 2 — …") or a "WARNING! …" (shown once per seq)
 *  - intent: 0 none, 1 attack, 2 heavy, 3 guard, 4 spell, 5 heal, 6 mark, 7 flee, 8 buff, 9 summon, 10 hex
 *  - side: 0 a hero (target = hero index), 1 a foe (target = foe index), 2 itself, 3 everyone, -1 none
 * Hero statuses (Game::getStatusEffects): "heroIdx,codes;..." with one letter per status.
 */
object EnemyIntentUi {
    data class Intent(
        val foeIndex: Int,
        val intent: Int,
        val side: Int,
        val target: Int,
        val label: String,
        val value: Int,
        val statuses: String
    )

    data class Snapshot(val numbers: Boolean, val bannerSeq: Int, val bannerText: String, val intents: List<Intent>)

    const val NONE = 0
    const val ATTACK = 1
    const val HEAVY = 2
    const val GUARD = 3
    const val SPELL = 4
    const val HEAL = 5
    const val MARK = 6
    const val FLEE = 7
    const val BUFF = 8
    const val SUMMON = 9
    const val HEX = 10

    val EMPTY = Snapshot(false, 0, "", emptyList())

    fun parseIntents(raw: String?): Snapshot {
        if (raw.isNullOrBlank()) return EMPTY
        val parts = raw.split('|', limit = 4)
        if (parts.size < 4) return EMPTY
        val numbers = parts[0].trim() == "1"
        val seq = parts[1].trim().toIntOrNull() ?: 0
        val banner = parts[2]
        val list = parts[3].split(';').mapNotNull { row ->
            val f = row.split(',')
            if (f.size < 6) return@mapNotNull null
            val idx = f[0].trim().toIntOrNull() ?: return@mapNotNull null
            Intent(
                foeIndex = idx,
                intent = f[1].trim().toIntOrNull() ?: NONE,
                side = f[2].trim().toIntOrNull() ?: -1,
                target = f[3].trim().toIntOrNull() ?: -1,
                label = f[4],
                value = f[5].trim().toIntOrNull() ?: 0,
                statuses = f.getOrNull(6)?.trim().orEmpty()
            )
        }
        return Snapshot(numbers, seq, banner, list)
    }

    /** heroIdx -> status letters. */
    fun parseStatuses(raw: String?): Map<Int, String> {
        if (raw.isNullOrBlank()) return emptyMap()
        val out = LinkedHashMap<Int, String>()
        for (row in raw.split(';')) {
            val f = row.split(',', limit = 2)
            val idx = f.getOrNull(0)?.trim()?.toIntOrNull() ?: continue
            val codes = f.getOrNull(1)?.trim().orEmpty()
            if (codes.isNotEmpty()) out[idx] = codes
        }
        return out
    }

    /** Sword attack, cracked hammer heavy, shield guard, flame spell, wick-heart heal, eye mark, arrow flee. */
    fun glyph(intent: Int): String = when (intent) {
        ATTACK -> "⚔"
        HEAVY -> "⚒"
        GUARD -> "🛡"
        SPELL -> "🔥"
        HEAL -> "♥"
        MARK -> "◉"
        FLEE -> "➜"
        BUFF -> "✚"
        SUMMON -> "✦"
        HEX -> "✖"
        else -> ""
    }

    fun intentName(intent: Int): String = when (intent) {
        ATTACK -> "Attack"
        HEAVY -> "Heavy hit"
        GUARD -> "Guard"
        SPELL -> "Spell"
        HEAL -> "Heal"
        MARK -> "Mark"
        FLEE -> "Flee"
        BUFF -> "Buff"
        SUMMON -> "Summon"
        HEX -> "Hex"
        else -> ""
    }

    /** Exact numbers only when the party is clever enough (INT 14+) and the player wants them. */
    fun showNumbers(s: Snapshot, prefExactNumbers: Boolean): Boolean = s.numbers && prefExactNumbers

    // v2.31 Read intent (INT): exact numbers at INT 14+, a rough range below it (pref on), glyph only (pref off).
    const val NUM_OFF = 0
    const val NUM_RANGE = 1
    const val NUM_EXACT = 2

    fun numberMode(s: Snapshot, prefExactNumbers: Boolean): Int = when {
        !prefExactNumbers -> NUM_OFF
        s.numbers -> NUM_EXACT
        else -> NUM_RANGE
    }

    /** Rough range for a hidden number: 60%–140% of it ("14–34"). */
    fun range(value: Int): String {
        val lo = maxOf(1, (value * 0.6).toInt())
        val hi = maxOf(lo, kotlin.math.ceil(value * 1.4).toInt())
        return "$lo–$hi"
    }

    fun chipText(i: Intent, mode: Int): String = when (mode) {
        NUM_EXACT -> chipText(i, numbers = true)
        NUM_RANGE -> {
            val g = glyph(i.intent)
            if (g.isEmpty()) "" else if (i.value > 0) "$g ${range(i.value)}" else g
        }
        else -> chipText(i, numbers = false)
    }

    fun describe(i: Intent, mode: Int): String = when (mode) {
        NUM_EXACT -> describe(i, true)
        NUM_RANGE -> {
            val d = describe(i, false)
            if (d.isEmpty() || i.value <= 0) d
            else d + (if (i.intent == HEAL) ", roughly +" else ", roughly ") + range(i.value) + " (INT 14 reads it exactly)"
        }
        else -> describe(i, false)
    }

    /** Chip text, e.g. "⚔ 12" with numbers or "⚔" without. */
    fun chipText(i: Intent, numbers: Boolean): String {
        val g = glyph(i.intent)
        if (g.isEmpty()) return ""
        return if (numbers && i.value > 0) "$g ${i.value}" else g
    }

    /** Screen-reader / long-press text: "Heavy hit: Snuff → Mira (next turn), about 24". */
    fun describe(i: Intent, numbers: Boolean): String {
        val base = intentName(i.intent).ifEmpty { return "" }
        val sb = StringBuilder(base)
        if (i.label.isNotBlank()) sb.append(": ").append(i.label)
        if (numbers && i.value > 0) sb.append(if (i.intent == HEAL) ", about +" else ", about ").append(i.value)
        return sb.toString()
    }

    /** Only telegraphed heavies pulse, and never with Reduce flashing on. */
    fun chipPulses(i: Intent, reduceFlash: Boolean): Boolean = !reduceFlash && i.intent == HEAVY

    /** A phase / warning banner animates (soft fade) only with Reduce flashing off. */
    fun bannerAnimates(reduceFlash: Boolean): Boolean = !reduceFlash

    /** New banner to show (seq increased and text present), else null. */
    fun newBanner(s: Snapshot, lastSeenSeq: Int): String? =
        if (s.bannerSeq > lastSeenSeq && s.bannerText.isNotBlank()) s.bannerText else null

    data class Status(val code: Char, val rounds: Int)

    /** Native status codes are "B2.M1" (letter + rounds left, dot separated). */
    fun parseCodes(codes: String): List<Status> = codes.split('.').mapNotNull { t ->
        val k = t.trim()
        if (k.isEmpty()) return@mapNotNull null
        Status(k[0], k.substring(1).toIntOrNull() ?: 1)
    }

    /** "B2.M1" -> "🔥 ◉" (unknown letters are skipped). */
    fun statusBadge(codes: String): String = parseCodes(codes).mapNotNull { statusGlyph(it.code) }.joinToString(" ")

    fun statusGlyph(c: Char): String? = when (c) {
        'B' -> "🔥"  // Burn
        'M' -> "◉"  // Marked
        'G' -> "🛡"  // Guarded
        'L' -> "◌"  // bLinded
        'S' -> "⌛"  // Slowed
        'K' -> "⛓"  // shacKled
        'I' -> "✚"  // Inspired
        'R' -> "≋"  // Rattled
        else -> null
    }

    fun statusName(c: Char): String? = when (c) {
        'B' -> "Burning"; 'M' -> "Marked"; 'G' -> "Guarded"; 'L' -> "Blinded"
        'S' -> "Slowed"; 'K' -> "Shackled"; 'I' -> "Inspired"; 'R' -> "Rattled"
        else -> null
    }

    /** "B2.M1" -> "Burning (2), Marked (1)". */
    fun statusNames(codes: String): String = parseCodes(codes).mapNotNull { st ->
        statusName(st.code)?.let { "$it (${st.rounds})" }
    }.joinToString(", ")
}
