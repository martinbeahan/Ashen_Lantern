package com.fintrack.dndbeginnerremote

import android.text.InputFilter

/**
 * Name / save text safety (Code Auditor C1, H2). Mirrors `cpp/SaveCodec.h` — keep the two in sync.
 *
 * The native save + sync format uses `, ; | ~ ^ :` as separators. Since v2.23 native code
 * percent-encodes free text on write, and names are sanitized at entry (here and in JNI) and on
 * remote join so they stay readable in the battle-roster / party-XP lists too.
 */
object TextSafety {
    /** Characters stripped from hero / Lanternkeeper / joiner names. */
    private const val FORBIDDEN = ",;|~^:%"
    const val MAX_NAME_CHARS = 24
    /** Valid CharacterClass ordinals: Fighter, Wizard, Rogue, Cleric, Bard. */
    val CLASS_IDS = 0..4

    /**
     * Same steps as dnd::savecodec::sanitizeName: control chars -> space, drop `, ; | ~ ^ : %`,
     * drop "(NPC)" (reserved for companions), collapse spaces, trim, cap, fallback.
     */
    fun sanitizeName(raw: String?, fallback: String = "Hero"): String {
        if (raw == null) return fallback
        val sb = StringBuilder(raw.length)
        for (c in raw) {
            when {
                c.code < 0x20 || c.code == 0x7F -> sb.append(' ')
                FORBIDDEN.indexOf(c) >= 0 -> {}
                else -> sb.append(c)
            }
        }
        var s = sb.toString().replace("(NPC)", "")
        s = s.replace(Regex(" {2,}"), " ").trim { it == ' ' }
        if (s.length > MAX_NAME_CHARS) {
            var cut = MAX_NAME_CHARS
            if (Character.isHighSurrogate(s[cut - 1])) cut-- // never split a surrogate pair
            s = s.substring(0, cut).trim { it == ' ' }
        }
        return s.ifEmpty { fallback }
    }

    /** Class id from Firebase / remote actions; anything outside 0..4 becomes Fighter (0). */
    fun validClassId(raw: Int?): Int = if (raw != null && raw in CLASS_IDS) raw else 0

    /** EditText filters: block the separator characters and cap the length while typing. */
    fun nameInputFilters(): Array<InputFilter> = arrayOf(
        InputFilter { source, start, end, _, _, _ ->
            val keep = StringBuilder()
            var changed = false
            for (i in start until end) {
                val c = source[i]
                if (FORBIDDEN.indexOf(c) >= 0 || c.code < 0x20) changed = true else keep.append(c)
            }
            if (changed) keep.toString() else null
        },
        InputFilter.LengthFilter(MAX_NAME_CHARS)
    )

    /** True when a native save header carries the v2.23 encoded-text marker ("E2", field 34). */
    fun isEncodedSave(data: String): Boolean =
        data.substringBefore('|').split(',').getOrNull(34) == "E2"

    /** Inverse of the native percent-encoding (malformed escapes are kept as-is). */
    fun decode(s: String): String {
        if ('%' !in s) return s
        val bytes = java.io.ByteArrayOutputStream(s.length)
        val raw = s.toByteArray(Charsets.UTF_8)
        var i = 0
        while (i < raw.size) {
            val b = raw[i]
            if (b == '%'.code.toByte() && i + 2 < raw.size) {
                val hi = Character.digit(raw[i + 1].toInt(), 16)
                val lo = Character.digit(raw[i + 2].toInt(), 16)
                if (hi >= 0 && lo >= 0) {
                    bytes.write((hi shl 4) or lo)
                    i += 3
                    continue
                }
            }
            bytes.write(b.toInt())
            i++
        }
        return bytes.toString(Charsets.UTF_8.name())
    }
}
