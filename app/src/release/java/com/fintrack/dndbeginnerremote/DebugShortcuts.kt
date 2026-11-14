package com.fintrack.dndbeginnerremote

import android.app.Activity

/**
 * Release builds: the debug test shortcuts do not exist (see src/debug/…/DebugShortcuts.kt).
 * No native method, no button, nothing reachable. Checked by the release-variant unit test and
 * app/build.gradle.kts `verifyReleaseHasNoDebugShortcuts`.
 */
object DebugShortcuts {
    const val ENABLED = false

    @Suppress("UNUSED_PARAMETER")
    fun attachToSettings(activity: Activity, anchor: android.view.View, onApplied: () -> Unit) = Unit
}
