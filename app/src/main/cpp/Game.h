#ifndef DND_GAME_H
#define DND_GAME_H

#include "GameLogic.h"
#include "QuestScript.h"
#include <vector>
#include <string>
#include <queue>
#include <deque>
#include <memory>
#include <random>

namespace dnd {

enum class VisualEventType {
    PLAYER_ATTACK,
    PLAYER_DAMAGE,
    ENEMY_ATTACK,
    ENEMY_DAMAGE
};

struct VisualEvent {
    VisualEventType type;
    int targetIndex;
};

class Game {
public:
    Game();
    void startNewGame(CharacterClass selectedClass = CharacterClass::FIGHTER, const std::string& playerName = "Hero",
                      int soloPlayMode = static_cast<int>(SoloPlayMode::STORY),
                      int difficulty = static_cast<int>(Difficulty::EASY),
                      CharacterClass companionClass = CharacterClass::WIZARD,
                      bool companionAutoAi = true);
    /** Convert current party into a Boss Raid encounter — keeps hero/companion/gear/progress. */
    bool beginBossRaidFromCurrent();
    /** After raid clear/wipe-recover: restore prior story/crawl mode without wiping the party. */
    void finishBossRaidKeepParty();
    /**
     * Challenge Dungeon (story-complete endgame): same saved hero as Boss Raid.
     * legendaryChancePercent comes from Kotlin prefs (dynamic formula).
     */
    bool beginChallengeDungeonFromCurrent(int legendaryChancePercent);
    /** After challenge clear/wipe-recover: restore prior mode; party preserved. */
    void finishChallengeDungeonKeepParty();
    /**
     * Ember Ring Arena (story-complete endgame): wave-based scored fights on Continue hero.
     */
    bool beginArenaFromCurrent();
    /** After arena clear/wipe-recover: restore prior mode; party preserved. */
    void finishArenaKeepParty();
    /**
     * Endless Deep / Ashen Deep (story-complete endgame): depth-scaling rooms on Continue hero.
     */
    bool beginEndlessDeepFromCurrent();
    /** After death/retreat/milestone exit: restore prior mode; party preserved. */
    void finishEndlessDeepKeepParty();
    void startDmSession(const std::string& dmName);
    void addAlly(const std::string& name, CharacterClass cl);
    /** Solo NPC companion helpers (host/solo). Empty name if none. */
    std::string getCompanionName() const;
    bool isCompanionAutoAi() const;
    void setCompanionAutoAi(bool autoAi);
    /** Reclass NPC companion when not in combat; returns false if unavailable. */
    bool setCompanionClass(CharacterClass cl);
    static std::string companionNameForClass(CharacterClass cl);
    void dmBeginDungeon();
    void dmAdvanceRoom();
    void dmNarrate(const std::string& line);
    void dmGrantShortRest();
    void prepareClientJoin();
    std::string getDmName() const { return dmName_; }
    bool isDmOnlyTable() const { return dmOnlyTable_; }

    // Player Actions
    void playerAttack(int targetEnemyIndex);
    void playerHeal(int targetPlayerIndex);
    void playerSpecialAction(int targetEnemyIndex);
    void playerInteract(const std::string& playerName);
    void playerRest(bool force = false);
    /** Full recover (HP, resources/specials). Same rest-opportunity gate as Short Rest. */
    void playerLongRest(bool force = false);
    /** Drink a bag potion: apply effect and consume. */
    bool useInventoryPotion(const std::string& playerName, int invIndex);
    void playerIncreaseStat(const std::string& playerName, int statIndex);
    /** Cleared-room advance (Onward) — not merchant Leave. */
    void playerAdvanceFromCleared();

    // Merchant Actions
    bool buyItem(const std::string& playerName, int itemIndex);
    std::string getShopManifest() const;

    // Inventory / gear upgrades
    std::string getInventoryManifest(const std::string& playerName) const;
    bool equipInventoryItem(const std::string& playerName, int invIndex);
    bool unequipSlot(const std::string& playerName, int slot); // 0=weapon, 1=armor
    bool upgradeInventoryItem(const std::string& playerName, int invIndex);
    bool upgradeEquippedItem(const std::string& playerName, int slot); // 0=weapon, 1=armor
    bool sellInventoryItem(const std::string& playerName, int invIndex);
    bool sellEquippedItem(const std::string& playerName, int slot); // 0=weapon, 1=armor; unequips then sells
    /** Move bag/equipped gear to the other party member (hero↔companion). Blocks if no ally. */
    bool transferInventoryItemToAlly(const std::string& fromName, int invIndex);
    bool transferEquippedItemToAlly(const std::string& fromName, int slot); // 0=weapon, 1=armor

