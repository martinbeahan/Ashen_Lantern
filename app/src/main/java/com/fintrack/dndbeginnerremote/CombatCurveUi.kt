package com.fintrack.dndbeginnerremote

/**
 * v2.29 combat curve — UI helpers (pure, JVM-tested). The native core adds one status line in
 * solo play: "Curve: EL 7 · Pressure 2/4 · Rekindle 1" (never on online Keeper tables or for
 * joiners). The HUD shows it next to the room number as "EL 7 · ◆◆◇◇", and a surprise boss's
 * wind-up arrives as a lastEvent starting with "WARNING!".
 */
object CombatCurveUi {
    data class State(val encounterLevel: Int, val pressure: Int, val pressureMax: Int, val rekindleRooms: Int)

    private val LINE = Regex("""^Curve: EL (\d+) · Pressure (\d+)/(\d+) · Rekindle (\d+)\s*$""")

    fun parse(status: String): State? {
        for (line in status.lineSequence()) {
            val m = LINE.find(line.trim()) ?: continue
            val (el, p, max, rek) = m.destructured
            return State(el.toInt(), p.toInt(), max.toInt().coerceAtLeast(1), rek.toInt())
        }
        return null
    }

    /** Filled / empty pressure pips, e.g. 2 of 4 -> "◆◆◇◇". */
    fun pressurePips(pressure: Int, max: Int): String {
        val m = max.coerceIn(1, 8)
        val p = pressure.coerceIn(0, m)
        return "◆".repeat(p) + "◇".repeat(m - p)
    }

    /** Text after the room number on the HUD: "EL 7 · ◆◆◇◇" (plus "· Rekindled" on a comeback). */
    fun roomBadge(s: State): String {
        val sb = StringBuilder("EL ${s.encounterLevel} · ${pressurePips(s.pressure, s.pressureMax)}")
        if (s.rekindleRooms > 0) sb.append(" · Rekindled")
        return sb.toString()
    }

    fun isWarning(event: String): Boolean = event.startsWith("WARNING!")

    /** The surprise-boss warning may pulse only when Reduce flashing is off (photosensitive-safe). */
    fun warningAnimates(reduceFlash: Boolean): Boolean = !reduceFlash
}
