package com.fintrack.dndbeginnerremote

import android.app.Activity
import android.util.Log
import android.view.ViewGroup
import android.widget.Button
import android.widget.Toast

/**
 * DEBUG BUILDS ONLY (src/debug). Test shortcut for v2.33 Rekindling: in Settings, during a solo
 * adventure, "Debug: level 50 + story complete" puts the hero and companion at level 50 with the
 * story finished, so the Rekindle explainer can be tested without a 120,000 XP climb.
 *
 * The release source set has a no-op [DebugShortcuts] instead, and the native entry point is
 * compiled out of release (NDEBUG). app/build.gradle.kts `verifyReleaseHasNoDebugShortcuts` fails
 * assembleRelease if [MARKER] or the native symbol shows up in the release APK.
 */
object DebugShortcuts {
    const val ENABLED = true
    /** Scanned for by the release check — must only ever exist in debug builds. */
    const val MARKER = "ASHEN_DEBUG_ONLY_SHORTCUT"

    /** Native side: Game::debugJumpToRekindleReady (main.cpp, #ifndef NDEBUG). */
    @JvmStatic external fun nativeJumpToRekindleReady(): Boolean

    /** Add the debug button above [anchor] (Settings → Close). [onApplied] refreshes and saves. */
    fun attachToSettings(activity: Activity, anchor: android.view.View, onApplied: () -> Unit) {
        val parent = anchor.parent as? ViewGroup ?: return
        if (parent.findViewWithTag<android.view.View>(MARKER) != null) return
        val btn = Button(activity).apply {
            tag = MARKER
            text = "Debug: level 50 + story complete"
            isAllCaps = false
            setOnClickListener {
                val ok = try { nativeJumpToRekindleReady() } catch (t: Throwable) {
                    Log.e(MARKER, "debug shortcut failed", t); false
                }
                if (ok) {
                    Toast.makeText(activity, "[debug] Party is level 50 and the story is complete.", Toast.LENGTH_LONG).show()
                    onApplied()
                } else {
                    Toast.makeText(activity, "[debug] Start or continue a solo adventure (outside a fight) first.", Toast.LENGTH_LONG).show()
                }
            }
        }
        parent.addView(btn, parent.indexOfChild(anchor))
    }
}