    // Turn Management
    void processTurn();
    void spawnRoomContent();
    void generateRoomDescription();
    void rollInitiative();
    void rebuildTurnOrder();

    // Authority (Multiplayer)
    void setAsHost(bool host) { isHost_ = host; }
    bool getIsHost() const { return isHost_; }

    // Social/Chat/Journal
    void addChatMessage(const std::string& sender, const std::string& message);
    std::string getChatHistory() const;
    void addJournalEntry(const std::string& entry);
    std::string getJournal() const;

    void setDifficulty(int d);
    int getDifficulty() const { return difficulty_; }
    int getSoloPlayMode() const { return soloPlayMode_; }
    /** Easy/Medium/Hard party-wipe continue. Nightmare should clear save in UI instead. */
    bool recoverFromPartyWipe();

    // Networking/Persistence
    std::string serialize();
    /**
     * Load a save / host state. Transactional (Code Auditor H1): the data is parsed into a
     * scratch Game first and only adopted when every section parsed; on failure *this is left
     * untouched, false is returned and *error (if given) says why.
     */
    bool deserialize(const std::string& data, std::string* error = nullptr);

    std::string getLastEvent() const { return lastEvent_; }
    std::string getRoomDescription() const { return roomDescription_; }
    std::string getSessionId() const { return sessionId_; }
    std::string getPartyStatus() const;
    std::string getBattleRoster() const;
    std::string getSpecialActionName() const;

    bool isGameOver() const { return gameOver_; }
    bool isMerchantRoom() const { return isMerchantRoom_; }
    bool isInCombat() const { return !isMerchantRoom_ && !enemies_.empty(); }
    /** No living foes, still in a dungeon chamber (Search/Rest/Onward beat). */
    bool isRoomCleared() const {
        return !gameOver_ && !isMerchantRoom_ && !dmOnlyTable_ && roomCount_ >= 1 && enemies_.empty();
    }
    bool hasSearchedRoom() const { return roomSearchUsed_; }
    /** True after Short or Long Rest this chamber (one rest opportunity between encounters). */
    bool hasUsedRoomRest() const { return roomRestUsed_; }
    /** Rooms until Long Rest is available again (0 = ready). Cooldown = 5 rooms after a Long Rest.
     *  Uses longRestProgress_ (not roomCount_) so Arena / Endless / Raid / Challenge / Story
     *  all tick the same 5-room gate even when roomCount_ is biased for loot scaling. */
    int getLongRestCooldownRooms() const {
        if (longRestProgress_ >= longRestNextAvailableRoom_) return 0;
        return longRestNextAvailableRoom_ - longRestProgress_;
    }
    bool canLongRestByCooldown() const { return getLongRestCooldownRooms() == 0; }
    int getRoomCount() const { return roomCount_; }
    int getQuestBeat() const { return questBeat_; }

