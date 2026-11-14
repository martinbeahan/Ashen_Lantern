package com.fintrack.dndbeginnerremote

import android.util.Log
import com.google.firebase.database.ChildEventListener
import com.google.firebase.database.DataSnapshot
import com.google.firebase.database.DatabaseError
import com.google.firebase.database.FirebaseDatabase
import com.google.firebase.database.ServerValue
import com.google.firebase.database.ValueEventListener

/**
 * Firebase Realtime Database session bridge.
 * sessions/{id}/meta, gameState, actions, lobby, chat
 *
 * v2.37 (#90): every write carries the writer's anonymous-auth [uid] and matches the locked-down rules
 * in firebase/database.rules.json:
 *  - meta {hostUid, hostName, role, sessionId, updatedAt} and gameState: the host (meta/hostUid) only;
 *  - lobby/{uid}: that uid only; actions/{push} and chat/{push}: created by the uid named inside,
 *    actions deleted by their author or the host.
 */
class MultiplayerManager(private val sessionId: String, private val uid: String = "") {
    private val tag = "Multiplayer"

    private var database: FirebaseDatabase? = try {
        FirebaseDatabase.getInstance().also {
            try { it.setPersistenceEnabled(false) } catch (_: Exception) { }
        }
    } catch (e: Exception) {
        Log.w(tag, "Firebase unavailable (${e.message}).")
        null
    }

    private val sessionRef = database?.getReference("sessions")?.child(sessionId)

    private var stateListener: ValueEventListener? = null
    private var actionListener: ChildEventListener? = null
    private var lobbyListener: ChildEventListener? = null
    private var chatListener: ChildEventListener? = null
    private var metaListener: ValueEventListener? = null

    val isAvailable: Boolean get() = sessionRef != null
    val sessionIdValue: String get() = sessionId

    fun publishMeta(hostName: String, role: String = "dm") {
        sessionRef?.child("meta")?.updateChildren(
            mapOf(
                "hostUid" to uid,
                "hostName" to hostName.take(OnlinePolicy.MAX_NAME_CHARS),
                "role" to role,
                "sessionId" to sessionId,
                "updatedAt" to ServerValue.TIMESTAMP
            )
        )
    }

    fun listenForMeta(onMeta: (hostName: String) -> Unit) {
        val ref = sessionRef?.child("meta") ?: return
        metaListener?.let { ref.removeEventListener(it) }
        val listener = object : ValueEventListener {
            override fun onDataChange(snapshot: DataSnapshot) {
                val name = snapshot.child("hostName").getValue(String::class.java)
                if (!name.isNullOrBlank()) onMeta(name)
            }
            override fun onCancelled(error: DatabaseError) {}
        }
        metaListener = listener
        ref.addValueEventListener(listener)
    }

    fun updateState(data: String) {
        sessionRef?.child("gameState")?.setValue(data)
            ?.addOnSuccessListener { Log.d(tag, "gameState published (${data.length} chars)") }
            ?.addOnFailureListener { Log.e(tag, "updateState failed: ${it.message}") }
    }

    fun listenForUpdates(onUpdate: (String) -> Unit) {
        val ref = sessionRef?.child("gameState") ?: return
        stateListener?.let { ref.removeEventListener(it) }
        val listener = object : ValueEventListener {
            override fun onDataChange(snapshot: DataSnapshot) {
                val data = snapshot.getValue(String::class.java)
                if (!data.isNullOrBlank()) onUpdate(data)
            }
            override fun onCancelled(error: DatabaseError) {
                Log.e(tag, "state cancelled: ${error.message}")
            }
        }
        stateListener = listener
        ref.addValueEventListener(listener)
    }

    fun pushAction(action: Map<String, Any>) {
        val payload = HashMap<String, Any>(action)
        payload["uid"] = uid // v2.37: the rules only accept an action created by the uid it names
        payload["ts"] = ServerValue.TIMESTAMP
        sessionRef?.child("actions")?.push()?.setValue(payload)
            ?.addOnFailureListener { Log.e(tag, "pushAction failed: ${it.message}") }
    }

    /** Joiners register here — more reliable than action maps for party sync. v2.37: keyed by auth uid. */
    fun publishLobbyJoin(playerName: String, classId: Int) {
        if (uid.isBlank()) return
        sessionRef?.child("lobby")?.child(uid)?.setValue(
            mapOf(
                "playerName" to playerName.take(OnlinePolicy.MAX_NAME_CHARS),
                "classId" to classId,
                "ts" to ServerValue.TIMESTAMP
            )
        )?.addOnFailureListener { Log.e(tag, "lobby join failed: ${it.message}") }
    }

