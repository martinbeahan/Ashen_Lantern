package com.fintrack.dndbeginnerremote

import android.content.SharedPreferences

import android.content.ClipboardManager
import android.content.Context
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.View
import android.widget.Button
import android.widget.CheckBox
import android.widget.EditText
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import android.view.Gravity
import android.widget.ImageView
import android.widget.ProgressBar
import android.view.animation.AccelerateDecelerateInterpolator
import android.view.animation.AlphaAnimation
import android.view.animation.Animation
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.google.firebase.FirebaseApp

class MainActivity : AppCompatActivity() {
    private val TAG = "DnDMain"

    private lateinit var statusText: TextView
    private lateinit var turnBanner: TextView
    private var hudXpRow: View? = null
    private var hudXpBar: EmberXpView? = null
    private var hudXpText: TextView? = null
    private var hudXpLabel: TextView? = null
    /** name -> last known pendingStatPoints; used to fire level-up toasts. */
    private val lastPendingByName = mutableMapOf<String, Int>()
    private var sheetPulseAnimator: android.animation.ObjectAnimator? = null
    private var lastLevelUpNotifyKey = ""
    private var levelUpDialogShowing = false
    private lateinit var logText: TextView
    private lateinit var roomDescText: TextView
    private lateinit var chatInput: EditText
    private lateinit var btnSendChat: Button
    private lateinit var btnAttack: Button
    private lateinit var btnSpecial: Button
    private lateinit var btnHeal: Button
    private lateinit var btnInteract: Button
    private lateinit var btnRest: Button
    private lateinit var btnReset: Button
    private lateinit var btnSheet: Button
    private lateinit var btnCompanion: Button
    private lateinit var btnJournal: Button
    private lateinit var btnHelp: Button
    private lateinit var btnInGameSettings: Button
    private lateinit var partyColumn: LinearLayout
    private lateinit var enemyColumn: LinearLayout
    private lateinit var battleArena: LinearLayout
    private var battleFxOverlay: FrameLayout? = null
    private var combatFlashOverlay: View? = null
    private var lastBattleBgKey = ""
    private var battleStageImage: ImageView? = null
    private var battleBossOverlay: View? = null
    
    private var multiplayer: MultiplayerManager? = null
    private var lastBattleRoster = ""
    private var lastAnimEvent = ""
    /** Dedupe short kill / rare-loot juice toasts within a session tick. */
    private var lastKillPopKey = ""
    private var lastRareLootKey = ""
    /** Roster slot index for foes (names can duplicate — never key selection by name alone). */
    private var selectedEnemyIndex: Int? = null
    /** Roster slot index for allies. */
    private var selectedAllyIndex: Int? = null
    private var foeTargetingMode = false
    private var allyTargetingMode = false
    private var lastSelectionRoomKey = ""
    private val combatFeed = ArrayDeque<String>()
    private var diceOverlayRoot: View? = null
    private var diceTitle: TextView? = null
    private var diceD20: TextView? = null
    private var diceDamage: TextView? = null
    private var diceSummary: TextView? = null
    private var diceHideRunnable: Runnable? = null
    private lateinit var mainMenuRoot: View
    private lateinit var btnMenuContinue: Button
    private lateinit var btnMenuSolo: Button
    private lateinit var btnMenuCrawl: Button
    private lateinit var btnMenuRaid: Button
    private lateinit var mainMenuRaidHint: TextView
    private lateinit var btnMenuChallenge: Button
    private lateinit var mainMenuChallengeHint: TextView
    private lateinit var btnMenuArena: Button
    private lateinit var mainMenuArenaHint: TextView
    private lateinit var btnMenuEndless: Button
    private lateinit var mainMenuEndlessHint: TextView
    private lateinit var btnMenuDifficulty: Button
    private lateinit var btnMenuHost: Button
    private lateinit var btnMenuJoin: Button
    private lateinit var btnMenuSettings: Button
    private lateinit var btnMenuQuit: Button
    private lateinit var mainMenuSubtitle: TextView
    private lateinit var mainMenuDifficultyLabel: TextView
    private lateinit var settingsRoot: View
    private lateinit var chkBeginnerTips: CheckBox
    private lateinit var chkMusic: CheckBox
    private lateinit var chkSfx: CheckBox
    private lateinit var chkDmVoice: CheckBox
    private lateinit var chkReduceFlash: CheckBox
    private lateinit var btnSettingsTutorial: Button
    private lateinit var btnSettingsAbandon: Button
    private lateinit var btnSettingsReturnMenu: Button
    private lateinit var btnSettingsBossRaid: Button
    private lateinit var settingsRaidHint: TextView
    private lateinit var btnSettingsChallenge: Button
    private lateinit var settingsChallengeHint: TextView
    private lateinit var btnSettingsArena: Button
    private lateinit var settingsArenaHint: TextView
    private lateinit var btnSettingsEndless: Button
    private lateinit var settingsEndlessHint: TextView
    private lateinit var btnSettingsEndlessRetreat: Button
    private lateinit var btnSettingsClose: Button
    private lateinit var settingsAboutText: TextView
    private lateinit var settingsDifficultyText: TextView
    private lateinit var mainMenuDailyQuests: TextView
    private lateinit var btnMenuClaimDaily: Button
    private lateinit var settingsDailyQuests: TextView
    private lateinit var btnSettingsClaimDaily: Button
    private lateinit var btnMenuLedger: Button
    private lateinit var mainMenuLedgerHint: TextView
    private lateinit var btnSettingsLedger: Button
    /** Last native daily-quest counter snap (rooms,fights,searches,damage,bosses). */
    private var lastDqSnap = intArrayOf(0, 0, 0, 0, 0)
    /** One-shot CTA after Acts 1–4 so player can open Boss Raid / menu without relaunch. */
    private var storyCompleteRoutesShown = false
    // Audio prefs + GameAudio (BGM / SFX / optional on-device TTS).
    private var musicEnabled = true
    private var sfxEnabled = true
    private var dmVoiceEnabled = false
    /** Softens/disables full-screen flashes & rapid opacity strobes (photosensitive-friendly). Default OFF. */
    private var reduceFlashEnabled = false
    private var gameAudio: GameAudio? = null
    private var lastAudioBed: GameAudio.Track? = null
    /** Sticky while a boss / raid fight is active so bed stays BOSS after the event scrolls. */
    private var bossFightActive = false
    private val handler = Handler(Looper.getMainLooper())
    private var uiLoopStarted = false
    private val uiTick = object : Runnable {
        override fun run() {
            if (!sessionActive || !nativeReady) {
                handler.postDelayed(this, 500)
                return
            }
            try {
                processGameTurn()
                updateUi()
            } catch (e: Exception) {
                Log.e(TAG, "ui tick failed", e)
            }
            handler.postDelayed(this, 300)
        }
    }
    /** Live raid-key countdown while the main menu (or Settings raid hint) is visible. */
    private val menuRaidKeyTick = object : Runnable {
        override fun run() {
            val menuVisible = ::mainMenuRoot.isInitialized && mainMenuRoot.visibility == View.VISIBLE
            val settingsVisible = ::settingsRoot.isInitialized && settingsRoot.visibility == View.VISIBLE
            if (!menuVisible && !settingsVisible) return
            try {
                refreshRaidKeyCountdownUi()
            } catch (e: Exception) {
                Log.e(TAG, "raid key countdown tick failed", e)
            }
            handler.postDelayed(this, 1000)
        }
    }
    private var lastProcessedEvent = ""
    private var lastRoomDesc = ""
    private var lastChatHistory = ""
    private var isUpdatingFromRemote = false
    private var localPlayerName = "Hero"
    private var localClassId = 0
    private var lastCoachTip = ""
    private var lastCoachTurnKey = ""
    private var soloCoachEnabled = true
    /** Preferred difficulty (0 Easy … 3 Nightmare). Locked into a run when adventure starts. */
    private var preferredDifficulty = 0
    /** Difficulty locked for the active run (-1 = none). */
    private var runDifficulty = -1
    private var wipeDialogShowing = false
    /** True after Continue / New Game / Join — avoids wiping save in onPause before that. */
    private var sessionActive = false
    /** True when this device is the online DM (authoritative). */
    private var isOnlineHost = false
    /** True when joined someone else's session. */
    private var isOnlineClient = false
    private var onlineSessionId = ""
    private var remoteDmName = ""

    companion object {
        private const val NATIVE_LIB = "dndbeginnerremote"
        /** versionCode that introduced save slots (recorded with the legacy backup). */
        private const val SLOTS_VERSION_CODE = 67
        /** False if the C++ library failed to load (common when NDK/CMake build failed). */
        @JvmStatic var nativeReady: Boolean = false
            private set

        init {
            try {
                System.loadLibrary(NATIVE_LIB)
                nativeReady = true
            } catch (e: UnsatisfiedLinkError) {
                nativeReady = false
                Log.e("DnDMain", "Failed to load native library '$NATIVE_LIB'. Rebuild with NDK installed.", e)
            }
        }
    }

    // JNI Methods
    external fun getPlayerStatus(): String
    /** v2.28: non-adopting summary of a save blob ("" = would not load). Tab-separated. */
    external fun describeSaveState(data: String): String
    external fun getBattleRoster(): String
    external fun getDetailedSheet(playerName: String): String
    /** "xp,threshold,level,pending" for one hero; empty if missing. */
    external fun getXpProgress(playerName: String): String
    /** "name|xp|threshold|level|pending;..." for the whole party. */
    external fun getPartyXpProgress(): String
    external fun getJournal(): String
    external fun getLastEvent(): String
    external fun getRoomDescription(): String
    external fun getSpecialName(): String
    external fun getChatHistory(): String
    external fun getSessionId(): String
    external fun getShopManifest(): String
    external fun isMerchantRoom(): Boolean
    external fun doAttack(targetIndex: Int)
    external fun doHeal(targetIndex: Int)
    external fun doSpecial(targetIndex: Int)
    external fun doInteract(playerName: String)
    external fun doRest()
    external fun doLongRest()
    external fun doUseInventoryPotion(playerName: String, invIndex: Int): Boolean
    external fun doIncreaseStat(playerName: String, statIndex: Int)
    external fun doBuyItem(playerName: String, itemIndex: Int): Boolean
    external fun getInventoryManifest(playerName: String): String
    external fun doEquipItem(playerName: String, invIndex: Int): Boolean
    external fun doUnequipItem(playerName: String, slot: Int): Boolean
    external fun doUpgradeInventoryItem(playerName: String, invIndex: Int): Boolean
    external fun doUpgradeEquippedItem(playerName: String, slot: Int): Boolean
    external fun doSellInventoryItem(playerName: String, invIndex: Int): Boolean
    external fun doSellEquippedItem(playerName: String, slot: Int): Boolean
    external fun doTransferInventoryItemToAlly(playerName: String, invIndex: Int): Boolean
    external fun doTransferEquippedItemToAlly(playerName: String, slot: Int): Boolean
    external fun isInCombat(): Boolean
    external fun isRoomCleared(): Boolean
    external fun hasSearchedRoom(): Boolean
    external fun hasUsedRoomRest(): Boolean
    external fun getLongRestCooldownRooms(): Int
    external fun getCurrentActorResources(): Int
    external fun getCurrentActorMaxResources(): Int
    external fun doAdvanceRoom()
    external fun setHost(isHost: Boolean)
    external fun addRemoteAlly(characterClass: Int, playerName: String)
    external fun sendChatMessage(sender: String, message: String)
    external fun processGameTurn()
    external fun resetGame(
        characterClass: Int,
        playerName: String,
        soloPlayMode: Int,
        difficulty: Int,
        companionClass: Int,
        companionAutoAi: Boolean
    )
    external fun getCompanionName(): String
    external fun isCompanionAutoAi(): Boolean
    external fun setCompanionAutoAi(autoAi: Boolean)
    external fun setCompanionClass(characterClass: Int): Boolean
    external fun setDifficulty(difficulty: Int)
    external fun getDifficulty(): Int
    external fun getSoloPlayMode(): Int
    /** SoloQuestBeat value (QuestScript.h) — drives the stage background. */
    external fun getQuestBeat(): Int
    /** v2.25 Kindling choices: 0 none, 1 Sallow Venn (beat 13), 2 the well (beat 19). */
    external fun getPendingStoryChoice(): Int
    external fun chooseStoryOption(kind: Int, option: Int): Boolean
    /** "venn,well": 0 undecided; Venn 1 spared / 2 ended; well 1 sealed in / 2 let rest. */
    external fun getStoryChoices(): String
    external fun refreshStoryRoomAfterLoad(): Boolean
    // v2.26 Memory Ledger — native decides grants; the store is MemoryLedger.Store (meta prefs, slot 1).
    external fun setMemoryLedgerContext(ownedCsv: String, crawlDry: Int)
    external fun needsMemoryContext(): Boolean
    external fun drainMemoryGrants(): String
    external fun getProgressStoryMemories(): String
    external fun previewStoryMemories(save: String): String
    external fun getMemoryCatalog(venn: Int, well: Int): String
    external fun isStoryFullyComplete(): Boolean
    external fun consumePendingRaidKeyDrop(): Boolean
    external fun beginBossRaidFromCurrent(): Boolean
    external fun finishBossRaidKeepParty()
    external fun beginChallengeDungeonFromCurrent(legendaryChancePercent: Int): Boolean
    external fun finishChallengeDungeonKeepParty()
    external fun consumePendingChallengeLegendaryFound(): Boolean
    external fun getChallengeLegendaryChance(): Int
    external fun setChallengeLegendaryChance(percent: Int)
    external fun beginArenaFromCurrent(): Boolean
    external fun finishArenaKeepParty()
    external fun getArenaWave(): Int
    external fun getArenaScore(): Int
    external fun getArenaWavesCleared(): Int
    external fun beginEndlessDeepFromCurrent(): Boolean
    external fun finishEndlessDeepKeepParty()
    external fun getEndlessDepth(): Int
    external fun getEndlessBestDepthThisRun(): Int
    external fun getDailyQuestCounters(): String
    external fun grantDailyQuestSpoils(gold: Int, giveLoot: Boolean): Boolean
    external fun recoverFromPartyWipe(): Boolean
    external fun startDmSession(dmName: String)
    external fun dmBeginDungeon()
    external fun dmAdvanceRoom()
    external fun dmNarrate(line: String)
    external fun dmGrantShortRest()
    external fun isDmTable(): Boolean
    external fun prepareClientJoin()
    external fun saveGameState(): String
    external fun loadGameState(data: String)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        Log.d(TAG, "onCreate started")
        verifyEnemySpriteTable()

        
        try {
            FirebaseApp.initializeApp(this)
        } catch (e: Exception) {
            Log.e(TAG, "Firebase init failed: ${e.message}")
        }

        setContentView(R.layout.activity_main)
        applyEdgeToEdgeInsets()

        if (!nativeReady) {
            Toast.makeText(
                this,
                "Native game library failed to load. In Android Studio: Tools → SDK Manager → SDK Tools → enable NDK and CMake, then Build → Rebuild Project.",
                Toast.LENGTH_LONG
            ).show()
        }
        
        statusText = findViewById(R.id.playerStatusText)
        turnBanner = findViewById(R.id.turnBanner)
        hudXpRow = findViewById(R.id.hudXpRow)
        hudXpBar = findViewById(R.id.hudXpBar)
        hudXpText = findViewById(R.id.hudXpText)
        hudXpLabel = findViewById(R.id.hudXpLabel)
        logText = findViewById(R.id.combatLogText)
        roomDescText = findViewById(R.id.roomDescText)
        chatInput = findViewById(R.id.chatInput)
        btnSendChat = findViewById(R.id.btnSendChat)
        btnAttack = findViewById(R.id.btnAttack)
        btnSpecial = findViewById(R.id.btnSpecial)
        btnHeal = findViewById(R.id.btnHeal)
        bindPotionButtonIcon()
        btnInteract = findViewById(R.id.btnInteract)
        btnRest = findViewById(R.id.btnRest)
        btnReset = findViewById(R.id.btnReset)
        btnSheet = findViewById(R.id.btnSheet)
        btnCompanion = findViewById(R.id.btnCompanion)
        btnJournal = findViewById(R.id.btnJournal)
        btnHelp = findViewById(R.id.btnHelp)
        btnInGameSettings = findViewById(R.id.btnInGameSettings)
        partyColumn = findViewById(R.id.partyColumn)
        enemyColumn = findViewById(R.id.enemyColumn)
        battleArena = findViewById(R.id.battleArena)
        battleFxOverlay = findViewById(R.id.battleFxOverlay)
        battleStageImage = findViewById(R.id.battleStageImage)
        battleBossOverlay = findViewById(R.id.battleBossOverlay)
        combatFlashOverlay = findViewById(R.id.combatFlashOverlay)
        wireDiceOverlay()
        wireMainMenuAndSettings()

        btnAttack.setOnClickListener { 
            Log.d(TAG, "Attack clicked")
            if (isMerchantRoom()) {
                showShopDialog()
                return@setOnClickListener
            }
            val cleared = try { isRoomCleared() } catch (_: Exception) { false }
            if (cleared) {
                performOrQueue("advance", 0) { doAdvanceRoom() }
                return@setOnClickListener
            }
            resolveTargetedAction(needEnemy = true, actionType = "attack") { i ->
                doAttack(i); animateAttack(true, targetEnemyIndex = i); scheduleDiceFromLastEvent()
            }
        }
        