    // --- The Kindling story choices (v2.25). Values are saved after the E2 header marker. ---
    /** Act 2 beat 13: what happens to Sallow Venn. */
    enum VennChoice : int { VENN_UNDECIDED = 0, VENN_SPARED = 1, VENN_ENDED = 2 };
    /** Act 3 beat 19: what the Ember Seal does to the ash-dead below the well. */
    enum WellChoice : int { WELL_UNDECIDED = 0, WELL_SEALED_IN = 1, WELL_LET_REST = 2 };
    /** Prompt kinds for the UI: 0 none, 1 = Venn (beat 13), 2 = the well (beat 19). */
    enum StoryPrompt : int { PROMPT_NONE = 0, PROMPT_VENN = 1, PROMPT_WELL = 2 };
    int getVennChoice() const { return vennChoice_; }
    int getWellChoice() const { return wellChoice_; }
    /** The choice the UI should ask for right now (solo story, on the beat, not yet decided). */
    int pendingStoryChoice() const;
    /** Apply a choice from the UI. kind = StoryPrompt, option = 1 or 2. False if not pending. */
    bool chooseStoryOption(int kind, int option);
    /** "venn,well" — for the UI / the Memory Ledger PR. */
    std::string getStoryChoices() const;
    /**
     * After a solo Continue: re-read the current scripted beat's room text from QuestScript.h so
     * pre-v2.25 saves stop showing the old Act 2–4 copy. Journal, chat and lastEvent are history
     * and are left alone. Returns true if the description changed.
     */
    bool refreshStoryRoomAfterLoad();
    bool isQuestLanternRecovered() const { return questLanternRecovered_; }
    bool isQuestComplete() const { return questComplete_; }
    /** Solo scripted Act 1–4; false for crawl / raid / Host-as-DM / online. */
    bool isSoloQuestScripted() const {
        return isScriptedSoloBeat(questBeat_);
    }
    bool isSoloCrawl() const {
        return soloPlayMode_ == static_cast<int>(SoloPlayMode::CRAWL);
    }
    bool isBossRaid() const {
        return soloPlayMode_ == static_cast<int>(SoloPlayMode::RAID);
    }
    bool isChallengeDungeon() const {
        return soloPlayMode_ == static_cast<int>(SoloPlayMode::CHALLENGE);
    }
    bool isArena() const {
        return soloPlayMode_ == static_cast<int>(SoloPlayMode::ARENA);
    }
    bool isEndlessDeep() const {
        return soloPlayMode_ == static_cast<int>(SoloPlayMode::ENDLESS);
    }
    /** Boss Raid / Challenge / Arena / Endless Deep — focused endgame run on the Continue hero. */
    bool isFocusedEndgameRun() const {
        return isBossRaid() || isChallengeDungeon() || isArena() || isEndlessDeep();
    }
    /** Acts 1–4 finished — gates Legendary loot, endgame bosses, Boss Raid / Challenge meta. */
    bool isStoryFullyComplete() const { return questAct4Complete_; }
    bool isQuestAct2Complete() const { return questAct2Complete_; }
    bool isQuestAct3Complete() const { return questAct3Complete_; }
    bool isQuestAct4Complete() const { return questAct4Complete_; }
    /** Pending raid-key drop for Kotlin SharedPreferences (consume after read). */
    bool consumePendingRaidKeyDrop() {
        bool v = pendingRaidKeyDrop_;
        pendingRaidKeyDrop_ = false;
        return v;
    }
    bool peekPendingRaidKeyDrop() const { return pendingRaidKeyDrop_; }

    /** Challenge Dungeon: Legendary rolled this kill — Kotlin applies per-drop decay in prefs. */
    bool consumePendingChallengeLegendaryFound() {
        bool v = pendingChallengeLegendaryFound_;
        pendingChallengeLegendaryFound_ = false;
        return v;
    }
    int getChallengeLegendaryChance() const { return challengeLegendaryChance_; }
    void setChallengeLegendaryChance(int percent) {
        if (percent < 0) percent = 0;
        if (percent > 100) percent = 100;
        challengeLegendaryChance_ = percent;
    }

    static constexpr int ARENA_MAX_WAVES = 5;
    int getArenaWave() const { return arenaWave_; }
    int getArenaScore() const { return arenaScore_; }
    int getArenaWavesCleared() const { return arenaWavesCleared_; }

    static constexpr int ENDLESS_MILESTONE_EVERY = 5;
    static constexpr int ENDLESS_APEX_EVERY = 10;
    int getEndlessDepth() const { return endlessDepth_; }
    int getEndlessBestDepthThisRun() const { return endlessBestDepthThisRun_; }

    /**
     * Session-lifetime daily-quest counters (not serialized).
     * Format: rooms,fights,searches,damage,bosses — Kotlin diffs against last poll.
     */
    std::string getDailyQuestCounters() const;
    /** Apply gold + optional small loot to the living party purse (solo Continue hero). */
    bool grantDailyQuestSpoils(int gold, bool giveLoot);

    const std::vector<std::unique_ptr<Character>>& getPlayers() const { return players_; }
    const std::vector<std::unique_ptr<Character>>& getEnemies() const { return enemies_; }

    Character* getCurrentActor() const {
        if (turnOrder_.empty() || currentTurnIndex_ < 0 || static_cast<size_t>(currentTurnIndex_) >= turnOrder_.size()) return nullptr;
        return turnOrder_[static_cast<size_t>(currentTurnIndex_)];
    }

    bool hasVisualEvent() const { return !visualEvents_.empty(); }
    VisualEvent popVisualEvent() {
        VisualEvent e = visualEvents_.front();
        visualEvents_.pop();
        return e;
    }

