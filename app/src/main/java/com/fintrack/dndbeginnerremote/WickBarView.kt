package com.fintrack.dndbeginnerremote

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.util.AttributeSet
import android.view.View

/**
 * Ash & Ember "wick" HP bar (Phase 3 UI kit, README note 9): a candle burning down a socket.
 *
 * - rounded socket track (#141216, 1dp rim)
 * - wax fill (#C9A98A, #C77A5A below 30%) with a white@22% sheen line and a few drips
 * - a 3dp char cap at the fill edge
 * - a two-path amber / core flame riding the fill edge with a two-circle glow
 * Below 30% the flame shrinks to 65% — the non-colour cue for "guttering".
 *
 * Entirely static (no flicker), so it is Reduce-flashing safe by construction.
 */
class WickBarView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    var max: Int = 100
        set(value) { field = value.coerceAtLeast(1); invalidate() }
    var progress: Int = 100
        set(value) { field = value.coerceIn(0, max); invalidate() }

    fun setHp(current: Int, maximum: Int) {
        max = maximum.coerceAtLeast(1)
        progress = current
        contentDescription = "HP $current of $maximum"
    }

    private val d = resources.displayMetrics.density
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val rect = RectF()
    private val path = Path()

    private val cSocket = context.getColor(R.color.ae_far_soot)
    private val cRim = context.getColor(R.color.ae_rim)
    private val cWax = context.getColor(R.color.ae_wax_fill)
    private val cWaxLow = context.getColor(R.color.ae_wax_low)
    private val cChar = context.getColor(R.color.ae_char)
    private val cAmber = context.getColor(R.color.ae_amber)
    private val cCore = context.getColor(R.color.ae_amber_core)

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val wantH = (18 * d).toInt() + paddingTop + paddingBottom
        val w = resolveSize((96 * d).toInt(), widthMeasureSpec)
        val h = resolveSize(wantH, heightMeasureSpec)
        setMeasuredDimension(w, h)
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val left = paddingLeft.toFloat()
        val right = (width - paddingRight).toFloat()
        val bottom = (height - paddingBottom).toFloat()
        val trackH = 7f * d
        val top = bottom - trackH - 1f * d
        val frac = progress.toFloat() / max.toFloat()
        val low = frac < 0.30f

        // Socket track
        rect.set(left, top, right, top + trackH)
        paint.style = Paint.Style.FILL
        paint.color = cSocket
        canvas.drawRoundRect(rect, trackH / 2f, trackH / 2f, paint)
        paint.style = Paint.Style.STROKE
        paint.strokeWidth = 1f * d
        paint.color = cRim
        canvas.drawRoundRect(rect, trackH / 2f, trackH / 2f, paint)
        if (progress <= 0) return

        // Wax fill
        val inset = 1f * d
        val fillRight = left + inset + (right - left - 2 * inset) * frac
        rect.set(left + inset, top + inset, fillRight, top + trackH - inset)
        paint.style = Paint.Style.FILL
        paint.color = if (low) cWaxLow else cWax
        val r = (trackH - 2 * inset) / 2f
        canvas.drawRoundRect(rect, r, r, paint)
        // Drips hanging below the wax (only where the wax reaches)
        for (f in floatArrayOf(0.22f, 0.48f, 0.74f)) {
            val x = left + (right - left) * f
            if (x < fillRight - 4f * d) {
                rect.set(x - 1.1f * d, top + trackH - inset - 1f * d, x + 1.1f * d, top + trackH + 1.6f * d)
                canvas.drawRoundRect(rect, 1.1f * d, 1.1f * d, paint)
            }
        }
        // Sheen: white @22%
        paint.color = 0x38FFFFFF
        rect.set(left + inset + 2f * d, top + inset + 0.6f * d, (fillRight - 3f * d).coerceAtLeast(left + inset + 2f * d), top + inset + 1.6f * d)
        canvas.drawRect(rect, paint)
        // Char cap at the burning edge
        paint.color = cChar
        rect.set((fillRight - 3f * d).coerceAtLeast(left + inset), top + inset, fillRight, top + trackH - inset)
        canvas.drawRect(rect, paint)

        // Flame riding the fill edge (65% when guttering)
        val s = if (low) 0.65f else 1f
        val fx = fillRight - 1.5f * d
        val baseY = top + trackH / 2f
        val fh = 13f * d * s
        val fw = 4.2f * d * s
        paint.color = cAmber
        paint.alpha = 0x24
        canvas.drawCircle(fx, baseY - fh * 0.45f, 7.5f * d * s, paint)
        paint.alpha = 0x3D
        canvas.drawCircle(fx, baseY - fh * 0.40f, 4.5f * d * s, paint)
        paint.alpha = 0xFF
        path.reset()
        path.moveTo(fx, baseY - fh)
        path.quadTo(fx + fw * 1.5f, baseY - fh * 0.38f, fx + fw * 0.8f, baseY - fh * 0.06f)
        path.quadTo(fx, baseY + fh * 0.12f, fx - fw * 0.8f, baseY - fh * 0.06f)
        path.quadTo(fx - fw * 1.5f, baseY - fh * 0.38f, fx, baseY - fh)
        path.close()
        canvas.drawPath(path, paint)
        paint.color = cCore
        path.reset()
        path.moveTo(fx, baseY - fh * 0.55f)
        path.quadTo(fx + fw * 0.75f, baseY - fh * 0.22f, fx + fw * 0.4f, baseY - fh * 0.04f)
        path.quadTo(fx, baseY + fh * 0.05f, fx - fw * 0.4f, baseY - fh * 0.04f)
        path.quadTo(fx - fw * 0.75f, baseY - fh * 0.22f, fx, baseY - fh * 0.55f)
        path.close()
        canvas.drawPath(path, paint)
    }
}
