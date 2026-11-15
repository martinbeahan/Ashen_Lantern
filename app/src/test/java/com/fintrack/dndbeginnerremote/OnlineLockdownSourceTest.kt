package com.fintrack.dndbeginnerremote

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

/** v2.37 #90 regression guards on MainActivity's online wiring (it isn't unit-testable directly). */
class OnlineLockdownSourceTest {
    private val main: String = generateSequence(File("").absoluteFile) { it.parentFile }
        .map { File(it, "app/src/main/java/com/fintrack/dndbeginnerremote/MainActivity.kt") }
        .first { it.isFile }.readText()

    private fun body(fn: String): String {
        val i = main.indexOf("private fun $fn(")
        assertTrue(fn, i >= 0)
        val j = main.indexOf("\n    private fun ", i + 10)
        return main.substring(i, if (j < 0) main.length else j)
    }

    @Test fun hostDoesNotSubscribeToGameState() {
        val s = body("setupMultiplayer")
        assertTrue(s.contains("if (OnlinePolicy.appliesRemoteState(asHost)) mp.listenForUpdates"))
        assertFalse(Regex("\n\\s+mp\\.listenForUpdates").containsMatchIn(s))
    }

    @Test fun hostChecksWhoSentEachAction() {
        val s = body("setupMultiplayer")
        assertTrue(s.contains("OnlinePolicy.actionAllowed("))
        assertTrue(s.indexOf("OnlinePolicy.actionAllowed(") < s.indexOf("applyHostAction(action)"))
        assertTrue(s.contains("OnlinePolicy.claim("))
        assertTrue(s.contains("MultiplayerManager(id, uid)"))
    }

    @Test fun hostAndJoinSignInFirstAndUseNewIds() {
        assertTrue(body("startHostingAsDm").contains("withOnlineSignIn("))
        assertTrue(body("startHostingAsDmSignedIn").contains("SessionIds.newId()"))
        assertFalse(body("startHostingAsDmSignedIn").contains("getSessionId()"))
        assertTrue(main.contains("\"join\" -> withOnlineSignIn("))
        assertTrue(body("showJoinDialog").contains("SessionIds.normalize("))
    }

    @Test fun joinStillLeavesSoloSaveAlone() {
        val j = body("joinSignedIn")
        assertTrue(j.contains("activateSession(armCrashGuard = false)"))
        assertFalse(j.contains("\"save_state\""))
    }

    @Test fun resumeNeedsANewIdAndExistingSignIn() {
        assertTrue(main.contains("SessionIds.isValid(savedSid)"))
        assertTrue(main.contains("OnlineAuth.currentUid()"))
    }
}