    // Random helper
    int getRandomInt(int min, int max) {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(rng_);
    }

private:
    struct LoadOnlyTag {};
    explicit Game(LoadOnlyTag);
    /** Parse every section into this (scratch) Game; throws on malformed data. Returns
     *  whether the descriptions section was present. */
    bool parseSave(const std::string& data);

    std::vector<std::unique_ptr<Character>> players_;
    std::vector<std::unique_ptr<Character>> enemies_;
    std::vector<Character*> turnOrder_;
    int currentTurnIndex_;

    std::string lastEvent_;
    std::string roomDescription_;
    std::string sessionId_;

    std::deque<std::string> chatHistory_;
    std::deque<std::string> journalEntries_;
    std::queue<VisualEvent> visualEvents_;

    std::vector<std::shared_ptr<Item>> shopInventory_;
    int pendingBossXpBonus_ = 0;
    int pendingBossLootLuck_ = 0;
    /** Set by noteBossDefeat for Hollow Crown / Ember Hydra / Nightfang / Boss Raid / Challenge only. */
    bool pendingBossAllowLegendary_ = false;
    /** Challenge Dungeon dynamic Legendary % (synced from prefs; default 18). */
    int challengeLegendaryChance_ = 18;
    bool pendingChallengeLegendaryFound_ = false;
    int pendingBossGoldBonus_ = 0;
    /** Ember Ring Arena — current wave (1..ARENA_MAX_WAVES) and running score. */
    int arenaWave_ = 0;
    int arenaScore_ = 0;
    int arenaWavesCleared_ = 0;
    /** Endless Deep — current depth (1+) and best reached this run. */
    int endlessDepth_ = 0;
    int endlessBestDepthThisRun_ = 0;
    /** Active authored set-piece (AA step 2); cleared on room clear / new spawn. */
    int activeSetPieceId_ = 0; // EncounterAuthorship::TemplateId as int
    int pendingSetPieceXpBonus_ = 0;
    int pendingSetPieceGoldBonus_ = 0;
    bool pendingSetPieceGuaranteedLoot_ = false;
    std::string pendingSetPieceClearFeed_;
    bool bossSeenGk_ = false;
    bool bossSeenSk_ = false;
    bool bossSeenDrake_ = false;
    bool bossSeenHollow_ = false;
    bool bossSeenHydra_ = false;
    bool bossSeenNightfang_ = false;
    bool pendingRaidKeyDrop_ = false;
    // Daily-quest session counters (live poll only; never written into save_state).
    int dqRoomsCleared_ = 0;
    int dqFightsWon_ = 0;
    int dqSearches_ = 0;
    int dqDamageDealt_ = 0;
    int dqBossesDefeated_ = 0;
    void noteDailyQuestDamage(int dmg);

    int turnCounter_;
    int roomCount_;
    bool gameOver_;
    bool isHost_ = false;
    bool isMerchantRoom_ = false;
    bool dmOnlyTable_ = false;
    bool roomSearchUsed_ = false; // one Search attempt per chamber (anti-exploit)
    bool roomRestUsed_ = false; // one Short/Long Rest per rest opportunity (anti-spam)
    /** Earliest longRestProgress_ at which Long Rest is allowed again (5-room cooldown). */
    int longRestNextAvailableRoom_ = 0;
    /** Monotonic rooms/waves/depths cleared for Long Rest cooldown (mode-safe; unlike roomCount_). */
    int longRestProgress_ = 0;
    /** Tick Long Rest cooldown progress (call on every Onward / room advance across all modes). */
    void advanceLongRestProgress();
    // Solo story / crawl (original; SRD-compatible monsters only). Online DM path leaves beat at NONE.
    int questBeat_ = 0;
    bool questLanternRecovered_ = false;
    bool questComplete_ = false;       // Act 1 (Ashen Lantern) finished
    bool questCryptKeyFound_ = false;
    bool questAct2Complete_ = false;   // Act 2 (The Tithe; was Millhollow's Debt) finished
    bool questAct2LedgerFound_ = false;
    bool questAct3Complete_ = false;   // Act 3 (Emberdeep; was Emberdeep Breach) finished
    bool questAct3SealFound_ = false;
    bool questAct4Complete_ = false;   // Act 4 (Ashwake Vigil) finished — story-complete / endgame gate
    bool questAct4EmberFound_ = false;
    int vennChoice_ = VENN_UNDECIDED;  // header field 35 (after the E2 marker)
    int wellChoice_ = WELL_UNDECIDED;  // header field 36
    int heraldTolls_ = 0;              // flavour counter for the Bell-Herald (not saved)
    int soloPlayMode_ = static_cast<int>(SoloPlayMode::STORY);
    int difficulty_ = static_cast<int>(Difficulty::EASY);
    /** Snapshot when entering raid/challenge from a loaded adventure (-1 = none). */
    int preRaidSoloPlayMode_ = -1;
    int preRaidRoomCount_ = 0;
    std::string dmName_;