    fun listenForLobby(onJoin: (uid: String, playerName: String, classId: Int) -> Unit) {
        val ref = sessionRef?.child("lobby") ?: return
        lobbyListener?.let { ref.removeEventListener(it) }
        val listener = object : ChildEventListener {
            override fun onChildAdded(snapshot: DataSnapshot, previousChildName: String?) {
                val name = snapshot.child("playerName").getValue(String::class.java)
                    ?: snapshot.key
                    ?: return
                val rawClass = try {
                    snapshot.child("classId").getValue(Long::class.java)?.toInt()
                } catch (_: Exception) { null } // non-numeric classId from a bad client
                // H2: only 0..4 are real classes; anything else joins as Fighter.
                val classId = TextSafety.validClassId(rawClass)
                if (rawClass != classId) Log.w(tag, "lobby: bad classId $rawClass for '$name' — using 0")
                onJoin(snapshot.key.orEmpty(), TextSafety.sanitizeName(name, "Hero"), classId)
            }
            override fun onChildChanged(snapshot: DataSnapshot, previousChildName: String?) {}
            override fun onChildRemoved(snapshot: DataSnapshot) {}
            override fun onChildMoved(snapshot: DataSnapshot, previousChildName: String?) {}
            override fun onCancelled(error: DatabaseError) {
                Log.e(tag, "lobby cancelled: ${error.message}")
            }
        }
        lobbyListener = listener
        ref.addChildEventListener(listener)
    }

    fun listenForActions(onAction: (actionId: String, fromUid: String?, action: Map<String, Any>) -> Unit) {
        val ref = sessionRef?.child("actions") ?: return
        actionListener?.let { ref.removeEventListener(it) }
        val listener = object : ChildEventListener {
            override fun onChildAdded(snapshot: DataSnapshot, previousChildName: String?) {
                val id = snapshot.key ?: return
                val raw = hashMapOf<String, Any>()
                for (child in snapshot.children) {
                    val k = child.key ?: continue
                    val v = child.value ?: continue
                    raw[k] = v
                }
                if (raw.isEmpty()) return
                onAction(id, raw["uid"] as? String, raw)
            }
            override fun onChildChanged(snapshot: DataSnapshot, previousChildName: String?) {}
            override fun onChildRemoved(snapshot: DataSnapshot) {}
            override fun onChildMoved(snapshot: DataSnapshot, previousChildName: String?) {}
            override fun onCancelled(error: DatabaseError) {
                Log.e(tag, "actions cancelled: ${error.message}")
            }
        }
        actionListener = listener
        ref.addChildEventListener(listener)
    }

    fun ackAction(actionId: String) {
        sessionRef?.child("actions")?.child(actionId)?.removeValue()
    }

    /** v2.37: chat entries are {uid, text, ts} (the rules refuse a bare string or another uid). */
    fun sendChat(message: String) {
        if (uid.isBlank()) return
        sessionRef?.child("chat")?.push()?.setValue(
            mapOf("uid" to uid, "text" to OnlinePolicy.outgoingChat(message), "ts" to ServerValue.TIMESTAMP)
        )
    }

    fun listenForChat(callback: (String) -> Unit) {
        val ref = sessionRef?.child("chat") ?: return
        chatListener?.let { ref.removeEventListener(it) }
        val listener = object : ChildEventListener {
            override fun onChildAdded(snapshot: DataSnapshot, previousChildName: String?) {
                // v2.37: {uid, text, ts}; a bare string from an older client is still shown. Safe casts only.
                val raw = snapshot.value
                val msg = (raw as? String) ?: (snapshot.child("text").value as? String)
                if (!msg.isNullOrBlank()) callback(msg.take(OnlinePolicy.MAX_CHAT_CHARS))
            }
            override fun onChildChanged(snapshot: DataSnapshot, previousChildName: String?) {}
            override fun onChildRemoved(snapshot: DataSnapshot) {}
            override fun onChildMoved(snapshot: DataSnapshot, previousChildName: String?) {}
            override fun onCancelled(error: DatabaseError) {}
        }
        chatListener = listener
        ref.addChildEventListener(listener)
    }

    fun detach() {
        stateListener?.let { sessionRef?.child("gameState")?.removeEventListener(it) }
        actionListener?.let { sessionRef?.child("actions")?.removeEventListener(it) }
        lobbyListener?.let { sessionRef?.child("lobby")?.removeEventListener(it) }
        chatListener?.let { sessionRef?.child("chat")?.removeEventListener(it) }
        metaListener?.let { sessionRef?.child("meta")?.removeEventListener(it) }
        stateListener = null
        actionListener = null
        lobbyListener = null
        chatListener = null
        metaListener = null
    }
}
