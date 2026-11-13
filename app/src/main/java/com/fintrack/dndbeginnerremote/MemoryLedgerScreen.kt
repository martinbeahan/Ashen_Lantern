package com.fintrack.dndbeginnerremote

import android.app.Activity
import android.app.Dialog
import android.graphics.Typeface
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.view.Window
import android.widget.Button
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import androidx.core.content.ContextCompat
import androidx.core.content.res.ResourcesCompat

/**
 * Memory Ledger overlay (Ash & Ember UI kit): ribbon tabs (Wren / Maren / The Lantern), source chips,
 * collected cards (tap to expand; NEW until opened) and uncollected cards that reveal only a category.
 * A book of voices — no selling, equipping or anything that looks like inventory. No animation, so
 * Reduce Flashing has nothing to calm here.
 */
class MemoryLedgerScreen(
    private val activity: Activity,
    private val store: MemoryLedger.Store,
    private val catalog: List<MemoryLedger.Card>
) {
    private var ribbon = MemoryLedger.RIBBON_WREN
    /** -1 = All; else a MemoryLedger.SOURCE_* value. */
    private var source = -1
    private val expanded = mutableSetOf<String>()
    private lateinit var root: View

    private val sources = listOf(
        -1 to "All",
        MemoryLedger.SOURCE_STORY to "Story",
        MemoryLedger.SOURCE_CRAWL to "Rounds",
        MemoryLedger.SOURCE_DEEP to "Deep",
        MemoryLedger.SOURCE_ARENA to "Ring",
        MemoryLedger.SOURCE_RAID to "Raids"
    )

    fun show() {
        val dialog = Dialog(activity, android.R.style.Theme_Black_NoTitleBar_Fullscreen)
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE)
        root = activity.layoutInflater.inflate(R.layout.dialog_memory_ledger, null)
        dialog.setContentView(root)
        dialog.window?.setBackgroundDrawableResource(android.R.color.transparent)
        root.findViewById<Button>(R.id.ledgerClose).setOnClickListener { dialog.dismiss() }
        // Open on the first ribbon that has something in it.
        val owned = store.ids().toSet()
        ribbon = listOf(MemoryLedger.RIBBON_WREN, MemoryLedger.RIBBON_MAREN, MemoryLedger.RIBBON_LANTERN)
            .firstOrNull { r -> catalog.any { it.ribbon == r && it.id in owned } } ?: MemoryLedger.RIBBON_WREN
        render()
        dialog.show()
    }

    private fun dp(v: Int): Int = (v * activity.resources.displayMetrics.density).toInt()
    private fun font(id: Int): Typeface? = try { ResourcesCompat.getFont(activity, id) } catch (_: Exception) { null }
    private fun color(id: Int): Int = ContextCompat.getColor(activity, id)

    private fun render() {
        val owned = store.ids().toSet()
        val unread = store.unread()
        root.findViewById<TextView>(R.id.ledgerProgress).text = "${owned.count { id -> catalog.any { it.id == id } }} of ${catalog.size} kindled"
        renderPips(owned)
        renderChips(root.findViewById(R.id.ledgerTabs), listOf(
            MemoryLedger.RIBBON_WREN to "Wren", MemoryLedger.RIBBON_MAREN to "Maren", MemoryLedger.RIBBON_LANTERN to "The Lantern"
        ), ribbon, fill = true) { ribbon = it; render() }
        renderChips(root.findViewById(R.id.ledgerSources), sources, source, fill = false) { source = it; render() }

        val list = root.findViewById<LinearLayout>(R.id.ledgerCards)
        list.removeAllViews()
        val inRibbon = catalog.filter { it.ribbon == ribbon && (source < 0 || it.source == source) }
        val (have, locked) = inRibbon.partition { it.id in owned }
        have.forEach { list.addView(collectedCard(it, it.id in unread)) }
        locked.forEach { list.addView(lockedCard(it)) }
        if (inRibbon.isEmpty()) {
            list.addView(TextView(activity).apply {
                text = "No voices of this kind in this ribbon."
                typeface = font(R.font.cormorant)
                setTypeface(typeface, Typeface.ITALIC)
                setTextColor(color(R.color.ae_fog))
                gravity = Gravity.CENTER
                setPadding(0, dp(16), 0, dp(16))
            })
        }
        val ribbonAll = catalog.filter { it.ribbon == ribbon }
        root.findViewById<TextView>(R.id.ledgerFooter).text =
            "${MemoryLedger.ribbonLabel(ribbon)} · ${ribbonAll.count { it.id in owned }} of ${ribbonAll.size} in this ribbon"
    }

    private fun renderPips(owned: Set<String>) {
        val row = root.findViewById<LinearLayout>(R.id.ledgerPips)
        row.removeAllViews()
        val total = 16
        val lit = if (catalog.isEmpty()) 0 else ((owned.size.toDouble() / catalog.size) * total + 0.5).toInt()
            .coerceIn(if (owned.isEmpty()) 0 else 1, total)
        for (i in 0 until total) {
            row.addView(ImageView(activity).apply {
                setImageResource(if (i < lit) R.drawable.ic_ember_pip_lit else R.drawable.ic_ember_pip_unlit)
                layoutParams = LinearLayout.LayoutParams(dp(11), dp(11)).apply { marginEnd = dp(3) }
                contentDescription = null
            })
        }
    }

    private fun renderChips(
        row: LinearLayout,
        items: List<Pair<Int, String>>,
        selected: Int,
        fill: Boolean,
        onPick: (Int) -> Unit
    ) {
        row.removeAllViews()
        items.forEachIndexed { i, (value, label) ->
            val on = value == selected
            row.addView(TextView(activity).apply {
                text = label
                typeface = font(R.font.cinzel)
                textSize = if (fill) 13f else 12f
                gravity = Gravity.CENTER
                setTextColor(color(if (on) R.color.ae_amber else R.color.ae_bone))
                setBackgroundResource(if (on) R.drawable.bg_ledger_chip_on else R.drawable.bg_ledger_chip_off)
                setPadding(dp(12), dp(8), dp(12), dp(8))
                isClickable = true
                contentDescription = label + if (on) " (selected)" else ""
                setOnClickListener { onPick(value) }
                layoutParams = (if (fill) LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
                else LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT))
                    .apply { if (i < items.size - 1) marginEnd = dp(6) }
            })
        }
    }

    private fun cardParams() = LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
        .apply { bottomMargin = dp(8) }

    private fun collectedCard(c: MemoryLedger.Card, isNew: Boolean): View {
        val open = c.id in expanded
        val card = LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundResource(R.drawable.bg_memory_card)
            setPadding(dp(14), dp(10), dp(12), dp(10))
            layoutParams = cardParams()
            isClickable = true
            setOnClickListener {
                if (c.id in expanded) expanded.remove(c.id) else { expanded.add(c.id); store.markRead(c.id) }
                render()
            }
        }
        val head = LinearLayout(activity).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL }
        head.addView(ImageView(activity).apply {
            setImageResource(R.drawable.ic_memory_ember)
            layoutParams = LinearLayout.LayoutParams(dp(18), dp(18)).apply { marginEnd = dp(10) }
            contentDescription = null
        })
        val titles = LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
        }
        titles.addView(TextView(activity).apply {
            text = c.title.uppercase()
            typeface = font(R.font.cinzel)
            textSize = 14f
            letterSpacing = 0.05f
            setTextColor(color(if (open) R.color.ae_amber else R.color.ae_bone))
        })
        titles.addView(TextView(activity).apply {
            text = "${c.speaker} · ${c.place}"
            typeface = font(R.font.cormorant)
            setTypeface(typeface, Typeface.ITALIC)
            textSize = 13f
            setTextColor(color(R.color.ae_fog))
        })
        head.addView(titles)
        if (isNew) {
            head.addView(TextView(activity).apply {
                text = "NEW"
                typeface = font(R.font.cinzel)
                textSize = 10f
                setTextColor(color(R.color.ae_amber))
                setBackgroundResource(R.drawable.bg_memory_tag_new)
                setPadding(dp(6), dp(1), dp(6), dp(1))
            })
        }
        if (!open) {
            head.addView(TextView(activity).apply {
                text = "›"
                textSize = 18f
                setTextColor(color(R.color.ae_fog))
                setPadding(dp(8), 0, 0, 0)
            })
        }
        card.addView(head)
        if (open) {
            card.addView(TextView(activity).apply {
                text = c.body
                typeface = font(R.font.cormorant)
                setTypeface(typeface, Typeface.ITALIC)
                textSize = 15f
                setLineSpacing(dp(2).toFloat(), 1f)
                setTextColor(color(R.color.ae_bone))
                setPadding(0, dp(8), 0, 0)
            })
            if (c.wren.isNotBlank()) {
                val q = LinearLayout(activity).apply {
                    orientation = LinearLayout.VERTICAL
                    setBackgroundResource(R.drawable.bg_memory_quote)
                    setPadding(dp(10), dp(6), dp(8), dp(6))
                    layoutParams = LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
                        .apply { topMargin = dp(8) }
                }
                q.addView(TextView(activity).apply {
                    text = "WREN, FROM THE GLASS:"
                    typeface = font(R.font.cinzel)
                    textSize = 10f
                    setTextColor(color(R.color.ae_amber))
                })
                q.addView(TextView(activity).apply {
                    text = "\u201C${c.wren}\u201D"
                    typeface = font(R.font.cormorant)
                    textSize = 14f
                    setTextColor(color(R.color.ae_bone))
                })
                card.addView(q)
            }
        }
        return card
    }

    private fun lockedCard(c: MemoryLedger.Card): View {
        // Spoils nothing: no title, speaker or body — only the category it is found in.
        val card = LinearLayout(activity).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setBackgroundResource(R.drawable.bg_memory_card_locked)
            setPadding(dp(14), dp(10), dp(12), dp(10))
            layoutParams = cardParams()
        }
        card.addView(ImageView(activity).apply {
            setImageResource(R.drawable.ic_memory_unkindled)
            layoutParams = LinearLayout.LayoutParams(dp(18), dp(18)).apply { marginEnd = dp(10) }
            contentDescription = null
        })
        val col = LinearLayout(activity).apply { orientation = LinearLayout.VERTICAL }
        col.addView(TextView(activity).apply {
            text = "A VOICE NOT YET KINDLED"
            typeface = font(R.font.cinzel)
            textSize = 13f
            setTextColor(color(R.color.ae_fog))
        })
        col.addView(TextView(activity).apply {
            text = c.lockedHint
            typeface = font(R.font.cormorant)
            setTypeface(typeface, Typeface.ITALIC)
            textSize = 13f
            setTextColor(color(R.color.ae_ash))
        })
        card.addView(col)
        return card
    }
}