    std::mt19937 rng_;

    void enemyTurn();
    void allyTurn();
    bool isAllyAi(const Character* c) const;
    Character* findNpcCompanion() const;
    // Lightweight combat AI helpers (solo + host-side only).
    int scoreHeroThreat(const Character& hero, bool partyHasDowned) const;
    int pickEnemyAiTarget() const;
    int pickAllyAiEnemyTarget() const;
    int pickAllyAiHealTarget() const;
    bool allySpecialBeatsBasic(const Character& actor, const Character& enemy) const;
    void removeFromTurnOrder(Character* c);
    void checkPartyDefeat();
    void advanceTurn();
    void purgeDownedEnemies();
    void enterClearedRoom(Character* actor);
    void grantKillLoot(Character* actor, const std::string& foeName, const std::string& foeId);
    void restockShop();
    void maybeSpawnBossEncounter();
    /** Try an authored set-piece instead of pure-random trash (crawl / post-quest). */
    bool trySpawnAuthoredEncounter();
    void clearSetPiecePending();
    /** Spawn a boss by stable enemy ID (Bestiary.h); tier comes from the bestiary table. */
    void spawnNamedBoss(const std::string& enemyId);
    void markBossSeen(const std::string& enemyId);
    /** New foe for a bestiary ID: display name from the table, enemyId set, unique uid. */
    std::unique_ptr<Character> newEnemy(const std::string& enemyId, CharacterClass cl, const std::string& uidTag = "");
    bool hasLivingBossEnemy() const;
    void noteBossDefeat(const std::string& foeName, const std::string& foeId);
    Character* findTransferAlly(const Character* from) const;
    void deliverItemToAlly(Character* ally, std::shared_ptr<Item> item);
    void partyPreferClasses(int& outA, int& outB) const;
    void dmSay(const std::string& line);
    /** Wren, from inside the Lantern (chat tag [Lantern]) — only once she is in the glass. */
    void wrenSay(const std::string& line);
    void onStoryRoomCleared();
    void applyVennChoice(int option, bool silent);
    void applyWellChoice(int option, bool silent);
    void resetSoloQuestState();
    void applySoloQuestRoom();
    void spawnSoloQuestEnemies();
    void grantAshenLantern(Character* actor);
    void maybeFinishQuestOnResolutionEnter();
    void maybeFinishAct2OnSettledEnter();
    void maybeFinishAct3OnSealedEnter();
    void maybeFinishAct4OnKindledEnter();
    bool trySoloQuestSearch(Character* hero);
    bool endgameContentAllowed() const;
    void maybeGrantRaidKeyFromBoss(const std::string& foeName, const std::string& foeId);
    void spawnArenaWave(int wave);
    void spawnEndlessDepth(int depth);
    void applyLegendaryOnHit(Character* actor, int damageDealt);
    void tryFightShield(Character* defender, int& incomingDamage);
    void revivePartyForDifficulty();
    void rollbackOneRoomOrBeat();
    void applyStarterPaddingForStory();
    int proficiencyBonus() const;
    void resolveEnemyDefeated(Character* actor, int targetEnemyIndex);
    bool performWeaponAttack(Character* actor, Character& target, int attackerVisualIndex, bool targetIsEnemy, int targetIndex, bool sneakAttack);
    /** L5: bounded — with no Renderer (window destroyed / UI-only build) nothing drains it. */
    static constexpr size_t kMaxVisualEvents = 64;
    void pushVisualEvent(VisualEventType type, int index) {
        while (visualEvents_.size() >= kMaxVisualEvents) visualEvents_.pop();
        visualEvents_.push({type, index});
    }
public:
    void clearVisualEvents() { while (!visualEvents_.empty()) visualEvents_.pop(); }
private:
    Character* findCharacter(const std::string& name);
    bool isPartyMember(const Character* c) const {
        for (const auto& p : players_) if (p.get() == c) return true;
        return false;
    }
};

} // namespace dnd

#endif // DND_GAME_H