        btnSpecial.setOnClickListener {
            Log.d(TAG, "Special clicked")
            if (isMerchantRoom()) {
                val blurb = try { getRoomDescription() } catch (_: Exception) { "A merchant greets you." }
                Toast.makeText(this, blurb, Toast.LENGTH_LONG).show()
                return@setOnClickListener
            }
            // Cleric Healing Word picks ally internally; still allow foe targeting for other classes.
            val special = try { getSpecialName() } catch (_: Exception) { "" }
            val resLeft = try { getCurrentActorResources() } catch (_: Exception) { 0 }
            if (resLeft <= 0) {
                Toast.makeText(this, "No special uses left — Short Rest or Long Rest to recover.", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            if (special.contains("Healing", ignoreCase = true)) {
                performOrQueue("special", 0) {
                    doSpecial(0)
                    val ev = try { getLastEvent() } catch (_: Exception) { "" }
                    if (ev.contains("No ", ignoreCase = true) && (ev.contains("left", ignoreCase = true) || ev.contains("remaining", ignoreCase = true) || ev.contains("already used", ignoreCase = true))) {
                        Toast.makeText(this, ev, Toast.LENGTH_SHORT).show()
                    }
                    scheduleDiceFromLastEvent()
                }
            } else {
                resolveTargetedAction(needEnemy = true, actionType = "special") { i ->
                    doSpecial(i)
                    val ev = try { getLastEvent() } catch (_: Exception) { "" }
                    if (ev.contains("already used", ignoreCase = true) ||
                        (ev.contains("No ", ignoreCase = true) && (ev.contains("remaining", ignoreCase = true) || ev.contains("left", ignoreCase = true)))
                    ) {
                        Toast.makeText(this, ev, Toast.LENGTH_SHORT).show()
                    } else {
                        animateAttack(true, targetEnemyIndex = i)
                    }
                    scheduleDiceFromLastEvent()
                }
            }
        }
        btnHeal.setOnClickListener {
            Log.d(TAG, "Heal clicked")
            resolveTargetedAction(needEnemy = false, actionType = "heal") { i ->
                doHeal(i); scheduleDiceFromLastEvent()
            }
        }
        btnInteract.setOnClickListener {
            Log.d(TAG, "Interact clicked")
            val inCombat = try { isInCombat() } catch (_: Exception) { false }
            if (inCombat) {
                Toast.makeText(this, "Clear the room before Searching.", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            val searched = try { hasSearchedRoom() } catch (_: Exception) { false }
            if (searched) {
                Toast.makeText(this, "Already searched this room.", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            performOrQueue("interact", 0) { doInteract(localPlayerName) }
        }
        btnRest.setOnClickListener {
            Log.d(TAG, "Rest clicked")
            if (try { isMerchantRoom() } catch (_: Exception) { false }) {
                performOrQueue("rest", 0) { doRest() }
                return@setOnClickListener
            }
            val inCombat = try { isInCombat() } catch (_: Exception) { false }
            if (inCombat) {
                Toast.makeText(this, "Can't rest in the middle of a fight!", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            val alreadyRested = try { hasUsedRoomRest() } catch (_: Exception) { false }
            if (alreadyRested) {
                Toast.makeText(this, "Already rested here — press Onward for another chance.", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            showRestChooser()
        }

        btnSendChat.setOnClickListener {
            val msg = chatInput.text.toString()
            if (msg.isNotEmpty()) {
                val formattedMsg = "[$localPlayerName]: $msg"
                sendChatMessage(localPlayerName, formattedMsg)
                multiplayer?.sendChat(formattedMsg)
                chatInput.text.clear()
            }
        }

        btnSheet.setOnClickListener {
            if (isOnlineHost && (try { isDmTable() } catch (_: Exception) { true })) showDmToolsDialog()
            else showCharacterSheet()
        }
        btnCompanion.setOnClickListener {
            try { gameAudio?.playUiClick() } catch (_: Exception) {}
            showCompanionSheet()
        }
        btnJournal.setOnClickListener { showJournal() }
        btnHelp.setOnClickListener { showHelpMenu() }
        btnInGameSettings.setOnClickListener {
            try { gameAudio?.playUiClick() } catch (_: Exception) {}
            showSettingsOverlay()
        }
        btnReset.setOnClickListener {
            val status = try { getPlayerStatus() } catch (_: Exception) { "" }
            if (status.contains("Game Over", ignoreCase = true)) {
                wipeDialogShowing = false
                handlePartyWipeUi()
            } else {
                returnToMainMenuSaving(confirm = true)
            }
        }
        btnReset.text = "Menu"

        findViewById<View>(R.id.topBar).setOnLongClickListener {
            val sid = getSessionId()
            val clip = android.content.ClipData.newPlainText("AL_SESSION", sid)
            (getSystemService(CLIPBOARD_SERVICE) as ClipboardManager).setPrimaryClip(clip)
            Toast.makeText(this, "Session ID $sid Copied!", Toast.LENGTH_SHORT).show()
            true
        }

        soloCoachEnabled = prefs().getBoolean("solo_coach_enabled", true)
        musicEnabled = prefs().getBoolean("pref_music_enabled", true)
        sfxEnabled = prefs().getBoolean("pref_sfx_enabled", true)
        dmVoiceEnabled = prefs().getBoolean("pref_dm_voice_enabled", false)
        reduceFlashEnabled = prefs().getBoolean("pref_reduce_flash", false)
        preferredDifficulty = prefs().getInt("pref_difficulty", 0).coerceIn(0, 3)
        gameAudio = GameAudio(this).also {
            it.start(musicEnabled, sfxEnabled, dmVoiceEnabled)
        }

        // v2.28: move the single legacy save into slot 1 (idempotent; legacy keys kept as backup).
        runSlotMigration(showDialogs = true)
        // v2.28 softer crash guard: only the guarded slot; keep / restore snapshot / mark broken.
        recoverSlotAfterCrash()

        showStartDialog()
        startUiUpdateLoop()
    }

    override fun onDestroy() {
        handler.removeCallbacks(uiTick)
        handler.removeCallbacks(menuRaidKeyTick)
        uiLoopStarted = false
        gameAudio?.release()
        gameAudio = null
        super.onDestroy()
    }

    override fun onPause() {
        super.onPause()
        gameAudio?.onPause()
        // Flush save before process death so progress survives relaunch.
        // Skip until a session is active so the start dialog can't overwrite a good save with an empty game.
        if (!nativeReady || !sessionActive) return
        // Host/Join must never overwrite the solo/story save_state with ephemeral multiplayer state.
        if (isOnlineHost || isOnlineClient) return
        try {
            val data = saveGameState()
            flushPlaytime()
            if (data.isNotBlank() && data.contains("|") && !saveIsGameOver(data)) {
                // Good save written to the active slot — not an unclean exit.
                SaveSlots.writeSoloSave(rawPrefs(), isOnline(), data, localPlayerName, localClassId,
                    System.currentTimeMillis(), commit = true)
            }
        } catch (e: Exception) {
            Log.e(TAG, "onPause save failed", e)
        }
    }

    override fun onResume() {
        super.onResume()
        gameAudio?.onResume()
        if (sessionActive && !isOnline()) playClockStartMs = android.os.SystemClock.elapsedRealtime()
    }

    /** Detach Firebase listeners and clear online flags before mode switches / start menu. */
    private fun detachOnlineSession() {
        try { multiplayer?.detach() } catch (e: Exception) { Log.w(TAG, "detach failed", e) }
        multiplayer = null
        isOnlineHost = false
        isOnlineClient = false
        onlineSessionId = ""
        remoteDmName = ""
        joinTimeoutRunnable?.let { handler.removeCallbacks(it) }
        joinTimeoutRunnable = null
    }

    /** Clear Host/Join resume markers without touching solo save_state. */
    private fun clearOnlineResumePrefs() {
        prefs().edit()
            .remove("was_online_host")
            .remove("was_online_client")
            .remove("online_session_id")
            .apply()
    }

    private var joinTimeoutRunnable: Runnable? = null

    private fun resetToStartMenu() {
        sessionActive = false
        detachOnlineSession()
        lastPendingByName.clear()
        lastLevelUpNotifyKey = ""
        sheetPulseAnimator?.cancel()
        prefs().edit().putBoolean("crash_guard", false).apply()
        clearOnlineResumePrefs()
        showStartDialog()
    }

    /**
     * @param armCrashGuard When true (solo/crawl/raid), unclean exit clears save_state.
     * Online Host/Join must pass false so a crash cannot wipe the preserved solo save.
     */
    private fun activateSession(armCrashGuard: Boolean = true) {
        sessionActive = true
        hideMainMenu()
        hideSettingsOverlay()
        // Arm crash guard (for the active slot) only while a real solo session is running.
        SaveSlots.armCrashGuard(rawPrefs(), online = !armCrashGuard)
        playClockStartMs = if (armCrashGuard) android.os.SystemClock.elapsedRealtime() else 0L
        // Seed pending snapshot so Continue with unspent points highlights Sheet
        // without re-firing the level-up dialog.
        seedPendingSnapshot(notifyHighlightOnly = true)
        resetDailyQuestSnap()
        // Solo only: deliver stashed daily spoils into save_state (never online).
        if (armCrashGuard) {
            applyPendingDailySpoilsIfAny()
        }
    }

    private fun seedPendingSnapshot(notifyHighlightOnly: Boolean) {
        lastLevelUpNotifyKey = ""
        lastPendingByName.clear()
        if (!nativeReady) return
        for ((name, prog) in parsePartyXpProgress()) {
            lastPendingByName[name] = prog.pending
        }
        if (!notifyHighlightOnly) return
        val anyPending = lastPendingByName.values.any { it > 0 }
        val dmTable = isOnlineHost && try { isDmTable() } catch (_: Exception) { isOnlineHost }
        if (!dmTable) setSheetButtonHighlight(anyPending)
    }

    private fun currentActorName(): String {
        return try {
            val status = getPlayerStatus()
            val turnLine = status.lineSequence().firstOrNull { it.contains("Turn:") } ?: return ""
            turnLine.substringAfter("Turn:").trim().substringBefore("|").trim()
        } catch (_: Exception) {
            ""
        }
    }

    /** Host-side: reject actions that aren't from the current actor. */
    private fun validateActorOrReject(player: String): Boolean {
        val actor = currentActorName()
        if (actor.isBlank() || actor.equals("Safe", ignoreCase = true) || actor.equals("None", ignoreCase = true)) {
            return true // merchant / free moment
        }
        if (actor.equals(player, ignoreCase = true)) return true
        Toast.makeText(this, "Not $player's turn (it's $actor).", Toast.LENGTH_SHORT).show()
        try { sendChatMessage("System", "Rejected $player's action — current turn is $actor.") } catch (_: Exception) {}
        return false
    }

    private fun performOrQueue(actionType: String, targetIndex: Int, localApply: () -> Unit) {
        try {
            val st = getPlayerStatus()
            if (st.contains("Game Over", ignoreCase = true)) {
                Toast.makeText(this, "Game Over — use the wipe dialog or Menu.", Toast.LENGTH_LONG).show()
                btnReset.visibility = View.VISIBLE
                handlePartyWipeUi()
                return
            }
            val localDown = st.lineSequence().any {
                it.contains(localPlayerName) && (
                    it.contains("[DOWN]", ignoreCase = true) ||
                    it.contains("[DEAD]", ignoreCase = true) ||
                    it.contains("[STABLE]", ignoreCase = true)
                )
            }
            val companionTurn = isCompanionPlayerTurn()
            // Downed/dead heroes never attack/search/heal; party Rest/Onward after a clear may continue.
            // Player-controlled companion turns still allow Attack/Special/Potion.
            val clearedNow = try { isRoomCleared() } catch (_: Exception) { false }
            if (localDown && !companionTurn) {
                val partyContinue = clearedNow && actionType in setOf("rest", "advance")
                if (!partyContinue && actionType in setOf("attack", "special", "heal", "interact", "rest", "advance")) {
                    Toast.makeText(this, "You're down — wait for a heal or Game Over.", Toast.LENGTH_SHORT).show()
                    return
                }
            }
        } catch (_: Exception) {}
        if (isOnlineClient) {
            val mp = multiplayer
            if (mp == null || !mp.isAvailable) {
                Toast.makeText(this, "Not connected to the Lanternkeeper's session.", Toast.LENGTH_SHORT).show()
                return
            }
            mp.pushAction(
                mapOf(
                    "type" to actionType,
                    "targetIndex" to targetIndex,
                    "statIndex" to targetIndex,
                    "playerName" to localPlayerName,
                    "classId" to localClassId
                )
            )
            Toast.makeText(this, "Sent to the Lanternkeeper…", Toast.LENGTH_SHORT).show()
            return
        }
        localApply()
        syncAndSave()
    }

    private fun livingUnits(isEnemy: Boolean): List<Pair<Int, BattleUnit>> {
        val (party, foes) = parseBattleRoster(try { getBattleRoster() } catch (_: Exception) { "" })
        val list = if (isEnemy) foes else party
        return list.mapIndexedNotNull { idx, u ->
            // Foes: only living. Allies: include 0-HP downed so Potion/heal can target them.
            if (isEnemy && u.hp <= 0) null else idx to u
        }
    }

    private fun resolveSelectedIndex(needEnemy: Boolean): Int? {
        if (needEnemy) {
            val idx = selectedEnemyIndex ?: return null
            // Prefer exact roster slot; livingUnits filters dead foes but keeps original indices.
            return livingUnits(true).firstOrNull { it.first == idx }?.first
        }
        val idx = selectedAllyIndex
        if (idx != null) {
            livingUnits(false).firstOrNull { it.first == idx }?.first?.let { return it }
        }
        // Potion defaults to self
        return livingUnits(false).firstOrNull { it.second.name.equals(localPlayerName, true) }?.first
            ?: livingUnits(false).firstOrNull()?.first
    }

    private fun clearCombatSelection(reason: String? = null) {
        selectedEnemyIndex = null
        selectedAllyIndex = null
        foeTargetingMode = false
        allyTargetingMode = false
        if (reason != null) Log.d(TAG, "Cleared selection: $reason")
        applySelectionHighlights()
        stopTargetPulse()
    }

    private fun resolveTargetedAction(needEnemy: Boolean, actionType: String, action: (Int) -> Unit) {
        val idx = resolveSelectedIndex(needEnemy)
        if (idx != null) {
            Log.d(TAG, "Using selected index $idx for $actionType")
            if (needEnemy) foeTargetingMode = false else allyTargetingMode = false
            stopTargetPulse()
            performOrQueue(actionType, idx) {
                action(idx)
                // Clear selection after the action resolves
                if (needEnemy) selectedEnemyIndex = null else selectedAllyIndex = null
                applySelectionHighlights()
            }
            return
        }
        if (needEnemy) {
            val living = livingUnits(true)
            if (living.isEmpty()) {
                Toast.makeText(this, "No living foes.", Toast.LENGTH_SHORT).show()
                return
            }
            foeTargetingMode = true
            allyTargetingMode = false
            startTargetPulse(enemyColumn)
            Toast.makeText(this, "Tap a foe", Toast.LENGTH_SHORT).show()
            applySelectionHighlights()
        } else {
            val living = livingUnits(false)
            if (living.isEmpty()) {
                Toast.makeText(this, "No living allies.", Toast.LENGTH_SHORT).show()
                return
            }
            allyTargetingMode = true
            foeTargetingMode = false
            startTargetPulse(partyColumn)
            Toast.makeText(this, "Tap an ally (or yourself)", Toast.LENGTH_SHORT).show()
            applySelectionHighlights()
        }
    }

    /** Legacy AlertDialog picker retained only as fallback (unused by Attack/Special/Potion). */
    private fun handleActionWithTarget(
        title: String,
        isEnemy: Boolean = true,
        actionType: String,
        action: (Int) -> Unit
    ) {
        resolveTargetedAction(needEnemy = isEnemy, actionType = actionType, action = action)
    }

    private fun firebaseReady(): Boolean {
        return try {
            if (com.google.firebase.FirebaseApp.getApps(this).isEmpty()) {
                FirebaseApp.initializeApp(this)
            }
            val probe = MultiplayerManager("PROBE")
            val ok = probe.isAvailable
            probe.detach()
            ok
        } catch (e: Exception) {
            Log.w(TAG, "firebaseReady failed", e)
            false
        }
    }

    private fun showFirebaseRequiredDialog(then: (() -> Unit)? = null) {
        AlertDialog.Builder(this)
            .setTitle("Firebase needed for online play")
            .setMessage(
                "Host / Join need Firebase Realtime Database.\n\n" +
                    "1. Place app/google-services.json from the Firebase console (required locally)\n" +
                    "2. The Google Services plugin is already enabled in app/build.gradle.kts\n" +
                    "3. Enable Realtime Database (see FIREBASE_SETUP.md)\n" +
                    "4. Clean + Rebuild\n\n" +
                    "Host: enter your Lanternkeeper name only — friends Join and pick a class.\n" +
                    "Solo adventure works without Firebase."
            )
            .setPositiveButton("OK") { _, _ -> then?.invoke() }
            .setCancelable(false)
            .show()
    }

    private fun setupMultiplayer(id: String, asHost: Boolean) {
        Log.d(TAG, "Setting up multiplayer session=$id asHost=$asHost")
        multiplayer?.detach()
        val mp = MultiplayerManager(id)
        multiplayer = mp
        onlineSessionId = id
        isOnlineHost = asHost
        isOnlineClient = !asHost

        if (!mp.isAvailable) {
            Toast.makeText(this, "Firebase unavailable — online session not connected.", Toast.LENGTH_LONG).show()
            isOnlineHost = false
            isOnlineClient = false
            multiplayer = null
            return
        }

        if (asHost) {
            mp.publishMeta(localPlayerName, role = "dm")
            mp.listenForLobby { name, classId ->
                runOnUiThread {
                    try {
                        admitPlayer(name, classId)
                        syncAndSave()
                    } catch (e: Exception) {
                        Log.e(TAG, "lobby admit failed", e)
                    }
                }
            }
            mp.listenForActions { actionId, action ->
                runOnUiThread {
                    try {
                        applyHostAction(action)
                        mp.ackAction(actionId)
                        syncAndSave()
                    } catch (e: Exception) {
                        Log.e(TAG, "host action failed", e)
                    }
                }
            }
        } else {
            mp.listenForMeta { name ->
                runOnUiThread {
                    remoteDmName = name
                    updateUi()
                }
            }
        }

        mp.listenForUpdates { data ->
            runOnUiThread {
                if (data.isBlank() || isUpdatingFromRemote) return@runOnUiThread
                // Host ignores echoes of its own publishes
                if (isOnlineHost) {
                    try {
                        if (data == saveGameState()) return@runOnUiThread
                    } catch (_: Exception) {}
                }
                try {
                    isUpdatingFromRemote = true
                    loadGameState(data)
                    lastBattleRoster = ""
                    lastRoomDesc = ""
                    lastChatHistory = ""
                    // Successful sync from DM — cancel join wait.
                    if (isOnlineClient) {
                        joinTimeoutRunnable?.let { handler.removeCallbacks(it) }
                        joinTimeoutRunnable = null
                        // Do not stamp was_online_* — Continue owns the solo save_state slot only.
                        prefs().edit().putBoolean("crash_guard", false).apply()
                    }
                    refreshBattleArena()
                    updateUi()
                } catch (e: Exception) {
                    Log.e(TAG, "remote state apply failed", e)
                } finally {
                    isUpdatingFromRemote = false
                }
            }
        }

        mp.listenForChat { msg ->
            runOnUiThread {
                try {
                    if (!getChatHistory().contains(msg)) sendChatMessage("Remote", msg)
                } catch (e: Exception) {
                    Log.w(TAG, "remote chat failed", e)
                }
            }
        }
    }

    private fun partyHasPlayer(name: String): Boolean {
        return try {
            val partySide = getBattleRoster().substringBefore('|')
            partySide.split(';').any { entry ->
                entry.split(',').getOrNull(1)?.equals(name, ignoreCase = true) == true
            }
        } catch (_: Exception) {
            getPlayerStatus().lineSequence().any {
                it.startsWith("$name |") || it.startsWith("$name|")
            }
        }
    }

    private fun admitPlayer(rawPlayer: String, rawClassId: Int) {
        // C1 / H2: remote joiners are untrusted — sanitize the name and validate the class.
        val player = TextSafety.sanitizeName(rawPlayer, "Hero")
        val classId = TextSafety.validClassId(rawClassId)
        if (rawClassId != classId) Log.w(TAG, "Joiner '$player' sent class $rawClassId — using Fighter")
        if (partyHasPlayer(player)) return
        addRemoteAlly(classId, player)
        try { sendChatMessage("Keeper", "$player has joined the party.") } catch (_: Exception) {}
        Toast.makeText(this, "$player joined the table.", Toast.LENGTH_SHORT).show()
        lastBattleRoster = ""
        refreshBattleArena()
    }

    private fun applyHostAction(action: Map<String, Any>) {
        val type = action["type"]?.toString() ?: return
        // Sanitized the same way admitPlayer stored it, so turn validation matches older clients too.
        val player = TextSafety.sanitizeName(action["playerName"]?.toString(), "Hero")
        val target = (action["targetIndex"] as? Number)?.toInt()
            ?: action["targetIndex"]?.toString()?.toIntOrNull()
            ?: 0
        val classId = (action["classId"] as? Number)?.toInt()
            ?: action["classId"]?.toString()?.toIntOrNull()
            ?: 0
        val statIndex = (action["statIndex"] as? Number)?.toInt()
            ?: action["statIndex"]?.toString()?.toIntOrNull()
            ?: target

        when (type) {
            "join" -> admitPlayer(player, classId)
            "attack" -> {
                if (!validateActorOrReject(player)) return
                doAttack(target)
                animateAttack(true, targetEnemyIndex = target)
                scheduleDiceFromLastEvent()
            }
            "special" -> {
                if (!validateActorOrReject(player)) return
                doSpecial(target)
                animateAttack(true, targetEnemyIndex = target)
                scheduleDiceFromLastEvent()
            }
            "heal" -> {
                if (!validateActorOrReject(player)) return
                doHeal(target)
            }
            "rest" -> {
                if (!validateActorOrReject(player)) return
                val inCombat = try { isInCombat() } catch (_: Exception) { false }
                if (inCombat) {
                    Toast.makeText(this, "Can't Short Rest in combat.", Toast.LENGTH_SHORT).show()
                    return
                }
                if (try { hasUsedRoomRest() } catch (_: Exception) { false }) {
                    Toast.makeText(this, "Already rested here — press Onward for another chance.", Toast.LENGTH_SHORT).show()
                    return
                }
                doRest()
            }
            "longRest" -> {
                if (!validateActorOrReject(player)) return
                val inCombat = try { isInCombat() } catch (_: Exception) { false }
                if (inCombat) {
                    Toast.makeText(this, "Can't Long Rest in combat.", Toast.LENGTH_SHORT).show()
                    return
                }
                val lrCd = try { getLongRestCooldownRooms() } catch (_: Exception) { 0 }
                if (lrCd > 0) {
                    Toast.makeText(this, "Long Rest on cooldown — $lrCd room(s) left.", Toast.LENGTH_SHORT).show()
                    return
                }
                if (try { isMerchantRoom() } catch (_: Exception) { false }) {
                    Toast.makeText(this, "Leave the merchant before a Long Rest.", Toast.LENGTH_SHORT).show()
                    return
                }
                if (try { hasUsedRoomRest() } catch (_: Exception) { false }) {
                    Toast.makeText(this, "Already rested here — press Onward for another chance.", Toast.LENGTH_SHORT).show()
                    return
                }
                doLongRest()
            }
            "interact" -> {
                if (!validateActorOrReject(player)) return
                val inCombat = try { isInCombat() } catch (_: Exception) { false }
                if (inCombat) {
                    Toast.makeText(this, "Clear the room before Searching.", Toast.LENGTH_SHORT).show()
                    return
                }
                doInteract(player)
            }
            "advance" -> {
                if (!validateActorOrReject(player)) return
                doAdvanceRoom()
            }
            "buy" -> doBuyItem(player, target)
            "levelup", "increaseStat" -> doIncreaseStat(player, statIndex)
            "levelupBatch" -> applyLevelUpBatch(player, action["allocations"]?.toString().orEmpty())
            else -> Log.w(TAG, "Unknown action type: $type")
        }
    }

    private fun syncAndSave() {
        if (!nativeReady) return
        try {
            val data = saveGameState()
            val online = isOnlineHost || isOnlineClient
            if (online) {
                // Online Host/Join uses in-memory + Firebase state only.
                // Never overwrite prefs save_state (solo/story Continue slot).
                Log.d(TAG, "Online session — skipping local save_state write")
            } else if (saveIsGameOver(data)) {
                val d = try { getDifficulty() } catch (_: Exception) { preferredDifficulty }
                if (d == 3) {
                    clearSave("nightmare defeat during sync")
                } else {
                    // Keep Game Over save so Easy/Med/Hard Continue can recover.
                    flushPlaytime()
                    SaveSlots.writeSoloSave(rawPrefs(), online, data, localPlayerName, localClassId, System.currentTimeMillis())
                }
            } else {
                flushPlaytime()
                SaveSlots.writeSoloSave(rawPrefs(), online, data, localPlayerName, localClassId, System.currentTimeMillis())
            }
            // Only the DM/host publishes authoritative state
            if (!isUpdatingFromRemote && isOnlineHost) {
                try { multiplayer?.updateState(data) } catch (e: Exception) {
                    Log.w(TAG, "multiplayer update failed", e)
                }
            }
            lastBattleRoster = ""
            refreshBattleArena()
            // v2.28: online tables never touch any slot's endgame flags, records or pity.
            if (online) return
            // Raid victory / finishBossRaidKeepParty leaves soloPlayMode != RAID.
            if (prefs().getBoolean("raid_active", false)) {
                val mode = try { getSoloPlayMode() } catch (_: Exception) { 2 }
                if (mode != 2) clearRaidSessionFlags()
            }
            // Challenge Dungeon clear/wipe leaves soloPlayMode != CHALLENGE (3).
            if (prefs().getBoolean("challenge_active", false)) {
                val mode = try { getSoloPlayMode() } catch (_: Exception) { 3 }
                if (mode != 3) clearChallengeSessionFlags()
            }
            // Arena clear/wipe leaves soloPlayMode != ARENA (4).
            if (prefs().getBoolean("arena_active", false)) {
                val mode = try { getSoloPlayMode() } catch (_: Exception) { 4 }
                if (mode != 4) {
                    val score = try { getArenaScore() } catch (_: Exception) { 0 }
                    val waves = try { getArenaWavesCleared() } catch (_: Exception) { 0 }
                    Arena.onRunFinished(prefs(), score, waves)
                    if (score > 0) {
                        Toast.makeText(
                            this,
                            "Ember Ring score $score (best ${Arena.bestScore(prefs())}).",
                            Toast.LENGTH_LONG
                        ).show()
                    }
                    clearArenaSessionFlags()
                }
            }
            // Endless Deep clear/wipe leaves soloPlayMode != ENDLESS (5).
            if (prefs().getBoolean("endless_active", false)) {
                val mode = try { getSoloPlayMode() } catch (_: Exception) { 5 }
                if (mode != 5) {
                    val depth = try { getEndlessBestDepthThisRun() } catch (_: Exception) { 0 }
                    EndlessDeep.onRunFinished(prefs(), depth)
                    if (depth > 0) {
                        Toast.makeText(
                            this,
                            "Ashen Deep depth $depth (best ${EndlessDeep.bestDepth(prefs())}).",
                            Toast.LENGTH_LONG
                        ).show()
                    }
                    clearEndlessSessionFlags()
                }
            }
            // Challenge Dungeon: apply per-Legendary decay into meta prefs (Host/Join never).
            if (!isOnlineHost && !isOnlineClient) {
                try {
                    if (consumePendingChallengeLegendaryFound()) {
                        val next = ChallengeDungeon.onLegendaryFound(prefs())
                        try { setChallengeLegendaryChance(next) } catch (_: Exception) {}
                        Toast.makeText(
                            this,
                            "Legendary found — Challenge chance now $next% (floor ${ChallengeDungeon.FLOOR}%, cap ${ChallengeDungeon.CAP}%).",
                            Toast.LENGTH_LONG
                        ).show()
                    }
                } catch (e: Exception) {
                    Log.w(TAG, "challenge legendary decay failed", e)
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "syncAndSave failed", e)
        }
    }

    private fun savedState(): String? {
        val data = prefs().getString("save_state", null)
        return data?.takeIf { it.isNotBlank() && it.contains("|") }
    }

    /** Save header: sessionId,roomCount,gameOver,turn,merchant|... */
    private fun saveIsGameOver(data: String): Boolean {
        val bits = data.substringBefore('|').split(',')
        return bits.size >= 3 && bits[2].trim() == "1"
    }

    private fun clearSave(reason: String? = null) {
        prefs().edit().remove("save_state").apply()
        if (reason != null) Log.i(TAG, "Cleared save: $reason")
    }

    private fun livableSavedState(): String? {
        val data = savedState() ?: return null
        // Keep Game Over saves so Easy/Med/Hard can Continue via wipe dialog.
        // Nightmare clears save immediately on wipe.
        return data
    }

    private fun heroNameFromSave(data: String): String {
        val stored = prefs().getString("hero_name", null)?.trim().orEmpty()
        if (stored.isNotEmpty()) return stored
        val parts = data.split('|')
        if (parts.size < 3) return "Hero"
        val field = parts[2].substringBefore(';').substringBefore(',')
        val name = (if (TextSafety.isEncodedSave(data)) TextSafety.decode(field) else field).trim()
        return name.ifEmpty { "Hero" }
    }

    private fun continueSavedGame() {
        if (!nativeReady) {
            Toast.makeText(this, "Native library not ready — can't load save.", Toast.LENGTH_LONG).show()
            showStartDialog()
            return
        }
        detachOnlineSession()
        val data = livableSavedState() ?: run {
            Toast.makeText(this, "No living adventure to continue — start a new one.", Toast.LENGTH_LONG).show()
            showStartDialog()
            return
        }
        localPlayerName = heroNameFromSave(data)
        localClassId = prefs().getInt("hero_class", 0)
        val wasHost = prefs().getBoolean("was_online_host", false)
        val wasClient = prefs().getBoolean("was_online_client", false)
        val savedSid = prefs().getString("online_session_id", "")?.trim().orEmpty()
        try {
            loadGameState(data)
        } catch (e: Exception) {
            Log.e(TAG, "Failed to load save", e)
            // v2.28: never delete on a failed load — keep it, mark this slot, offer Retry / Delete.
            markSlotBrokenAndOffer(activeSlot())
            return
        }
        // v2.25: a mid-Act 2–4 save picks up the rewritten room text (journal / chat stay as history).
        try { refreshStoryRoomAfterLoad() } catch (_: Exception) {}
        lastBattleRoster = ""
        lastRoomDesc = ""
        lastChatHistory = ""
        lastProcessedEvent = ""
        lastCoachTip = ""
        lastCoachTurnKey = ""
        val status = try { getPlayerStatus() } catch (_: Exception) { "" }
        if (!status.contains(localPlayerName) && !status.contains("| HP:") && !status.contains("Lanternkeeper:") && !status.contains("DM:")) {
            markSlotBrokenAndOffer(activeSlot())
            return
        }
        if (status.contains("Game Over", ignoreCase = true) || saveIsGameOver(data)) {
            // Easy/Med/Hard may still Continue from wipe rules
            val d = try { getDifficulty() } catch (_: Exception) { preferredDifficulty }
            if (d in 0..2) {
                activateSession()
                runDifficulty = d
                hideMainMenu()
                handlePartyWipeUi()
                return
            }
            clearSave("continued into game over")
            Toast.makeText(this, "That adventure already ended. Start a new one.", Toast.LENGTH_LONG).show()
            showStartDialog()
            return
        }
        runDifficulty = try { getDifficulty() } catch (_: Exception) { preferredDifficulty }
        // Successful deserialize + live status — do not treat as unclean exit.
        prefs().edit().putBoolean("crash_guard", false).apply()

        val resumeOnline = savedSid.isNotBlank() && (wasHost || wasClient) && firebaseReady()
        if (resumeOnline && wasHost) {
            setHost(true)
            activateSession()
            prefs().edit().putBoolean("crash_guard", false).apply()
            btnReset.visibility = View.GONE
            setupMultiplayer(savedSid, asHost = true)
            if (!isOnlineHost || multiplayer?.isAvailable != true) {
                // Fall back to local authority with the loaded state.
                setHost(true)
                isOnlineHost = false
                isOnlineClient = false
                multiplayer = null
                onlineSessionId = ""
                Toast.makeText(this, "Welcome back (offline) — could not resume Host session.", Toast.LENGTH_LONG).show()
            } else {
                syncAndSave()
                Toast.makeText(this, "Welcome back, Lanternkeeper — resumed session $savedSid", Toast.LENGTH_SHORT).show()
            }
        } else if (resumeOnline && wasClient) {
            setHost(false)
            activateSession()
            prefs().edit().putBoolean("crash_guard", false).apply()
            btnReset.visibility = View.GONE
            setupMultiplayer(savedSid, asHost = false)
            if (!isOnlineClient || multiplayer?.isAvailable != true) {
                setHost(true) // keep playing the snapshot solo
                isOnlineHost = false
                isOnlineClient = false
                multiplayer = null
                onlineSessionId = ""
                Toast.makeText(this, "Welcome back (offline) — Lanternkeeper session unavailable.", Toast.LENGTH_LONG).show()
            } else {
                scheduleJoinTimeout(savedSid, resuming = true)
                Toast.makeText(this, "Rejoining $savedSid…", Toast.LENGTH_SHORT).show()
            }
        } else {
            setHost(true)
            isOnlineHost = false
            isOnlineClient = false
            multiplayer = null
            onlineSessionId = ""
            activateSession()
            // activateSession re-arms crash_guard; clear again after confirmed good load.
            prefs().edit().putBoolean("crash_guard", false).apply()
            btnReset.visibility = View.GONE
            Toast.makeText(this, "Welcome back, $localPlayerName (solo)", Toast.LENGTH_SHORT).show()
            syncLedgerAfterSoloLoad()
        }
        refreshBattleArena()
        updateUi()
    }



    /**
     * Pick one looping BGM bed from UI/game context. Music toggle still mutes entirely.
     * Order: main menu / merchant → TOWN; boss/raid combat → BOSS; combat → COMBAT; else EXPLORE.
     */
    private fun syncAudioBedFromState() {
        val audio = gameAudio ?: return
        val onMenu = ::mainMenuRoot.isInitialized && mainMenuRoot.visibility == View.VISIBLE
        val inCombat = try { isInCombat() } catch (_: Exception) { false }
        if (!inCombat) bossFightActive = false
        val merchant = try { isMerchantRoom() } catch (_: Exception) { false }
        val raid = try {
            val m = getSoloPlayMode()
            m == 2 || m == 3 || m == 4
        } catch (_: Exception) { false }
        val resolved = when {
            onMenu -> GameAudio.Track.TOWN
            merchant -> GameAudio.Track.TOWN
            inCombat && (raid || bossFightActive) -> GameAudio.Track.BOSS
            inCombat -> GameAudio.Track.COMBAT
            else -> GameAudio.Track.EXPLORE
        }
        if (lastAudioBed != resolved) {
            lastAudioBed = resolved
            audio.setBed(resolved)
        }
    }

    private fun showStartDialog() {
        Log.d(TAG, "Showing main menu")
        // Mode switch / Reset: drop any live Firebase listeners first.
        sessionActive = false
        detachOnlineSession()
        clearOnlineResumePrefs()
        hideSettingsOverlay()
        refreshMainMenuButtons()
        mainMenuRoot.visibility = View.VISIBLE
        mainMenuRoot.bringToFront()
        btnReset.visibility = View.GONE
        bossFightActive = false
        syncAudioBedFromState()
        startMenuRaidKeyCountdown()
    }

    private fun hideMainMenu() {
        if (::mainMenuRoot.isInitialized) {
            mainMenuRoot.visibility = View.GONE
        }
        stopMenuRaidKeyCountdownIfIdle()
        syncAudioBedFromState()
    }

    private fun difficultyLabel(d: Int): String = when (d.coerceIn(0, 3)) {
        0 -> "Easy"
        1 -> "Medium"
        2 -> "Hard"
        else -> "Nightmare"
    }

    private fun refreshMainMenuButtons() {
        if (!::btnMenuContinue.isInitialized) return
        if (::mainMenuDifficultyLabel.isInitialized) {
            mainMenuDifficultyLabel.text = "Difficulty: ${difficultyLabel(preferredDifficulty)}"
        }
        val saved = livableSavedState()
        if (saved != null) {
            val wasHost = prefs().getBoolean("was_online_host", false)
            val wasClient = prefs().getBoolean("was_online_client", false)
            val sid = prefs().getString("online_session_id", "")?.trim().orEmpty()
            val tag = when {
                wasHost && sid.isNotBlank() -> "Host $sid"
                wasClient && sid.isNotBlank() -> "Join $sid"
                else -> "solo"
            }
            btnMenuContinue.visibility = View.VISIBLE
            btnMenuContinue.text = "Continue ($tag) — ${heroNameFromSave(saved)}"
            mainMenuSubtitle.text = "Welcome back, adventurer"
        } else {
            btnMenuContinue.visibility = View.GONE
            mainMenuSubtitle.text = "Choose your path"
        }
        if (::btnMenuRaid.isInitialized) {
            syncRaidKeyDay()
            val storyDone = isMetaStoryComplete()
            val keys = raidKeysHeld()
            btnMenuRaid.isEnabled = storyDone
            btnMenuRaid.alpha = if (storyDone) 1f else 0.45f
            val raidHero = livableSavedState()?.let { heroNameFromSave(it) }
            btnMenuRaid.text = when {
                !storyDone -> "Boss Raid (locked)"
                raidHero != null -> "Boss Raid — $raidHero ($keys/3 keys)"
                else -> "Boss Raid ($keys/3 keys)"
            }
            if (::mainMenuRaidHint.isInitialized) {
                setRaidHint(mainMenuRaidHint, storyDone, keys, forMainMenu = true)
            }
        }
        if (::btnMenuChallenge.isInitialized) {
            val storyDone = isMetaStoryComplete()
            val hasSave = livableSavedState() != null
            val unlocked = storyDone && hasSave
            ChallengeDungeon.ensureInitialized(prefs())
            val chance = ChallengeDungeon.legendaryChance(prefs())
            btnMenuChallenge.isEnabled = unlocked
            btnMenuChallenge.alpha = if (unlocked) 1f else 0.45f
            btnMenuChallenge.text = if (unlocked) "Challenge Dungeon ($chance% Leg)" else "Challenge Dungeon (locked)"
            if (::mainMenuChallengeHint.isInitialized) {
                mainMenuChallengeHint.text = when {
                    !storyDone -> "Finish the story first (Acts 1–4)"
                    !hasSave -> "Need a Continue save (same hero as Boss Raid)"
                    else -> ChallengeDungeon.summaryLine(prefs())
                }
            }
        }
        if (::btnMenuArena.isInitialized) {
            val storyDone = isMetaStoryComplete()
            val hasSave = livableSavedState() != null
            val unlocked = storyDone && hasSave
            btnMenuArena.isEnabled = unlocked
            btnMenuArena.alpha = if (unlocked) 1f else 0.45f
            val best = Arena.bestScore(prefs())
            btnMenuArena.text = if (unlocked) {
                if (best > 0) "Arena (Ember Ring) · best $best" else "Arena (Ember Ring)"
            } else "Arena (locked)"
            if (::mainMenuArenaHint.isInitialized) {
                mainMenuArenaHint.text = when {
                    !storyDone -> "Finish the story first (Acts 1–4)"
                    !hasSave -> "Need a Continue save (same hero as Boss Raid)"
                    else -> Arena.summaryLine(prefs())
                }
            }
        }
        if (::btnMenuEndless.isInitialized) {
            val storyDone = isMetaStoryComplete()
            val hasSave = livableSavedState() != null
            val unlocked = storyDone && hasSave
            btnMenuEndless.isEnabled = unlocked
            btnMenuEndless.alpha = if (unlocked) 1f else 0.45f
            val best = EndlessDeep.bestDepth(prefs())
            btnMenuEndless.text = if (unlocked) {
                if (best > 0) "Endless Deep · best $best" else "Endless Deep (Ashen Deep)"
            } else "Endless Deep (locked)"
            if (::mainMenuEndlessHint.isInitialized) {
                mainMenuEndlessHint.text = when {
                    !storyDone -> "Finish the story first (Acts 1–4)"
                    !hasSave -> "Need a Continue save (same hero as Boss Raid)"
                    else -> EndlessDeep.summaryLine(prefs())
                }
            }
        }
        refreshDailyQuestsUi()
        refreshLedgerEntry()
    }

    private fun showDifficultyPicker(lockForRun: Boolean) {
        if (lockForRun && sessionActive && runDifficulty >= 0) {
            Toast.makeText(this, "Difficulty is locked for this run (${difficultyLabel(runDifficulty)}).", Toast.LENGTH_SHORT).show()
            return
        }
        val labels = arrayOf("Easy", "Medium", "Hard", "Nightmare")
        AlertDialog.Builder(this)
            .setTitle("Difficulty")
            .setSingleChoiceItems(labels, preferredDifficulty.coerceIn(0, 3)) { dialog, which ->
                preferredDifficulty = which
                prefs().edit().putInt("pref_difficulty", preferredDifficulty).apply()
                refreshMainMenuButtons()
                if (::settingsDifficultyText.isInitialized && settingsRoot.visibility == View.VISIBLE) {
                    updateSettingsDifficultyText()
                }
                dialog.dismiss()
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun updateSettingsDifficultyText() {
        if (!::settingsDifficultyText.isInitialized) return
        val locked = sessionActive && runDifficulty >= 0
        val shown = if (locked) runDifficulty else preferredDifficulty
        settingsDifficultyText.text = if (locked) {
            "Difficulty: ${difficultyLabel(shown)} (locked for this run)"
        } else {
            "Difficulty: ${difficultyLabel(shown)} (applies to next adventure)"
        }
    }

    private fun wireMainMenuAndSettings() {
        mainMenuRoot = findViewById(R.id.mainMenuRoot)
        findViewById<ImageView?>(R.id.mainMenuBackdrop)?.let { iv ->
            // Centre-cropped (never stretched); sharper 1080x2340 art on larger / denser screens.
            iv.setImageResource(if (useHdBackgrounds()) R.drawable.bg_menu_backdrop_hd else R.drawable.bg_menu_backdrop)
        }
        btnMenuContinue = findViewById(R.id.btnMenuContinue)
        btnMenuSolo = findViewById(R.id.btnMenuSolo)
        btnMenuCrawl = findViewById(R.id.btnMenuCrawl)
        btnMenuRaid = findViewById(R.id.btnMenuRaid)
        mainMenuRaidHint = findViewById(R.id.mainMenuRaidHint)
        btnMenuChallenge = findViewById(R.id.btnMenuChallenge)
        mainMenuChallengeHint = findViewById(R.id.mainMenuChallengeHint)
        btnMenuArena = findViewById(R.id.btnMenuArena)
        mainMenuArenaHint = findViewById(R.id.mainMenuArenaHint)
        btnMenuEndless = findViewById(R.id.btnMenuEndless)
        mainMenuEndlessHint = findViewById(R.id.mainMenuEndlessHint)
        btnMenuDifficulty = findViewById(R.id.btnMenuDifficulty)
        btnMenuHost = findViewById(R.id.btnMenuHost)
        btnMenuJoin = findViewById(R.id.btnMenuJoin)
        btnMenuSettings = findViewById(R.id.btnMenuSettings)
        btnMenuQuit = findViewById(R.id.btnMenuQuit)
        mainMenuSubtitle = findViewById(R.id.mainMenuSubtitle)
        mainMenuDifficultyLabel = findViewById(R.id.mainMenuDifficultyLabel)
        mainMenuDailyQuests = findViewById(R.id.mainMenuDailyQuests)
        btnMenuClaimDaily = findViewById(R.id.btnMenuClaimDaily)
        btnMenuLedger = findViewById(R.id.btnMenuLedger)
        mainMenuLedgerHint = findViewById(R.id.mainMenuLedgerHint)

        settingsRoot = findViewById(R.id.settingsRoot)
        chkBeginnerTips = findViewById(R.id.chkBeginnerTips)
        chkMusic = findViewById(R.id.chkMusic)
        chkSfx = findViewById(R.id.chkSfx)
        chkDmVoice = findViewById(R.id.chkDmVoice)
        chkReduceFlash = findViewById(R.id.chkReduceFlash)
        btnSettingsTutorial = findViewById(R.id.btnSettingsTutorial)
        btnSettingsReturnMenu = findViewById(R.id.btnSettingsReturnMenu)
        btnSettingsBossRaid = findViewById(R.id.btnSettingsBossRaid)
        settingsRaidHint = findViewById(R.id.settingsRaidHint)
        btnSettingsChallenge = findViewById(R.id.btnSettingsChallenge)
        settingsChallengeHint = findViewById(R.id.settingsChallengeHint)
        btnSettingsArena = findViewById(R.id.btnSettingsArena)
        settingsArenaHint = findViewById(R.id.settingsArenaHint)
        btnSettingsEndless = findViewById(R.id.btnSettingsEndless)
        settingsEndlessHint = findViewById(R.id.settingsEndlessHint)
        btnSettingsEndlessRetreat = findViewById(R.id.btnSettingsEndlessRetreat)
        btnSettingsAbandon = findViewById(R.id.btnSettingsAbandon)
        btnSettingsClose = findViewById(R.id.btnSettingsClose)
        settingsAboutText = findViewById(R.id.settingsAboutText)
        settingsDifficultyText = findViewById(R.id.settingsDifficultyText)
        settingsDailyQuests = findViewById(R.id.settingsDailyQuests)
        btnSettingsClaimDaily = findViewById(R.id.btnSettingsClaimDaily)
        btnSettingsLedger = findViewById(R.id.btnSettingsLedger)

        btnMenuContinue.setOnClickListener {
            // Menu stays until continueSavedGame succeeds (activateSession) or
            // failure paths re-show via showStartDialog().
            continueSavedGame()
        }
        // v2.28: New Story / Crawl go through the Adventures picker (empty lanterns only).
        btnMenuSolo.setOnClickListener { showAdventures(beginMode = "solo") }
        btnMenuCrawl.setOnClickListener { showAdventures(beginMode = "crawl") }
        findViewById<Button?>(R.id.btnMenuAdventures)?.setOnClickListener { showAdventures() }
        btnMenuRaid.setOnClickListener {
            promptBossRaidEntry(fromInAdventure = false)
        }
        btnMenuChallenge.setOnClickListener {
            promptChallengeDungeonEntry(fromInAdventure = false)
        }
        btnMenuArena.setOnClickListener {
            promptArenaEntry(fromInAdventure = false)
        }
        btnMenuEndless.setOnClickListener {
            promptEndlessDeepEntry(fromInAdventure = false)
        }
        btnMenuDifficulty.setOnClickListener {
            showDifficultyPicker(lockForRun = false)
        }
        btnMenuHost.setOnClickListener {
            if (!firebaseReady()) {
                showFirebaseRequiredDialog { showStartDialog() }
                return@setOnClickListener
            }
            askHeroName(hint = "Your Lanternkeeper name") { name ->
                localPlayerName = name
                startHostingAsDm()
            }
        }
        btnMenuJoin.setOnClickListener {
            if (!firebaseReady()) {
                showFirebaseRequiredDialog { showStartDialog() }
                return@setOnClickListener
            }
            askHeroName { name ->
                localPlayerName = name
                showJoinDialog()
            }
        }
        btnMenuSettings.setOnClickListener {
            gameAudio?.playUiClick()
            showSettingsOverlay()
        }
        btnMenuQuit.setOnClickListener { moveTaskToBack(true) }

        chkBeginnerTips.setOnCheckedChangeListener { _, checked ->
            soloCoachEnabled = checked
            prefs().edit().putBoolean("solo_coach_enabled", checked).apply()
        }
        chkMusic.setOnCheckedChangeListener { _, checked ->
            musicEnabled = checked
            prefs().edit().putBoolean("pref_music_enabled", checked).apply()
            gameAudio?.setMusicEnabled(checked)
        }
        chkSfx.setOnCheckedChangeListener { _, checked ->
            sfxEnabled = checked
            prefs().edit().putBoolean("pref_sfx_enabled", checked).apply()
            gameAudio?.setSfxEnabled(checked)
            if (checked) gameAudio?.playUiClick()
        }
        chkDmVoice.setOnCheckedChangeListener { _, checked ->
            dmVoiceEnabled = checked
            prefs().edit().putBoolean("pref_dm_voice_enabled", checked).apply()
            gameAudio?.setDmVoiceEnabled(checked)
        }
        chkReduceFlash.setOnCheckedChangeListener { _, checked ->
            reduceFlashEnabled = checked
            prefs().edit().putBoolean("pref_reduce_flash", checked).apply()
            if (::battleArena.isInitialized) {
                // Swap boss art to / from the calm (reduce-flash) variants.
                lastBattleRoster = ""
                refreshBattleArena()
            }
            if (checked) {
                // Stop any in-flight opacity strobes immediately.
                sheetPulseAnimator?.cancel()
                if (::btnSheet.isInitialized) btnSheet.alpha = 1f
                stopTargetPulse()
                combatFlashOverlay?.animate()?.cancel()
                combatFlashOverlay?.visibility = View.GONE
                combatFlashOverlay?.alpha = 1f
                if (::battleArena.isInitialized) {
                    battleArena.animate().cancel()
                    battleArena.translationX = 0f
                }
            }
        }
        btnSettingsTutorial.setOnClickListener {
            hideSettingsOverlay()
            showTutorial(0) {}
        }
        btnSettingsReturnMenu.setOnClickListener { returnToMainMenuSaving(confirm = true) }
        btnSettingsBossRaid.setOnClickListener {
            hideSettingsOverlay()
            promptBossRaidEntry(fromInAdventure = sessionActive)
        }
        btnSettingsChallenge.setOnClickListener {
            hideSettingsOverlay()
            promptChallengeDungeonEntry(fromInAdventure = sessionActive)
        }
        btnSettingsArena.setOnClickListener {
            hideSettingsOverlay()
            promptArenaEntry(fromInAdventure = sessionActive)
        }
        btnSettingsEndless.setOnClickListener {
            hideSettingsOverlay()
            promptEndlessDeepEntry(fromInAdventure = sessionActive)
        }
        btnSettingsEndlessRetreat.setOnClickListener {
            promptEndlessRetreat()
        }
        btnSettingsAbandon.setOnClickListener { confirmAbandonAdventure() }
        btnSettingsClose.setOnClickListener { hideSettingsOverlay() }
        btnMenuClaimDaily.setOnClickListener { claimDailyQuestsRewards() }
        btnSettingsClaimDaily.setOnClickListener { claimDailyQuestsRewards() }
        btnMenuLedger.setOnClickListener { openMemoryLedger() }
        btnSettingsLedger.setOnClickListener { openMemoryLedger() }
        findViewById<Button?>(R.id.btnSettingsAdventures)?.setOnClickListener {
            if (isOnline()) {
                Toast.makeText(this, "Online tables don't use your local adventures.", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            if (sessionActive) {
                // Save this lantern first (same as Return to Menu), then open the picker.
                returnToMainMenuSaving(confirm = false)
            } else {
                hideSettingsOverlay()
            }
            showAdventures()
        }
    }

    private fun showSettingsOverlay() {
        if (!::settingsRoot.isInitialized) return
        chkBeginnerTips.setOnCheckedChangeListener(null)
        chkMusic.setOnCheckedChangeListener(null)
        chkSfx.setOnCheckedChangeListener(null)
        chkDmVoice.setOnCheckedChangeListener(null)
        chkReduceFlash.setOnCheckedChangeListener(null)
        chkBeginnerTips.isChecked = soloCoachEnabled
        chkMusic.isChecked = musicEnabled
        chkSfx.isChecked = sfxEnabled
        chkDmVoice.isChecked = dmVoiceEnabled
        chkReduceFlash.isChecked = reduceFlashEnabled
        chkBeginnerTips.setOnCheckedChangeListener { _, checked ->
            soloCoachEnabled = checked
            prefs().edit().putBoolean("solo_coach_enabled", checked).apply()
        }
        chkMusic.setOnCheckedChangeListener { _, checked ->
            musicEnabled = checked
            prefs().edit().putBoolean("pref_music_enabled", checked).apply()
            gameAudio?.setMusicEnabled(checked)
        }
        chkSfx.setOnCheckedChangeListener { _, checked ->
            sfxEnabled = checked
            prefs().edit().putBoolean("pref_sfx_enabled", checked).apply()
            gameAudio?.setSfxEnabled(checked)
            if (checked) gameAudio?.playUiClick()
        }
        chkDmVoice.setOnCheckedChangeListener { _, checked ->
            dmVoiceEnabled = checked
            prefs().edit().putBoolean("pref_dm_voice_enabled", checked).apply()
            gameAudio?.setDmVoiceEnabled(checked)
        }
        chkReduceFlash.setOnCheckedChangeListener { _, checked ->
            reduceFlashEnabled = checked
            prefs().edit().putBoolean("pref_reduce_flash", checked).apply()
            if (::battleArena.isInitialized) {
                // Swap boss art to / from the calm (reduce-flash) variants.
                lastBattleRoster = ""
                refreshBattleArena()
            }
            if (checked) {
                sheetPulseAnimator?.cancel()
                if (::btnSheet.isInitialized) btnSheet.alpha = 1f
                stopTargetPulse()
                combatFlashOverlay?.animate()?.cancel()
                combatFlashOverlay?.visibility = View.GONE
                combatFlashOverlay?.alpha = 1f
                if (::battleArena.isInitialized) {
                    battleArena.animate().cancel()
                    battleArena.translationX = 0f
                }
            }
        }
        btnSettingsAbandon.visibility = if (sessionActive) View.VISIBLE else View.GONE
        findViewById<View?>(R.id.btnSettingsAdventures)?.visibility = if (isOnline()) View.GONE else View.VISIBLE
        btnSettingsReturnMenu.visibility = if (sessionActive) View.VISIBLE else View.GONE
        refreshSettingsRaidEntry()
        refreshDailyQuestsUi()
        refreshLedgerEntry()
        updateSettingsDifficultyText()
        settingsDifficultyText.setOnClickListener {
            if (sessionActive && runDifficulty >= 0) {
                Toast.makeText(this, "Difficulty is locked for this run.", Toast.LENGTH_SHORT).show()
            } else {
                showDifficultyPicker(lockForRun = false)
            }
        }
        val verCode = try {
            packageManager.getPackageInfo(packageName, 0).longVersionCode.toInt()
        } catch (_: Exception) {
            19
        }
        val verName = try {
            packageManager.getPackageInfo(packageName, 0).versionName ?: "1.3"
        } catch (_: Exception) {
            "1.3"
        }
        settingsAboutText.text =
            getString(R.string.app_name) + "\nversion " + verName + " (" + verCode + ")" +
                "\n\nAshen Lantern is not affiliated with, sponsored by, endorsed by, or approved by " +
                "Wizards of the Coast LLC. Dungeons & Dragons, D&D, and related marks are trademarks of " +
                "Wizards of the Coast LLC.\n\n" +
                "Compatible with 5e SRD concepts only (System Reference Document 5.1, " +
                "Creative Commons Attribution 4.0 International).\n\n" +
                "Full credits: ATTRIBUTION.md (repo / project docs).\n\n" +
                "Art (CC0): LuizMelo idle/attack frames; Nidhoggn battlebacks; ansimuz Phantasy dungeon; " +
                "quantumelle Dark forest path; Clint Bellanger Tiny Creatures.\n\n" +
                "Audio (CC0): Ironchest Dungeon Loops (explore / town / combat / boss beds); " +
                "StarNinjas sword/clash SFX; Darsycho monster snarl; bart interface beep. " +
                "Lanternkeeper voice uses on-device Text-to-Speech. See docs/AUDIO_BEDS.md."
        settingsRoot.visibility = View.VISIBLE
        settingsRoot.bringToFront()
    }

    private fun hideSettingsOverlay() {
        if (::settingsRoot.isInitialized) {
            settingsRoot.visibility = View.GONE
        }
        stopMenuRaidKeyCountdownIfIdle()
    }

    private fun confirmAbandonAdventure() {
        val online = isOnline()
        if (!online) {
            // v2.28: Abandon = Delete for the active lantern, with the named confirmation.
            confirmDeleteSlot(activeSlot(), fromAbandon = true)
            return
        }
        AlertDialog.Builder(this)
            .setTitle("Leave online session?")
            .setMessage("Leaves the Host/Join table and returns to the main menu. Your local adventures are kept.")
            .setPositiveButton("Leave") { _, _ -> abandonAdventure() }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun abandonAdventure() {
        hideSettingsOverlay()
        val leavingOnline = isOnline()
        val slot = activeSlot()
        if (!leavingOnline) flushPlaytime()
        sessionActive = false
        detachOnlineSession()
        rawPrefs().edit()
            .putBoolean("crash_guard", false)
            .remove(SaveSlots.KEY_CRASH_GUARD_SLOT)
            .remove("was_online_host")
            .remove("was_online_client")
            .remove("online_session_id")
            .apply()
        runDifficulty = -1
        wipeDialogShowing = false
        storyCompleteRoutesShown = false
        if (!leavingOnline) {
            // Clear this slot's run flags first, then delete the slot (blob, Ledger, keys, pity,
            // records). Leaving Host / Join touches no slot at all.
            clearRaidSessionFlags()
            clearChallengeSessionFlags()
            clearArenaSessionFlags()
            clearEndlessSessionFlags()
            SaveSlots.deleteSlot(rawPrefs(), slot)
            ledgerStoreCache = null
        }
        Toast.makeText(
            this,
            if (leavingOnline) "Left online session. Local adventures kept."
            else "Adventure abandoned.",
            Toast.LENGTH_SHORT
        ).show()
        showStartDialog()
    }

    // --- v2.28 save slots --------------------------------------------------------------------------

    /** Slot chosen on an empty card (Begin); consumed by beginSoloOrCrawl. */
    private var pendingBeginSlot: Int? = null
    private var playClockStartMs = 0L
    private var adventuresDialog: android.app.Dialog? = null

    private fun nativeSaveLoads(blob: String): Boolean =
        try { describeSaveState(blob).isNotBlank() } catch (_: Throwable) { false }

    private fun slotSummary(slot: Int): SaveSlots.Summary? {
        val blob = SaveSlots.blob(rawPrefs(), slot) ?: return null
        if (!nativeReady) return null
        return try { SaveSlots.Summary.parse(describeSaveState(blob)) } catch (_: Throwable) { null }
    }

    /** Copy the legacy single save into slot 1 (idempotent). Unreadable saves are kept, never dropped. */
    private fun runSlotMigration(showDialogs: Boolean) {
        val result = try {
            SaveSlots.migrate(
                rawPrefs(), System.currentTimeMillis(), SLOTS_VERSION_CODE,
                readable = if (nativeReady) { b -> nativeSaveLoads(b) } else null,
                storyCompleteFromSave = if (nativeReady) { b ->
                    SaveSlots.Summary.parse(try { describeSaveState(b) } catch (_: Throwable) { "" })?.storyComplete
                        ?: SaveSlots.storyCompleteFromHeader(b)
                } else null,
            )
        } catch (e: Exception) {
            Log.e(TAG, "slot migration failed", e)
            return
        }
        Log.i(TAG, "Save slots migration: $result")
        ledgerStoreCache = null
        if (!showDialogs) return
        val msg = when (result) {
            SaveSlots.Migration.PENDING_UNREADABLE ->
                "Your previous adventure could not be read. It has not been deleted."
            SaveSlots.Migration.PENDING_NO_EMPTY_SLOT ->
                "Your previous adventure is waiting for an empty lantern. Delete one to bring it back. It has not been deleted."
            else -> return
        }
        handler.post {
            AlertDialog.Builder(this)
                .setTitle("Adventures")
                .setMessage(msg)
                .setPositiveButton("Retry") { _, _ -> runSlotMigration(showDialogs = true); refreshMainMenuButtons() }
                .setNegativeButton("Later", null)
                .show()
        }
    }

    /** Softer crash guard: only the guarded slot; keep it, restore its snapshot, or mark it broken. */
    private fun recoverSlotAfterCrash() {
        val (outcome, slot) = try {
            SaveSlots.recoverFromCrash(rawPrefs(), if (nativeReady) { b -> nativeSaveLoads(b) } else null)
        } catch (e: Exception) {
            Log.e(TAG, "crash recovery failed", e)
            return
        }
        val text = when (outcome) {
            SaveSlots.CrashOutcome.NONE -> return
            SaveSlots.CrashOutcome.KEPT -> "The game closed unexpectedly. Your adventure was kept."
            SaveSlots.CrashOutcome.RESTORED_SNAPSHOT -> "The game closed unexpectedly. Your hero was restored from before the run."
            SaveSlots.CrashOutcome.MARKED_BROKEN -> "Lantern $slot could not be read after a crash. It has not been deleted."
        }
        Log.w(TAG, "crash_guard on slot $slot: $outcome")
        Toast.makeText(this, text, Toast.LENGTH_LONG).show()
    }

    private fun flushPlaytime() {
        val start = playClockStartMs
        if (start <= 0L || !sessionActive || isOnline()) return
        val now = android.os.SystemClock.elapsedRealtime()
        playClockStartMs = now
        SaveSlots.addPlaytime(rawPrefs(), activeSlot(), (now - start).coerceIn(0L, 6L * 3_600_000L))
    }

    /** A failed / empty load: keep the save, mark this slot, offer Retry or Delete (never auto-delete). */
    private fun markSlotBrokenAndOffer(slot: Int) {
        rawPrefs().edit().putBoolean(SaveSlots.slotKey(slot, SaveSlots.BROKEN), true)
            .putBoolean("crash_guard", false).remove(SaveSlots.KEY_CRASH_GUARD_SLOT).commit()
        sessionActive = false
        showStartDialog()
        AlertDialog.Builder(this)
            .setTitle("Save could not be read")
            .setMessage("Lantern $slot didn't load. It has not been deleted. You can try again, or delete it.")
            .setPositiveButton("Retry") { _, _ -> continueSavedGame() }
            .setNeutralButton("Delete…") { _, _ -> confirmDeleteSlot(slot, fromAbandon = false) }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun slotHeroName(slot: Int): String {
        val stored = rawPrefs().getString(SaveSlots.slotKey(slot, "hero_name"), null)?.trim().orEmpty()
        if (stored.isNotEmpty()) return stored
        return slotSummary(slot)?.name ?: "this hero"
    }

    /** Delete / Abandon confirmation that names the hero. Only that slot is cleared. */
    private fun confirmDeleteSlot(slot: Int, fromAbandon: Boolean) {
        val name = slotHeroName(slot)
        AlertDialog.Builder(this)
            .setTitle(if (fromAbandon) "Abandon $name's adventure?" else "Delete $name's adventure?")
            .setMessage("This will end $name's adventure — gear, gold, memories, and endgame progress. This cannot be undone.")
            .setPositiveButton(if (fromAbandon) "Abandon" else "Delete") { _, _ ->
                if (fromAbandon) {
                    abandonAdventure()
                } else {
                    SaveSlots.deleteSlot(rawPrefs(), slot)
                    ledgerStoreCache = null
                    Toast.makeText(this, "$name's lantern is out.", Toast.LENGTH_SHORT).show()
                    refreshMainMenuButtons()
                    if (adventuresDialog?.isShowing == true) renderAdventures(adventuresBeginMode)
                }
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun showAllLanternsLitDialog() {
        AlertDialog.Builder(this)
            .setTitle("All three lanterns are lit")
            .setMessage("Delete one to begin again. Nothing is overwritten.")
            .setPositiveButton("Adventures") { _, _ -> showAdventures() }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private var adventuresBeginMode: String? = null
    private var adventuresRoot: View? = null

    /**
     * The Adventures picker. beginMode = "solo" / "crawl" when opened from New Story / Dungeon
     * Crawl: empty cards then start that mode directly.
     */
    private fun showAdventures(beginMode: String? = null) {
        if (isOnline()) return
        adventuresDialog?.dismiss()
        val dialog = android.app.Dialog(this, android.R.style.Theme_Black_NoTitleBar_Fullscreen)
        dialog.requestWindowFeature(android.view.Window.FEATURE_NO_TITLE)
        val root = layoutInflater.inflate(R.layout.dialog_adventures, null)
        dialog.setContentView(root)
        dialog.window?.setBackgroundDrawableResource(android.R.color.transparent)
        root.findViewById<Button>(R.id.adventuresClose).setOnClickListener { dialog.dismiss() }
        dialog.setOnDismissListener { if (adventuresDialog === dialog) { adventuresDialog = null; adventuresRoot = null }; refreshMainMenuButtons() }
        adventuresDialog = dialog
        adventuresRoot = root
        renderAdventures(beginMode)
        dialog.show()
        if (beginMode != null && SaveSlots.allFull(rawPrefs())) {
            Toast.makeText(this, "All three lanterns are lit. Delete one to begin again.", Toast.LENGTH_LONG).show()
        }
    }

    private fun renderAdventures(beginMode: String?) {
        adventuresBeginMode = beginMode
        val root = adventuresRoot ?: return
        val raw = rawPrefs()
        root.findViewById<TextView>(R.id.adventuresSubtitle).text = when (beginMode) {
            "solo" -> "Choose an unlit lantern for a new story."
            "crawl" -> "Choose an unlit lantern for a new crawl."
            else -> "Three lanterns. Three lives."
        }
        val list = root.findViewById<LinearLayout>(R.id.adventuresList)
        list.removeAllViews()
        val active = SaveSlots.activeSlot(raw)
        val now = System.currentTimeMillis()
        val dayIndex: (Long) -> Long = { ms ->
            val c = java.util.Calendar.getInstance().apply { timeInMillis = ms }
            c.get(java.util.Calendar.YEAR) * 400L + c.get(java.util.Calendar.DAY_OF_YEAR)
        }
        for (slot in 1..SaveSlots.COUNT) {
            if (!SaveSlots.isFilled(raw, slot)) {
                val v = layoutInflater.inflate(R.layout.item_adventure_empty, list, false)
                v.findViewById<TextView>(R.id.emptyHint).text = when (beginMode) {
                    "solo" -> "Begin a new story here."
                    "crawl" -> "Begin a new crawl here."
                    else -> "Begin a new story or crawl."
                }
                v.findViewById<Button>(R.id.emptyBegin).setOnClickListener { beginInSlot(slot, beginMode) }
                list.addView(v)
                continue
            }
            val v = layoutInflater.inflate(R.layout.item_adventure_slot, list, false)
            val summary = slotSummary(slot)
            val broken = summary == null || SaveSlots.isBroken(raw, slot)
            val classId = summary?.classId ?: raw.getInt(SaveSlots.slotKey(slot, "hero_class"), 0)
            v.findViewById<ImageView>(R.id.slotPortrait).setImageResource(SpriteTables.heroSprite(classId))
            v.findViewById<TextView>(R.id.slotName).text = slotHeroName(slot)
            v.findViewById<TextView>(R.id.slotLevel).text =
                if (summary != null) "Lv ${summary.level} · ${SaveSlots.className(summary.classId)}" else SaveSlots.className(classId)
            v.findViewById<TextView>(R.id.slotProgress).text = when {
                summary == null -> "This save could not be read. It has not been deleted."
                broken -> "${SaveSlots.progressLine(summary)} · last load failed"
                else -> SaveSlots.progressLine(summary)
            }
            val last = raw.getLong(SaveSlots.slotKey(slot, SaveSlots.LAST_PLAYED_MS), 0L)
            val play = raw.getLong(SaveSlots.slotKey(slot, SaveSlots.PLAYTIME_MS), 0L)
            v.findViewById<TextView>(R.id.slotPlayed).text =
                "Last played ${SaveSlots.lastPlayedLabel(last, now, dayIndex)} · ${SaveSlots.playtimeLabel(play)}"
            if (slot == active) {
                v.findViewById<View>(R.id.slotCard).setBackgroundResource(R.drawable.bg_adventure_card_last)
                v.findViewById<View>(R.id.slotLast).visibility = View.VISIBLE
            }
            val cont = v.findViewById<Button>(R.id.slotContinue)
            cont.text = if (summary == null) "Retry" else "Continue"
            cont.setOnClickListener {
                SaveSlots.setActive(raw, slot)
                ledgerStoreCache = null
                adventuresDialog?.dismiss()
                continueSavedGame()
            }
            v.findViewById<Button>(R.id.slotDelete).setOnClickListener { confirmDeleteSlot(slot, fromAbandon = false) }
            list.addView(v)
        }
    }

    /** Begin on an empty card: Story / Crawl (asked if the picker was opened plainly), then the usual flow. */
    private fun beginInSlot(slot: Int, beginMode: String?) {
        val start = { mode: String ->
            adventuresDialog?.dismiss()
            pendingBeginSlot = slot
            askHeroName { name ->
                localPlayerName = name
                showClassSelection(mode = mode)
            }
        }
        if (beginMode == "solo" || beginMode == "crawl") {
            start(beginMode)
            return
        }
        AlertDialog.Builder(this)
            .setTitle("Light lantern $slot")
            .setItems(arrayOf("New Story — The Kindling", "Dungeon Crawl")) { _, which ->
                start(if (which == 1) "crawl" else "solo")
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    /**
     * Leave the current run for the main menu without wiping Continue / gear /
     * quest flags / raid keys. Syncs save first so progress survives.
     */
    private fun returnToMainMenuSaving(confirm: Boolean) {
        val leave = {
            hideSettingsOverlay()
            val wasOnline = isOnlineHost || isOnlineClient
            if (sessionActive && nativeReady && !wasOnline) {
                markMetaStoryCompleteIfNeeded()
                try { syncAndSave() } catch (e: Exception) {
                    Log.e(TAG, "save before main menu failed", e)
                }
            }
            sessionActive = false
            detachOnlineSession()
            clearOnlineResumePrefs()
            lastPendingByName.clear()
            lastLevelUpNotifyKey = ""
            sheetPulseAnimator?.cancel()
            prefs().edit().putBoolean("crash_guard", false).apply()
            runDifficulty = -1
            wipeDialogShowing = false
            clearRaidSessionFlags()
            Toast.makeText(
                this,
                if (wasOnline) "Left online session. Local story progress kept."
                else "Progress saved.",
                Toast.LENGTH_SHORT
            ).show()
            showStartDialog()
        }
        if (confirm && sessionActive) {
            val online = isOnlineHost || isOnlineClient
            AlertDialog.Builder(this)
                .setTitle("Return to main menu?")
                .setMessage(
                    if (online) {
                        "Leave the online session? Your local story Continue save stays untouched."
                    } else {
                        "Progress is saved (Continue, gear, quest flags, and Raid Keys stay).\n\n" +
                        "You can open Boss Raid from the main menu when the story is complete."
                    }
                )
                .setPositiveButton("Main menu") { _, _ -> leave() }
                .setNegativeButton("Stay", null)
                .show()
        } else {
            leave()
        }
    }

    private fun refreshSettingsRaidEntry() {
        if (!::btnSettingsBossRaid.isInitialized) return
        syncRaidKeyDay()
        val storyDone = isMetaStoryComplete()
        val keys = raidKeysHeld()
        // Always offer entry while in-adventure (or after story) so locked reason is visible.
        val show = sessionActive || storyDone
        btnSettingsBossRaid.visibility = if (show) View.VISIBLE else View.GONE
        settingsRaidHint.visibility = if (show) View.VISIBLE else View.GONE
        if (show) {
            btnSettingsBossRaid.isEnabled = storyDone
            btnSettingsBossRaid.alpha = if (storyDone) 1f else 0.45f
            btnSettingsBossRaid.text = if (storyDone) "Boss Raid ($keys/3 keys)" else "Boss Raid (locked)"
            setRaidHint(settingsRaidHint, storyDone, keys, forMainMenu = false)
            startMenuRaidKeyCountdown()
        }
        refreshSettingsChallengeEntry()
        refreshSettingsArenaEntry()
        refreshSettingsEndlessEntry()
    }

    private fun refreshSettingsChallengeEntry() {
        if (!::btnSettingsChallenge.isInitialized) return
        val storyDone = isMetaStoryComplete()
        val hasSave = livableSavedState() != null
        val show = sessionActive || storyDone
        btnSettingsChallenge.visibility = if (show) View.VISIBLE else View.GONE
        settingsChallengeHint.visibility = if (show) View.VISIBLE else View.GONE
        if (!show) return
        ChallengeDungeon.ensureInitialized(prefs())
        val unlocked = storyDone && hasSave
        val chance = ChallengeDungeon.legendaryChance(prefs())
        btnSettingsChallenge.isEnabled = unlocked
        btnSettingsChallenge.alpha = if (unlocked) 1f else 0.45f
        btnSettingsChallenge.text = if (unlocked) "Challenge Dungeon ($chance% Leg)" else "Challenge Dungeon (locked)"
        settingsChallengeHint.text = when {
            !storyDone -> "Finish the story first (Acts 1–4)"
            !hasSave -> "Need a Continue save"
            else -> ChallengeDungeon.summaryLine(prefs())
        }
    }

    private fun refreshSettingsArenaEntry() {
        if (!::btnSettingsArena.isInitialized) return
        val storyDone = isMetaStoryComplete()
        val hasSave = livableSavedState() != null
        val show = sessionActive || storyDone
        btnSettingsArena.visibility = if (show) View.VISIBLE else View.GONE
        settingsArenaHint.visibility = if (show) View.VISIBLE else View.GONE
        if (!show) return
        val unlocked = storyDone && hasSave
        val best = Arena.bestScore(prefs())
        btnSettingsArena.isEnabled = unlocked
        btnSettingsArena.alpha = if (unlocked) 1f else 0.45f
        btnSettingsArena.text = if (unlocked) {
            if (best > 0) "Arena (Ember Ring) · best $best" else "Arena (Ember Ring)"
        } else "Arena (locked)"
        settingsArenaHint.text = when {
            !storyDone -> "Finish the story first (Acts 1–4)"
            !hasSave -> "Need a Continue save"
            else -> Arena.summaryLine(prefs())
        }
    }

    private fun refreshSettingsEndlessEntry() {
        if (!::btnSettingsEndless.isInitialized) return
        val storyDone = isMetaStoryComplete()
        val hasSave = livableSavedState() != null
        val show = sessionActive || storyDone
        val inEndless = prefs().getBoolean("endless_active", false)
        btnSettingsEndless.visibility = if (show) View.VISIBLE else View.GONE
        settingsEndlessHint.visibility = if (show) View.VISIBLE else View.GONE
        if (::btnSettingsEndlessRetreat.isInitialized) {
            btnSettingsEndlessRetreat.visibility = if (inEndless && sessionActive) View.VISIBLE else View.GONE
        }
        if (!show) return
        val unlocked = storyDone && hasSave
        val best = EndlessDeep.bestDepth(prefs())
        btnSettingsEndless.isEnabled = unlocked
        btnSettingsEndless.alpha = if (unlocked) 1f else 0.45f
        btnSettingsEndless.text = if (unlocked) {
            if (best > 0) "Endless Deep · best $best" else "Endless Deep (Ashen Deep)"
        } else "Endless Deep (locked)"
        settingsEndlessHint.text = when {
            !storyDone -> "Finish the story first (Acts 1–4)"
            !hasSave -> "Need a Continue save"
            inEndless -> {
                val d = try { getEndlessDepth() } catch (_: Exception) { 0 }
                "In Ashen Deep — depth $d. Use Retreat to soft-exit with spoils."
            }
            else -> EndlessDeep.summaryLine(prefs())
        }
    }

    /** Shared Boss Raid entry — loads the saved story hero, then spawns the raid boss. */
    private fun promptBossRaidEntry(fromInAdventure: Boolean) {
        // From an active adventure: save + park on main menu first so Continue/gear stay
        // intact if the player cancels before spending a key.
        if (fromInAdventure && sessionActive) {
            returnToMainMenuSaving(confirm = false)
            handler.post { promptBossRaidEntry(fromInAdventure = false) }
            return
        }
        if (!isMetaStoryComplete()) {
            AlertDialog.Builder(this)
                .setTitle("Boss Raid locked")
                .setMessage("Finish the story first (Acts 1–4). Boss Raid unlocks after Vigil Kindled.")
                .setPositiveButton("OK", null)
                .show()
            return
        }
        val save = livableSavedState()
        if (save == null) {
            AlertDialog.Builder(this)
                .setTitle("Boss Raid")
                .setMessage("No saved hero found. Finish the story (Acts 1–4) and keep a Continue save, then enter Boss Raid with that same character.")
                .setPositiveButton("OK", null)
                .show()
            return
        }
        syncRaidKeyDay()
        val keys = raidKeysHeld()
        if (keys <= 0) {
            AlertDialog.Builder(this)
                .setTitle("Boss Raid")
                .setMessage(
                    "Boss Raid costs 1 Raid Key.\n\n" +
                    "${raidKeyCountdownLabel(keys)}\n\n" +
                    "Keys drop randomly from endgame bosses (deep crawl after story + raid clears).\n" +
                    "You can hold and be granted at most 3 keys per calendar day."
                )
                .setPositiveButton("OK", null)
                .show()
            return
        }
        val hero = heroNameFromSave(save)
        AlertDialog.Builder(this)
            .setTitle("Boss Raid")
            .setMessage(
                "Spend 1 Raid Key to enter a focused endgame boss fight with $hero?\n\n" +
                "Keys held: $keys/3 · ${raidKeyCountdownLabel(keys)}\n" +
                "Your saved stats, gear, inventory, gold, companion, level, and Legendary upgrades carry over.\n" +
                "Rewards: gold, XP, Legendary chance, possible key drop."
            )
            .setPositiveButton("Enter raid") { _, _ ->
                if (!spendRaidKey()) {
                    Toast.makeText(this, "No Raid Key.", Toast.LENGTH_SHORT).show()
                    refreshMainMenuButtons()
                    return@setPositiveButton
                }
                if (!startBossRaidFromSave()) {
                    // Refund key if we failed to start (save load / native).
                    prefs().edit().putInt("raid_keys", (raidKeysHeld() + 1).coerceAtMost(3)).apply()
                    Toast.makeText(this, "Could not start Boss Raid with your saved hero.", Toast.LENGTH_LONG).show()
                    refreshMainMenuButtons()
                    showStartDialog()
                }
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    /**
     * Load Continue save, stash pre-raid snapshot, then spawn raid boss on the SAME party.
     * Never calls resetGame / class select for raids.
     */
    private fun startBossRaidFromSave(): Boolean {
        if (!nativeReady) {
            Toast.makeText(this, "Native library not ready — can't start raid.", Toast.LENGTH_LONG).show()
            return false
        }
        val data = livableSavedState() ?: return false
        if (saveIsGameOver(data)) {
            Toast.makeText(this, "Your save is mid–Game Over. Continue the wipe first, then enter Boss Raid.", Toast.LENGTH_LONG).show()
            return false
        }
        detachOnlineSession()
        // Snapshot adventure so wipe/menu can restore without creating a blank character.
        prefs().edit()
            .putString("pre_raid_save_state", data)
            .putBoolean("raid_active", true)
            .apply()
        localPlayerName = heroNameFromSave(data)
        localClassId = prefs().getInt("hero_class", 0)
        try {
            loadGameState(data)
        } catch (e: Exception) {
            Log.e(TAG, "Boss Raid load save failed", e)
            prefs().edit().remove("pre_raid_save_state").putBoolean("raid_active", false).apply()
            return false
        }
        val ok = try { beginBossRaidFromCurrent() } catch (e: Exception) {
            Log.e(TAG, "beginBossRaidFromCurrent failed", e)
            false
        }
        if (!ok) {
            prefs().edit().remove("pre_raid_save_state").putBoolean("raid_active", false).apply()
            return false
        }
        lastBattleRoster = ""
        lastRoomDesc = ""
        lastChatHistory = ""
        lastProcessedEvent = ""
        lastCoachTip = ""
        lastCoachTurnKey = ""
        wipeDialogShowing = false
        storyCompleteRoutesShown = true
        runDifficulty = try { getDifficulty() } catch (_: Exception) { preferredDifficulty.coerceIn(0, 3) }
        setHost(true)
        hideMainMenu()
        activateSession()
        btnReset.visibility = View.GONE
        syncAndSave()
        updateUi()
        Toast.makeText(this, "Boss Raid — $localPlayerName enters the arena!", Toast.LENGTH_SHORT).show()
        return true
    }

    private fun clearRaidSessionFlags() {
        prefs().edit()
            .remove("pre_raid_save_state")
            .putBoolean("raid_active", false)
            .apply()
    }

    private fun clearChallengeSessionFlags() {
        prefs().edit()
            .remove("pre_challenge_save_state")
            .putBoolean("challenge_active", false)
            .apply()
    }

    private fun clearArenaSessionFlags() {
        prefs().edit()
            .remove("pre_arena_save_state")
            .putBoolean("arena_active", false)
            .apply()
    }

    private fun clearEndlessSessionFlags() {
        prefs().edit()
            .remove("pre_endless_save_state")
            .putBoolean("endless_active", false)
            .apply()
    }

    /** Shared Challenge Dungeon entry — loads Continue hero (no Raid Key). */
    private fun promptChallengeDungeonEntry(fromInAdventure: Boolean) {
        if (fromInAdventure && sessionActive) {
            returnToMainMenuSaving(confirm = false)
            handler.post { promptChallengeDungeonEntry(fromInAdventure = false) }
            return
        }
        if (!isMetaStoryComplete()) {
            AlertDialog.Builder(this)
                .setTitle("Challenge Dungeon locked")
                .setMessage("Finish the story first (Acts 1–4). Challenge Dungeon unlocks after Vigil Kindled.")
                .setPositiveButton("OK", null)
                .show()
            return
        }
        val save = livableSavedState()
        if (save == null) {
            AlertDialog.Builder(this)
                .setTitle("Challenge Dungeon")
                .setMessage("No saved hero found. Finish the story (Acts 1–4) and keep a Continue save, then enter with that same character.")
                .setPositiveButton("OK", null)
                .show()
            return
        }
        ChallengeDungeon.ensureInitialized(prefs())
        val hero = heroNameFromSave(save)
        val chance = ChallengeDungeon.legendaryChance(prefs())
        val attempts = ChallengeDungeon.attempts(prefs())
        AlertDialog.Builder(this)
            .setTitle("Challenge Dungeon")
            .setMessage(
                "Enter the Cinder Crucible with $hero?\n\n" +
                "Your saved stats, gear, inventory, gold, companion, level, and Legendary upgrades carry over.\n" +
                "Solo save is preserved (Host/Join never overwrite it).\n\n" +
                "Legendary chance: $chance% (base ${ChallengeDungeon.BASE_CHANCE}%; " +
                "−${ChallengeDungeon.DECAY_PER_LEGENDARY}% per Legendary found, floor ${ChallengeDungeon.FLOOR}%; " +
                "+${ChallengeDungeon.BOOST}% every ${ChallengeDungeon.ATTEMPTS_PER_BOOST} attempts, cap ${ChallengeDungeon.CAP}%).\n" +
                "Attempts so far: $attempts · Legendaries uncapped.\n" +
                "No Raid Key required."
            )
            .setPositiveButton("Enter crucible") { _, _ ->
                if (!startChallengeDungeonFromSave()) {
                    Toast.makeText(this, "Could not start Challenge Dungeon with your saved hero.", Toast.LENGTH_LONG).show()
                    refreshMainMenuButtons()
                    showStartDialog()
                }
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun startChallengeDungeonFromSave(): Boolean {
        if (!nativeReady) {
            Toast.makeText(this, "Native library not ready — can't start Challenge Dungeon.", Toast.LENGTH_LONG).show()
            return false
        }
        val data = livableSavedState() ?: return false
        if (saveIsGameOver(data)) {
            Toast.makeText(this, "Your save is mid–Game Over. Continue the wipe first, then enter Challenge Dungeon.", Toast.LENGTH_LONG).show()
            return false
        }
        detachOnlineSession()
        // Snapshot before attempt counter / chance boost so a failed native start does not burn an attempt.
        prefs().edit()
            .putString("pre_challenge_save_state", data)
            .putBoolean("challenge_active", true)
            .apply()
        localPlayerName = heroNameFromSave(data)
        localClassId = prefs().getInt("hero_class", 0)
        try {
            loadGameState(data)
        } catch (e: Exception) {
            Log.e(TAG, "Challenge Dungeon load save failed", e)
            clearChallengeSessionFlags()
            return false
        }
        // Pass current chance first; only count the attempt after native start succeeds.
        val chancePreview = ChallengeDungeon.legendaryChance(prefs())
        val ok = try {
            beginChallengeDungeonFromCurrent(chancePreview)
        } catch (e: Exception) {
            Log.e(TAG, "beginChallengeDungeonFromCurrent failed", e)
            false
        }
        if (!ok) {
            clearChallengeSessionFlags()
            return false
        }
        val chanceForRun = ChallengeDungeon.onAttemptStarted(prefs())
        try { setChallengeLegendaryChance(chanceForRun) } catch (_: Exception) {}
        lastBattleRoster = ""
        lastRoomDesc = ""
        lastChatHistory = ""
        lastProcessedEvent = ""
        lastCoachTip = ""
        lastCoachTurnKey = ""
        wipeDialogShowing = false
        storyCompleteRoutesShown = true
        runDifficulty = try { getDifficulty() } catch (_: Exception) { preferredDifficulty.coerceIn(0, 3) }
        setHost(true)
        hideMainMenu()
        activateSession()
        btnReset.visibility = View.GONE
        syncAndSave()
        updateUi()
        Toast.makeText(
            this,
            "Challenge Dungeon — $localPlayerName enters the Cinder Crucible ($chanceForRun% Leg)!",
            Toast.LENGTH_SHORT
        ).show()
        return true
    }

    /** Restore pre-challenge adventure save without blanking the hero. */
    private fun restorePreChallengeSaveOrKeepCurrent(reason: String) {
        val pre = prefs().getString("pre_challenge_save_state", null)
        if (!pre.isNullOrBlank()) {
            prefs().edit()
                .putString("save_state", pre)
                .putBoolean("challenge_active", false)
                .remove("pre_challenge_save_state")
                .apply()
            Log.i(TAG, "Restored pre-challenge save ($reason)")
        } else {
            clearChallengeSessionFlags()
            Log.w(TAG, "No pre-challenge snapshot ($reason) — keeping current save_state")
        }
    }

    /** Shared Arena entry — loads Continue hero into Ember Ring (no Raid Key). */
    private fun promptArenaEntry(fromInAdventure: Boolean) {
        if (fromInAdventure && sessionActive) {
            returnToMainMenuSaving(confirm = false)
            handler.post { promptArenaEntry(fromInAdventure = false) }
            return
        }
        if (!isMetaStoryComplete()) {
            AlertDialog.Builder(this)
                .setTitle("Arena locked")
                .setMessage("Finish the story first (Acts 1–4). The Ember Ring unlocks after Vigil Kindled.")
                .setPositiveButton("OK", null)
                .show()
            return
        }
        val save = livableSavedState()
        if (save == null) {
            AlertDialog.Builder(this)
                .setTitle("Arena — Ember Ring")
                .setMessage("No saved hero found. Finish the story (Acts 1–4) and keep a Continue save, then enter with that same character.")
                .setPositiveButton("OK", null)
                .show()
            return
        }
        val hero = heroNameFromSave(save)
        val best = Arena.bestScore(prefs())
        AlertDialog.Builder(this)
            .setTitle("Arena — Ember Ring")
            .setMessage(
                "Enter the Ember Ring with $hero?\n\n" +
                "Five scored waves. Your saved stats, gear, inventory, gold, companion, level, and Legendary upgrades carry over.\n" +
                "Solo save is preserved (Host/Join never overwrite it).\n\n" +
                "Rewards: gold + Rare/Epic loot on waves 1–4; Wave 5 apex may drop Legendary at 7% (same as Boss Raid — not Challenge Dungeon\u2019s dynamic forge).\n" +
                (if (best > 0) "Your best score: $best.\n" else "") +
                "No Raid Key required."
            )
            .setPositiveButton("Enter ring") { _, _ ->
                if (!startArenaFromSave()) {
                    Toast.makeText(this, "Could not start Arena with your saved hero.", Toast.LENGTH_LONG).show()
                    refreshMainMenuButtons()
                    showStartDialog()
                }
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun startArenaFromSave(): Boolean {
        if (!nativeReady) {
            Toast.makeText(this, "Native library not ready — can't start Arena.", Toast.LENGTH_LONG).show()
            return false
        }
        val data = livableSavedState() ?: return false
        if (saveIsGameOver(data)) {
            Toast.makeText(this, "Your save is mid–Game Over. Continue the wipe first, then enter Arena.", Toast.LENGTH_LONG).show()
            return false
        }
        detachOnlineSession()
        prefs().edit()
            .putString("pre_arena_save_state", data)
            .putBoolean("arena_active", true)
            .apply()
        localPlayerName = heroNameFromSave(data)
        localClassId = prefs().getInt("hero_class", 0)
        try {
            loadGameState(data)
        } catch (e: Exception) {
            Log.e(TAG, "Arena load save failed", e)
            clearArenaSessionFlags()
            return false
        }
        val ok = try {
            beginArenaFromCurrent()
        } catch (e: Exception) {
            Log.e(TAG, "beginArenaFromCurrent failed", e)
            false
        }
        if (!ok) {
            clearArenaSessionFlags()
            return false
        }
        lastBattleRoster = ""
        lastRoomDesc = ""
        lastChatHistory = ""
        lastProcessedEvent = ""
        lastCoachTip = ""
        lastCoachTurnKey = ""
        wipeDialogShowing = false
        storyCompleteRoutesShown = true
        runDifficulty = try { getDifficulty() } catch (_: Exception) { preferredDifficulty.coerceIn(0, 3) }
        setHost(true)
        hideMainMenu()
        activateSession()
        btnReset.visibility = View.GONE
        syncAndSave()
        updateUi()
        Toast.makeText(
            this,
            "Arena — $localPlayerName enters the Ember Ring!",
            Toast.LENGTH_SHORT
        ).show()
        return true
    }

    private fun restorePreArenaSaveOrKeepCurrent(reason: String) {
        val pre = prefs().getString("pre_arena_save_state", null)
        if (!pre.isNullOrBlank()) {
            prefs().edit()
                .putString("save_state", pre)
                .putBoolean("arena_active", false)
                .remove("pre_arena_save_state")
                .apply()
            Log.i(TAG, "Restored pre-arena save ($reason)")
        } else {
            clearArenaSessionFlags()
            Log.w(TAG, "No pre-arena snapshot ($reason) — keeping current save_state")
        }
    }

    /** Shared Endless Deep entry — loads Continue hero into Ashen Deep (no Raid Key). */
    private fun promptEndlessDeepEntry(fromInAdventure: Boolean) {
        if (fromInAdventure && sessionActive) {
            returnToMainMenuSaving(confirm = false)
            handler.post { promptEndlessDeepEntry(fromInAdventure = false) }
            return
        }
        if (!isMetaStoryComplete()) {
            AlertDialog.Builder(this)
                .setTitle("Endless Deep locked")
                .setMessage("Finish the story first (Acts 1–4). The Ashen Deep unlocks after Vigil Kindled.")
                .setPositiveButton("OK", null)
                .show()
            return
        }
        val save = livableSavedState()
        if (save == null) {
            AlertDialog.Builder(this)
                .setTitle("Endless Deep — Ashen Deep")
                .setMessage("No saved hero found. Finish the story (Acts 1–4) and keep a Continue save, then descend with that same character.")
                .setPositiveButton("OK", null)
                .show()
            return
        }
        val hero = heroNameFromSave(save)
        val best = EndlessDeep.bestDepth(prefs())
        AlertDialog.Builder(this)
            .setTitle("Endless Deep — Ashen Deep")
            .setMessage(
                "Descend into the Ashen Deep with $hero?\n\n" +
                "Endless depth-scaling rooms. Soft end via death, Retreat, or milestone.\n" +
                "Your saved stats, gear, inventory, gold, companion, level, and Legendary upgrades carry over.\n" +
                "Solo save is preserved (Host/Join never overwrite it).\n\n" +
                "Rewards scale with depth. Legendary only on apex depths (10, 20, …) at 7% — same as Boss Raid, not Challenge Dungeon.\n" +
                (if (best > 0) "Your best depth: $best.\n" else "") +
                "No Raid Key required."
            )
            .setPositiveButton("Descend") { _, _ ->
                if (!startEndlessDeepFromSave()) {
                    Toast.makeText(this, "Could not start Endless Deep with your saved hero.", Toast.LENGTH_LONG).show()
                    refreshMainMenuButtons()
                    showStartDialog()
                }
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun startEndlessDeepFromSave(): Boolean {
        if (!nativeReady) {
            Toast.makeText(this, "Native library not ready — can't start Endless Deep.", Toast.LENGTH_LONG).show()
            return false
        }
        val data = livableSavedState() ?: return false
        if (saveIsGameOver(data)) {
            Toast.makeText(this, "Your save is mid–Game Over. Continue the wipe first, then descend.", Toast.LENGTH_LONG).show()
            return false
        }
        detachOnlineSession()
        prefs().edit()
            .putString("pre_endless_save_state", data)
            .putBoolean("endless_active", true)
            .apply()
        localPlayerName = heroNameFromSave(data)
        localClassId = prefs().getInt("hero_class", 0)
        try {
            loadGameState(data)
        } catch (e: Exception) {
            Log.e(TAG, "Endless Deep load save failed", e)
            clearEndlessSessionFlags()
            return false
        }
        val ok = try {
            beginEndlessDeepFromCurrent()
        } catch (e: Exception) {
            Log.e(TAG, "beginEndlessDeepFromCurrent failed", e)
            false
        }
        if (!ok) {
            clearEndlessSessionFlags()
            return false
        }
        lastBattleRoster = ""
        lastRoomDesc = ""
        lastChatHistory = ""
        lastProcessedEvent = ""
        lastCoachTip = ""
        lastCoachTurnKey = ""
        wipeDialogShowing = false
        storyCompleteRoutesShown = true
        runDifficulty = try { getDifficulty() } catch (_: Exception) { preferredDifficulty.coerceIn(0, 3) }
        setHost(true)
        hideMainMenu()
        activateSession()
        btnReset.visibility = View.GONE
        syncAndSave()
        updateUi()
        Toast.makeText(
            this,
            "Endless Deep — $localPlayerName descends into the Ashen Deep!",
            Toast.LENGTH_SHORT
        ).show()
        return true
    }

    private fun restorePreEndlessSaveOrKeepCurrent(reason: String) {
        val pre = prefs().getString("pre_endless_save_state", null)
        if (!pre.isNullOrBlank()) {
            prefs().edit()
                .putString("save_state", pre)
                .putBoolean("endless_active", false)
                .remove("pre_endless_save_state")
                .apply()
            Log.i(TAG, "Restored pre-endless save ($reason)")
        } else {
            clearEndlessSessionFlags()
            Log.w(TAG, "No pre-endless snapshot ($reason) — keeping current save_state")
        }
    }

    /** Soft exit: keep spoils on Continue hero, record best depth, leave Endless mode. */
    private fun promptEndlessRetreat() {
        if (!prefs().getBoolean("endless_active", false)) {
            Toast.makeText(this, "Not in the Ashen Deep.", Toast.LENGTH_SHORT).show()
            return
        }
        val depth = try { getEndlessBestDepthThisRun() } catch (_: Exception) { 0 }
        AlertDialog.Builder(this)
            .setTitle("Retreat from Ashen Deep?")
            .setMessage(
                "Leave at depth $depth with your spoils on the Continue hero?\n\n" +
                "Best depth will be recorded. Soft ending — you can descend again later."
            )
            .setPositiveButton("Retreat") { _, _ ->
                hideSettingsOverlay()
                try { finishEndlessDeepKeepParty() } catch (e: Exception) {
                    Log.e(TAG, "finishEndlessDeepKeepParty failed", e)
                }
                EndlessDeep.onRunFinished(prefs(), depth)
                try { syncAndSave() } catch (_: Exception) {}
                clearEndlessSessionFlags()
                sessionActive = false
                runDifficulty = -1
                Toast.makeText(
                    this,
                    "Retreated at depth $depth (best ${EndlessDeep.bestDepth(prefs())}). Spoils kept.",
                    Toast.LENGTH_LONG
                ).show()
                showStartDialog()
            }
            .setNegativeButton("Stay", null)
            .show()
    }

    /** Restore the pre-raid adventure save (same hero) without starting a blank character. */
    private fun restorePreRaidSaveOrKeepCurrent(reason: String) {
        val pre = prefs().getString("pre_raid_save_state", null)
        if (!pre.isNullOrBlank() && pre.contains("|")) {
            prefs().edit()
                .putString("save_state", pre)
                .putBoolean("crash_guard", false)
                .putBoolean("raid_active", false)
                .remove("pre_raid_save_state")
                .apply()
            Log.i(TAG, "Restored pre-raid save ($reason)")
        } else {
            // Keep whatever syncAndSave wrote (hero progress), but clear raid flags.
            clearRaidSessionFlags()
            Log.i(TAG, "No pre-raid snapshot; keeping current save ($reason)")
        }
    }

    private fun showStoryCompleteRoutesDialog() {
        if (!sessionActive) return
        syncRaidKeyDay()
        val keys = raidKeysHeld()
        AlertDialog.Builder(this)
            .setTitle("Story complete!")
            .setMessage(
                "Acts 1–4 are finished. Endgame crawl, Legendary gear, Boss Raids, Challenge Dungeon, Arena (Ember Ring), and Endless Deep (Ashen Deep) are unlocked.\n\n" +
                "Raid Keys: $keys/3 · ${raidKeyCountdownLabel(keys)}\n\n" +
                "Open Boss Raid now, return to the main menu (progress saved), or keep exploring."
            )
            .setPositiveButton("Boss Raid") { _, _ ->
                promptBossRaidEntry(fromInAdventure = true)
            }
            .setNeutralButton("Main menu") { _, _ ->
                returnToMainMenuSaving(confirm = false)
            }
            .setNegativeButton("Keep exploring", null)
            .show()
    }

    private fun askHeroName(hint: String = "Your hero name", onName: (String) -> Unit) {
        val input = EditText(this).apply {
            this.hint = hint
            filters = TextSafety.nameInputFilters() // C1: no save separators in names
            val prior = prefs().getString("hero_name", null)
            if (!prior.isNullOrBlank()) setText(prior)
            setSelection(text.length)
        }
        AlertDialog.Builder(this)
            .setTitle(hint)
            .setView(input)
            .setCancelable(false)
            .setPositiveButton("Next") { _, _ ->
                // Same rule as native resetGame/startDmSession, so name lookups keep matching.
                onName(TextSafety.sanitizeName(input.text.toString(), "Hero"))
            }
            .setNegativeButton("Back") { _, _ -> showStartDialog() }
            .show()
    }

    private fun startHostingAsDm() {
        if (!nativeReady) {
            Toast.makeText(this, "Native library not ready.", Toast.LENGTH_LONG).show()
            showStartDialog()
            return
        }
        detachOnlineSession()
        try {
            startDmSession(localPlayerName)
            setHost(true)
        } catch (e: Exception) {
            Log.e(TAG, "startDmSession failed", e)
            Toast.makeText(this, "Could not open the Lanternkeeper table.", Toast.LENGTH_LONG).show()
            showStartDialog()
            return
        }
        // Do not arm crash_guard / write save_state — solo Continue must survive Host.
        activateSession(armCrashGuard = false)
        btnReset.visibility = View.GONE
        val sidNow = try { getSessionId() } catch (_: Exception) { "" }
        setupMultiplayer(sidNow, asHost = true)
        if (!isOnlineHost || multiplayer?.isAvailable != true) {
            Toast.makeText(this, "Could not start online session — check Firebase.", Toast.LENGTH_LONG).show()
            detachOnlineSession()
            sessionActive = false
            showFirebaseRequiredDialog { showStartDialog() }
            return
        }
        syncAndSave()
        updateUi()
        showSessionShareDialog()
        showDmToolsDialog(firstOpen = true)
    }

    private fun showClassSelection(mode: String, sid: String = "") {
        val classes = arrayOf("Fighter", "Wizard", "Rogue", "Cleric", "Bard")
        val title = when (mode) {
            "join" -> "Choose your class"
            "crawl" -> "Dungeon Crawl — choose class"
            "raid" -> "Boss Raid — choose class"
            "solo" -> "Story — choose class"
            else -> "Choose your class"
        }
        AlertDialog.Builder(this).setTitle(title).setItems(classes) { _, which ->
            Log.d(TAG, "Class $which selected. mode=$mode")
            localClassId = which
            when (mode) {
                "solo", "crawl", "raid" -> showCompanionSetup(mode = mode, heroClass = which)
                "join" -> {
                    detachOnlineSession()
                    setHost(false)
                    try { prepareClientJoin() } catch (e: Exception) {
                        Log.e(TAG, "prepareClientJoin failed", e)
                    }
                    // Do not arm crash_guard / write save_state — solo Continue must survive Join.
                    activateSession(armCrashGuard = false)
                    btnReset.visibility = View.GONE
                    lastBattleRoster = ""
                    setupMultiplayer(sid, asHost = false)
                    if (multiplayer?.isAvailable != true) {
                        Toast.makeText(this, "Could not connect — check Firebase.", Toast.LENGTH_LONG).show()
                        resetToStartMenu()
                        return@setItems
                    }
                    multiplayer?.publishLobbyJoin(localPlayerName, which)
                    multiplayer?.pushAction(
                        mapOf(
                            "type" to "join",
                            "playerName" to localPlayerName,
                            "classId" to which,
                            "targetIndex" to 0
                        )
                    )
                    scheduleJoinTimeout(sid, resuming = false)
                    updateUi()
                    Toast.makeText(
                        this,
                        "Joining $sid — waiting for the Lanternkeeper to admit you…",
                        Toast.LENGTH_LONG
                    ).show()
                }
            }
        }.setNegativeButton("Back") { _, _ -> showStartDialog() }.show()
    }

    private fun companionDisplayName(classId: Int): String = when (classId) {
        0 -> "Bren (NPC)"
        1 -> "Corvin (NPC)"
        2 -> "Sable (NPC)"
        3 -> "Miren (NPC)"
        4 -> "Jory (NPC)"
        else -> "Corvin (NPC)"
    }

    private fun showCompanionSetup(mode: String, heroClass: Int) {
        val classes = arrayOf(
            "Fighter — Bren",
            "Wizard — Corvin",
            "Rogue — Sable",
            "Cleric — Miren",
            "Bard — Jory"
        )
        val title = when (mode) {
            "crawl" -> "Dungeon Crawl — companion class"
            "raid" -> "Boss Raid — companion class"
            else -> "Story — companion class"
        }
        AlertDialog.Builder(this).setTitle(title).setItems(classes) { _, companionClass ->
            AlertDialog.Builder(this)
                .setTitle("Companion control")
                .setMessage(
                    "${companionDisplayName(companionClass)} can fight automatically, or you can choose " +
                        "Attack / Special / Potion on their turn."
                )
                .setPositiveButton("Auto (AI)") { _, _ ->
                    beginSoloOrCrawl(mode, heroClass, companionClass, companionAutoAi = true)
                }
                .setNeutralButton("Player-controlled") { _, _ ->
                    beginSoloOrCrawl(mode, heroClass, companionClass, companionAutoAi = false)
                }
                .setNegativeButton("Back") { _, _ -> showCompanionSetup(mode, heroClass) }
                .show()
        }.setNegativeButton("Back") { _, _ -> showClassSelection(mode) }.show()
    }

    private fun beginSoloOrCrawl(
        mode: String,
        heroClass: Int,
        companionClass: Int,
        companionAutoAi: Boolean
    ) {
        detachOnlineSession()
        // v2.28: a new Story / Crawl always takes an empty lantern; never overwrite a filled one.
        val target = SaveSlots.slotForNewRun(rawPrefs(), pendingBeginSlot)
        pendingBeginSlot = null
        if (target == null) {
            showAllLanternsLitDialog()
            return
        }
        SaveSlots.deleteSlot(rawPrefs(), target) // leftovers of a Nightmare-wiped hero stay behind
        SaveSlots.setActive(rawPrefs(), target)
        ledgerStoreCache = null
        rawPrefs().edit().putLong(SaveSlots.slotKey(target, SaveSlots.PLAYTIME_MS), 0L).apply()
        val playMode = when (mode) {
            "crawl" -> 1
            "raid" -> 2
            else -> 0
        }
        runDifficulty = preferredDifficulty.coerceIn(0, 3)
        resetGame(heroClass, localPlayerName, playMode, runDifficulty, companionClass, companionAutoAi)
        setHost(true)
        // New Game: this slot's Ledger starts empty (per-hero; no spoilers on a fresh run).
        try {
            ledgerStore().startFresh(getSessionId())
            setMemoryLedgerContext("", 0)
        } catch (e: Exception) { Log.e(TAG, "ledger reset failed", e) }
        storyCompleteRoutesShown = (playMode == 2 || playMode == 3 || playMode == 4) // raid/challenge/arena past story gate
        maybeOfferTutorialThenCoach()
        activateSession()
        btnReset.visibility = View.GONE
        wipeDialogShowing = false
        prefs().edit()
            .putInt("hero_class", heroClass)
            .putString("hero_name", localPlayerName)
            .putInt("companion_class", companionClass)
            .putBoolean("companion_auto_ai", companionAutoAi)
            .apply()
        syncAndSave()
        updateUi()
    }

    private fun showJoinDialog() {
        val input = EditText(this).apply { hint = "Session ID (e.g. AL-A1B2)" }
        AlertDialog.Builder(this).setTitle("Join a Lanternkeeper session").setView(input)
            .setPositiveButton("Join") { _, _ ->
                val sid = input.text.toString().trim().uppercase()
                if (sid.isNotEmpty()) showClassSelection(mode = "join", sid = sid)
                else showStartDialog()
            }
            .setNegativeButton("Back") { _, _ -> showStartDialog() }
            .show()
    }


    private fun showDmToolsDialog(firstOpen: Boolean = false) {
        val options = arrayOf(
            "Begin dungeon (when heroes joined)",
            "Advance to next room",
            "Narrate / set scene",
            "Grant short rest",
            "Show session ID again",
            "Close"
        )
        val title = if (firstOpen) "Lanternkeeper tools — you control the story" else "Lanternkeeper tools"
        AlertDialog.Builder(this)
            .setTitle(title)
            .setItems(options) { _, which ->
                when (which) {
                    0 -> {
                        dmBeginDungeon(); syncAndSave(); updateUi()
                        Toast.makeText(this, "Dungeon begun.", Toast.LENGTH_SHORT).show()
                    }
                    1 -> {
                        dmAdvanceRoom(); syncAndSave(); updateUi()
                    }
                    2 -> {
                        val input = EditText(this).apply { hint = "What do the heroes see?" }
                        AlertDialog.Builder(this)
                            .setTitle("Narrate")
                            .setView(input)
                            .setPositiveButton("Speak") { _, _ ->
                                val line = input.text.toString().trim()
                                if (line.isNotEmpty()) {
                                    dmNarrate(line)
                                    syncAndSave()
                                    updateUi()
                                }
                            }
                            .setNegativeButton("Cancel", null)
                            .show()
                    }
                    3 -> {
                        dmGrantShortRest(); syncAndSave(); updateUi()
                    }
                    4 -> showSessionShareDialog()
                }
            }
            .show()
    }

    private fun showSessionShareDialog() {
        val sid = onlineSessionId.ifBlank {
            try { getSessionId() } catch (_: Exception) { "" }
        }
        if (sid.isBlank()) return
        val clip = android.content.ClipData.newPlainText("AL_SESSION", sid)
        (getSystemService(CLIPBOARD_SERVICE) as ClipboardManager).setPrimaryClip(clip)
        AlertDialog.Builder(this)
            .setTitle("You are the Lanternkeeper")
            .setMessage(
                "Session ID:\n\n$sid\n\n" +
                    "You are the Lanternkeeper — not a hero on the battle map.\n" +
                    "Friends: Join session, enter this ID, pick a class.\n" +
                    "Use the Keeper button for Begin dungeon / Advance / Narrate.\n\n" +
                    "(Copied to clipboard.)"
            )
            .setPositiveButton("Got it", null)
            .show()
    }


    data class XpProgress(val xp: Int, val need: Int, val level: Int, val pending: Int)

    private fun parseXpProgress(playerName: String): XpProgress? {
        if (!nativeReady || playerName.isBlank()) return null
        val raw = try { getXpProgress(playerName) } catch (_: Exception) { "" }
        if (raw.isBlank()) return null
        val parts = raw.split(',')
        if (parts.size < 4) return null
        val xp = parts[0].toIntOrNull() ?: return null
        val need = parts[1].toIntOrNull() ?: return null
        val level = parts[2].toIntOrNull() ?: return null
        val pending = parts[3].toIntOrNull() ?: return null
        return XpProgress(xp, need, level, pending)
    }

    private fun parsePartyXpProgress(): List<Pair<String, XpProgress>> {
        if (!nativeReady) return emptyList()
        val raw = try { getPartyXpProgress() } catch (_: Exception) { "" }
        if (raw.isBlank()) return emptyList()
        return raw.split(';').mapNotNull { entry ->
            val bits = entry.split('|')
            if (bits.size < 5) return@mapNotNull null
            val name = bits[0]
            val xp = bits[1].toIntOrNull() ?: return@mapNotNull null
            val need = bits[2].toIntOrNull() ?: return@mapNotNull null
            val level = bits[3].toIntOrNull() ?: return@mapNotNull null
            val pending = bits[4].toIntOrNull() ?: return@mapNotNull null
            name to XpProgress(xp, need, level, pending)
        }
    }

    private fun classIdFromSheet(sheet: String, forName: String): Int {
        if (forName.equals(localPlayerName, true)) return localClassId
        val lvlLine = sheet.lineSequence().firstOrNull { it.startsWith("Lvl ") }.orEmpty()
        return Regex("""Lvl \d+ (\w+)""").find(lvlLine)?.groupValues?.getOrNull(1)?.let {
            when (it) {
                "Fighter" -> 0; "Wizard" -> 1; "Rogue" -> 2; "Cleric" -> 3; "Bard" -> 4; else -> localClassId
            }
        } ?: localClassId
    }

    private fun refreshHudXp() {
        val row = hudXpRow ?: return
        val bar = hudXpBar ?: return
        val label = hudXpText ?: return
        val dmTable = isOnlineHost && try { isDmTable() } catch (_: Exception) { isOnlineHost }
        if (dmTable || !sessionActive) {
            row.visibility = View.GONE
            return
        }
        row.visibility = View.VISIBLE
        val prog = parseXpProgress(localPlayerName)
        if (prog == null) {
            label.text = "—"
            bar.setProgress(0, 1)
            return
        }
        bar.setProgress(prog.xp, prog.need)
        label.text = "${prog.xp}/${prog.need}"
        hudXpLabel?.text = "XP L${prog.level}"
    }

    private fun setSheetButtonHighlight(active: Boolean) {
        if (active) {
            btnSheet.text = "Sheet!"
            btnSheet.isActivated = true // active HUD chip: far-soot + amber stroke
            if (reduceFlashEnabled) {
                // Soft highlight only — no rapid opacity pulse.
                sheetPulseAnimator?.cancel()
                btnSheet.alpha = 1f
            } else if (sheetPulseAnimator == null) {
                sheetPulseAnimator = android.animation.ObjectAnimator.ofFloat(btnSheet, "alpha", 1f, 0.45f).apply {
                    duration = 650
                    repeatMode = android.animation.ValueAnimator.REVERSE
                    repeatCount = android.animation.ValueAnimator.INFINITE
                    start()
                }
            } else if (sheetPulseAnimator?.isRunning != true) {
                sheetPulseAnimator?.start()
            }
        } else {
            sheetPulseAnimator?.cancel()
            btnSheet.alpha = 1f
            if (!(isOnlineHost && try { isDmTable() } catch (_: Exception) { false })) {
                btnSheet.text = "Sheet"
            }
            btnSheet.isActivated = false
        }
    }

    private fun maybeNotifyLevelUps() {
        if (!sessionActive || !nativeReady) return
        val party = parsePartyXpProgress()
        if (party.isEmpty()) return
        val newly = mutableListOf<Pair<String, Int>>()
        var anyPending = false
        for ((name, prog) in party) {
            val prev = lastPendingByName[name] ?: 0
            if (prog.pending > prev) {
                newly.add(name to prog.level)
            }
            lastPendingByName[name] = prog.pending
            if (prog.pending > 0) anyPending = true
        }
        // Drop names that left the party
        val alive = party.map { it.first }.toSet()
        lastPendingByName.keys.retainAll(alive)

        val dmTable = isOnlineHost && try { isDmTable() } catch (_: Exception) { isOnlineHost }
        if (!dmTable) setSheetButtonHighlight(anyPending)

        if (newly.isEmpty()) return
        val key = newly.joinToString("|") { "${it.first}:${it.second}" }
        if (key == lastLevelUpNotifyKey) return
        lastLevelUpNotifyKey = key
        val msg = newly.joinToString("\n") { (name, lvl) ->
            "$name reached level $lvl — open Sheet to spend points"
        }
        Toast.makeText(this, msg, Toast.LENGTH_LONG).show()
        appendCombatFeed("LEVEL UP: ${newly.joinToString(", ") { it.first }}")
        pulseAccent(AccentKind.LEVEL_UP)
        // One-shot banner dialog for the first new leveler this tick
        if (isFinishing || levelUpDialogShowing) return
        val (who, lvl) = newly.first()
        levelUpDialogShowing = true
        AlertDialog.Builder(this)
            .setTitle("Level up!")
            .setMessage("$who reached level $lvl.\n\nOpen Sheet (or Ally) to assign attribute points.")
            .setPositiveButton("Open Sheet") { _, _ ->
                showCharacterSheet(who)
            }
            .setNegativeButton("Later", null)
            .setOnDismissListener { levelUpDialogShowing = false }
            .show()
    }

    private fun showCharacterSheet(forName: String = localPlayerName) {
        if (!nativeReady) return
        val sheet = try { getDetailedSheet(forName) } catch (_: Exception) { "Hero not found." }
        if (sheet.contains("not found", ignoreCase = true)) {
            Toast.makeText(this, sheet, Toast.LENGTH_SHORT).show()
            return
        }
        val view = layoutInflater.inflate(R.layout.dialog_character_sheet, null)
        val portrait = view.findViewById<ImageView>(R.id.sheetPortrait)
        val nameTv = view.findViewById<TextView>(R.id.sheetName)
        val classTv = view.findViewById<TextView>(R.id.sheetClass)
        val goldTv = view.findViewById<TextView>(R.id.sheetGold)
        val hpLabel = view.findViewById<TextView>(R.id.sheetHpLabel)
        val hpBar = view.findViewById<WickBarView>(R.id.sheetHpBar)
        val resourcesTv = view.findViewById<TextView>(R.id.sheetResources)
        val equippedTv = view.findViewById<TextView>(R.id.sheetEquipped)
        val pointsHint = view.findViewById<TextView>(R.id.sheetPointsHint)
        val levelUpBtn = view.findViewById<Button>(R.id.sheetLevelUp)

        nameTv.text = forName
        val lvlLine = sheet.lineSequence().firstOrNull { it.startsWith("Lvl ") }.orEmpty()
        classTv.text = lvlLine.substringBefore("|").trim().ifBlank { "Hero" }
        val gold = Regex("""Gold:\s*(\d+)""").find(sheet)?.groupValues?.getOrNull(1) ?: "?"
        goldTv.text = "Gold: $gold"
        val hp = Regex("""HP:\s*(\d+)/(\d+)""").find(sheet)
        val cur = hp?.groupValues?.getOrNull(1)?.toIntOrNull() ?: 0
        val max = hp?.groupValues?.getOrNull(2)?.toIntOrNull() ?: 1
        val ac = Regex("""AC:\s*(\d+)""").find(sheet)?.groupValues?.getOrNull(1) ?: "?"
        hpLabel.text = "HP $cur/$max · AC $ac"
        hpBar.setHp(cur, max)
        val res = Regex("""Resources:\s*(\d+)/(\d+)""").find(sheet)
        resourcesTv.text = if (res != null) {
            "Resources: ${res.groupValues[1]}/${res.groupValues[2]}"
        } else "Resources: —"
        val xpProg = parseXpProgress(forName)
        val sheetXpLabel = view.findViewById<TextView>(R.id.sheetXpLabel)
        val sheetXpBar = view.findViewById<EmberXpView>(R.id.sheetXpBar)
        if (xpProg != null) {
            val (xp, need, lvl, _) = xpProg
            sheetXpLabel.text = "XP $xp/$need · Level $lvl → ${lvl + 1}"
            sheetXpBar.setProgress(xp, need)
        } else {
            sheetXpLabel.text = "XP —"
            sheetXpBar.setProgress(0, 1)
        }
        fun stat(key: String): String {
            val m = Regex("""$key:\s*(\d+)\s*\(([^)]+)\)""").find(sheet)
            return if (m != null) "$key ${m.groupValues[1]} (${m.groupValues[2]})" else "$key —"
        }
        view.findViewById<TextView>(R.id.sheetStatStr).text = stat("STR")
        view.findViewById<TextView>(R.id.sheetStatDex).text = stat("DEX")
        view.findViewById<TextView>(R.id.sheetStatCon).text = stat("CON")
        view.findViewById<TextView>(R.id.sheetStatInt).text = stat("INT")
        view.findViewById<TextView>(R.id.sheetStatWis).text = stat("WIS")
        view.findViewById<TextView>(R.id.sheetStatCha).text = stat("CHA")
        view.findViewById<TextView>(R.id.sheetStatTips).text =
            (0..5).joinToString("\n") { "• ${BeginnerGuide.statShortHint(it)}" }
        val weapon = sheet.lineSequence().firstOrNull { it.startsWith("Weapon:") }?.removePrefix("Weapon:")?.trim()
        val armor = sheet.lineSequence().firstOrNull { it.startsWith("Armor:") }?.removePrefix("Armor:")?.trim()
        equippedTv.text = buildString {
            append("Weapon: ${weapon ?: "None"}")
            append("\nArmor: ${armor ?: "None"}")
            append("\n\n")
            append(BeginnerGuide.gearStatsHelpBlurb())
        }
        val classId = when {
            forName.equals(localPlayerName, true) -> localClassId
            else -> Regex("""Lvl \d+ (\w+)""").find(lvlLine)?.groupValues?.getOrNull(1)?.let {
                when (it) {
                    "Fighter" -> 0; "Wizard" -> 1; "Rogue" -> 2; "Cleric" -> 3; "Bard" -> 4; else -> 1
                }
            } ?: 1
        }
        view.findViewById<TextView>(R.id.sheetClassTip).text = BeginnerGuide.classStatTip(classId)
        portrait.setImageResource(spriteFor(BattleUnit(true, forName, classId, cur, max)))
        val canLevel = sheet.contains("POINTS TO SPEND")
        if (canLevel) {
            val pts = Regex("""POINTS TO SPEND:\s*(\d+)""").find(sheet)?.groupValues?.getOrNull(1) ?: ""
            pointsHint.visibility = View.VISIBLE
            pointsHint.text = "Attribute points ready: $pts — assign any or all below."
            levelUpBtn.visibility = View.VISIBLE
        } else {
            pointsHint.visibility = View.GONE
            levelUpBtn.visibility = View.GONE
        }
        val dialog = AlertDialog.Builder(this)
            .setTitle(if (forName.equals(localPlayerName, true)) "Hero Sheet" else "Companion Sheet")
            .setView(view)
            .setPositiveButton("Close", null)
            .create()
        val invBtn = view.findViewById<Button>(R.id.sheetInventory)
        invBtn.setOnClickListener {
            dialog.dismiss()
            showInventoryDialog(forName)
        }
        levelUpBtn.setOnClickListener {
            dialog.dismiss()
            showStatUpgradeDialog(forName)
        }
        dialog.show()
    }

    private fun showCompanionSheet() {
        if (!nativeReady) return
        val dmTable = isOnlineHost && try { isDmTable() } catch (_: Exception) { isOnlineHost }
        if (dmTable) {
            Toast.makeText(this, "The Lanternkeeper table has no solo companion.", Toast.LENGTH_SHORT).show()
            return
        }
        val name = try { getCompanionName() } catch (_: Exception) { "" }
        if (name.isBlank()) {
            Toast.makeText(this, "No companion in this party.", Toast.LENGTH_SHORT).show()
            return
        }
        val auto = try { isCompanionAutoAi() } catch (_: Exception) { true }
        val controlLabel = if (auto) "Auto (AI)" else "Player-controlled"
        val inCombat = try { isInCombat() } catch (_: Exception) { false }
        val options = mutableListOf(
            "View sheet",
            if (auto) "Switch to Player-controlled" else "Switch to Auto (AI)"
        )
        if (!inCombat) options.add("Change companion class…")
        AlertDialog.Builder(this)
            .setTitle("$name · $controlLabel")
            .setItems(options.toTypedArray()) { _, which ->
                when (which) {
                    0 -> showCharacterSheet(name)
                    1 -> {
                        try {
                            setCompanionAutoAi(!auto)
                            syncAndSave()
                            updateUi()
                            Toast.makeText(
                                this,
                                if (!auto) "$name is Auto (AI)." else "$name is Player-controlled — act on their turn.",
                                Toast.LENGTH_LONG
                            ).show()
                        } catch (e: Exception) {
                            Log.e(TAG, "setCompanionAutoAi failed", e)
                        }
                    }
                    2 -> {
                        val classes = arrayOf("Fighter — Bren", "Wizard — Corvin", "Rogue — Sable", "Cleric — Miren", "Bard — Jory")
                        AlertDialog.Builder(this).setTitle("Companion class").setItems(classes) { _, cl ->
                            val ok = try { setCompanionClass(cl) } catch (_: Exception) { false }
                            if (ok) {
                                if (!isOnline()) prefs().edit().putInt("companion_class", cl).apply()
                                syncAndSave()
                                updateUi()
                                showCharacterSheet(try { getCompanionName() } catch (_: Exception) { name })
                            } else {
                                Toast.makeText(this, "Can't change class during combat.", Toast.LENGTH_SHORT).show()
                            }
                        }.setNegativeButton("Cancel", null).show()
                    }
                }
            }
            .setNegativeButton("Close", null)
            .show()
    }

    private fun companionNameSafe(): String =
        try { getCompanionName() } catch (_: Exception) { "" }

    private fun isCompanionPlayerTurn(): Boolean {
        val c = companionNameSafe()
        if (c.isBlank()) return false
        val auto = try { isCompanionAutoAi() } catch (_: Exception) { true }
        if (auto) return false
        return currentActorName().equals(c, ignoreCase = true)
    }

    private fun parsePendingStatPoints(sheet: String): Int =
        Regex("""POINTS TO SPEND:\s*(\d+)""").find(sheet)?.groupValues?.getOrNull(1)?.toIntOrNull() ?: 0

    private fun parseStatScore(sheet: String, key: String): Int =
        Regex("""$key:\s*(\d+)\s*\([^)]+\)""").find(sheet)?.groupValues?.getOrNull(1)?.toIntOrNull() ?: 10

    /** Host-authoritative multi-point spend: allocations = "s0,s1,s2,s3,s4,s5" counts per stat. */
    private fun applyLevelUpBatch(playerName: String, allocations: String) {
        val parts = allocations.split(',').map { it.trim().toIntOrNull() ?: 0 }
        if (parts.isEmpty() || parts.all { it <= 0 }) return
        for (i in 0 until minOf(6, parts.size)) {
            repeat(parts[i].coerceAtLeast(0)) {
                doIncreaseStat(playerName, i)
            }
        }
    }

    private fun commitStatAllocations(forName: String, deltas: IntArray) {
        val total = deltas.sum()
        if (total <= 0) return
        val allocStr = deltas.joinToString(",")
        if (isOnlineClient) {
            val mp = multiplayer
            if (mp == null || !mp.isAvailable) {
                Toast.makeText(this, "Not connected to the Lanternkeeper's session.", Toast.LENGTH_SHORT).show()
                return
            }
            mp.pushAction(
                mapOf(
                    "type" to "levelupBatch",
                    "allocations" to allocStr,
                    "targetIndex" to 0,
                    "statIndex" to 0,
                    "playerName" to forName,
                    "classId" to localClassId
                )
            )
            Toast.makeText(this, "Sent $total point(s) to the Lanternkeeper…", Toast.LENGTH_SHORT).show()
            return
        }
        applyLevelUpBatch(forName, allocStr)
        syncAndSave()
        updateUi()
        Toast.makeText(
            this,
            if (total == 1) "Spent 1 attribute point." else "Spent $total attribute points.",
            Toast.LENGTH_SHORT
        ).show()
    }

    private fun showStatUpgradeDialog(forName: String = localPlayerName) {
        if (!nativeReady) return
        val sheet = try { getDetailedSheet(forName) } catch (_: Exception) { "" }
        val pending = parsePendingStatPoints(sheet)
        if (pending <= 0) {
            Toast.makeText(this, "No attribute points to spend.", Toast.LENGTH_SHORT).show()
            return
        }
        val labels = arrayOf(
            "Strength" to "STR",
            "Dexterity" to "DEX",
            "Constitution" to "CON",
            "Intelligence" to "INT",
            "Wisdom" to "WIS",
            "Charisma" to "CHA"
        )
        val baseScores = IntArray(6) { i -> parseStatScore(sheet, labels[i].second) }
        val allocated = IntArray(6)
        var remaining = pending

        val view = layoutInflater.inflate(R.layout.dialog_levelup_assign, null)
        val remainingTv = view.findViewById<TextView>(R.id.levelUpRemaining)
        val list = view.findViewById<LinearLayout>(R.id.levelUpStatList)
        val confirmBtn = view.findViewById<Button>(R.id.levelUpConfirm)
        val classTipTv = view.findViewById<TextView>(R.id.levelUpClassTip)
        val classIdForTips = classIdFromSheet(sheet, forName)
        classTipTv.text = BeginnerGuide.classStatTip(classIdForTips)
        val rowUis = mutableListOf<Triple<Button, TextView, Button>>()

        fun refreshUi() {
            remainingTv.text = "Points remaining: $remaining  ·  Assigned: ${allocated.sum()} / $pending"
            confirmBtn.isEnabled = allocated.sum() > 0
            confirmBtn.alpha = if (confirmBtn.isEnabled) 1f else 0.45f
            val n = allocated.sum()
            confirmBtn.text = when {
                n == 0 -> "Confirm"
                remaining == 0 -> "Confirm — spend all $n"
                else -> "Confirm — spend $n (keep $remaining)"
            }
            for (i in 0 until 6) {
                val (minus, addTv, plus) = rowUis[i]
                val preview = baseScores[i] + allocated[i]
                addTv.text = if (allocated[i] > 0) "+${allocated[i]}" else "0"
                addTv.setTextColor(
                    if (allocated[i] > 0) getColor(R.color.gold) else getColor(R.color.parchment)
                )
                // Update current line on the row
                val row = list.getChildAt(i)
                row?.findViewById<TextView>(R.id.statAssignCurrent)?.text =
                    "Now ${baseScores[i]} → $preview"
                minus.isEnabled = allocated[i] > 0
                plus.isEnabled = remaining > 0
                minus.alpha = if (minus.isEnabled) 1f else 0.4f
                plus.alpha = if (plus.isEnabled) 1f else 0.4f
            }
        }

        labels.forEachIndexed { index, (full, short) ->
            val row = layoutInflater.inflate(R.layout.item_stat_assign_row, list, false)
            row.findViewById<TextView>(R.id.statAssignName).text = "$short · $full"
            row.findViewById<TextView>(R.id.statAssignCurrent).text =
                "Now ${baseScores[index]} → ${baseScores[index]}"
            row.findViewById<TextView>(R.id.statAssignBlurb).text = BeginnerGuide.statShortHint(index)
            val minus = row.findViewById<Button>(R.id.statAssignMinus)
            val addTv = row.findViewById<TextView>(R.id.statAssignAdd)
            val plus = row.findViewById<Button>(R.id.statAssignPlus)
            minus.setOnClickListener {
                if (allocated[index] <= 0) return@setOnClickListener
                allocated[index]--
                remaining++
                refreshUi()
            }
            plus.setOnClickListener {
                if (remaining <= 0) return@setOnClickListener
                allocated[index]++
                remaining--
                refreshUi()
            }
            rowUis.add(Triple(minus, addTv, plus))
            list.addView(row)
        }

        val dialog = AlertDialog.Builder(this)
            .setTitle("Level up — $forName")
            .setView(view)
            .setNegativeButton("Cancel", null)
            .create()
        confirmBtn.setOnClickListener {
            if (allocated.sum() <= 0) return@setOnClickListener
            dialog.dismiss()
            commitStatAllocations(forName, allocated.copyOf())
            // If points remain (partial spend), reopen assign panel with fresh sheet.
            val left = try {
                parsePendingStatPoints(getDetailedSheet(forName))
            } catch (_: Exception) { 0 }
            // Online clients won't see spend until host syncs — skip auto-reopen.
            if (!isOnlineClient && left > 0) {
                showStatUpgradeDialog(forName)
            }
        }
        refreshUi()
        dialog.show()
    }

    private fun parsePlayerGold(): Int {
        return try {
            val sheet = getDetailedSheet(localPlayerName)
            Regex("""Gold:\s*(\d+)""").find(sheet)?.groupValues?.getOrNull(1)?.toIntOrNull() ?: 0
        } catch (_: Exception) { 0 }
    }

    private fun appendCombatFeed(line: String) {
        val cleaned = line.trim()
        if (cleaned.isEmpty()) return
        if (combatFeed.isNotEmpty() && combatFeed.last() == cleaned) return
        combatFeed.addLast(cleaned)
        while (combatFeed.size > 14) combatFeed.removeFirst()
        logText.text = combatFeed.joinToString("\n")
        val scroll = findViewById<android.widget.ScrollView?>(R.id.combatLogScroll)
        scroll?.post { scroll.fullScroll(View.FOCUS_DOWN) }
    }

    private fun rarityKey(rarity: String): String = when (rarity.lowercase()) {
        "uncommon", "rare", "epic", "legendary" -> rarity.lowercase()
        else -> "common"
    }

    /** Rarity row stripe, glyph socket and pip (colour + shape, so colour-blind players can tell them apart). */
    private fun bindRarityRow(
        row: View, rarity: String, socket: ImageView?, pip: ImageView?,
        itemName: String = "", itemType: String = "",
    ) {
        val k = rarityKey(rarity)
        row.background = getDrawable(
            when (k) {
                "uncommon" -> R.drawable.bg_item_row_uncommon
                "rare" -> R.drawable.bg_item_row_rare
                "epic" -> R.drawable.bg_item_row_epic
                "legendary" -> R.drawable.bg_item_row_legendary
                else -> R.drawable.bg_item_row_common
            }
        )
        // v2.27: socket = rarity frame (background) + gear glyph (src). Calm Legendary when reduce-flash is on.
        socket?.setBackgroundResource(GearIcons.socketFor(k))
        socket?.setImageResource(GearIcons.forItem(itemName, itemType, k, calm = reduceFlashEnabled))
        pip?.setImageResource(
            when (k) {
                "uncommon" -> R.drawable.ic_rarity_uncommon
                "rare" -> R.drawable.ic_rarity_rare
                "epic" -> R.drawable.ic_rarity_epic
                "legendary" -> R.drawable.ic_rarity_legendary
                else -> R.drawable.ic_rarity_common
            }
        )
        pip?.contentDescription = "$k rarity"
    }

    private fun rarityColor(rarity: String): Int {
        return when (rarity.lowercase()) {
            "uncommon" -> getColor(R.color.rarity_uncommon)
            "rare" -> getColor(R.color.rarity_rare)
            "epic" -> getColor(R.color.rarity_epic)
            "legendary" -> getColor(R.color.rarity_legendary)
            else -> getColor(R.color.rarity_common)
        }
    }

    /** Calendar day string in local timezone for raid-key daily cap. */
    private fun raidKeyToday(): String {
        val cal = java.util.Calendar.getInstance()
        return String.format(
            "%04d-%02d-%02d",
            cal.get(java.util.Calendar.YEAR),
            cal.get(java.util.Calendar.MONTH) + 1,
            cal.get(java.util.Calendar.DAY_OF_MONTH)
        )
    }

    /** Max 3 Raid Keys held; grants also capped at 3 per calendar day. */
    private fun syncRaidKeyDay() {
        val today = raidKeyToday()
        val prefs = prefs()
        if (prefs.getString("raid_keys_date", "") != today) {
            prefs.edit()
                .putString("raid_keys_date", today)
                .putInt("raid_keys_granted_today", 0)
                .apply()
        }
    }

    private fun raidKeysHeld(): Int {
        syncRaidKeyDay()
        return prefs().getInt("raid_keys", 0).coerceIn(0, 3)
    }

    private fun raidKeysGrantedToday(): Int {
        syncRaidKeyDay()
        return prefs().getInt("raid_keys_granted_today", 0).coerceIn(0, 3)
    }

    /** Milliseconds until local midnight, when raid_keys_granted_today resets. */
    private fun msUntilRaidKeyDayRollover(): Long {
        val cal = java.util.Calendar.getInstance()
        cal.add(java.util.Calendar.DAY_OF_MONTH, 1)
        cal.set(java.util.Calendar.HOUR_OF_DAY, 0)
        cal.set(java.util.Calendar.MINUTE, 0)
        cal.set(java.util.Calendar.SECOND, 0)
        cal.set(java.util.Calendar.MILLISECOND, 0)
        return (cal.timeInMillis - System.currentTimeMillis()).coerceAtLeast(0L)
    }

    /** Human-readable countdown: "3h 12m" or "12m 05s" under one hour. */
    private fun formatRaidKeyCountdown(ms: Long): String {
        val totalSec = ((ms + 999) / 1000).coerceAtLeast(0L)
        val h = totalSec / 3600
        val m = (totalSec % 3600) / 60
        val s = totalSec % 60
        return if (h > 0) {
            "${h}h ${m}m"
        } else {
            String.format("%dm %02ds", m, s)
        }
    }

    /**
     * Raid-key refresh uses the existing calendar-day grant window (`raid_keys_date`).
     * Below max: countdown to local midnight (when daily grants refresh).
     * At max: "Keys full".
     */
    private fun raidKeyCountdownLabel(keys: Int): String {
        return if (keys >= 3) {
            "Keys full"
        } else {
            "Next key in ${formatRaidKeyCountdown(msUntilRaidKeyDayRollover())}"
        }
    }

    private fun raidKeyHintText(storyDone: Boolean, keys: Int, forMainMenu: Boolean): String {
        if (!storyDone) return "Finish the story first (Acts 1–4)"
        val countdown = raidKeyCountdownLabel(keys)
        return if (forMainMenu) {
            when {
                keys >= 3 -> "Keys full · Costs 1 Raid Key · strong rewards"
                keys <= 0 -> "$countdown · drops from endgame bosses (max 3/day)"
                else -> "$countdown · Costs 1 Raid Key · max 3/day"
            }
        } else {
            when {
                keys >= 3 -> "Keys full · continues your saved hero into the raid boss"
                keys <= 0 -> "$countdown · earn from endgame bosses (max 3/day)"
                else -> "$countdown · Costs 1 key · continues your saved hero"
            }
        }
    }

    /** v2.27: the combat Potion button shows the draught glyph (dims when disabled) instead of an emoji. */
    private fun bindPotionButtonIcon() {
        val icon = getDrawable(R.drawable.ic_gear_potion_button) ?: return
        val px = (18 * resources.displayMetrics.density).toInt()
        val mb = btnHeal as? com.google.android.material.button.MaterialButton
        if (mb != null) {
            mb.icon = icon
            mb.iconGravity = com.google.android.material.button.MaterialButton.ICON_GRAVITY_TOP
            mb.iconSize = px
            mb.iconPadding = 0
            mb.iconTint = null
        } else {
            icon.setBounds(0, 0, px, px)
            btnHeal.setCompoundDrawablesRelative(null, icon, null, null)
        }
        btnHeal.text = "Potion"
    }

    /** v2.27: Raid Key hint text plus a 22dp raid_key glyph once raids are unlocked. */
    private fun setRaidHint(tv: TextView, storyDone: Boolean, keys: Int, forMainMenu: Boolean) {
        tv.text = raidKeyHintText(storyDone, keys, forMainMenu)
        tv.setCompoundDrawablesRelative(if (storyDone) gearChip(GearIcons.RAID_KEY, 22) else null, null, null, null)
    }

    /** A gear glyph sized for a TextView/Button compound drawable. */
    private fun gearChip(glyph: String, sizeDp: Int): android.graphics.drawable.Drawable? {
        val d = getDrawable(GearIcons.drawable(glyph))?.mutate() ?: return null
        val px = (sizeDp * resources.displayMetrics.density).toInt()
        d.setBounds(0, 0, px, px)
        return d
    }

    private fun refreshRaidKeyCountdownUi() {
        syncRaidKeyDay()
        val storyDone = isMetaStoryComplete()
        val keys = raidKeysHeld()
        if (::mainMenuRoot.isInitialized && mainMenuRoot.visibility == View.VISIBLE) {
            if (::btnMenuRaid.isInitialized && storyDone) {
                btnMenuRaid.text = "Boss Raid ($keys/3 keys)"
            }
            if (::mainMenuRaidHint.isInitialized) {
                setRaidHint(mainMenuRaidHint, storyDone, keys, forMainMenu = true)
            }
        }
        if (::settingsRoot.isInitialized && settingsRoot.visibility == View.VISIBLE) {
            if (::btnSettingsBossRaid.isInitialized && storyDone) {
                btnSettingsBossRaid.text = "Boss Raid ($keys/3 keys)"
            }
            if (::settingsRaidHint.isInitialized) {
                setRaidHint(settingsRaidHint, storyDone, keys, forMainMenu = false)
            }
        }
        refreshDailyQuestsUi()
    }

    private fun startMenuRaidKeyCountdown() {
        handler.removeCallbacks(menuRaidKeyTick)
        handler.post(menuRaidKeyTick)
    }

    private fun stopMenuRaidKeyCountdownIfIdle() {
        val menuVisible = ::mainMenuRoot.isInitialized && mainMenuRoot.visibility == View.VISIBLE
        val settingsVisible = ::settingsRoot.isInitialized && settingsRoot.visibility == View.VISIBLE
        if (!menuVisible && !settingsVisible) {
            handler.removeCallbacks(menuRaidKeyTick)
        }
    }

    private fun tryGrantRaidKeyFromDrop(): Boolean {
        syncRaidKeyDay()
        val held = raidKeysHeld()
        if (held >= 3) {
            Toast.makeText(this, "Raid Key found, but you already hold 3 (daily max).", Toast.LENGTH_LONG).show()
            return false
        }
        if (raidKeysGrantedToday() >= 3) {
            Toast.makeText(this, "Raid Key found, but daily grant cap (3) already reached.", Toast.LENGTH_LONG).show()
            return false
        }
        prefs().edit()
            .putInt("raid_keys", held + 1)
            .putInt("raid_keys_granted_today", raidKeysGrantedToday() + 1)
            .apply()
        Toast.makeText(this, "Raid Key obtained! (${held + 1}/3 held).", Toast.LENGTH_LONG).show()
        return true
    }

    private fun spendRaidKey(): Boolean {
        syncRaidKeyDay()
        val held = raidKeysHeld()
        if (held <= 0) return false
        prefs().edit().putInt("raid_keys", held - 1).apply()
        return true
    }

    /** v2.28: per slot. The live native flag only counts while this slot's hero is loaded solo. */
    private fun isMetaStoryComplete(): Boolean {
        if (prefs().getBoolean("story_fully_complete", false)) return true
        if (!sessionActive || isOnline()) return false
        return try { isStoryFullyComplete() } catch (_: Exception) { false }
    }

    private fun markMetaStoryCompleteIfNeeded() {
        // Only the loaded solo hero can finish its own slot's story (never online / stale menu state).
        if (!sessionActive || isOnline()) return
        try {
            if (isStoryFullyComplete()) {
                val wasComplete = prefs().getBoolean("story_fully_complete", false)
                prefs().edit().putBoolean("story_fully_complete", true).apply()
                if (!wasComplete && sessionActive && !storyCompleteRoutesShown) {
                    storyCompleteRoutesShown = true
                    // Defer so we don't nest dialogs inside the UI tick.
                    handler.post { showStoryCompleteRoutesDialog() }
                }
            }
        } catch (_: Exception) { }
    }

    private var storyChoiceDialog: AlertDialog? = null

    /**
     * v2.25 Kindling choices: Sallow Venn on the green (beat 13) and the Ember Seal over the well
     * (beat 19). Native decides when a choice is pending (solo story, on the beat, undecided); this
     * only asks. Not cancelable, so Return to Menu + Continue re-asks. Never on an online client.
     */
    private fun pollStoryChoice() {
        if (isOnlineClient || storyChoiceDialog?.isShowing == true) return
        val kind = try { getPendingStoryChoice() } catch (_: Exception) { 0 }
        if (kind != 1 && kind != 2) return
        val (title, message, first, second) = if (kind == 1) {
            listOf(
                "Sallow Venn",
                "Sallow Venn is on his knees in the wet grass. He was ten when they chose his father.",
                "Spare him",
                "End it"
            )
        } else {
            listOf(
                "The Well's Judgment",
                "The First Lamplighter's seal will close the well. The ash-dead are still down there.\n\n" +
                    "Seal them in, and the Lantern stays bright. Let them rest, and it burns dimmer.",
                "Seal them in",
                "Let them rest"
            )
        }
        val choose = { option: Int ->
            storyChoiceDialog = null
            val ok = try { chooseStoryOption(kind, option) } catch (_: Exception) { false }
            if (ok) {
                syncAndSave()
                updateUi()
            }
        }
        storyChoiceDialog = AlertDialog.Builder(this)
            .setTitle(title)
            .setMessage(message)
            .setPositiveButton(first) { _, _ -> choose(1) }
            .setNegativeButton(second) { _, _ -> choose(2) }
            .setCancelable(false)
            .show()
    }

    // --- v2.26 Memory Ledger ----------------------------------------------------------------------

    private var ledgerStoreCache: MemoryLedger.Store? = null
    /** The active lantern's Ledger (ledger_slot_N_… in meta prefs; v2.26 already wrote slot 1). */
    private fun ledgerStore(): MemoryLedger.Store {
        val slot = activeSlot()
        ledgerStoreCache?.let { if (it.slot == slot) return it }
        return MemoryLedger.Store(rawPrefs(), slot).also { ledgerStoreCache = it }
    }

    /** Solo play on the Continue hero only. Host / Join never read or write the Ledger. */
    private fun ledgerSoloSession(): Boolean {
        if (!sessionActive || isOnlineHost || isOnlineClient) return false
        return try { !isDmTable() } catch (_: Exception) { false }
    }

    /** Main menu: tie the Ledger to the hero in the Continue save and migrate it once (silently). */
    private fun migrateLedgerFromSaveIfNeeded() {
        if (!nativeReady || sessionActive) return
        val save = livableSavedState() ?: return
        val store = ledgerStore()
        try {
            store.ensureOwner(MemoryLedger.sessionIdFromSave(save))
            if (!store.isMigrated()) store.migrateFrom(previewStoryMemories(save))
        } catch (e: Exception) { Log.e(TAG, "ledger migration (menu) failed", e) }
    }

    /** After a solo Continue: same owner check, top up story memories by progress, push context. */
    private fun syncLedgerAfterSoloLoad() {
        if (!nativeReady || !ledgerSoloSession()) return
        try {
            val store = ledgerStore()
            store.ensureOwner(getSessionId())
            val firstTime = !store.isMigrated()
            val added = store.migrateFrom(getProgressStoryMemories())
            setMemoryLedgerContext(store.ids().joinToString(","), store.crawlDry())
            if (firstTime && added > 0) {
                showLedgerToast("The Lantern remembers $added voices. Open the Memory Ledger.")
            }
        } catch (e: Exception) { Log.e(TAG, "ledger sync failed", e) }
    }

    private fun pollMemoryLedger() {
        val raw = try { drainMemoryGrants() } catch (_: Exception) { return }
        if (!ledgerSoloSession()) return // online / Lanternkeeper: drained and dropped, store untouched
        val store = ledgerStore()
        try {
            if (needsMemoryContext()) setMemoryLedgerContext(store.ids().joinToString(","), store.crawlDry())
            val (dry, ids) = MemoryLedger.splitIds(raw)
            dry.toIntOrNull()?.let { if (it != store.crawlDry()) store.setCrawlDry(it) }
            val ch = getStoryChoices()
            val (v0, w0) = store.choices()
            if (ch != "$v0,$w0") store.setChoices(ch)
            if (ids.isEmpty()) return
            val added = store.add(ids, markUnread = true)
            if (added.isEmpty()) return
            val (venn, well) = store.choices()
            val title = MemoryLedger.parseCatalog(getMemoryCatalog(venn, well))
                .firstOrNull { it.id == added.last() }?.title ?: "a voice"
            showLedgerToast(
                if (added.size == 1) "A soul settles in the glass — $title. Open the Memory Ledger."
                else "${added.size} souls settle in the glass — $title and more. Open the Memory Ledger."
            )
            refreshLedgerEntry()
        } catch (e: Exception) { Log.e(TAG, "ledger poll failed", e) }
    }

    private fun refreshLedgerEntry() {
        if (!::btnMenuLedger.isInitialized) return
        migrateLedgerFromSaveIfNeeded()
        val store = ledgerStore()
        val hasSave = livableSavedState() != null
        // Opens once Act 1 is done or any memory exists (a migrated Act 1 save opens at once).
        val open = !store.isEmpty() && (hasSave || sessionActive)
        val total = try { MemoryLedger.parseCatalog(getMemoryCatalog(0, 0)).size } catch (_: Exception) { 0 }
        btnMenuLedger.isEnabled = open
        btnMenuLedger.alpha = if (open) 1f else 0.45f
        val unread = store.unread().size
        btnMenuLedger.text = if (open && unread > 0) "Memory Ledger · $unread new" else "Memory Ledger"
        mainMenuLedgerHint.text = if (open) "${store.count()} of $total kindled" else "Finish the Kindling to open the glass."
        if (::btnSettingsLedger.isInitialized) {
            btnSettingsLedger.visibility = if (ledgerSoloSession() && !store.isEmpty()) View.VISIBLE else View.GONE
        }
    }

    private fun openMemoryLedger() {
        val store = ledgerStore()
        if (store.isEmpty() || isOnlineHost || isOnlineClient) {
            Toast.makeText(this, "Finish the Kindling to open the glass.", Toast.LENGTH_SHORT).show()
            return
        }
        val (venn, well) = store.choices()
        val catalog = try { MemoryLedger.parseCatalog(getMemoryCatalog(venn, well)) } catch (_: Exception) { emptyList() }
        if (catalog.isEmpty()) return
        MemoryLedgerScreen(this, store, catalog).show()
    }

    private var ledgerToastView: TextView? = null

    /** Short amber toast; a plain fade unless Reduce Flashing is on (then it simply appears). */
    private fun showLedgerToast(message: String) {
        val content = findViewById<android.widget.FrameLayout>(android.R.id.content) ?: return
        ledgerToastView?.let { content.removeView(it) }
        val d = resources.displayMetrics.density
        val tv = TextView(this).apply {
            text = message
            setTextColor(androidx.core.content.ContextCompat.getColor(this@MainActivity, R.color.ae_amber_core))
            textSize = 14f
            try { typeface = androidx.core.content.res.ResourcesCompat.getFont(this@MainActivity, R.font.cormorant) } catch (_: Exception) {}
            setBackgroundResource(R.drawable.bg_narration_card)
            setPadding((14 * d).toInt(), (10 * d).toInt(), (14 * d).toInt(), (10 * d).toInt())
            gravity = android.view.Gravity.CENTER
            isClickable = true
            setOnClickListener { content.removeView(this); ledgerToastView = null }
        }
        val lp = android.widget.FrameLayout.LayoutParams(
            android.widget.FrameLayout.LayoutParams.MATCH_PARENT, android.widget.FrameLayout.LayoutParams.WRAP_CONTENT,
            android.view.Gravity.TOP
        ).apply { val m = (16 * d).toInt(); setMargins(m, (72 * d).toInt(), m, 0) }
        content.addView(tv, lp)
        ledgerToastView = tv
        if (reduceFlashEnabled) {
            tv.alpha = 1f
        } else {
            tv.alpha = 0f
            tv.animate().alpha(1f).setDuration(220).start()
        }
        handler.postDelayed({
            if (ledgerToastView === tv) { content.removeView(tv); ledgerToastView = null }
        }, 3600)
    }

    private fun pollRaidKeyDrop() {
        try {
            if (consumePendingRaidKeyDrop()) {
                // Host / Join never grant keys into a slot.
                if (isOnline() || !sessionActive) return
                tryGrantRaidKeyFromDrop()
                refreshMainMenuButtons()
            }
        } catch (_: Exception) { }
    }

    private fun resetDailyQuestSnap() {
        lastDqSnap = intArrayOf(0, 0, 0, 0, 0)
        if (!nativeReady) return
        try {
            val parts = getDailyQuestCounters().split(',')
            for (i in 0 until 5) {
                lastDqSnap[i] = parts.getOrNull(i)?.toIntOrNull() ?: 0
            }
        } catch (_: Exception) { }
    }

    /** Progress only for solo story-complete sessions — never Host/Join. */
    private fun canTrackDailyQuests(): Boolean {
        if (!sessionActive || !nativeReady) return false
        if (isOnlineHost || isOnlineClient) return false
        if (!isMetaStoryComplete()) return false
        return true
    }

    private fun pollDailyQuestProgress() {
        if (!canTrackDailyQuests()) return
        try {
            val parts = getDailyQuestCounters().split(',')
            if (parts.size < 5) return
            val snap = IntArray(5) { i -> parts[i].toIntOrNull() ?: 0 }
            val dRooms = (snap[0] - lastDqSnap[0]).coerceAtLeast(0)
            val dFights = (snap[1] - lastDqSnap[1]).coerceAtLeast(0)
            val dSearches = (snap[2] - lastDqSnap[2]).coerceAtLeast(0)
            val dDamage = (snap[3] - lastDqSnap[3]).coerceAtLeast(0)
            val dBosses = (snap[4] - lastDqSnap[4]).coerceAtLeast(0)
            lastDqSnap = snap
            if (DailyQuests.applyDelta(prefs(), dRooms, dFights, dSearches, dDamage, dBosses)) {
                // Soft toast when an objective just completed
                val st = DailyQuests.readForUi(prefs(), true, livableSavedState() != null)
                if (st.objectives.any { it.complete && it.progress >= it.target }) {
                    // No spam: only when claimable flips
                    if (st.claimable) {
                        Toast.makeText(this, "Daily Quests complete — claim from the menu!", Toast.LENGTH_SHORT).show()
                    }
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "daily quest poll failed", e)
        }
    }

    private fun refreshDailyQuestsUi() {
        if (!::mainMenuDailyQuests.isInitialized && !::settingsDailyQuests.isInitialized) return
        val storyDone = isMetaStoryComplete()
        val hasSave = livableSavedState() != null
        val state = DailyQuests.readForUi(prefs(), storyDone, hasSave)
        val summary = DailyQuests.summaryLines(state)
        val refreshHint = if (state.lockedReason == null && !state.claimed) {
            val ms = DailyQuests.msUntilLocalMidnight()
            " · refreshes in ${formatRaidKeyCountdown(ms)}"
        } else if (state.claimed) {
            val ms = DailyQuests.msUntilLocalMidnight()
            " · new board in ${formatRaidKeyCountdown(ms)}"
        } else ""
        if (::mainMenuDailyQuests.isInitialized) {
            mainMenuDailyQuests.text = summary + refreshHint
        }
        if (::btnMenuClaimDaily.isInitialized) {
            btnMenuClaimDaily.visibility = if (state.claimable) View.VISIBLE else View.GONE
        }
        if (::settingsDailyQuests.isInitialized) {
            settingsDailyQuests.text = summary + refreshHint
        }
        if (::btnSettingsClaimDaily.isInitialized) {
            btnSettingsClaimDaily.visibility = if (state.claimable) View.VISIBLE else View.GONE
        }
    }

    private fun claimDailyQuestsRewards() {
        val storyDone = isMetaStoryComplete()
        val hasSave = livableSavedState() != null
        syncRaidKeyDay()
        val result = DailyQuests.tryClaim(
            prefs = prefs(),
            storyComplete = storyDone,
            hasContinueSave = hasSave,
            raidKeysHeld = raidKeysHeld(),
            raidKeysGrantedToday = raidKeysGrantedToday(),
            grantRaidKey = {
                // Fixed 1-key grant under held + daily caps (meta prefs only).
                tryGrantRaidKeyFromDrop()
            }
        ) ?: run {
            Toast.makeText(this, "Nothing to claim yet.", Toast.LENGTH_SHORT).show()
            refreshDailyQuestsUi()
            return
        }
        val appliedNow = tryApplyDailySpoilsNow(result.gold, result.loot)
        if (!appliedNow) {
            DailyQuests.stashPendingSpoils(prefs(), result.gold, result.loot)
            Toast.makeText(
                this,
                result.message + " — spoils apply next Continue.",
                Toast.LENGTH_LONG
            ).show()
        } else {
            Toast.makeText(this, result.message, Toast.LENGTH_LONG).show()
        }
        refreshMainMenuButtons()
        refreshDailyQuestsUi()
    }

    /** Grant gold/loot into the live solo party and persist save_state (never online). */
    private fun tryApplyDailySpoilsNow(gold: Int, loot: Boolean): Boolean {
        if (!nativeReady) return false
        if (isOnlineHost || isOnlineClient) return false
        if (!sessionActive) return false
        return try {
            val ok = grantDailyQuestSpoils(gold, loot)
            if (ok) syncAndSave()
            ok
        } catch (e: Exception) {
            Log.e(TAG, "grantDailyQuestSpoils failed", e)
            false
        }
    }

    private fun applyPendingDailySpoilsIfAny() {
        if (!nativeReady || !sessionActive) return
        if (isOnlineHost || isOnlineClient) return
        val (gold, loot) = DailyQuests.takePendingSpoils(prefs())
        if (gold <= 0 && !loot) return
        try {
            if (grantDailyQuestSpoils(gold, loot)) {
                syncAndSave()
                Toast.makeText(this, "Daily quest spoils delivered (+${gold}g).", Toast.LENGTH_LONG).show()
            } else {
                // Put back if party not ready
                DailyQuests.stashPendingSpoils(prefs(), gold, loot)
            }
        } catch (e: Exception) {
            Log.e(TAG, "pending daily spoils failed", e)
            DailyQuests.stashPendingSpoils(prefs(), gold, loot)
        }
    }

    private fun wearerClassLabel(forName: String): String {
        return try {
            val sheet = getDetailedSheet(forName)
            // "Lvl N ClassName | Gold: ..."
            val line = sheet.lineSequence().firstOrNull { it.contains("Lvl ") && it.contains("|") } ?: return ""
            val afterLvl = line.substringAfter("Lvl ").trim()
            val bits = afterLvl.split(Regex("\\s+"))
            if (bits.size >= 2) bits[1].trim() else ""
        } catch (_: Exception) {
            ""
        }
    }

    private fun classLockMessage(itemClass: String): String {
        val c = itemClass.trim()
        return when {
            c.equals("Any", ignoreCase = true) || c.isBlank() -> ""
            else -> "$c only"
        }
    }


    private fun showRestChooser() {
        val shortLabel = "Short Rest — recover some HP and special uses (once per clear)"
        val cd = try { getLongRestCooldownRooms() } catch (_: Exception) { 0 }
        val longLabel = if (cd > 0) {
            "Long Rest — on cooldown ($cd room${if (cd == 1) "" else "s"} left; every 5 rooms)"
        } else {
            "Long Rest — full HP and special/supply restore (every 5 rooms; once per clear)"
        }
        AlertDialog.Builder(this)
            .setTitle("Rest")
            .setItems(arrayOf(shortLabel, longLabel)) { _, which ->
                when (which) {
                    0 -> performOrQueue("rest", 0) {
                        doRest()
                        val ev = try { getLastEvent() } catch (_: Exception) { "" }
                        if (ev.contains("Already rested", ignoreCase = true)) {
                            Toast.makeText(this, ev, Toast.LENGTH_SHORT).show()
                        }
                    }
                    1 -> {
                        val left = try { getLongRestCooldownRooms() } catch (_: Exception) { 0 }
                        if (left > 0) {
                            Toast.makeText(
                                this,
                                "Long Rest on cooldown — clear $left more room${if (left == 1) "" else "s"} first.",
                                Toast.LENGTH_LONG
                            ).show()
                            return@setItems
                        }
                        performOrQueue("longRest", 0) {
                            doLongRest()
                            val ev = try { getLastEvent() } catch (_: Exception) { "" }
                            if (ev.contains("Already rested", ignoreCase = true) ||
                                ev.contains("cooldown", ignoreCase = true)
                            ) {
                                Toast.makeText(this, ev, Toast.LENGTH_SHORT).show()
                            }
                        }
                    }
                }
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun showInventoryDialog(forName: String = localPlayerName) {
        if (!nativeReady) {
            Toast.makeText(this, "Native library not loaded — cannot open inventory.", Toast.LENGTH_SHORT).show()
            return
        }
        val view = layoutInflater.inflate(R.layout.dialog_inventory, null)
        val goldTv = view.findViewById<TextView>(R.id.invGoldText)
        goldTv.setCompoundDrawablesRelative(gearChip(GearIcons.GOLD, 20), null, null, null)
        goldTv.compoundDrawablePadding = (6 * resources.displayMetrics.density).toInt()
        val list = view.findViewById<LinearLayout>(R.id.invItemList)
        val dialog = AlertDialog.Builder(this)
            .setTitle("Inventory — $forName")
            .setView(view)
            .setNegativeButton("Close", null)
            .create()

        fun confirmSell(itemLabel: String, price: Int, onYes: () -> Unit) {
            AlertDialog.Builder(this)
                .setTitle("Sell item")
                .setMessage("Sell $itemLabel for ${price}g?")
                .setPositiveButton("Sell") { _, _ -> onYes() }
                .setNegativeButton("Cancel", null)
                .show()
        }

        fun refresh() {
            list.removeAllViews()
            val gold = try {
                val sheet = getDetailedSheet(forName)
                Regex("""Gold:\s*(\d+)""").find(sheet)?.groupValues?.getOrNull(1)?.toIntOrNull() ?: parsePlayerGold()
            } catch (_: Exception) { parsePlayerGold() }
            goldTv.text = "Gold: ${gold}g"
            val manifest = try { getInventoryManifest(forName) } catch (_: Exception) { "" }
            val entries = manifest.split(';').map { it.trim() }.filter { it.isNotEmpty() }
            if (entries.isEmpty()) {
                val empty = TextView(this).apply {
                    text = "No gear yet. Visit a merchant or clear rooms for loot."
                    setTextColor(getColor(R.color.ae_bone))
                    textSize = 13f
                }
                list.addView(empty)
                return
            }
            for (entry in entries) {
                // idx:name|bonus|type|rarity|class|slot|upgradeLevel|upgradeCost|sellPrice
                val idxPart = entry.substringBefore(':')
                val rest = entry.substringAfter(':', "")
                val parts = rest.split('|')
                if (parts.size < 8) continue
                val name = parts[0]
                val bonus = parts[1]
                val type = parts[2]
                val rarity = parts[3]
                val cls = parts[4]
                val slot = parts[5]
                val upgLvl = parts[6]
                val upgCost = parts[7].toIntOrNull() ?: 0
                val sellPrice = parts.getOrNull(8)?.toIntOrNull() ?: 1
                val legText = parts.getOrNull(9)?.trim().orEmpty()
                val index = idxPart.toIntOrNull() ?: continue
                val row = layoutInflater.inflate(R.layout.item_inventory_row, list, false)
                val nameTv = row.findViewById<TextView>(R.id.invItemName)
                val metaTv = row.findViewById<TextView>(R.id.invItemMeta)
                val equipBtn = row.findViewById<Button>(R.id.invEquipBtn)
                val upgBtn = row.findViewById<Button>(R.id.invUpgradeBtn)
                val transferBtn = row.findViewById<Button>(R.id.invTransferBtn)
                val sellBtn = row.findViewById<Button>(R.id.invSellBtn)
                val itemLabel = "$name (+$bonus)"
                nameTv.text = itemLabel
                nameTv.setTextColor(rarityColor(rarity))
                bindRarityRow(row, rarity, row.findViewById(R.id.invItemSocket), row.findViewById(R.id.invItemPip), name, type)
                row.findViewById<View?>(R.id.invEquippedTag)?.visibility =
                    if (slot == "weapon" || slot == "armor") View.VISIBLE else View.GONE
                val slotLabel = when (slot) {
                    "weapon" -> "Equipped · Weapon"
                    "armor" -> "Equipped · Armor"
                    else -> "Bag"
                }
                val wearerClass = wearerClassLabel(forName)
                val classLocked = type != "Potion"
                    && !cls.equals("Any", ignoreCase = true)
                    && wearerClass.isNotBlank()
                    && !cls.equals(wearerClass, ignoreCase = true)
                val lockMsg = classLockMessage(cls)
                metaTv.text = buildString {
                    append("$rarity · $cls · $type · $slotLabel · upg $upgLvl · sell ${sellPrice}g")
                    if (classLocked) append(" · $lockMsg")
                    val bonusInt = bonus.toIntOrNull() ?: 0
                    if (type.equals("Weapon", true) || type.equals("Armor", true) || type.equals("Potion", true)) {
                        append("\n")
                        append(BeginnerGuide.gearBonusFlavor(type, bonusInt))
                    }
                    if (legText.isNotBlank()) append("\n$legText")
                }
                // Long-press for a fuller beginner explanation (matches existing inspect UX).
                row.setOnLongClickListener {
                    val bonusInt = bonus.toIntOrNull() ?: 0
                    val body = buildString {
                        append(itemLabel)
                        append("\n\n")
                        append(BeginnerGuide.gearBonusFlavor(type, bonusInt))
                        append("\n\n")
                        append(BeginnerGuide.gearStatsHelpBlurb())
                        if (legText.isNotBlank()) append("\n\n").append(legText)
                    }
                    AlertDialog.Builder(this)
                        .setTitle("Item info")
                        .setMessage(body)
                        .setPositiveButton("OK", null)
                        .show()
                    true
                }
                sellBtn.text = "Sell (${sellPrice}g)"
                val hasAlly = companionNameSafe().isNotBlank()
                transferBtn.text = "Give to ally"
                transferBtn.isEnabled = hasAlly
                if (!hasAlly) {
                    transferBtn.alpha = 0.45f
                } else {
                    transferBtn.alpha = 1f
                }
                if (slot == "weapon" || slot == "armor") {
                    equipBtn.text = "Unequip"
                    equipBtn.setOnClickListener {
                        val s = if (slot == "weapon") 0 else 1
                        val ok = try { doUnequipItem(forName, s) } catch (_: Exception) { false }
                        val ev = try { getLastEvent() } catch (_: Exception) { "" }
                        if (ok) {
                            syncAndSave(); appendCombatFeed(ev); refresh(); updateUi()
                        } else {
                            Toast.makeText(this, ev.ifBlank { "Could not unequip." }, Toast.LENGTH_SHORT).show()
                        }
                    }
                    upgBtn.text = "Upgrade (${upgCost}g)"
                    upgBtn.isEnabled = gold >= upgCost && type != "Potion"
                    upgBtn.setOnClickListener {
                        val s = if (slot == "weapon") 0 else 1
                        val ok = try { doUpgradeEquippedItem(forName, s) } catch (_: Exception) { false }
                        val ev = try { getLastEvent() } catch (_: Exception) { "" }
                        if (ok) {
                            syncAndSave(); appendCombatFeed(ev); refresh(); updateUi()
                        } else {
                            Toast.makeText(this, ev.ifBlank { "Not enough gold!" }, Toast.LENGTH_SHORT).show()
                            refresh()
                        }
                    }
                    transferBtn.setOnClickListener {
                        if (!hasAlly) {
                            Toast.makeText(this, "No companion to transfer to.", Toast.LENGTH_SHORT).show()
                            return@setOnClickListener
                        }
                        val s = if (slot == "weapon") 0 else 1
                        val ok = try { doTransferEquippedItemToAlly(forName, s) } catch (_: Exception) { false }
                        val ev = try { getLastEvent() } catch (_: Exception) { "" }
                        if (ok) {
                            syncAndSave(); appendCombatFeed(ev); refresh(); updateUi()
                        } else {
                            Toast.makeText(this, ev.ifBlank { "Could not transfer." }, Toast.LENGTH_SHORT).show()
                        }
                    }
                    sellBtn.setOnClickListener {
                        val s = if (slot == "weapon") 0 else 1
                        confirmSell(itemLabel, sellPrice) {
                            val ok = try { doSellEquippedItem(forName, s) } catch (_: Exception) { false }
                            val ev = try { getLastEvent() } catch (_: Exception) { "" }
                            if (ok) {
                                syncAndSave(); appendCombatFeed(ev); refresh(); updateUi()
                            } else {
                                Toast.makeText(this, ev.ifBlank { "Could not sell." }, Toast.LENGTH_SHORT).show()
                                refresh()
                            }
                        }
                    }
                } else {
                    if (type == "Potion") {
                        val effect = legText.ifBlank { "Drink to apply the potion's effect. Consumed on use." }
                        metaTv.text = buildString {
                            append("$rarity · Potion · Bag · sell ${sellPrice}g")
                            append("\n$effect")
                        }
                        equipBtn.text = "Use"
                        equipBtn.isEnabled = true
                        equipBtn.alpha = 1f
                        equipBtn.setOnClickListener {
                            val ok = try { doUseInventoryPotion(forName, index) } catch (_: Exception) { false }
                            val ev = try { getLastEvent() } catch (_: Exception) { "" }
                            if (ok) {
                                syncAndSave(); appendCombatFeed(ev); refresh(); updateUi()
                                Toast.makeText(this, ev.ifBlank { "Potion used." }, Toast.LENGTH_SHORT).show()
                            } else {
                                Toast.makeText(this, ev.ifBlank { "Cannot use potion." }, Toast.LENGTH_SHORT).show()
                            }
                        }
                        upgBtn.isEnabled = false
                        upgBtn.alpha = 0.45f
                    } else if (classLocked) {
                        equipBtn.text = lockMsg.ifBlank { "Class locked" }
                        equipBtn.isEnabled = false
                        equipBtn.alpha = 0.45f
                        equipBtn.setOnClickListener {
                            Toast.makeText(this, lockMsg.ifBlank { "Wrong class for this gear." }, Toast.LENGTH_SHORT).show()
                        }
                    } else {
                        equipBtn.text = "Equip"
                        equipBtn.isEnabled = true
                        equipBtn.alpha = 1f
                        equipBtn.setOnClickListener {
                            val ok = try { doEquipItem(forName, index) } catch (_: Exception) { false }
                            val ev = try { getLastEvent() } catch (_: Exception) { "" }
                            if (ok) {
                                syncAndSave(); appendCombatFeed(ev); refresh(); updateUi()
                            } else {
                                Toast.makeText(this, ev.ifBlank { "Cannot equip." }, Toast.LENGTH_SHORT).show()
                            }
                        }
                    }
                    upgBtn.text = "Upgrade (${upgCost}g)"
                    upgBtn.isEnabled = gold >= upgCost && type != "Potion"
                    upgBtn.setOnClickListener {
                        val ok = try { doUpgradeInventoryItem(forName, index) } catch (_: Exception) { false }
                        val ev = try { getLastEvent() } catch (_: Exception) { "" }
                        if (ok) {
                            syncAndSave(); appendCombatFeed(ev); refresh(); updateUi()
                        } else {
                            Toast.makeText(this, ev.ifBlank { "Not enough gold!" }, Toast.LENGTH_SHORT).show()
                            refresh()
                        }
                    }
                    transferBtn.setOnClickListener {
                        if (!hasAlly) {
                            Toast.makeText(this, "No companion to transfer to.", Toast.LENGTH_SHORT).show()
                            return@setOnClickListener
                        }
                        val ok = try { doTransferInventoryItemToAlly(forName, index) } catch (_: Exception) { false }
                        val ev = try { getLastEvent() } catch (_: Exception) { "" }
                        if (ok) {
                            syncAndSave(); appendCombatFeed(ev); refresh(); updateUi()
                        } else {
                            Toast.makeText(this, ev.ifBlank { "Could not transfer." }, Toast.LENGTH_SHORT).show()
                        }
                    }
                    sellBtn.setOnClickListener {
                        confirmSell(itemLabel, sellPrice) {
                            val ok = try { doSellInventoryItem(forName, index) } catch (_: Exception) { false }
                            val ev = try { getLastEvent() } catch (_: Exception) { "" }
                            if (ok) {
                                syncAndSave(); appendCombatFeed(ev); refresh(); updateUi()
                            } else {
                                Toast.makeText(this, ev.ifBlank { "Could not sell." }, Toast.LENGTH_SHORT).show()
                                refresh()
                            }
                        }
                    }
                }
                list.addView(row)
            }
        }
        refresh()
        dialog.show()
    }

    private fun showShopDialog() {
        if (!nativeReady) {
            Toast.makeText(this, "Native library not loaded — cannot open shop.", Toast.LENGTH_SHORT).show()
            return
        }
        val manifest = getShopManifest()
        if (manifest.isBlank()) {
            Toast.makeText(this, "The merchant has nothing for sale.", Toast.LENGTH_SHORT).show()
            return
        }
        // C++ format: "0:name|bonus|type|rarity|class|cost;..."
        val entries = manifest.split(';').map { it.trim() }.filter { it.isNotEmpty() }
        if (entries.isEmpty()) {
            Toast.makeText(this, "The merchant has nothing for sale.", Toast.LENGTH_SHORT).show()
            return
        }
        val view = layoutInflater.inflate(R.layout.dialog_shop, null)
        val goldTv = view.findViewById<TextView>(R.id.shopGoldText)
        val list = view.findViewById<LinearLayout>(R.id.shopItemList)
        fun refreshGold() {
            goldTv.text = "Your gold: ${parsePlayerGold()}g"
        }
        refreshGold()
        val dialog = AlertDialog.Builder(this)
            .setTitle("Merchant")
            .setView(view)
            .setNeutralButton("Inventory") { _, _ -> showInventoryDialog(localPlayerName) }
            .setNegativeButton("Leave", null)
            .create()

        fun rebuild() {
            list.removeAllViews()
            val m2 = try { getShopManifest() } catch (_: Exception) { "" }
            val ents = m2.split(';').map { it.trim() }.filter { it.isNotEmpty() }
            refreshGold()
            for (entry in ents) {
                val index = entry.substringBefore(':').toIntOrNull() ?: continue
                val rest = entry.substringAfter(':', "")
                val parts = rest.split('|')
                val name: String
                val cost: Int
                val rarity: String
                val cls: String
                val type: String
                val bonus: String
                if (parts.size >= 6) {
                    name = parts[0]
                    bonus = parts[1]
                    type = parts[2]
                    rarity = parts[3]
                    cls = parts[4]
                    cost = parts[5].toIntOrNull() ?: 0
                } else {
                    // Legacy fallback: "Name (25g)"
                    val label = rest
                    cost = Regex("""\((\d+)g\)""").find(label)?.groupValues?.getOrNull(1)?.toIntOrNull() ?: 0
                    name = label.replace(Regex("""\s*\(\d+g\)\s*$"""), "").trim()
                    rarity = "Common"; cls = "Any"; type = "Item"; bonus = "0"
                }
                val row = layoutInflater.inflate(R.layout.item_shop_row, list, false)
                val nameTv = row.findViewById<TextView>(R.id.shopItemName)
                nameTv.text = "$name (+$bonus) · $type"
                nameTv.setTextColor(rarityColor(rarity))
                bindRarityRow(row, rarity, row.findViewById(R.id.shopItemSocket), row.findViewById(R.id.shopItemPip), name, type)
                val bonusInt = bonus.toIntOrNull() ?: 0
                val flavor = BeginnerGuide.gearBonusFlavor(type, bonusInt)
                row.findViewById<TextView>(R.id.shopItemCost).text = "$rarity · $cls · ${cost}g\n$flavor"
                row.setOnLongClickListener {
                    AlertDialog.Builder(this)
                        .setTitle(name)
                        .setMessage("$flavor\n\n${BeginnerGuide.gearStatsHelpBlurb()}")
                        .setPositiveButton("OK", null)
                        .show()
                    true
                }
                val buy = row.findViewById<Button>(R.id.shopBuyBtn)
                fun updateBuyEnabled() {
                    buy.isEnabled = parsePlayerGold() >= cost
                }
                updateBuyEnabled()
                buy.setOnClickListener {
                    if (isOnlineClient) {
                        performOrQueue("buy", index) { }
                        dialog.dismiss()
                        return@setOnClickListener
                    }
                    val ok = try { doBuyItem(localPlayerName, index) } catch (_: Exception) { false }
                    val ev = try { getLastEvent() } catch (_: Exception) { "" }
                    if (ok) {
                        syncAndSave()
                        appendCombatFeed(ev.ifBlank { "Bought $name." })
                        rebuild()
                        updateUi()
                    } else {
                        appendCombatFeed(ev.ifBlank { "Could not buy that." })
                        Toast.makeText(this, ev.ifBlank { "Could not buy that." }, Toast.LENGTH_SHORT).show()
                        refreshGold()
                    }
                }
                list.addView(row)
            }
        }
        rebuild()
        dialog.show()
    }

    private fun showJournal() {
        if (!nativeReady) return
        val journal = try { getJournal() } catch (_: Exception) { "" }
        val chatLog = try { getChatHistory() } catch (_: Exception) { "" }
        val recent = chatLog.lineSequence()
            .filter { it.isNotBlank() }
            .toList()
            .takeLast(12)
            .joinToString("\n")
        val body = buildString {
            if (recent.isNotBlank()) {
                append("Recent\n")
                append(recent)
                append("\n\n")
            }
            append("Journal\n")
            append(journal.ifBlank { "(empty)" })
        }
        AlertDialog.Builder(this)
            .setTitle("Adventure Log")
            .setMessage(body)
            .setPositiveButton("Close", null)
            .show()
    }

    private fun startUiUpdateLoop() {
        if (uiLoopStarted) return
        uiLoopStarted = true
        handler.post(uiTick)
    }


    private fun updateUi() {
        if (!nativeReady) return
        val status = getPlayerStatus()
        markMetaStoryCompleteIfNeeded()
        pollStoryChoice()
        pollMemoryLedger()
        pollRaidKeyDrop()
        pollDailyQuestProgress()
        val isShop = isMerchantRoom()

        // Compact HUD — full sheet stays behind Sheet
        val turnLine = status.lineSequence().firstOrNull { it.contains("Turn:") } ?: "Turn: —"
        val roomLine = status.lineSequence().firstOrNull {
            it.startsWith("Room") || it.contains("Room ")
        } ?: "Room ?"
        val whose = turnLine.substringAfter("Turn:", "—").trim()
        val companionTurnNow = isCompanionPlayerTurn()
        val myTurnBanner = whose.equals(localPlayerName, ignoreCase = true) ||
            whose.equals("You", ignoreCase = true) || companionTurnNow
        turnBanner.text = when {
            isOnlineHost && status.contains("Game Over", ignoreCase = true) -> "Keeper · defeat…"
            isOnlineHost && status.contains("waiting for players", ignoreCase = true) -> "Keeper · waiting for heroes"
            isOnlineHost -> "Keeper · directing the table"
            isOnlineClient && remoteDmName.isNotBlank() -> "Keeper: $remoteDmName · $whose"
            status.contains("Story complete", ignoreCase = true) &&
                try { isRoomCleared() } catch (_: Exception) { false } ->
                "Story complete · Menu / Boss Raid / Onward"
            status.contains("Story complete", ignoreCase = true) &&
                status.contains("endgame", ignoreCase = true) -> "Story complete — Menu or Boss Raid"
            status.contains("Story complete", ignoreCase = true) -> "Story complete — Acts 1–4"
            status.contains("Mode: Boss Raid", ignoreCase = true) -> "Boss Raid"
            status.contains("Mode: Challenge Dungeon", ignoreCase = true) -> "Challenge Dungeon"
            status.contains("Quest: Ashwake", ignoreCase = true) &&
                try { isRoomCleared() } catch (_: Exception) { false } ->
                "Act 4 · clear — Search, Rest, or Onward"
            status.contains("Quest: Ashwake", ignoreCase = true) ->
                status.lineSequence().firstOrNull { it.startsWith("Quest:") }?.removePrefix("Quest:")?.trim()
                    ?.let { "Quest · $it" } ?: "Act 4 — Ashwake Vigil"
            status.contains("Quest: Emberdeep", ignoreCase = true) &&
                try { isRoomCleared() } catch (_: Exception) { false } ->
                "Act 3 · clear — Search, Rest, or Onward"
            status.contains("Quest: Emberdeep", ignoreCase = true) ->
                status.lineSequence().firstOrNull { it.startsWith("Quest:") }?.removePrefix("Quest:")?.trim()
                    ?.let { "Quest · $it" } ?: "Act 3 — Emberdeep"
            status.contains("Quest complete", ignoreCase = true) &&
                try { isRoomCleared() } catch (_: Exception) { false } ->
                "Main quest done — Ashen Lantern · Search, Rest, or Onward"
            status.contains("Quest complete", ignoreCase = true) -> "Main quest done — Ashen Lantern"
            (status.contains("Quest: The Kindling", ignoreCase = true) ||
                status.contains("Quest: Ashen Lantern", ignoreCase = true) ||
                status.contains("Quest: Millhollow", ignoreCase = true) ||
                status.contains("Quest: The Tithe", ignoreCase = true) ||
                status.contains("Quest: Emberdeep", ignoreCase = true) ||
                status.contains("Quest: Ashwake", ignoreCase = true)) &&
                try { isRoomCleared() } catch (_: Exception) { false } ->
                status.lineSequence().firstOrNull { it.startsWith("Quest:") }?.removePrefix("Quest:")?.trim()
                    ?.let { "Quest · $it · clear" } ?: "Room clear — Search, Rest, or Onward"
            status.contains("Mode: Dungeon Crawl", ignoreCase = true) &&
                try { isRoomCleared() } catch (_: Exception) { false } ->
                "Crawl · Room clear — Search, Rest, or Onward"
            isShop -> "Safe haven — merchant"
            try { isRoomCleared() } catch (_: Exception) { false } -> "Room clear — Search, Rest, or Onward"
            status.contains("Game Over", ignoreCase = true) -> "Defeat…"
            companionTurnNow -> "Your move — ${companionNameSafe()}"
            myTurnBanner -> "Your move, $localPlayerName"
            else -> "$whose acts…"
        }
        val roomBit = roomLine.substringBefore("|").trim().ifBlank { roomLine }
        statusText.text = when {
            isOnlineHost && onlineSessionId.isNotBlank() -> "$roomBit · ID $onlineSessionId"
            isOnlineClient && onlineSessionId.isNotBlank() -> "$roomBit · joined $onlineSessionId"
            else -> roomBit
        }

        val specialName = try { getSpecialName() } catch (_: Exception) { "Special" }
        // Two-line labels so text stays visible on narrow / large-font screens
        val roomCleared = !isShop && try { isRoomCleared() } catch (_: Exception) { false }
        val searchedRoom = try { hasSearchedRoom() } catch (_: Exception) { false }
        btnSpecial.text = if (isShop) "Info" else "✦\n$specialName"
        btnAttack.text = when {
            isShop -> "🛒\nShop"
            roomCleared -> "🚪\nOnward"
            else -> "⚔\nAttack"
        }
        btnHeal.text = "Potion"
        btnRest.text = if (isShop) "Leave" else "🌙\nRest"
        btnInteract.visibility = if (isShop) View.GONE else View.VISIBLE
        btnInteract.text = if (searchedRoom) "🔍\nDone" else "🔍\nSearch"

        val dmTable = isOnlineHost && try { isDmTable() } catch (_: Exception) { isOnlineHost }
        if (dmTable) {
            btnSheet.text = "Keeper"
            btnCompanion.visibility = View.GONE
            turnBanner.text = when {
                status.contains("Game Over", ignoreCase = true) -> "Keeper · defeat…"
                getBattleRoster().substringBefore('|').isBlank() -> "Keeper · waiting for heroes to Join"
                else -> "Keeper · tap Keeper for story tools"
            }
        } else {
            btnCompanion.visibility = if (companionNameSafe().isNotBlank()) View.VISIBLE else View.GONE
            if (isOnlineClient && remoteDmName.isNotBlank()) {
                btnSheet.text = "Sheet"
            }
        }


        val isGameOver = status.contains("Game Over", ignoreCase = true)
        val localDown = status.lineSequence().any {
            it.contains(localPlayerName) && (
                it.contains("[DOWN]", ignoreCase = true) ||
                it.contains("[DEAD]", ignoreCase = true) ||
                it.contains("[STABLE]", ignoreCase = true)
            )
        }
        val clearedForTurn = !isShop && try { isRoomCleared() } catch (_: Exception) { false }
        val companionTurnUi = isCompanionPlayerTurn()
        val isMyTurn = !isGameOver && (
            companionTurnUi || (
                !localDown && (
                    isShop || clearedForTurn ||
                        status.contains("Turn: Safe", ignoreCase = true) ||
                        status.contains("Turn: $localPlayerName") || status.contains("Turn: You")
                )
            )
        )

        val dmTableLock = isOnlineHost && try { isDmTable() } catch (_: Exception) { isOnlineHost }
        if (dmTableLock) {
            btnAttack.isEnabled = false
            btnSpecial.isEnabled = false
            btnHeal.isEnabled = false
            btnRest.isEnabled = false
            btnInteract.isEnabled = false
        } else {
            val inCombat = try { isInCombat() } catch (_: Exception) { false }
            val cleared = !isShop && try { isRoomCleared() } catch (_: Exception) { false }
            val searched = try { hasSearchedRoom() } catch (_: Exception) { false }
            // Cleared chamber uses Turn: Safe — treat like shop exploration for button unlocks.
            val canAct = isMyTurn || (!isGameOver && !localDown && (isShop || cleared ||
                status.contains("Turn: Safe", ignoreCase = true)))
            val actorRes = try { getCurrentActorResources() } catch (_: Exception) { 1 }
            btnSpecial.isEnabled = canAct && !cleared && !isShop && actorRes > 0
            btnSpecial.alpha = if (!isShop && !cleared && actorRes <= 0) 0.45f else 1f
            btnHeal.isEnabled = canAct && !cleared && !isShop && actorRes > 0
            // Onward / shop after clear even if local hero is downed (party continues).
            btnAttack.isEnabled = when {
                isGameOver -> false
                isShop -> canAct || !localDown
                cleared -> true
                else -> canAct && (inCombat || isMyTurn)
            }
            // Short/Long Rest out of combat; one rest opportunity per cleared chamber.
            val alreadyRested = try { hasUsedRoomRest() } catch (_: Exception) { false }
            btnRest.isEnabled = !isGameOver && (isShop || (!inCombat && (canAct || cleared) && !alreadyRested))
            btnRest.alpha = if (!isShop && alreadyRested) 0.45f else 1f
            if (!isShop) {
                btnRest.text = if (alreadyRested) "🌙\nRested" else "🌙\nRest"
            }
            // Search only when room clear, once per room (never while downed/dead).
            btnInteract.isEnabled = canAct && !isShop && cleared && !searched
        }

        val roomDesc = getRoomDescription()
        if (roomDesc != lastRoomDesc) {
            roomDescText.text = roomDesc
            lastRoomDesc = roomDesc
        }

        // Track chat for journal, but never clobber the combat feed with chat last-line.
        val chat = getChatHistory()
        if (chat != lastChatHistory) {
            lastChatHistory = chat
            gameAudio?.maybeSpeakDmFromChat(chat)
        }

        syncAudioBedFromState()

        val roomKey = try { getRoomDescription().take(40) } catch (_: Exception) { "" } + "|" +
            (try { getPlayerStatus().lineSequence().firstOrNull { it.startsWith("Room") }.orEmpty() } catch (_: Exception) { "" })
        if (roomKey != lastSelectionRoomKey) {
            if (lastSelectionRoomKey.isNotEmpty()) clearCombatSelection("room change")
            lastSelectionRoomKey = roomKey
        }

        val lastEv = getLastEvent()
        if (lastEv.isNotEmpty() && lastEv != lastProcessedEvent) {
            lastProcessedEvent = lastEv
            appendCombatFeed(lastEv)
            if (lastEv.startsWith("BOSS!") || lastEv.contains("Boss encounter", ignoreCase = true)
                || lastEv.contains("BOSS RAID", ignoreCase = true)
                || lastEv.contains("CHALLENGE DUNGEON", ignoreCase = true)) {
                bossFightActive = true
                syncAudioBedFromState()
            }
            if (lastEv.startsWith("BOSS!") || lastEv.contains("Boss encounter", ignoreCase = true)
                || lastEv.startsWith("SET-PIECE!")) {
                Toast.makeText(this, lastEv, Toast.LENGTH_LONG).show()
            }
            maybeKillPop(lastEv)
            maybeRareLootAccent(lastEv)
            maybeSpecialAbilityJuice(lastEv)
            if (lastEv.contains("Game Over", ignoreCase = true)) {
                btnReset.visibility = View.VISIBLE
                handlePartyWipeUi()
            }
            maybeAnimateFromEvent(lastEv)
            // NPC/enemy AI attacks: show dice when we didn't already schedule from a player tap
            if (looksLikeCombatRoll(lastEv)) {
                maybeShowDiceOverlay(lastEv)
            }
        }

        if (isGameOver) {
            btnReset.visibility = View.VISIBLE
            btnAttack.isEnabled = false
            btnSpecial.isEnabled = false
            btnHeal.isEnabled = false
            btnRest.isEnabled = false
            btnInteract.isEnabled = false
            handlePartyWipeUi()
        }

        refreshHudXp()
        maybeNotifyLevelUps()
        refreshBattleBackground()
        refreshBattleArena()
        if (!isGameOver) maybeSoloDmCoach(status, isShop)
    }

    private data class BattleUnit(
        val isPlayer: Boolean,
        val name: String,
        val classId: Int,
        val hp: Int,
        val maxHp: Int,
        /** Stable enemy ID from native (Bestiary.h); empty for heroes. */
        val enemyId: String = ""
    )

    private fun parseBattleRoster(raw: String): Pair<List<BattleUnit>, List<BattleUnit>> {
        val party = mutableListOf<BattleUnit>()
        val foes = mutableListOf<BattleUnit>()
        if (raw.isBlank()) return party to foes
        val parts = raw.split('|', limit = 2)
        fun parseSide(side: String, players: Boolean) {
            if (side.isBlank()) return
            for (entry in side.split(';')) {
                if (entry.isBlank()) continue
                val bits = entry.split(',')
                if (bits.size < 5) continue
                val list = if (players) party else foes
                list += BattleUnit(
                    isPlayer = players,
                    name = bits[1],
                    classId = bits[2].toIntOrNull() ?: 0,
                    hp = bits[3].toIntOrNull() ?: 0,
                    maxHp = bits[4].toIntOrNull() ?: 1,
                    enemyId = bits.getOrNull(5).orEmpty().trim()
                )
            }
        }
        parseSide(parts.getOrNull(0).orEmpty(), true)
        parseSide(parts.getOrNull(1).orEmpty(), false)
        return party to foes
    }

    // Sprite tables live in SpriteTables.kt (Code Auditor L10: MainActivity size). These thin
    // wrappers bind them to the current Reduce-flashing setting.
    private fun isAshEmberSprite(res: Int): Boolean = SpriteTables.isAshEmberSprite(res)

    private fun spriteForEnemyId(enemyId: String): Int =
        SpriteTables.spriteForEnemyId(enemyId, reduceFlashEnabled)

    private fun verifyEnemySpriteTable() {
        SpriteTables.tableProblem()?.let { Log.e("AshEmber", "Enemy sprite table broken: $it") }
    }

    private fun spriteFor(unit: BattleUnit): Int =
        if (!unit.isPlayer) spriteForEnemyId(unit.enemyId) else SpriteTables.heroSprite(unit.classId)

    private fun attackSpriteFor(idleRes: Int): Int = SpriteTables.attackSprite(idleRes, reduceFlashEnabled)

    private val heroSpriteRes: Set<Int> get() = SpriteTables.heroSpriteRes

    /** Sharper 1080-wide stage / menu art on tablets and dense phones (centre-cropped either way). */
    private fun useHdBackgrounds(): Boolean {
        val dm = resources.displayMetrics
        return resources.configuration.smallestScreenWidthDp >= 600 || dm.widthPixels > 900
    }

    /** Stage art by play mode + story beat — see [StageMap.stageForContext]. */
    internal fun stageForContext(mode: Int, beat: Int, merchant: Boolean, rosterIds: List<String>): String =
        StageMap.stageForContext(mode, beat, merchant, rosterIds)

    private fun stageDrawable(stage: String, hd: Boolean): Int = when (stage) {
        "ember_ring" -> if (hd) R.drawable.bg_stage_ember_ring_hd else R.drawable.bg_stage_ember_ring
        "cinder_crucible" -> if (hd) R.drawable.bg_stage_cinder_crucible_hd else R.drawable.bg_stage_cinder_crucible
        "ashen_deep" -> if (hd) R.drawable.bg_stage_ashen_deep_hd else R.drawable.bg_stage_ashen_deep
        "raid_corrans_rest" -> if (hd) R.drawable.bg_stage_raid_corrans_rest_hd else R.drawable.bg_stage_raid_corrans_rest
        "raid_kilnmouth" -> if (hd) R.drawable.bg_stage_raid_kilnmouth_hd else R.drawable.bg_stage_raid_kilnmouth
        "raid_weftholme" -> if (hd) R.drawable.bg_stage_raid_weftholme_hd else R.drawable.bg_stage_raid_weftholme
        "kindling_pyre" -> if (hd) R.drawable.bg_stage_kindling_pyre_hd else R.drawable.bg_stage_kindling_pyre
        "thornpath_woods" -> if (hd) R.drawable.bg_stage_thornpath_woods_hd else R.drawable.bg_stage_thornpath_woods
        "ossuary" -> if (hd) R.drawable.bg_stage_ossuary_hd else R.drawable.bg_stage_ossuary
        "millhollow_green" -> if (hd) R.drawable.bg_stage_millhollow_green_hd else R.drawable.bg_stage_millhollow_green
        "mill_weir" -> if (hd) R.drawable.bg_stage_mill_weir_hd else R.drawable.bg_stage_mill_weir
        "mill_hall" -> if (hd) R.drawable.bg_stage_mill_hall_hd else R.drawable.bg_stage_mill_hall
        "emberdeep" -> if (hd) R.drawable.bg_stage_emberdeep_hd else R.drawable.bg_stage_emberdeep
        "greyfen_marsh" -> if (hd) R.drawable.bg_stage_greyfen_marsh_hd else R.drawable.bg_stage_greyfen_marsh
        else -> if (hd) R.drawable.bg_stage_catacombs_hd else R.drawable.bg_stage_catacombs
    }

    private fun refreshBattleBackground() {
        val img = battleStageImage ?: return
        val mode = try { getSoloPlayMode() } catch (_: Exception) { 0 }
        val beat = try { getQuestBeat() } catch (_: Exception) { 0 }
        val merchant = try { isMerchantRoom() } catch (_: Exception) { false }
        val roster = try { getBattleRoster() } catch (_: Exception) { "" }
        val ids = parseBattleRoster(roster).second.map { it.enemyId }
        val stage = stageForContext(mode, beat, merchant, ids)
        val boss = ids.any { it.startsWith("boss_") }
        val hd = useHdBackgrounds()
        val key = "$stage|$hd|$boss"
        if (key == lastBattleBgKey) return
        lastBattleBgKey = key
        img.setImageResource(stageDrawable(stage, hd))
        battleBossOverlay?.visibility = if (boss) View.VISIBLE else View.GONE
    }

    private fun refreshBattleArena() {
        val roster = getBattleRoster()
        if (roster == lastBattleRoster) {
            applySelectionHighlights()
            return
        }
        val oldNames = lastBattleRoster
        lastBattleRoster = roster
        val (party, foes) = parseBattleRoster(roster)
        // Structural change (names / count) clears selection; HP-only updates keep name selection.
        val oldPartyNames = oldNames.substringBefore('|').split(';').mapNotNull { it.split(',').getOrNull(1) }
        val oldFoeNames = oldNames.substringAfter('|', "").split(';').mapNotNull { it.split(',').getOrNull(1) }
        val newPartyNames = party.map { it.name }
        val newFoeNames = foes.map { it.name }
        if (oldNames.isNotEmpty() && (oldPartyNames != newPartyNames || oldFoeNames != newFoeNames)) {
            clearCombatSelection("roster membership changed")
        } else {
            // Drop selection if the named unit is now dead / gone
            val eIdx = selectedEnemyIndex
            if (eIdx != null && (eIdx !in foes.indices || foes[eIdx].hp <= 0)) {
                selectedEnemyIndex = null
            }
            val aIdx = selectedAllyIndex
            if (aIdx != null && aIdx !in party.indices) {
                selectedAllyIndex = null
            }
        }
        // Ash-dissolve: foes that vanished from the roster (native erases the dead) dissolve
        // into ash where they stood. Only when the new roster is a subset of the old one, so a
        // fresh room / wave never triggers it.
        if (oldNames.isNotEmpty()) {
            val oldFoes = parseBattleRoster(oldNames).second
            val pool = foes.map { it.enemyId to it.name }.toMutableList()
            val gone = mutableListOf<Int>()
            oldFoes.forEachIndexed { i, u ->
                val k = u.enemyId to u.name
                if (!pool.remove(k)) gone += i
            }
            if (pool.isEmpty() && gone.isNotEmpty()) {
                for (i in gone) findSpriteByIndex(enemyColumn, i)?.let { playAshDissolve(it) }
            }
        }
        rebuildColumn(partyColumn, party, alignEnd = false, isEnemyColumn = false)
        rebuildColumn(enemyColumn, foes, alignEnd = true, isEnemyColumn = true)
        applySelectionHighlights()
        if (foeTargetingMode) startTargetPulse(enemyColumn)
        if (allyTargetingMode) startTargetPulse(partyColumn)
    }

    private fun rebuildColumn(
        column: LinearLayout,
        units: List<BattleUnit>,
        alignEnd: Boolean,
        isEnemyColumn: Boolean
    ) {
        for (i in 0 until column.childCount) {
            val ring = column.getChildAt(i).findViewWithTag<ImageView?>("ring")
            (ring?.getTag(R.id.battleArena) as? android.animation.Animator)?.cancel()
        }
        column.removeAllViews()
        val density = resources.displayMetrics.density
        val spriteSize = (112 * density).toInt()
        val barW = (96 * density).toInt()
        val cinzel = androidx.core.content.res.ResourcesCompat.getFont(this, R.font.cinzel)
        units.forEachIndexed { index, unit ->
            val wrap = LinearLayout(this).apply {
                orientation = LinearLayout.VERTICAL
                gravity = if (alignEnd) Gravity.END else Gravity.START
                setPadding(4, 4, 4, 4)
                tag = index // unique slot (duplicate names allowed)
                clipChildren = false
                clipToPadding = false
            }
            val idleRes = spriteFor(unit)
            // Sprite slot: static backlight (so dark silhouettes read on dark floors), a ring under
            // the feet (teal target / amber selected hero; hidden until selected), then the sprite.
            val slot = FrameLayout(this).apply {
                layoutParams = LinearLayout.LayoutParams(spriteSize, spriteSize)
                clipChildren = false
                clipToPadding = false
                tag = "slot"
            }
            val backlight = View(this).apply {
                background = getDrawable(
                    if (isEnemyColumn) R.drawable.bg_sprite_backlight_enemy else R.drawable.bg_sprite_backlight_hero
                )
                layoutParams = FrameLayout.LayoutParams(spriteSize, spriteSize)
                importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO
                alpha = if (unit.hp <= 0) 0.35f else 1f
            }
            val ring = ImageView(this).apply {
                setImageResource(if (isEnemyColumn) R.drawable.ic_target_ring else R.drawable.ic_select_ring)
                scaleType = ImageView.ScaleType.FIT_CENTER
                layoutParams = FrameLayout.LayoutParams(
                    (spriteSize * 0.92f).toInt(), (spriteSize * 0.30f).toInt(),
                    Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL
                ).apply { bottomMargin = -(spriteSize * 0.06f).toInt() }
                visibility = View.INVISIBLE
                tag = "ring"
                importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO
            }
            val img = ImageView(this).apply {
                setImageResource(idleRes)
                setTag(R.id.sheetPortrait, idleRes)
                layoutParams = FrameLayout.LayoutParams(spriteSize, spriteSize)
                adjustViewBounds = true
                scaleType = ImageView.ScaleType.FIT_CENTER
                setPadding(4, 4, 4, 4)
                tag = "sprite"
                // Heroes face right (never mirrored); foes on the right face left toward the party.
                // Ash & Ember vectors are authored facing their side already — don't mirror them.
                scaleX = if (isEnemyColumn && !isAshEmberSprite(idleRes)) -1f else 1f
                alpha = if (unit.hp <= 0) 0.35f else 1f
            }.also { img ->
                if (unit.hp > 0) startIdleBob(img)
            }
            slot.addView(backlight)
            slot.addView(ring)
            slot.addView(img)
            val dupeCount = units.count { it.name == unit.name }
            val displayName = if (dupeCount > 1) "${unit.name} #${index + 1}" else unit.name
            val name = TextView(this).apply {
                text = displayName
                setTextColor(getColor(R.color.ae_bone))
                textSize = 11f
                typeface = cinzel
                letterSpacing = 0.06f
                isAllCaps = true
                setShadowLayer(2f * density, 0f, 1f * density, getColor(R.color.ae_void))
                gravity = if (alignEnd) Gravity.END else Gravity.START
                // Longer bestiary names wrap to 2 lines within the sprite width so the
                // duplicate-slot "#N" suffix (#71) stays visible.
                maxLines = 2
                ellipsize = android.text.TextUtils.TruncateAt.END
                layoutParams = LinearLayout.LayoutParams(spriteSize, LinearLayout.LayoutParams.WRAP_CONTENT)
            }
            val bar: View = if (isEnemyColumn) {
                // Enemy HP: thin teal soul thread.
                ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal).apply {
                    max = unit.maxHp.coerceAtLeast(1)
                    progress = unit.hp.coerceIn(0, max)
                    progressDrawable = getDrawable(R.drawable.bg_hp_bar_enemy)?.mutate()
                    layoutParams = LinearLayout.LayoutParams(barW, (5 * density).toInt()).apply {
                        topMargin = (5 * density).toInt()
                        bottomMargin = (3 * density).toInt()
                    }
                }
            } else {
                // Hero HP: the burning wick.
                WickBarView(this).apply {
                    setHp(unit.hp, unit.maxHp)
                    layoutParams = LinearLayout.LayoutParams(barW, (18 * density).toInt()).apply {
                        topMargin = (1 * density).toInt()
                    }
                }
            }
            val hp = TextView(this).apply {
                text = "${unit.hp}/${unit.maxHp}"
                setTextColor(getColor(R.color.ae_fog))
                textSize = 12f
                setShadowLayer(1.5f * density, 0f, 1f * density, getColor(R.color.ae_void))
                gravity = if (alignEnd) Gravity.END else Gravity.START
            }
            // Child order kept: 0 = sprite slot, 1 = name (findSpriteByName), 2 = bar, 3 = hp.
            wrap.addView(slot)
            wrap.addView(name)
            wrap.addView(bar)
            wrap.addView(hp)
            val selectable = if (isEnemyColumn) unit.hp > 0 else true
            if (selectable) {
                wrap.isClickable = true
                wrap.isFocusable = true
                wrap.setOnClickListener {
                    onBattleUnitTapped(unit.name, index, isEnemyColumn)
                }
            } else {
                wrap.isClickable = false
                wrap.setOnClickListener(null)
            }
            column.addView(wrap)
        }
    }

    private fun onBattleUnitTapped(name: String, index: Int, isEnemy: Boolean) {
        if (isEnemy) {
            if (selectedEnemyIndex == index) {
                selectedEnemyIndex = null // retap clears
            } else {
                selectedEnemyIndex = index
            }
            foeTargetingMode = false
            stopTargetPulse()
            applySelectionHighlights()
            val dupe = livingUnits(true).count { it.second.name == name } > 1
            val label = if (dupe) "$name #${index + 1}" else name
            Toast.makeText(this, "Target: $label", Toast.LENGTH_SHORT).show()
        } else {
            if (allyTargetingMode) {
                if (selectedAllyIndex == index) {
                    selectedAllyIndex = null
                } else {
                    selectedAllyIndex = index
                }
                allyTargetingMode = false
                stopTargetPulse()
                applySelectionHighlights()
                Toast.makeText(this, "Ally: $name", Toast.LENGTH_SHORT).show()
            } else if (name.equals(localPlayerName, ignoreCase = true)) {
                showCharacterSheet(localPlayerName)
            } else if (name.equals(companionNameSafe(), ignoreCase = true) || name.contains("(NPC)")) {
                showCompanionSheet()
            } else {
                // Online party member — open read-only sheet
                showCharacterSheet(name)
            }
        }
    }

    private fun applySelectionHighlights() {
        // Selected foe: teal target ring (gentle 0.6 -> 1 alpha pulse over 900 ms; static with
        // Reduce flashing). Selected hero: static amber ring. No box highlight any more.
        fun paint(column: LinearLayout, selectedIndex: Int?, pulse: Boolean) {
            for (i in 0 until column.childCount) {
                val child = column.getChildAt(i)
                child.background = null
                val slot = child.tag as? Int
                val selectedHere = selectedIndex != null && slot == selectedIndex
                val ring = child.findViewWithTag<ImageView?>("ring") ?: continue
                if (selectedHere) {
                    if (ring.visibility != View.VISIBLE) {
                        ring.visibility = View.VISIBLE
                        ring.animate().cancel()
                        ring.alpha = 1f
                        if (pulse && !reduceFlashEnabled) {
                            ring.alpha = 0.6f
                            val anim = android.animation.ObjectAnimator.ofFloat(ring, "alpha", 0.6f, 1f).apply {
                                duration = 900
                                repeatMode = android.animation.ValueAnimator.REVERSE
                                repeatCount = android.animation.ValueAnimator.INFINITE
                            }
                            ring.setTag(R.id.battleArena, anim)
                            anim.start()
                        }
                    } else if (reduceFlashEnabled) {
                        (ring.getTag(R.id.battleArena) as? android.animation.Animator)?.cancel()
                        ring.alpha = 1f
                    }
                } else {
                    (ring.getTag(R.id.battleArena) as? android.animation.Animator)?.cancel()
                    ring.setTag(R.id.battleArena, null)
                    ring.visibility = View.INVISIBLE
                    ring.alpha = 1f
                }
            }
        }
        paint(partyColumn, selectedAllyIndex, pulse = false)
        paint(enemyColumn, selectedEnemyIndex, pulse = true)
    }

    private fun startTargetPulse(column: LinearLayout) {
        stopTargetPulse()
        // Photosensitive mode: keep static selection glow only (no rapid alpha blink).
        if (reduceFlashEnabled) return
        for (i in 0 until column.childCount) {
            val child = column.getChildAt(i)
            val img = child.findViewWithTag<ImageView?>("sprite") ?: continue
            if (img.alpha < 0.5f) continue
            val anim = AlphaAnimation(0.40f, 1f).apply {
                duration = 480
                repeatMode = Animation.REVERSE
                repeatCount = Animation.INFINITE
            }
            img.startAnimation(anim)
        }
    }

    private fun stopTargetPulse() {
        fun clear(column: LinearLayout) {
            for (i in 0 until column.childCount) {
                val img = column.getChildAt(i).findViewWithTag<ImageView?>("sprite")
                img?.clearAnimation()
            }
        }
        clear(partyColumn)
        clear(enemyColumn)
    }

    private fun findSpriteByName(column: LinearLayout, name: String): ImageView? {
        // Legacy name lookup (animations / chat). Prefer findSpriteByIndex for targeting.
        for (i in 0 until column.childCount) {
            val child = column.getChildAt(i)
            val nameTv = (child as? LinearLayout)?.getChildAt(1) as? TextView
            val label = nameTv?.text?.toString().orEmpty()
            if (label.equals(name, ignoreCase = true) || label.contains(name, ignoreCase = true) ||
                name.contains(label, ignoreCase = true)
            ) {
                return child.findViewWithTag("sprite") as? ImageView
            }
        }
        return null
    }

    private fun findSpriteByIndex(column: LinearLayout, index: Int): ImageView? {
        for (i in 0 until column.childCount) {
            val child = column.getChildAt(i)
            if (child.tag as? Int == index) {
                return child.findViewWithTag("sprite") as? ImageView
            }
        }
        return column.getChildAt(index)?.findViewWithTag("sprite") as? ImageView
    }

    private fun firstSprite(column: LinearLayout): ImageView? {
        if (column.childCount == 0) return null
        return column.getChildAt(0).findViewWithTag("sprite") as? ImageView
    }

    
    /**
     * Gentle idle bob (all sprites) plus a slow sway for Ash & Ember silhouettes, pivoting at
     * the feet. Skipped when reduce-flash is on (photosensitive mode keeps sprites still).
     */
    private fun startIdleBob(img: ImageView) {
        if (reduceFlashEnabled) return
        img.animate().cancel()
        img.translationY = 0f
        val res = (img.getTag(R.id.sheetPortrait) as? Int) ?: 0
        val sway = if (isAshEmberSprite(res)) 1.6f else 0f
        if (sway != 0f) {
            img.pivotX = img.width / 2f
            img.pivotY = img.height * 0.94f
        }
        img.animate()
            .translationY(-5f)
            .rotation(-sway)
            .setDuration(if (sway != 0f) 900L else 700L)
            .setInterpolator(AccelerateDecelerateInterpolator())
            .withEndAction {
                img.animate()
                    .translationY(0f)
                    .rotation(sway)
                    .setDuration(if (sway != 0f) 900L else 700L)
                    .setInterpolator(AccelerateDecelerateInterpolator())
                    .withEndAction { startIdleBob(img) }
                    .start()
            }.start()
    }

    /**
     * Ash-dissolve on death: a ghost copy of the sprite greys to ash, sinks/thins and fades
     * while ash flecks (and a few embers) drift up. Reduce-flash: a plain slow fade, no flecks.
     */
    private fun playAshDissolve(sprite: ImageView) {
        val host = battleFxOverlay ?: return
        val d = sprite.drawable?.constantState?.newDrawable()?.mutate() ?: return
        if (sprite.width <= 0 || sprite.height <= 0) return
        val density = resources.displayMetrics.density
        val locHost = IntArray(2)
        val locSprite = IntArray(2)
        host.getLocationOnScreen(locHost)
        sprite.getLocationOnScreen(locSprite)
        val x = (locSprite[0] - locHost[0]).toFloat()
        val y = (locSprite[1] - locHost[1]).toFloat()
        val ghost = ImageView(this).apply {
            setImageDrawable(d)
            scaleType = ImageView.ScaleType.FIT_CENTER
            setPadding(sprite.paddingLeft, sprite.paddingTop, sprite.paddingRight, sprite.paddingBottom)
            layoutParams = FrameLayout.LayoutParams(sprite.width, sprite.height)
            translationX = x
            translationY = y
            scaleX = if (sprite.scaleX < 0f) -1f else 1f
            pivotY = sprite.height * 0.94f
            isClickable = false
            isFocusable = false
        }
        host.addView(ghost)
        if (reduceFlashEnabled) {
            ghost.animate().alpha(0f).setDuration(700L)
                .withEndAction { host.removeView(ghost) }.start()
            return
        }
        ghost.setColorFilter(withAlpha(R.color.ae_ash, 0xB0), android.graphics.PorterDuff.Mode.SRC_ATOP)
        ghost.animate()
            .alpha(0f)
            .translationY(y + 10f * density)
            .scaleY(0.82f)
            .setStartDelay(120L)
            .setDuration(820L)
            .setInterpolator(AccelerateDecelerateInterpolator())
            .withEndAction { host.removeView(ghost) }
            .start()
        val flecks = 14
        val rnd = java.util.Random()
        for (i in 0 until flecks) {
            val size = ((2 + rnd.nextInt(3)) * density).toInt()
            val ember = i % 5 == 0
            val fleck = View(this).apply {
                layoutParams = FrameLayout.LayoutParams(size, size)
                setBackgroundColor(
                    getColor(
                        when {
                            ember -> R.color.ae_amber
                            i % 2 == 0 -> R.color.ae_bone
                            else -> R.color.ae_fog
                        }
                    )
                )
                translationX = x + sprite.width * (0.25f + rnd.nextFloat() * 0.5f)
                translationY = y + sprite.height * (0.30f + rnd.nextFloat() * 0.55f)
                alpha = 0.9f
            }
            host.addView(fleck)
            fleck.animate()
                .translationXBy((rnd.nextFloat() - 0.6f) * 40f * density)
                .translationYBy(-(30f + rnd.nextFloat() * 60f) * density)
                .alpha(0f)
                .setStartDelay((rnd.nextInt(220)).toLong())
                .setDuration(900L + rnd.nextInt(400))
                .withEndAction { host.removeView(fleck) }
                .start()
        }
    }

    private fun animateAttack(
        attackerIsPlayer: Boolean,
        targetEnemyIndex: Int? = null,
        targetAllyIndex: Int? = null,
        attackerName: String? = null,
        critical: Boolean = false,
        damageAmount: Int? = null,
        missed: Boolean = false
    ) {
        val attackerCol = if (attackerIsPlayer) partyColumn else enemyColumn
        val defenderCol = if (attackerIsPlayer) enemyColumn else partyColumn
        val a = when {
            !attackerName.isNullOrBlank() -> findSpriteByName(attackerCol, attackerName)
                ?: firstSprite(attackerCol)
            else -> firstSprite(attackerCol)
        } ?: return
        val defendView = when {
            attackerIsPlayer && targetEnemyIndex != null && targetEnemyIndex < enemyColumn.childCount ->
                enemyColumn.getChildAt(targetEnemyIndex).findViewWithTag<ImageView?>("sprite")
            !attackerIsPlayer && targetAllyIndex != null && targetAllyIndex < partyColumn.childCount ->
                partyColumn.getChildAt(targetAllyIndex).findViewWithTag<ImageView?>("sprite")
            selectedEnemyIndex != null && attackerIsPlayer ->
                findSpriteByIndex(enemyColumn, selectedEnemyIndex!!)
            selectedAllyIndex != null && !attackerIsPlayer ->
                findSpriteByIndex(partyColumn, selectedAllyIndex!!)
            else -> firstSprite(defenderCol)
        }
        val facing = if (a.scaleX < 0f) -1f else 1f
        val dx = (if (attackerIsPlayer) 128f else -128f)
        // First-pass real animation using existing LuizMelo idle/attack frames:
        // wind-up (idle) → attack frame + lunge → recover to idle. Reduce-flash shortens motion.
        val idleRes = (a.getTag(R.id.sheetPortrait) as? Int) ?: 0
        val windMs = if (reduceFlashEnabled) 50L else 70L
        val lungeMs = if (reduceFlashEnabled) 100L else 130L
        val recoverMs = if (reduceFlashEnabled) 140L else 180L
        a.animate().cancel()
        a.translationX = 0f
        fun playImpact() {
            defendView?.let { d ->
                flashHit(d, critical)
                val baseSx = d.scaleX.let { if (it == 0f) facing else it.coerceIn(-2f, 2f) }
                val sign = if (baseSx < 0f) -1f else 1f
                val impact = if (critical) 1.48f else 1.26f
                d.animate().cancel()
                d.animate()
                    .scaleX(sign * impact).scaleY(impact)
                    .setDuration(if (critical) 120 else 90)
                    .withEndAction {
                        d.animate()
                            .scaleX(sign).scaleY(1f)
                            .setDuration(160)
                            .start()
                    }.start()
                if (damageAmount != null && damageAmount > 0) {
                    spawnFloatingDamage(d, damageAmount, critical)
                    flashScreen(critical)
                    shakeBattleArena(critical)
                } else if (critical) {
                    flashScreen(true)
                    shakeBattleArena(true)
                } else if (missed) {
                    spawnFloatingText(d, "miss", FloatKind.MISS)
                }
            } ?: run {
                if (critical) {
                    flashScreen(true)
                    shakeBattleArena(true)
                }
            }
        }
        // Wind-up pull-back on idle, then attack frame + lunge, then recover.
        a.animate()
            .translationX(-dx * 0.18f)
            .setDuration(windMs)
            .setInterpolator(AccelerateDecelerateInterpolator())
            .withEndAction {
                if (idleRes != 0) a.setImageResource(attackSpriteFor(idleRes))
                // Foes (face left, no attack frame) lean -6°; heroes (face right) lean +6° into the
                // swing. Reduce flashing: no lean.
                val lean = when {
                    reduceFlashEnabled -> 0f
                    idleRes in heroSpriteRes -> 6f
                    isAshEmberSprite(idleRes) -> -6f
                    else -> 0f
                }
                a.animate()
                    .rotation(lean)
                    .translationX(dx)
                    .setDuration(lungeMs)
                    .setInterpolator(AccelerateDecelerateInterpolator())
                    .withEndAction {
                        playImpact()
                        a.animate()
                            .translationX(0f)
                            .rotation(0f)
                            .setDuration(recoverMs)
                            .withEndAction {
                                if (idleRes != 0) a.setImageResource(idleRes)
                            }.start()
                    }.start()
            }.start()
    }


    private fun flashHit(target: ImageView, critical: Boolean) {
        if (reduceFlashEnabled) {
            // Gentle brief tint — no hard white/red blink.
            val soft = if (critical) withAlpha(R.color.ae_amber, 0x66) else withAlpha(R.color.ae_ember, 0x55)
            target.setColorFilter(soft, android.graphics.PorterDuff.Mode.SRC_ATOP)
            target.animate().cancel()
            handler.postDelayed({ target.clearColorFilter() }, 280L)
            return
        }
        val flash = if (critical) getColor(R.color.ae_amber_core) else getColor(R.color.ae_ember)
        target.setColorFilter(flash, android.graphics.PorterDuff.Mode.SRC_ATOP)
        target.animate().cancel()
        handler.postDelayed({
            target.clearColorFilter()
        }, if (critical) 220L else 140L)
    }

    private fun flashScreen(critical: Boolean) {
        // Full-screen white/red/gold pulses are the highest photosensitivity risk.
        if (reduceFlashEnabled) return
        val overlay = combatFlashOverlay ?: return
        val color = if (critical) withAlpha(R.color.ae_amber_core, 0xBB) else withAlpha(R.color.ae_ember, 0x77)
        overlay.setBackgroundColor(color)
        overlay.visibility = View.VISIBLE
        overlay.alpha = if (critical) 0.9f else 0.6f
        overlay.animate().cancel()
        overlay.animate()
            .alpha(0f)
            .setDuration(if (critical) 320L else 200L)
            .withEndAction {
                overlay.visibility = View.GONE
                overlay.alpha = 1f
            }
            .start()
    }

    /** Floating damage number over defender — does not intercept taps. */
    private enum class FloatKind { HIT, CRIT, HEAL, MISS }

    private fun spawnFloatingDamage(anchor: View, amount: Int, critical: Boolean) =
        spawnFloatingText(anchor, if (critical) "CRIT -$amount" else "-$amount",
            if (critical) FloatKind.CRIT else FloatKind.HIT)

    /**
     * Combat floats (Phase 3 kit): hit = bone Cinzel, crit = Amber Core Cinzel with a 2dp ember
     * underline, heal = lichen, miss = fog italic. Never intercepts taps.
     */
    private fun spawnFloatingText(anchor: View, label: String, kind: FloatKind) {
        val critical = kind == FloatKind.CRIT
        val host = battleFxOverlay ?: return
        val density = resources.displayMetrics.density
        val locHost = IntArray(2)
        val locAnchor = IntArray(2)
        host.getLocationOnScreen(locHost)
        anchor.getLocationOnScreen(locAnchor)
        val tv = TextView(this).apply {
            text = label
            setTextColor(
                getColor(
                    when (kind) {
                        FloatKind.CRIT -> R.color.ae_amber_core
                        FloatKind.HEAL -> R.color.ae_lichen
                        FloatKind.MISS -> R.color.ae_fog
                        FloatKind.HIT -> R.color.ae_bone
                    }
                )
            )
            textSize = when (kind) { FloatKind.CRIT -> 24f; FloatKind.MISS -> 17f; else -> 18f }
            typeface = if (kind == FloatKind.MISS) {
                android.graphics.Typeface.create(
                    androidx.core.content.res.ResourcesCompat.getFont(this@MainActivity, R.font.cormorant),
                    android.graphics.Typeface.ITALIC
                )
            } else {
                androidx.core.content.res.ResourcesCompat.getFont(this@MainActivity, R.font.cinzel_title)
            }
            setShadowLayer(5f * density, 0f, 1f * density, getColor(R.color.ae_void))
            background = getDrawable(
                if (critical) R.drawable.bg_damage_float_crit else R.drawable.bg_damage_float
            )
            importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_NO
            isClickable = false
            isFocusable = false
        }
        val lp = FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.WRAP_CONTENT,
            FrameLayout.LayoutParams.WRAP_CONTENT
        )
        host.addView(tv, lp)
        val maxX = (host.width - 56).coerceAtLeast(0).toFloat()
        tv.x = (locAnchor[0] - locHost[0] + anchor.width / 5f).coerceIn(0f, maxX)
        tv.y = (locAnchor[1] - locHost[1]).toFloat().coerceAtLeast(0f)
        tv.alpha = 1f
        tv.scaleX = if (critical) 1.2f else 1f
        tv.scaleY = tv.scaleX
        tv.animate()
            .translationY(if (critical) -88f * density else -68f * density)
            .alpha(0f)
            .scaleX(if (critical) 1.45f else 1.12f)
            .scaleY(if (critical) 1.45f else 1.12f)
            .setDuration(if (critical) 980L else 720L)
            .withEndAction {
                host.removeView(tv)
            }
            .start()
    }

    private fun maybeAnimateFromEvent(event: String) {
        if (event == lastAnimEvent) return
        lastAnimEvent = event
        val e = event.lowercase()
        // Heals ("… drinks … for N HP." / "… Healing Word on X for N HP."): lichen "+N" float.
        Regex("""(?i)(?:drinks|healing word|restores).*?(?:\bon\s+(.+?)\s+)?for\s+(\d+)\s+HP""").find(event)?.let { m ->
            val amount = m.groupValues[2].toIntOrNull() ?: 0
            val who = m.groupValues[1].trim()
            val target = (if (who.isNotBlank()) findSpriteByName(partyColumn, who) else null) ?: firstSprite(partyColumn)
            if (amount > 0 && target != null) spawnFloatingText(target, "+$amount", FloatKind.HEAL)
            return
        }
        when {
            e.contains("attacks") || e.contains("hits") || e.contains("slashes") ||
                e.contains("fireball") || e.contains("sneak") || e.contains("action surge") ||
                e.contains("magic missile") || e.contains("cutting quip") ||
                e.contains("psychic sting") -> {
                val attackMatch = Regex(
                    """^(.+?)\s+(?:attacks|Cutting Quip vs)\s+(.+?)(?:!|\.|$)""",
                    RegexOption.IGNORE_CASE
                ).find(event.trim())
                val attackerName = attackMatch?.groupValues?.getOrNull(1)?.trim().orEmpty()
                // Foe attribution: current roster foe names (bestiary display names from native),
                // plus generic keywords as a fallback for log lines that abbreviate.
                val rosterFoeNames = parseBattleRoster(lastBattleRoster).second
                    .map { it.name.lowercase() }
                    .filter { it.isNotBlank() }
                val foeNames = rosterFoeNames + listOf(
                    "wickthief", "ashbound", "gloamhound", "tallow rat", "millstone brute", "tithe",
                    "crier", "scavenger", "collector", "sexton", "lamplighter", "herald", "drake",
                    "hydra", "nightfang", "crown", "prior", "king"
                )
                val attackerIsFoe = foeNames.any { attackerName.lowercase().contains(it) } ||
                    (attackerName.isBlank() && foeNames.any { e.contains(it) } &&
                        !e.startsWith(localPlayerName.lowercase()))
                val isCrit = e.contains("critical")
                val dmg = Regex("""(?i)(?:Damage|deal(?:s)?|for|Psychic sting)\s+(\d+)""")
                    .find(event)?.groupValues?.getOrNull(1)?.toIntOrNull()
                val landed = (dmg != null && dmg > 0) ||
                    e.contains("hit!") || e.contains("critical") || e.contains("psychic sting")
                animateAttack(
                    attackerIsPlayer = !attackerIsFoe,
                    attackerName = attackerName.ifBlank { null },
                    critical = isCrit,
                    damageAmount = dmg,
                    missed = !landed && e.contains("miss")
                )
                // Shake only when animateAttack had no damageAmount to drive it.
                if (landed && dmg == null) {
                    shakeBattleArena(isCrit)
                }
                if (attackerIsFoe) {
                    gameAudio?.playGrowl()
                    gameAudio?.playAttack()
                } else {
                    gameAudio?.playAttack()
                }
                if (e.contains("hit") || e.contains("damage") || e.contains("critical") ||
                    e.contains("psychic sting")
                ) {
                    gameAudio?.playHit()
                }
            }
            e.contains("damage") || e.contains("struck") || e.contains("wounded") ||
                e.contains("psychic sting") -> {
                gameAudio?.playHit()
                val dmg = Regex("""(?i)(?:Damage|deal(?:s)?|for|Psychic sting)\s+(\d+)""")
                    .find(event)?.groupValues?.getOrNull(1)?.toIntOrNull()
                val isCrit = e.contains("critical")
                if (dmg != null && dmg > 0) {
                    val target = firstSprite(partyColumn) ?: firstSprite(enemyColumn)
                    if (target != null) {
                        spawnFloatingDamage(target, dmg, isCrit)
                        flashScreen(isCrit)
                        flashHit(target, isCrit)
                        shakeBattleArena(isCrit)
                    }
                }
            }
        }
    }

    private enum class AccentKind { HIT, CRIT, KILL, SPECIAL, LEVEL_UP, RARE_LOOT }

    /**
     * Restrained combat juice helpers. Heavy flashes / shakes mute when
     * [reduceFlashEnabled] (pref_reduce_flash) is on — photosensitive-safe.
     */
    private fun shakeBattleArena(critical: Boolean) {
        if (!::battleArena.isInitialized) return
        // Photosensitive: skip shake entirely (vestibular + motion sensitivity).
        if (reduceFlashEnabled) return
        val amp = if (critical) 10f else 5f
        val dur = if (critical) 55L else 40L
        battleArena.animate().cancel()
        battleArena.translationX = 0f
        battleArena.animate()
            .translationX(amp)
            .setDuration(dur)
            .withEndAction {
                battleArena.animate()
                    .translationX(-amp * 0.7f)
                    .setDuration(dur)
                    .withEndAction {
                        battleArena.animate()
                            .translationX(0f)
                            .setDuration(dur + 20L)
                            .start()
                    }
                    .start()
            }
            .start()
    }

    /** Palette colour with an explicit alpha byte (keeps every tint on the ae_* tokens). */
    private fun withAlpha(colorRes: Int, alpha: Int): Int =
        (getColor(colorRes) and 0x00FFFFFF) or ((alpha and 0xFF) shl 24)

    /** Set by maybeSpecialAbilityJuice: true when the special came from a foe (teal), else amber. */
    private var accentFromEnemy = false

    /** One-shot soft UI pulse — never a rapid strobe. Soft tint only when reduce-flash. */
    private fun pulseAccent(kind: AccentKind) {
        val overlay = combatFlashOverlay
        val color = when (kind) {
            AccentKind.KILL -> withAlpha(R.color.ae_amber, 0x66)
            // Gloam teal only for enemy effects; the party's own specials glow amber.
            AccentKind.SPECIAL -> if (accentFromEnemy) withAlpha(R.color.ae_gloam, 0x55) else withAlpha(R.color.ae_amber, 0x55)
            AccentKind.LEVEL_UP -> withAlpha(R.color.ae_amber_core, 0x66)
            AccentKind.RARE_LOOT -> withAlpha(R.color.ae_epic_violet, 0x66)
            AccentKind.CRIT -> withAlpha(R.color.ae_amber_core, 0x88)
            AccentKind.HIT -> withAlpha(R.color.ae_ember, 0x55)
        }
        // Light button pulse for specials (always safe — single scale, no opacity strobe).
        if (kind == AccentKind.SPECIAL && ::btnSpecial.isInitialized) {
            btnSpecial.animate().cancel()
            btnSpecial.animate()
                .scaleX(1.08f).scaleY(1.08f)
                .setDuration(90L)
                .withEndAction {
                    btnSpecial.animate().scaleX(1f).scaleY(1f).setDuration(140L).start()
                }
                .start()
        }
        if (kind == AccentKind.LEVEL_UP && ::btnSheet.isInitialized) {
            btnSheet.animate().cancel()
            btnSheet.animate()
                .scaleX(1.06f).scaleY(1.06f)
                .setDuration(100L)
                .withEndAction {
                    btnSheet.animate().scaleX(1f).scaleY(1f).setDuration(160L).start()
                }
                .start()
        }
        // Soft full-screen wash — skipped when reduce-flash (no extra flashes).
        if (reduceFlashEnabled || overlay == null) return
        if (kind == AccentKind.HIT) return // hits already use flashScreen / flashHit
        overlay.setBackgroundColor(color)
        overlay.visibility = View.VISIBLE
        overlay.alpha = when (kind) {
            AccentKind.CRIT -> 0.55f
            AccentKind.KILL, AccentKind.LEVEL_UP, AccentKind.RARE_LOOT -> 0.35f
            AccentKind.SPECIAL -> 0.28f
            else -> 0.25f
        }
        overlay.animate().cancel()
        overlay.animate()
            .alpha(0f)
            .setDuration(if (kind == AccentKind.CRIT) 280L else 220L)
            .withEndAction {
                overlay.visibility = View.GONE
                overlay.alpha = 1f
            }
            .start()
    }

    private fun maybeKillPop(event: String) {
        val felled = Regex("""(?i)^Felled\s+(.+?)!""").find(event.trim())
        val foe = felled?.groupValues?.getOrNull(1)?.trim().orEmpty()
        if (foe.isBlank()) return
        val key = "kill:$foe:${event.hashCode()}"
        if (key == lastKillPopKey) return
        lastKillPopKey = key
        Toast.makeText(this, "Felled $foe", Toast.LENGTH_SHORT).show()
        pulseAccent(AccentKind.KILL)
    }

    private fun maybeRareLootAccent(event: String) {
        val e = event.lowercase()
        val rare = e.contains("[rare]") || e.contains("[epic]") || e.contains("[legendary]") ||
            e.contains("(rare)") || e.contains("(epic)") || e.contains("(legendary)") ||
            (e.contains("legendary") && (e.contains("finds") || e.contains("claims") || e.contains("loot")))
        if (!rare) return
        val key = "loot:${event.take(80)}"
        if (key == lastRareLootKey) return
        lastRareLootKey = key
        val label = when {
            e.contains("legendary") -> "Legendary loot!"
            e.contains("epic") -> "Epic loot!"
            else -> "Rare loot!"
        }
        Toast.makeText(this, label, Toast.LENGTH_SHORT).show()
        pulseAccent(AccentKind.RARE_LOOT)
    }

    private fun maybeSpecialAbilityJuice(event: String) {
        val e = event.lowercase()
        val special = e.contains("cutting quip") || e.contains("magic missile") ||
            e.contains("action surge") || e.contains("sneak attack") ||
            e.contains("healing word")
        if (!special) return
        // Foe-originated special? (log lines start with the actor's name.)
        val rosterFoeNames = parseBattleRoster(lastBattleRoster).second.map { it.name.lowercase() }
        accentFromEnemy = rosterFoeNames.any { it.isNotBlank() && e.startsWith(it) }
        // Combat-log line is already appended; light UI pulse only (no spam toast).
        pulseAccent(AccentKind.SPECIAL)
        accentFromEnemy = false
    }

    private fun wireDiceOverlay() {
        val root = findViewById<View?>(R.id.diceOverlayRoot) ?: return
        diceOverlayRoot = root
        diceTitle = root.findViewById(R.id.diceTitle)
        diceD20 = root.findViewById(R.id.diceD20)
        diceDamage = root.findViewById(R.id.diceDamage)
        diceSummary = root.findViewById(R.id.diceSummary)
        root.visibility = View.GONE
    }

    private fun looksLikeCombatRoll(event: String): Boolean {
        val e = event.lowercase()
        return e.contains("d20=") || e.contains("damage") || e.contains("miss!") ||
            e.contains("magic missile") || e.contains("healing") || e.contains("potion") ||
            e.contains("cutting quip") || e.contains("psychic sting")
    }

    private var lastDiceEvent = ""
    private fun scheduleDiceFromLastEvent() {
        handler.postDelayed({
            val ev = try { getLastEvent() } catch (_: Exception) { "" }
            if (ev.isNotBlank()) maybeShowDiceOverlay(ev, force = true)
        }, 80)
    }

    private fun maybeShowDiceOverlay(event: String, force: Boolean = false) {
        if (!force && event == lastDiceEvent) return
        if (!looksLikeCombatRoll(event)) return
        lastDiceEvent = event
        showDiceOverlay(event)
    }

    private fun showDiceOverlay(event: String) {
        val root = diceOverlayRoot ?: return
        val d20Match = Regex("""d20=(\d+)""").find(event)
        val dmgMatch = Regex("""(?i)(?:Damage|deal(?:s)?|for|Psychic sting)\s+(\d+)""").find(event)
        val miss = event.contains("Miss!", ignoreCase = true)
        val crit = event.contains("CRITICAL", ignoreCase = true)
        diceTitle?.text = when {
            event.contains("Cutting Quip", ignoreCase = true) -> "Cutting Quip"
            event.contains("Magic Missile", ignoreCase = true) -> "Magic Missile"
            event.contains("Potion", ignoreCase = true) || event.contains("Healing", ignoreCase = true) -> "Healing"
            miss -> "Attack — Miss"
            crit -> "Critical Hit!"
            d20Match != null -> "Attack Roll"
            else -> "Combat"
        }
        // Hex die faces: natural 20 = amber stroke + glow, natural 1 = old-blood stroke + rose text.
        val natural = d20Match?.groupValues?.getOrNull(1)?.toIntOrNull()
        diceD20?.setBackgroundResource(
            when (natural) {
                20 -> R.drawable.bg_die_face_crit
                1 -> R.drawable.bg_die_face_fumble
                else -> R.drawable.bg_die_face
            }
        )
        diceD20?.setTextColor(
            getColor(
                when (natural) {
                    20 -> R.color.ae_amber_core
                    1 -> R.color.ae_danger_text
                    else -> R.color.ae_bone
                }
            )
        )
        diceTitle?.setTextColor(getColor(R.color.ae_amber))
        if (d20Match != null) {
            diceD20?.visibility = View.VISIBLE
            diceD20?.text = d20Match.groupValues[1]
        } else if (event.contains("Magic Missile", ignoreCase = true)) {
            diceD20?.visibility = View.VISIBLE
            diceD20?.text = "auto"
        } else if (event.contains("Potion", ignoreCase = true) || event.contains("Healing", ignoreCase = true)) {
            diceD20?.visibility = View.VISIBLE
            diceD20?.text = "♥"
        } else {
            diceD20?.visibility = View.VISIBLE
            diceD20?.text = "—"
        }
        if (dmgMatch != null && !miss) {
            diceDamage?.visibility = View.VISIBLE
            diceDamage?.text = dmgMatch.groupValues[1]
        } else {
            diceDamage?.visibility = View.GONE
        }
        val summary = event.let { if (it.length > 120) it.take(117) + "…" else it }
        diceSummary?.text = summary
        root.isClickable = true
        root.isFocusable = true
        root.visibility = View.VISIBLE
        root.alpha = 0f
        root.scaleX = 0.92f
        root.scaleY = 0.92f
        root.animate().alpha(1f).scaleX(1f).scaleY(1f).setDuration(130).start()
        if (crit) {
            diceTitle?.setTextColor(getColor(R.color.ae_amber_core))
            diceTitle?.animate()?.scaleX(1.2f)?.scaleY(1.2f)?.setDuration(100)
                ?.withEndAction {
                    diceTitle?.animate()?.scaleX(1f)?.scaleY(1f)?.setDuration(100)?.start()
                }?.start()
            flashScreen(true)
        }
        diceHideRunnable?.let { handler.removeCallbacks(it) }
        // Readable on phones: hold longer; tap overlay to dismiss early (combat never stuck).
        val holdMs = if (crit) 3600L else 3000L
        val hide = Runnable {
            root.animate().alpha(0f).setDuration(200).withEndAction {
                root.visibility = View.GONE
                root.setOnClickListener(null)
            }.start()
        }
        diceHideRunnable = hide
        handler.postDelayed(hide, holdMs)
        root.setOnClickListener {
            diceHideRunnable?.let { handler.removeCallbacks(it) }
            hide.run()
        }
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) hideSystemUi()
    }


    /** If the DM never syncs / session is missing, stop waiting and return to start. */
    private fun scheduleJoinTimeout(sid: String, resuming: Boolean) {
        joinTimeoutRunnable?.let { handler.removeCallbacks(it) }
        val timeoutMs = 20_000L
        val r = Runnable {
            if (!isOnlineClient) return@Runnable
            val status = try { getPlayerStatus() } catch (_: Exception) { "" }
            val admitted = status.contains(localPlayerName) && status.contains("| HP:")
            val hasParty = try {
                getBattleRoster().substringBefore('|').isNotBlank()
            } catch (_: Exception) { false }
            if (admitted || hasParty) return@Runnable
            Log.w(TAG, "Join timeout for session $sid (resuming=$resuming)")
            Toast.makeText(
                this,
                "Could not reach the Lanternkeeper / session $sid (timed out). Returning to menu.",
                Toast.LENGTH_LONG
            ).show()
            resetToStartMenu()
        }
        joinTimeoutRunnable = r
        handler.postDelayed(r, timeoutMs)
    }

    private fun currentDifficultyForWipe(): Int {
        return try {
            getDifficulty().coerceIn(0, 3)
        } catch (_: Exception) {
            if (runDifficulty >= 0) runDifficulty else preferredDifficulty.coerceIn(0, 3)
        }
    }

    /** Party fully fallen — show difficulty-correct Continue / Restart. Persist save for Easy–Hard. */
    private fun handlePartyWipeUi() {
        if (!sessionActive || wipeDialogShowing) return
        val d = currentDifficultyForWipe()
        val inRaid = prefs().getBoolean("raid_active", false) ||
            try { getSoloPlayMode() == 2 } catch (_: Exception) { false }
        val inChallenge = prefs().getBoolean("challenge_active", false) ||
            try { getSoloPlayMode() == 3 } catch (_: Exception) { false }
        val inArena = prefs().getBoolean("arena_active", false) ||
            try { getSoloPlayMode() == 4 } catch (_: Exception) { false }
        val inEndless = prefs().getBoolean("endless_active", false) ||
            try { getSoloPlayMode() == 5 } catch (_: Exception) { false }
        if (d == 3) {
            wipeDialogShowing = true
            if (inRaid) {
                // Nightmare raid wipe: do NOT create a blank character — restore pre-raid hero save.
                restorePreRaidSaveOrKeepCurrent("nightmare raid wipe")
                AlertDialog.Builder(this)
                    .setTitle("Game Over — Nightmare Raid")
                    .setMessage(
                        "The party fell in the Boss Raid. Nightmare ends the raid attempt, " +
                        "but your story hero save is restored (no blank character)."
                    )
                    .setCancelable(false)
                    .setPositiveButton("Main menu") { _, _ ->
                        wipeDialogShowing = false
                        runDifficulty = -1
                        sessionActive = false
                        detachOnlineSession()
                        clearRaidSessionFlags()
                        showStartDialog()
                    }
                    .show()
            } else if (inChallenge) {
                restorePreChallengeSaveOrKeepCurrent("nightmare challenge wipe")
                AlertDialog.Builder(this)
                    .setTitle("Game Over — Nightmare Challenge")
                    .setMessage(
                        "The party fell in the Challenge Dungeon. Nightmare ends the attempt, " +
                        "but your story hero save is restored (no blank character)."
                    )
                    .setCancelable(false)
                    .setPositiveButton("Main menu") { _, _ ->
                        wipeDialogShowing = false
                        runDifficulty = -1
                        sessionActive = false
                        detachOnlineSession()
                        clearChallengeSessionFlags()
                        showStartDialog()
                    }
                    .show()
            } else if (inArena) {
                val score = try { getArenaScore() } catch (_: Exception) { 0 }
                val waves = try { getArenaWavesCleared() } catch (_: Exception) { 0 }
                Arena.onRunFinished(prefs(), score, waves)
                restorePreArenaSaveOrKeepCurrent("nightmare arena wipe")
                AlertDialog.Builder(this)
                    .setTitle("Game Over — Nightmare Arena")
                    .setMessage(
                        "The party fell in the Ember Ring. Nightmare ends the attempt, " +
                        "but your story hero save is restored (no blank character)."
                    )
                    .setCancelable(false)
                    .setPositiveButton("Main menu") { _, _ ->
                        wipeDialogShowing = false
                        runDifficulty = -1
                        sessionActive = false
                        detachOnlineSession()
                        clearArenaSessionFlags()
                        showStartDialog()
                    }
                    .show()
            } else if (inEndless) {
                val depth = try { getEndlessBestDepthThisRun() } catch (_: Exception) { 0 }
                EndlessDeep.onRunFinished(prefs(), depth)
                restorePreEndlessSaveOrKeepCurrent("nightmare endless wipe")
                AlertDialog.Builder(this)
                    .setTitle("Game Over — Nightmare Endless Deep")
                    .setMessage(
                        "The party fell in the Ashen Deep. Nightmare ends the attempt, " +
                        "but your story hero save is restored (no blank character)."
                    )
                    .setCancelable(false)
                    .setPositiveButton("Main menu") { _, _ ->
                        wipeDialogShowing = false
                        runDifficulty = -1
                        sessionActive = false
                        detachOnlineSession()
                        clearEndlessSessionFlags()
                        showStartDialog()
                    }
                    .show()
            } else {
                clearSave("nightmare wipe")
                AlertDialog.Builder(this)
                    .setTitle("Game Over — Nightmare")
                    .setMessage("The entire party has fallen. Nightmare difficulty ends the run — stats, gear, and gold are lost.")
                    .setCancelable(false)
                    .setPositiveButton("Main menu") { _, _ ->
                        wipeDialogShowing = false
                        runDifficulty = -1
                        abandonAdventure()
                    }
                    .show()
            }
            return
        }
        try { syncAndSave() } catch (_: Exception) {}
        wipeDialogShowing = true
        val title = "Game Over — ${difficultyLabel(d)}"
        val msg = when {
            (inRaid || inChallenge || inArena || inEndless) && d == 0 ->
                "Easy wipe: Continue revives your story hero (full HP) and ends the focused run without a new character."
            (inRaid || inChallenge || inArena || inEndless) && d == 1 ->
                "Medium wipe: Continue revives your story hero (half HP) and ends the focused run without a new character."
            inRaid || inChallenge || inArena || inEndless ->
                "Hard wipe: Continue revives your story hero (gear/gold stripped to starters) and ends the focused run."
            d == 0 -> "Easy: no loss of stats, gear, or gold. Continue revives the party with full HP one chamber back."
            d == 1 -> "Medium: no loss of stats, gear, or gold. Continue revives with half HP one chamber back."
            else -> "Hard: stats kept, but gold and gear are stripped to starters. Continue revives one chamber back."
        }
        AlertDialog.Builder(this)
            .setTitle(title)
            .setMessage(msg)
            .setCancelable(false)
            .setPositiveButton("Continue") { _, _ ->
                wipeDialogShowing = false
                val ok = try { recoverFromPartyWipe() } catch (e: Exception) {
                    Log.e(TAG, "recoverFromPartyWipe failed", e)
                    false
                }
                if (ok) {
                    btnReset.visibility = View.GONE
                    if (inArena) {
                        val score = try { getArenaScore() } catch (_: Exception) { 0 }
                        val waves = try { getArenaWavesCleared() } catch (_: Exception) { 0 }
                        Arena.onRunFinished(prefs(), score, waves)
                    }
                    if (inEndless) {
                        val depth = try { getEndlessBestDepthThisRun() } catch (_: Exception) { 0 }
                        EndlessDeep.onRunFinished(prefs(), depth)
                    }
                    clearRaidSessionFlags()
                    clearChallengeSessionFlags()
                    clearArenaSessionFlags()
                    clearEndlessSessionFlags()
                    syncAndSave()
                    updateUi()
                    Toast.makeText(this, "The party rises…", Toast.LENGTH_SHORT).show()
                } else {
                    Toast.makeText(this, "Could not Continue — return to menu.", Toast.LENGTH_LONG).show()
                    when {
                        inRaid -> restorePreRaidSaveOrKeepCurrent("recover failed raid")
                        inChallenge -> restorePreChallengeSaveOrKeepCurrent("recover failed challenge")
                        inArena -> restorePreArenaSaveOrKeepCurrent("recover failed arena")
                        inEndless -> restorePreEndlessSaveOrKeepCurrent("recover failed endless")
                        else -> clearSave("recover failed")
                    }
                    resetToStartMenu()
                }
            }
            .setNegativeButton("Main menu") { _, _ ->
                wipeDialogShowing = false
                when {
                    inRaid -> restorePreRaidSaveOrKeepCurrent("wipe quit to menu raid")
                    inChallenge -> restorePreChallengeSaveOrKeepCurrent("wipe quit to menu challenge")
                    inArena -> {
                        val score = try { getArenaScore() } catch (_: Exception) { 0 }
                        val waves = try { getArenaWavesCleared() } catch (_: Exception) { 0 }
                        Arena.onRunFinished(prefs(), score, waves)
                        restorePreArenaSaveOrKeepCurrent("wipe quit to menu arena")
                    }
                    inEndless -> {
                        val depth = try { getEndlessBestDepthThisRun() } catch (_: Exception) { 0 }
                        EndlessDeep.onRunFinished(prefs(), depth)
                        restorePreEndlessSaveOrKeepCurrent("wipe quit to menu endless")
                    }
                    else -> clearSave("wipe quit to menu")
                }
                runDifficulty = -1
                resetToStartMenu()
            }
            .show()
    }

    /** Raw activity prefs (account-wide keys and explicit slot_N_ keys). */
    private fun rawPrefs(): SharedPreferences = getPreferences(MODE_PRIVATE)

    private var slotPrefsCache: SlotPrefs? = null
    /**
     * v2.28: per-slot keys (save_state, hero prefs, snapshots, held keys, pity, bests, story flag,
     * pending daily spoils) resolve to the ACTIVE slot; everything else is account-wide.
     */
    private fun prefs(): SharedPreferences =
        slotPrefsCache ?: rawPrefs().let { raw -> SlotPrefs(raw) { SaveSlots.activeSlot(raw) } }.also { slotPrefsCache = it }

    private fun activeSlot(): Int = SaveSlots.activeSlot(rawPrefs())
    private fun isOnline(): Boolean = isOnlineHost || isOnlineClient

    private fun maybeOfferTutorialThenCoach() {
        maybeShowLegalDisclaimer {
            val seen = prefs().getBoolean("seen_beginner_tutorial", false)
            if (!seen) {
                showTutorial(0) {
                    prefs().edit().putBoolean("seen_beginner_tutorial", true).apply()
                    postCoach(
                        "Keeper: Tutorial done. I'll coach you on your turns. Tap Help anytime. " +
                            BeginnerGuide.specialBlurb(localClassId)
                    )
                }
            } else {
                postCoach("Keeper: Welcome back. Tap Help if you forget what a button does.")
            }
        }
    }

    /** One-time legal / trademark notice before the beginner tutorial (or for returning players who never saw it). */
    private fun maybeShowLegalDisclaimer(onContinue: () -> Unit) {
        if (prefs().getBoolean("seen_legal_disclaimer", false)) {
            onContinue()
            return
        }
        AlertDialog.Builder(this)
            .setTitle("Legal notice")
            .setMessage(
                "Ashen Lantern is an independent tabletop fantasy adventure.\n\n" +
                    "It is not affiliated with, sponsored by, endorsed by, or approved by " +
                    "Wizards of the Coast LLC. Dungeons & Dragons, D&D, and related marks are " +
                    "trademarks of Wizards of the Coast LLC.\n\n" +
                    "Game rules are compatible with 5e SRD concepts only (SRD 5.1, CC-BY 4.0). " +
                    "See Settings → About and ATTRIBUTION.md for full credits."
            )
            .setCancelable(false)
            .setPositiveButton("Continue") { _, _ ->
                prefs().edit().putBoolean("seen_legal_disclaimer", true).apply()
                onContinue()
            }
            .show()
    }

    private fun showHelpMenu() {
        val options = arrayOf(
            "Show beginner tutorial",
            "Action quick reference",
            "What do stats do?",
            "Toggle solo Lanternkeeper tips",
            "Settings…"
        )
        AlertDialog.Builder(this)
            .setTitle("Help")
            .setItems(options) { _, which ->
                when (which) {
                    0 -> showTutorial(0) {}
                    1 -> AlertDialog.Builder(this)
                        .setTitle("Actions")
                        .setMessage(BeginnerGuide.actionReference())
                        .setPositiveButton("Got it", null)
                        .show()
                    2 -> AlertDialog.Builder(this)
                        .setTitle("Stats in this game")
                        .setMessage(BeginnerGuide.statsHelpMessage())
                        .setPositiveButton("Got it", null)
                        .show()
                    3 -> {
                        soloCoachEnabled = !soloCoachEnabled
                        val state = if (soloCoachEnabled) "ON" else "OFF"
                        Toast.makeText(this, "Solo Lanternkeeper tips: $state", Toast.LENGTH_SHORT).show()
                        prefs().edit().putBoolean("solo_coach_enabled", soloCoachEnabled).apply()
                    }
                    4 -> showSettingsOverlay()
                }
            }
            .setNegativeButton("Close", null)
            .show()
    }

    private fun showTutorial(pageIndex: Int, onFinished: () -> Unit) {
        if (pageIndex !in BeginnerGuide.tutorialPages.indices) {
            onFinished()
            return
        }
        val page = BeginnerGuide.tutorialPages[pageIndex]
        val last = pageIndex == BeginnerGuide.tutorialPages.lastIndex
        val builder = AlertDialog.Builder(this)
            .setTitle(page.title)
            .setMessage(page.body)
            .setCancelable(false)
        if (last) {
            builder.setPositiveButton("Let's play") { _, _ -> onFinished() }
            builder.setNeutralButton("Back") { _, _ -> showTutorial(pageIndex - 1, onFinished) }
        } else {
            builder.setPositiveButton("Next") { _, _ -> showTutorial(pageIndex + 1, onFinished) }
            if (pageIndex > 0) {
                builder.setNeutralButton("Back") { _, _ -> showTutorial(pageIndex - 1, onFinished) }
            }
            builder.setNegativeButton("Skip") { _, _ -> onFinished() }
        }
        builder.show()
    }

    private fun postCoach(tip: String) {
        if (!nativeReady || tip.isBlank()) return
        if (tip == lastCoachTip) return
        lastCoachTip = tip
        try {
            sendChatMessage("Keeper", tip)
        } catch (e: Exception) {
            Log.e(TAG, "coach chat failed: ${e.message}")
        }
        // Surface coach tips in the lasting combat feed (not a flicker overwrite)
        appendCombatFeed(tip)
    }

    private fun maybeSoloDmCoach(status: String, isShop: Boolean) {
        if (!soloCoachEnabled || !nativeReady) return
        val turnKey = status.lineSequence().firstOrNull { it.contains("Turn:") } ?: status.take(40)
        if (turnKey == lastCoachTurnKey) return
        lastCoachTurnKey = turnKey
        val special = try { getSpecialName() } catch (_: Exception) { "Special" }
        val tip = BeginnerGuide.turnCoachTip(
            status = status,
            localPlayerName = localPlayerName,
            isShop = isShop,
            specialName = special,
            classId = localClassId
        ) ?: return
        postCoach(tip)
    }

    /**
     * M8: immersive full screen through WindowInsetsControllerCompat (the View.systemUiVisibility
     * flags are deprecated since API 30). Bars come back as transient overlays on a swipe.
     */
    private fun hideSystemUi() {
        WindowCompat.setDecorFitsSystemWindows(window, false)
        WindowCompat.getInsetsController(window, window.decorView).apply {
            systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            hide(WindowInsetsCompat.Type.systemBars())
        }
    }

    /**
     * Android 15/16 edge-to-edge (targetSdk 36 removes the opt-out): the app draws behind the
     * hidden system bars, so keep content clear of display cutouts and of the keyboard (chat
     * input) by padding the content root. The window background (ae_void) fills the margins.
     */
    private fun applyEdgeToEdgeInsets() {
        val root = findViewById<View>(android.R.id.content) ?: return
        ViewCompat.setOnApplyWindowInsetsListener(root) { v, insets ->
            val cut = insets.getInsets(WindowInsetsCompat.Type.displayCutout())
            val ime = insets.getInsets(WindowInsetsCompat.Type.ime())
            v.setPadding(cut.left, cut.top, cut.right, maxOf(cut.bottom, ime.bottom))
            insets
        }
        ViewCompat.requestApplyInsets(root)
    }
}
