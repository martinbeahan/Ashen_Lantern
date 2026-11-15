package com.fintrack.dndbeginnerremote

import android.util.Log
import com.google.firebase.auth.FirebaseAuth

/**
 * v2.37 (#90): Firebase anonymous sign-in before Host / Join. The database rules refuse every read
 * and write without auth, so a hero signs in (silently, no account or personal data) and then
 * writes as that uid. The uid is kept by Firebase across launches on this device.
 */
object OnlineAuth {
    private const val TAG = "OnlineAuth"

    /** The signed-in uid, or null (Firebase missing / not signed in). */
    fun currentUid(): String? = try { FirebaseAuth.getInstance().currentUser?.uid } catch (e: Exception) { null }

    /** Calls [done] on the main thread with the uid, or with null and an error detail. */
    fun ensureSignedIn(done: (uid: String?, error: String?) -> Unit) {
        val auth = try { FirebaseAuth.getInstance() } catch (e: Exception) {
            Log.w(TAG, "FirebaseAuth unavailable (${e.message})")
            done(null, "Firebase not configured"); return
        }
        auth.currentUser?.uid?.let { done(it, null); return }
        try {
            auth.signInAnonymously()
                .addOnSuccessListener { r ->
                    val uid = r.user?.uid
                    if (uid.isNullOrBlank()) done(null, "no user id") else done(uid, null)
                }
                .addOnFailureListener { e ->
                    Log.w(TAG, "anonymous sign-in failed", e)
                    done(null, e.message?.take(120))
                }
        } catch (e: Exception) {
            Log.w(TAG, "anonymous sign-in threw", e)
            done(null, e.message?.take(120))
        }
    }
}
