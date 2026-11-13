package com.fintrack.dndbeginnerremote

import android.content.Context
import android.util.AttributeSet
import android.view.Gravity
import android.widget.ImageView
import android.widget.LinearLayout

/**
 * Ash & Ember XP: a row of 10 ember pips (Phase 3 UI kit, README note 11). Each pip is the
 * level-list vector @drawable/ic_ember_pip — level 0 unlit (ash outline), 1 half (amber bottom
 * half), 2 lit (amber + core diamond + glow). Level progress is shown in 20 half-steps.
 * Static (no pulse), so it is Reduce-flashing safe.
 */
class EmberXpView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : LinearLayout(context, attrs) {

    private val pips = ArrayList<ImageView>(PIPS)

    var max: Int = 100
        private set
    var progress: Int = 0
        private set

    init {
        orientation = HORIZONTAL
        gravity = Gravity.CENTER_VERTICAL
        val d = resources.displayMetrics.density
        val size = (12 * d).toInt()
        val gap = (2 * d).toInt()
        for (i in 0 until PIPS) {
            val iv = ImageView(context).apply {
                setImageResource(R.drawable.ic_ember_pip)
                importantForAccessibility = IMPORTANT_FOR_ACCESSIBILITY_NO
                layoutParams = LayoutParams(size, size).apply { if (i > 0) marginStart = gap }
            }
            pips += iv
            addView(iv)
        }
        setProgress(0, 100)
    }

    fun setProgress(xp: Int, need: Int) {
        max = need.coerceAtLeast(1)
        progress = xp.coerceIn(0, max)
        val halfSteps = (progress.toLong() * PIPS * 2 / max).toInt().coerceIn(0, PIPS * 2)
        val lit = halfSteps / 2
        val half = halfSteps % 2 == 1
        for (i in 0 until PIPS) {
            pips[i].setImageLevel(
                when {
                    i < lit -> 2
                    i == lit && half -> 1
                    else -> 0
                }
            )
        }
        contentDescription = "XP $progress of $max"
    }

    companion object { const val PIPS = 10 }
}
