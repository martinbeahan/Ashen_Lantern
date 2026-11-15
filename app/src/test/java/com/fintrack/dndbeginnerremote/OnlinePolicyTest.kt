package com.fintrack.dndbeginnerremote

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/** v2.37 #90: host is authoritative; joiners act only as the hero they joined with. */
class OnlinePolicyTest {
    @Test fun hostNeverAppliesRemoteState() {
        assertFalse(OnlinePolicy.appliesRemoteState(isHost = true))
        assertTrue(OnlinePolicy.appliesRemoteState(isHost = false))
    }

    @Test fun actionsNeedTheSendersOwnHero() {
        val joined = mapOf("uidA" to "Ada", "uidB" to "Bo")
        assertTrue(OnlinePolicy.actionAllowed("attack", "uidA", "Ada", joined))
        assertFalse("impersonation", OnlinePolicy.actionAllowed("attack", "uidB", "Ada", joined))
        assertFalse("unknown uid", OnlinePolicy.actionAllowed("attack", "uidC", "Ada", joined))
        assertFalse("no uid (pre-2.37 client)", OnlinePolicy.actionAllowed("attack", null, "Ada", joined))
        assertFalse(OnlinePolicy.actionAllowed("attack", "", "Ada", joined))
        assertTrue("join is how a uid arrives", OnlinePolicy.actionAllowed("join", "uidC", "Cy", joined))
        assertFalse(OnlinePolicy.actionAllowed("join", null, "Cy", joined))
    }

    @Test fun claimRefusesAnotherPlayersHero() {
        val joined = mutableMapOf<String, String>()
        assertTrue(OnlinePolicy.claim(joined, "uidA", "Ada"))
        assertTrue("same uid again", OnlinePolicy.claim(joined, "uidA", "Ada"))
        assertFalse("taken by uidA", OnlinePolicy.claim(joined, "uidB", "Ada"))
        assertEquals("Ada", joined["uidA"])
        assertFalse(joined.containsKey("uidB"))
        assertTrue(OnlinePolicy.claim(joined, "uidB", "Bo"))
        assertFalse(OnlinePolicy.actionAllowed("attack", "uidB", "Ada", joined))
    }

    @Test fun claimRefusesHostAndNpcNames() {
        val joined = mutableMapOf<String, String>()
        assertFalse(OnlinePolicy.claim(joined, "uidA", "Keeper", setOf("Keeper")))
        assertFalse(OnlinePolicy.claim(joined, "uidA", "Bren (NPC)"))
        assertFalse(OnlinePolicy.claim(joined, null, "Ada"))
        assertFalse(OnlinePolicy.claim(joined, "uidA", " "))
        assertTrue(joined.isEmpty())
    }

    @Test fun outgoingChatMatchesRulesLimit() {
        assertEquals("hi", OnlinePolicy.outgoingChat("hi"))
        assertEquals(500, OnlinePolicy.outgoingChat("z".repeat(9000)).length)
        val emoji = "a".repeat(499) + "\uD83D\uDE00" + "tail"
        val cut = OnlinePolicy.outgoingChat(emoji)
        assertEquals(499, cut.length)
        assertFalse(Character.isHighSurrogate(cut.last()))
    }

    @Test fun signInFailurePointsAtTheDoc() {
        val m = OnlinePolicy.signInFailedMessage("CONFIGURATION_NOT_FOUND")
        assertTrue(m.contains("CONFIGURATION_NOT_FOUND"))
        assertTrue(m.contains("ONLINE_SECURITY.md"))
        assertTrue(OnlinePolicy.signInFailedMessage(null).contains("Solo adventures still work"))
    }
}
