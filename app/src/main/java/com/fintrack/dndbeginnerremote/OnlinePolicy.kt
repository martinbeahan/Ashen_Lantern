package com.fintrack.dndbeginnerremote

/**
 * v2.37 (#90): the client-side half of the online lockdown (the database rules are the other half,
 * firebase/database.rules.json). Pure Kotlin so the decisions are JVM-tested.
 *
 *  - The host is authoritative: it never loads a game state from the database (it only writes one).
 *  - Every joiner writes as its Firebase anonymous-auth uid. The host remembers which hero name each
 *    uid joined with (from lobby/{uid}) and ignores actions that name somebody else's hero.
 *  - Outgoing chat / names are cut to the same limits the rules enforce (#88).
 */
object OnlinePolicy {
    const val MAX_CHAT_CHARS = 500
    const val MAX_NAME_CHARS = 64

    /** Only a joiner applies remote game state; the host ignores it entirely. */
    fun appliesRemoteState(isHost: Boolean): Boolean = !isHost

    /**
     * Host side: may the action from [uid], naming [playerName], run? "join" is allowed for any signed-in
     * uid (it is how a hero arrives); everything else needs the uid to have joined with that hero name.
     */
    fun actionAllowed(type: String, uid: String?, playerName: String, joined: Map<String, String>): Boolean {
        if (uid.isNullOrBlank()) return false
        if (type == "join") return true
        val name = joined[uid] ?: return false
        return name == playerName
    }

    /**
     * Host side: [uid] arrives as hero [playerName] (lobby entry or "join" action). Returns false — and
     * changes nothing — when another uid already holds that name, so nobody can take over a hero by
     * joining with its name. A uid that re-joins keeps its name (a rename moves its claim).
     */
    fun claim(
        joined: MutableMap<String, String>,
        uid: String?,
        playerName: String,
        reserved: Set<String> = emptySet()
    ): Boolean {
        if (uid.isNullOrBlank() || playerName.isBlank()) return false
        // The host's own name and the NPC companions ("Bren (NPC)") are never a joiner's hero.
        if (playerName in reserved || playerName.endsWith("(NPC)")) return false
        val holder = joined.entries.firstOrNull { it.value == playerName }?.key
        if (holder != null && holder != uid) return false
        joined[uid] = playerName
        return true
    }

    /** Chat text as sent: cut to [MAX_CHAT_CHARS] without splitting an emoji. */
    fun outgoingChat(text: String): String {
        if (text.length <= MAX_CHAT_CHARS) return text
        var cut = MAX_CHAT_CHARS
        if (Character.isHighSurrogate(text[cut - 1])) cut--
        return text.substring(0, cut)
    }

    /** Message shown when anonymous sign-in fails (usually: not enabled in the Firebase console yet). */
    fun signInFailedMessage(detail: String?): String =
        "Couldn't sign in for online play" + (if (detail.isNullOrBlank()) "." else " ($detail).") +
            "\n\nThe Lanternkeeper's Firebase project needs Anonymous sign-in turned on " +
            "(see docs/ONLINE_SECURITY.md). Solo adventures still work."
}
