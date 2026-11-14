#include "Game.h"
#include "EncounterAuthorship.h"
#include "AndroidOut.h"
#include "SaveCodec.h"
#include "MemoryCatalog.h"
#include "CombatCurve.h"
#include "EnemyAi.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <vector>

namespace dnd {

Game::Game(LoadOnlyTag) : currentTurnIndex_(0), turnCounter_(0), roomCount_(0), gameOver_(false) {
    // Scratch target for deserialize(): no RNG seeding, no starter party.
}

Game::Game() : currentTurnIndex_(0), turnCounter_(0), roomCount_(0), gameOver_(false), isHost_(false), isMerchantRoom_(false), dmOnlyTable_(false) {
    std::random_device rd;
    rng_.seed(rd());

    std::stringstream ss;
    ss << "AL-" << std::hex << std::uppercase << getRandomInt(0, 0xFFFF);
    sessionId_ = ss.str();

    startNewGame();
}

void Game::startNewGame(CharacterClass selectedClass, const std::string& playerName,
                        int soloPlayMode, int difficulty,
                        CharacterClass companionClass, bool companionAutoAi) {
    if (!savecodec::isValidClassId(static_cast<int>(selectedClass))) selectedClass = CharacterClass::FIGHTER; // H2
    turnOrder_.clear();
    players_.clear();
    enemies_.clear();
    chatHistory_.clear();
    journalEntries_.clear();
    shopInventory_.clear();
    resetMemoryRuntime();
    pendingBossXpBonus_ = 0;
    pendingBossLootLuck_ = 0;
    pendingBossAllowLegendary_ = false;
    pendingBossGoldBonus_ = 0;
    pendingChallengeLegendaryFound_ = false;
    arenaWave_ = 0;
    arenaScore_ = 0;
    arenaWavesCleared_ = 0;
    endlessDepth_ = 0;
    endlessBestDepthThisRun_ = 0;
    clearSetPiecePending();
    bossSeenGk_ = false;
    bossSeenSk_ = false;
    bossSeenDrake_ = false;
    bossSeenHollow_ = false;
    bossSeenHydra_ = false;
    bossSeenNightfang_ = false;
    pendingRaidKeyDrop_ = false;
    dqRoomsCleared_ = 0;
    dqFightsWon_ = 0;
    dqSearches_ = 0;
    dqDamageDealt_ = 0;
    dqBossesDefeated_ = 0;
    while(!visualEvents_.empty()) visualEvents_.pop();
    resetCombatCurveState();

    gameOver_ = false;
    turnCounter_ = 0;
    roomCount_ = 1;
    dmOnlyTable_ = false;
    roomSearchUsed_ = false; roomRestUsed_ = false;
    longRestProgress_ = 0;
    longRestNextAvailableRoom_ = 0;
    isMerchantRoom_ = false;
    dmName_.clear();
    // Local / solo table is always the turn authority (clients opt out via prepareClientJoin).
    isHost_ = true;
    if (soloPlayMode == static_cast<int>(SoloPlayMode::CRAWL))
        soloPlayMode_ = static_cast<int>(SoloPlayMode::CRAWL);
    else if (soloPlayMode == static_cast<int>(SoloPlayMode::RAID))
        soloPlayMode_ = static_cast<int>(SoloPlayMode::RAID);
    else if (soloPlayMode == static_cast<int>(SoloPlayMode::CHALLENGE))
        soloPlayMode_ = static_cast<int>(SoloPlayMode::CHALLENGE);
    else if (soloPlayMode == static_cast<int>(SoloPlayMode::ARENA))
        soloPlayMode_ = static_cast<int>(SoloPlayMode::ARENA);
    else if (soloPlayMode == static_cast<int>(SoloPlayMode::ENDLESS))
        soloPlayMode_ = static_cast<int>(SoloPlayMode::ENDLESS);
    else
        soloPlayMode_ = static_cast<int>(SoloPlayMode::STORY);
    difficulty_ = difficulty;
    if (difficulty_ < 0 || difficulty_ > 3) difficulty_ = static_cast<int>(Difficulty::EASY);

    questLanternRecovered_ = false;
    questComplete_ = false;
    questCryptKeyFound_ = false;
    questAct2Complete_ = false;
    questAct2LedgerFound_ = false;
    questAct3Complete_ = false;
    questAct3SealFound_ = false;
    questAct4Complete_ = false;
    questAct4EmberFound_ = false;

    players_.push_back(std::make_unique<Character>(playerName, selectedClass, "local-player"));
    players_.back()->aiControlled = false;
    {
        CharacterClass cc = companionClass;
        if (static_cast<int>(cc) < 0 || static_cast<int>(cc) > 4) cc = CharacterClass::WIZARD;
        addAlly(companionNameForClass(cc), cc);
        if (!players_.empty()) {
            Character* ally = findNpcCompanion();
            if (ally) ally->aiControlled = companionAutoAi;
        }
    }

    if (soloPlayMode_ == static_cast<int>(SoloPlayMode::RAID)) {
        // Boss Raid: endgame-only focused boss fight(s). Caller must gate story-complete + spend a key.
        questBeat_ = static_cast<int>(SoloQuestBeat::NONE);
        questComplete_ = true;
        questAct2Complete_ = true;
        questAct3Complete_ = true; // endgame loot/gates active for this run
        questAct4Complete_ = true;
        roomCount_ = 20;
        generateRoomDescription();
        // Force an endgame boss immediately
        enemies_.clear();
        isMerchantRoom_ = false;
        roomSearchUsed_ = false; roomRestUsed_ = false;
        {
            int pick = getRandomInt(0, 2);
            if (pick == 0) spawnNamedBoss("boss_hollow_crown");
            else if (pick == 1) spawnNamedBoss("boss_ember_hydra");
            else spawnNamedBoss("boss_nightfang");
            roomDescription_ = "Raid arena — a sealed endgame vault. One Raid Key spent. Defeat the raid boss for rich spoils.";
            lastEvent_ = "BOSS RAID! Stand ready!";
            dmSay("Boss Raid begins. Spend was paid in keys — earn gold, XP, and a chance at Legendary gear and another key.");
        }
        if (!enemies_.empty()) rollInitiative();
        else { turnOrder_.clear(); currentTurnIndex_ = 0; }
        addChatMessage("System", "Boss Raid begins (difficulty: " + std::string(difficultyName(difficulty_)) + ").");
        return;
    }

    if (soloPlayMode_ == static_cast<int>(SoloPlayMode::CRAWL)) {
        // Dungeon Crawl: skip quest script — classic procedural rooms from the start.
        // Early crawl keeps GK/SK/Drake; true endgame bosses require story complete (Continue from post-Act3).
        questBeat_ = static_cast<int>(SoloQuestBeat::NONE);
        generateRoomDescription();
        spawnRoomContent();
        if (!isMerchantRoom_ && !enemies_.empty()) {
            rollInitiative();
        } else {
            turnOrder_.clear();
            currentTurnIndex_ = 0;
        }
        lastEvent_ = "Dungeon Crawl — Room 1. No story script; clear chambers and press Onward.";
        addChatMessage("System", "Dungeon Crawl begins (difficulty: " + std::string(difficultyName(difficulty_)) + ").");
        dmSay("Welcome, Lamplighter. I am the Lanternkeeper, and I will tell this tale.");
        dmSay("Crawl mode: procedural rooms only — finish the story (Acts 1–3) to unlock endgame bosses and Legendary loot.");
        dmSay("Use Attack, Special, Potion, Search once per clear chamber, Short Rest, and Onward.");
        return;
    }

    // Story Solo: Act 1 → Act 2 → Act 3 → Act 4, then endgame procedural.
    questBeat_ = static_cast<int>(SoloQuestBeat::MILLHOLLOW);
    applyStarterPaddingForStory();
    // Act 1 "The Kindling" title card — first line the player reads.
    dmSay(kKindlingTitleCard);
    applySoloQuestRoom();
    rollInitiative();

    lastEvent_ = "The Kindling — the Lantern is stolen! A Wickthief Scout blocks the road.";
    addChatMessage("System", "A new Lamplighter begins Act 1: The Kindling (difficulty: " +
                   std::string(difficultyName(difficulty_)) + ").");
    dmSay("I am the Lanternkeeper, and I will tell this tale.");
    dmSay("Use Attack for a weapon strike, your class Special for a signature move, Potion for healing, Search once per clear chamber, and Short Rest when safe.");
    {
        Character* ally = findNpcCompanion();
        const std::string allyName = ally ? ally->name : std::string("your companion");
        dmSay("You carry an extra supply for this quest — drink a Potion when bloodied; " + allyName + " will help when you are low.");
    }
}

void Game::applyStarterPaddingForStory() {
    // Beginner Solo padding: +2 max HP and +1 potion/feature charge (story only).
    for (auto& p : players_) {
        if (!p) continue;
        p->bonusMaxHp += 2; // v2.29: kept through level-ups (was lost at the first level-up)
        p->recomputeMaxHp();
        p->currentHp = p->maxHp;
        p->bonusMaxResources += 1; // v2.30: kept through level-ups (was lost at the first level-up)
        p->applyStatsForLevel();
        p->currentHp = p->maxHp;
        p->resources = p->maxResources;
    }
}

void Game::setDifficulty(int d) {
    if (d < 0 || d > 3) d = static_cast<int>(Difficulty::EASY);
    difficulty_ = d;
}

void Game::startDmSession(const std::string& dmName) {
    resetMemoryRuntime(); // Host tables never grant memories
    turnOrder_.clear();
    players_.clear();
    enemies_.clear();
    chatHistory_.clear();
    journalEntries_.clear();
    shopInventory_.clear();
    while(!visualEvents_.empty()) visualEvents_.pop();

    gameOver_ = false;
    turnCounter_ = 0;
    roomCount_ = 0;
    isMerchantRoom_ = false;
    roomSearchUsed_ = false; roomRestUsed_ = false;
    dmOnlyTable_ = true;
    resetSoloQuestState();
    resetCombatCurveState();
    dmName_ = dmName.empty() ? "The Lanternkeeper" : dmName;
    isHost_ = true;

    roomDescription_ = "A candlelit tavern table waits for heroes. The Lanternkeeper (" + dmName_ + ") prepares the adventure. Share the session ID so friends can Join.";
    lastEvent_ = "Lanternkeeper table open — waiting for players to join.";
    addChatMessage("System", "Lanternkeeper " + dmName_ + " opened an online table.");
    addJournalEntry("The Lanternkeeper lit the candles and opened the table.");
    dmSay("I am " + dmName_ + ", your Lanternkeeper. When heroes Join, I will begin the dungeon.");
}

void Game::dmBeginDungeon() {
    if (players_.empty()) {
        lastEvent_ = "No heroes have joined yet.";
        dmSay("Patience — wait for at least one adventurer to Join the session.");
        return;
    }
    roomCount_ = 1;
    isMerchantRoom_ = false;
    roomSearchUsed_ = false; roomRestUsed_ = false;
    gameOver_ = false;
    resetSoloQuestState(); // Host-as-DM uses procedural rooms, not solo script
    generateRoomDescription();
    spawnRoomContent();
    rollInitiative();
    lastEvent_ = "The dungeon begins! Room 1 — roll for initiative!";
    addChatMessage("System", "The Lanternkeeper began the dungeon.");
    dmSay("The tavern doors close behind you. Steel yourselves.");
}

void Game::dmAdvanceRoom() {
    if (players_.empty()) {
        lastEvent_ = "No party to advance.";
        return;
    }
    if (gameOver_) {
        lastEvent_ = "Game Over — start a new table.";
        return;
    }
    roomCount_++;
    advanceLongRestProgress();
    roomSearchUsed_ = false; roomRestUsed_ = false;
    spawnRoomContent();
    const bool bossRoom = hasLivingBossEnemy();
    const std::string bossToast = lastEvent_;
    if (!isMerchantRoom_) {
        if (!bossRoom) generateRoomDescription();
        if (!enemies_.empty()) rollInitiative();
        else { turnOrder_.clear(); currentTurnIndex_ = 0; }
    } else {
        turnOrder_.clear();
        currentTurnIndex_ = 0;
    }
    if (bossRoom && bossToast.rfind("BOSS!", 0) == 0) {
        lastEvent_ = bossToast;
        addChatMessage("System", lastEvent_);
    } else {
        lastEvent_ = "The Lanternkeeper advanced the party to room " + std::to_string(roomCount_) + ".";
        addChatMessage("System", lastEvent_);
        dmSay("Onward — a new chamber opens before you.");
    }
}

void Game::dmNarrate(const std::string& line) {
    if (line.empty()) return;
    roomDescription_ = line;
    dmSay(line);
    lastEvent_ = "The Lanternkeeper narrates…";
}


void Game::prepareClientJoin() {
    resetMemoryRuntime(); // Join never grants memories
    turnOrder_.clear();
    players_.clear();
    enemies_.clear();
    chatHistory_.clear();
    journalEntries_.clear();
    shopInventory_.clear();
    while (!visualEvents_.empty()) visualEvents_.pop();

    gameOver_ = false;
    turnCounter_ = 0;
    roomCount_ = 0;
    isMerchantRoom_ = false;
    roomSearchUsed_ = false; roomRestUsed_ = false;
    dmOnlyTable_ = false;
    resetSoloQuestState();
    dmName_.clear();
    isHost_ = false;
    resetCombatCurveState();
    currentTurnIndex_ = 0;

    roomDescription_ = "Connecting to the Lanternkeeper's table…";
    lastEvent_ = "Waiting for the Lanternkeeper to sync the party…";
    addChatMessage("System", "Joining online session…");
}

void Game::dmGrantShortRest() {
    if (gameOver_ || players_.empty()) return;
    // Reuse short-rest recovery without merchant leave logic; force past combat lock.
    bool wasMerchant = isMerchantRoom_;
    isMerchantRoom_ = false;
    playerRest(/*force=*/true);
    isMerchantRoom_ = wasMerchant;
    dmSay("The Lanternkeeper grants a short rest.");
}

void Game::addAlly(const std::string& name, CharacterClass cl) {
    // H2: never build a Character from an out-of-range class (remote join / bad caller).
    if (!savecodec::isValidClassId(static_cast<int>(cl))) {
        aout << "addAlly: class " << static_cast<int>(cl) << " out of range, using Fighter" << std::endl;
        cl = CharacterClass::FIGHTER;
    }
    for (const auto& p : players_) {
        if (p->name == name) return;
    }
    std::string uid = "ally-" + std::to_string(players_.size());
    players_.push_back(std::make_unique<Character>(name, cl, uid));
    // Solo NPC companions default to AI; joined humans (no "(NPC)") wait for their own input.
    players_.back()->aiControlled = (name.find("(NPC)") != std::string::npos);
    rebuildTurnOrder();
}

std::string Game::companionNameForClass(CharacterClass cl) {
    switch (cl) {
        case CharacterClass::FIGHTER: return "Bren (NPC)";
        case CharacterClass::WIZARD: return "Corvin (NPC)";
        case CharacterClass::ROGUE: return "Sable (NPC)";
        case CharacterClass::CLERIC: return "Miren (NPC)";
        case CharacterClass::BARD: return "Jory (NPC)";
    }
    return "Corvin (NPC)";
}

Character* Game::findNpcCompanion() const {
    for (const auto& p : players_) {
        if (p && p->name.find("(NPC)") != std::string::npos) return p.get();
    }
    return nullptr;
}

std::string Game::getCompanionName() const {
    Character* c = findNpcCompanion();
    return c ? c->name : "";
}

bool Game::isCompanionAutoAi() const {
    Character* c = findNpcCompanion();
    if (!c) return true;
    return c->aiControlled;
}

void Game::setCompanionAutoAi(bool autoAi) {
    Character* c = findNpcCompanion();
    if (!c) return;
    c->aiControlled = autoAi;
    lastEvent_ = c->name + (autoAi ? " will act automatically." : " awaits your commands on their turn.");
    dmSay(lastEvent_);
    addChatMessage("System", lastEvent_);
}

bool Game::setCompanionClass(CharacterClass cl) {
    if (gameOver_ || isInCombat()) return false;
    Character* old = findNpcCompanion();
    if (!old) return false;
    if (static_cast<int>(cl) < 0 || static_cast<int>(cl) > 4) return false;
    if (old->characterClass == cl) {
        // Still refresh name if needed
        std::string want = companionNameForClass(cl);
        if (old->name != want) old->name = want;
        return true;
    }
    const int level = old->level;
    const int xp = old->xp;
    const int gold = old->gold;
    auto keptInv = old->inventory;
    auto keptW = old->equippedWeapon;
    auto keptA = old->equippedArmor;
    const int pending = old->pendingStatPoints;
    const int initiative = old->initiative;
    const bool ai = old->aiControlled;
    const bool down = old->isDowned;
    const bool dead = old->isDead;
    const bool stable = old->isStable;
    const int dss = old->deathSaveSuccesses;
    const int dsf = old->deathSaveFailures;
    const int keptBonusHp = old->bonusMaxHp; // v2.29: story padding / CON-point HP survive a reclass
    const int keptBonusRes = old->bonusMaxResources; // v2.30: story +1 resource survives too
    const std::string uid = old->uid;
    const std::string newName = companionNameForClass(cl);

    for (auto& p : players_) {
        if (p.get() != old) continue;
        auto neu = std::make_unique<Character>(newName, cl, uid);
        neu->level = level;
        neu->xp = xp;
        neu->gold = gold;
        neu->pendingStatPoints = pending;
        neu->initiative = initiative;
        neu->aiControlled = ai;
        neu->inventory = keptInv;
        // Keep gear if still class-legal; else stash to bag.
        neu->equippedWeapon = nullptr;
        neu->equippedArmor = nullptr;
        if (keptW) {
            if (keptW->canEquip(cl)) neu->equippedWeapon = keptW;
            else neu->addToInventory(keptW);
        }
        if (keptA) {
            if (keptA->canEquip(cl)) neu->equippedArmor = keptA;
            else neu->addToInventory(keptA);
        }
        if (!neu->equippedWeapon || !neu->equippedArmor) {
            // Ensure starters fill empty slots without wiping bag.
            const int cls = static_cast<int>(cl);
            if (!neu->equippedWeapon) {
                if (cl == CharacterClass::FIGHTER) neu->equippedWeapon = Item::make("Longsword", ItemType::WEAPON, 0, ItemRarity::COMMON, cls);
                else if (cl == CharacterClass::ROGUE) neu->equippedWeapon = Item::make("Shortsword", ItemType::WEAPON, 0, ItemRarity::COMMON, cls);
                else if (cl == CharacterClass::WIZARD) neu->equippedWeapon = Item::make("Quarterstaff", ItemType::WEAPON, 0, ItemRarity::COMMON, cls);
                else if (cl == CharacterClass::BARD) neu->equippedWeapon = Item::make("Rapier", ItemType::WEAPON, 0, ItemRarity::COMMON, cls);
                else neu->equippedWeapon = Item::make("Mace", ItemType::WEAPON, 0, ItemRarity::COMMON, cls);
            }
            if (!neu->equippedArmor) {
                if (cl == CharacterClass::FIGHTER) neu->equippedArmor = Item::make("Chain Shirt", ItemType::ARMOR, 3, ItemRarity::COMMON, cls);
                else if (cl == CharacterClass::ROGUE || cl == CharacterClass::BARD) neu->equippedArmor = Item::make("Leather Armor", ItemType::ARMOR, 1, ItemRarity::COMMON, cls);
                else if (cl == CharacterClass::WIZARD) neu->equippedArmor = Item::make("Traveler Clothes", ItemType::ARMOR, 0, ItemRarity::COMMON, -1);
                else neu->equippedArmor = Item::make("Scale Mail", ItemType::ARMOR, 4, ItemRarity::COMMON, cls);
            }
        }
        neu->bonusMaxHp = keptBonusHp;
        neu->bonusMaxResources = keptBonusRes;
        neu->applyStatsForLevel();
        neu->calculateAC();
        neu->currentHp = neu->maxHp;
        neu->resources = neu->maxResources;
        neu->isDowned = down;
        neu->isDead = dead;
        neu->isStable = stable;
        neu->deathSaveSuccesses = dss;
        neu->deathSaveFailures = dsf;
        if (dead || down) neu->currentHp = 0;
        // Story padding: if hero had padding, companion already got it at start via applyStarterPaddingForStory on all.
        p = std::move(neu);
        break;
    }
    rebuildTurnOrder();
    lastEvent_ = newName + " takes the field as a " +
        (cl == CharacterClass::FIGHTER ? "Fighter" :
         cl == CharacterClass::WIZARD ? "Wizard" :
         cl == CharacterClass::ROGUE ? "Rogue" :
         cl == CharacterClass::BARD ? "Bard" : "Cleric") + ".";
    dmSay(lastEvent_);
    addChatMessage("System", lastEvent_);
    return true;
}

void Game::resetSoloQuestState() {
    questBeat_ = static_cast<int>(SoloQuestBeat::NONE);
    questLanternRecovered_ = false;
    questComplete_ = false;
    questCryptKeyFound_ = false;
    questAct2Complete_ = false;
    questAct2LedgerFound_ = false;
    questAct3Complete_ = false;
    questAct3SealFound_ = false;
    questAct4Complete_ = false;
    questAct4EmberFound_ = false;
    vennChoice_ = VENN_UNDECIDED;
    wellChoice_ = WELL_UNDECIDED;
    heraldTolls_ = 0;
    pendingRaidKeyDrop_ = false;
    soloPlayMode_ = static_cast<int>(SoloPlayMode::STORY);
}

void Game::applySoloQuestRoom() {
    enemies_.clear();
    shopInventory_.clear();
    isMerchantRoom_ = false;
    roomSearchUsed_ = false; roomRestUsed_ = false;

    const QuestBeatScript* beat = findQuestBeat(questBeat_);
    if (!beat) {
        // Fallback to procedural if beat table missing
        generateRoomDescription();
        spawnRoomContent();
        return;
    }

    roomDescription_ = std::string(beat->roomDescription);
    dmSay(std::string("[") + beat->title + "] " + beat->dmEnter);
    dmSay(roomDescription_);
    addJournalEntry(beat->journalOnEnter);
    if (beat->wrenOnEnter) wrenSay(beat->wrenOnEnter);
    heraldTolls_ = 0;

    spawnSoloQuestEnemies();
    maybeFinishQuestOnResolutionEnter();
    maybeFinishAct2OnSettledEnter();
    maybeFinishAct3OnSealedEnter();
    maybeFinishAct4OnKindledEnter();
    memoryOnBeatEnter();

    if (enemies_.empty()) {
        // Soft-open story beats: Search / Rest / Onward available immediately.
        turnOrder_.clear();
        currentTurnIndex_ = 0;
        lastEvent_ = std::string(beat->title) + " — chamber open. Search, Rest, or Onward.";
    } else {
        lastEvent_ = std::string(beat->title) + " — foes ahead!";
    }
}

void Game::spawnSoloQuestEnemies() {
    enemies_.clear();
    const auto beat = static_cast<SoloQuestBeat>(questBeat_);

    // Solo Ashen Lantern: tune early rooms for beginners. attackStatDelta lowers to-hit/damage
    // (and Rogue AC via Dex). resourceCap strips Action Surge / Sneak so early foes don't one-shot.
    // Post-quest procedural spawns are unchanged.
    auto makeFoe = [&](const std::string& enemyId, CharacterClass cl, int bonusHp, int bonusAc,
                       int resourceCap = -1, int attackStatDelta = 0) {
        auto foe = newEnemy(enemyId, cl);
        if (attackStatDelta != 0) {
            switch (cl) {
                case CharacterClass::ROGUE:
                    foe->attributes.dexterity = std::max(3, foe->attributes.dexterity + attackStatDelta);
                    break;
                case CharacterClass::FIGHTER:
                    foe->attributes.strength = std::max(3, foe->attributes.strength + attackStatDelta);
                    break;
                case CharacterClass::WIZARD:
                    foe->attributes.intelligence = std::max(3, foe->attributes.intelligence + attackStatDelta);
                    break;
                case CharacterClass::CLERIC:
                    foe->attributes.wisdom = std::max(3, foe->attributes.wisdom + attackStatDelta);
                    break;
                case CharacterClass::BARD:
                    foe->attributes.charisma = std::max(3, foe->attributes.charisma + attackStatDelta);
                    break;
            }
            foe->calculateAC();
        }
        foe->maxHp = std::max(4, foe->maxHp + bonusHp);
        foe->currentHp = foe->maxHp;
        foe->armorClass += bonusAc;
        if (resourceCap >= 0) {
            foe->maxResources = resourceCap;
            foe->resources = resourceCap;
        }
        enemies_.push_back(std::move(foe));
    };

    switch (beat) {
        case SoloQuestBeat::MILLHOLLOW:
            // Soft open: one weak scout (was HP-2 full Rogue with Sneak).
            makeFoe("enemy_wickthief_scout", CharacterClass::ROGUE, -4, 0, /*resourceCap=*/0, /*attackStatDelta=*/-2);
            break;
        case SoloQuestBeat::THORNPATH:
            // Always a single wolf — no random second goblin.
            makeFoe("enemy_gloamhound", CharacterClass::ROGUE, 0, 0, /*resourceCap=*/0, /*attackStatDelta=*/-2);
            break;
        case SoloQuestBeat::CRYPT_DOORS:
            // Puzzle-lite soft open: Search once for the rune-key (no mandatory fight).
            break;
        case SoloQuestBeat::BONE_GALLERY:
            // Medium+: small chance the gallery hosts the Skeleton King (alone). Easy keeps soft pair.
            if (difficulty_ >= static_cast<int>(Difficulty::MEDIUM) && getRandomInt(1, 100) <= 18) {
                spawnNamedBoss("boss_prior");
                pendingSurprise_ = true; // v2.29: Elite role, capped at 1.5x the room budget, 1-turn warning
                lastEvent_ = "BOSS! The Ossuary Prior — stand ready!";
                dmSay("A mitred horror stirs among the bones — the Ossuary Prior (original foe). Bones knit into a mitre… it needs a moment to rise.");
            } else {
                makeFoe("enemy_ashbound", CharacterClass::FIGHTER, 0, 0, /*resourceCap=*/0, /*attackStatDelta=*/-2);
                makeFoe("enemy_ashbound", CharacterClass::FIGHTER, -2, 0, /*resourceCap=*/0, /*attackStatDelta=*/-2);
            }
            break;
        case SoloQuestBeat::LANTERN_VAULT:
            // Climax retained: one Champion, HP/AC one notch down, single surge max.
            makeFoe("boss_sexton", CharacterClass::FIGHTER, 12, 1, /*resourceCap=*/1, /*attackStatDelta=*/-2);
            {
                Character* ally = findNpcCompanion();
                std::string who = ally ? ally->name : "your companion";
                dmSay("The vault's undying custodian stirs — the Lantern Sexton. It can surge once; keep a Potion ready and let " + who + " help.");
            }
            break;
        case SoloQuestBeat::RESOLUTION:
            // Shrine: no fight — resolution beat.
            break;
        case SoloQuestBeat::ACT2_GREEN:
            // Soft open fight: one Tithe Enforcer (bandit-flavored fighter), toned down.
            makeFoe("enemy_tithe_enforcer", CharacterClass::FIGHTER, 2, 0, /*resourceCap=*/0, /*attackStatDelta=*/-2);
            break;
        case SoloQuestBeat::ACT2_WEIR:
            // Medium+: rare Goblin King ambush at the weir (Easy keeps soft river goblins).
            if (difficulty_ >= static_cast<int>(Difficulty::MEDIUM) && getRandomInt(1, 100) <= 16) {
                spawnNamedBoss("boss_wick_king");
                pendingSurprise_ = true; // v2.29: Elite role, capped at 1.5x the room budget, 1-turn warning
                lastEvent_ = "BOSS! The Wick King — stand ready!";
                dmSay("A crown of melted candles by the millrace — the Wick King (original) bars the path! Hot wax still drips from his crown… he needs a moment.");
            } else {
                makeFoe("enemy_wickthief_river", CharacterClass::ROGUE, 0, 0, /*resourceCap=*/0, /*attackStatDelta=*/-2);
                makeFoe("enemy_wickthief_river", CharacterClass::ROGUE, -2, 0, /*resourceCap=*/0, /*attackStatDelta=*/-2);
            }
            break;
        case SoloQuestBeat::ACT2_CELLAR:
            makeFoe("enemy_ashbound", CharacterClass::FIGHTER, 2, 0, /*resourceCap=*/0, /*attackStatDelta=*/-2);
            makeFoe("enemy_tallow_rat", CharacterClass::ROGUE, -2, 0, /*resourceCap=*/0, /*attackStatDelta=*/-3);
            break;
        case SoloQuestBeat::ACT2_LOFT:
            // Soft open: Search for the coerced ledger.
            break;
        case SoloQuestBeat::ACT2_HALL:
            makeFoe("boss_collector", CharacterClass::FIGHTER, 14, 1, /*resourceCap=*/1, /*attackStatDelta=*/-1);
            {
                Character* ally = findNpcCompanion();
                std::string who = ally ? ally->name : "your companion";
                dmSay("Sallow Venn speaks before he draws: \"I don't enjoy this. I was ten when they chose my father. "
                      "Ask Hesk who held the quill.\" Keep a Potion ready — " + who + " will help.");
            }
            break;
        case SoloQuestBeat::ACT2_SETTLED:
            break;
        case SoloQuestBeat::ACT3_RUMOR:
            makeFoe("enemy_tithe_scavenger", CharacterClass::ROGUE, 2, 0, /*resourceCap=*/0, /*attackStatDelta=*/-2);
            break;
        case SoloQuestBeat::ACT3_WELL:
            makeFoe("enemy_tallow_rat", CharacterClass::ROGUE, 0, 0, /*resourceCap=*/0, /*attackStatDelta=*/-2);
            makeFoe("enemy_wickthief_root", CharacterClass::ROGUE, 2, 0, /*resourceCap=*/0, /*attackStatDelta=*/-2);
            break;
        case SoloQuestBeat::ACT3_ROOTS:
            makeFoe("enemy_ashbound", CharacterClass::FIGHTER, 4, 0, /*resourceCap=*/0, /*attackStatDelta=*/-1);
            makeFoe("enemy_ashbound", CharacterClass::FIGHTER, 2, 0, /*resourceCap=*/0, /*attackStatDelta=*/-2);
            break;
        case SoloQuestBeat::ACT3_RELIC:
            // Soft open: Search for Ember Seal
            break;
        case SoloQuestBeat::ACT3_WARDEN:
            makeFoe("boss_warden", CharacterClass::FIGHTER, 22, 2, /*resourceCap=*/2, /*attackStatDelta=*/0);
            {
                Character* ally = findNpcCompanion();
                std::string who = ally ? ally->name : "your companion";
                dmSay("The First Lamplighter, bound as a gate, bars the ember wound. He fights because he cannot do "
                      "otherwise. Keep potions ready — " + who + " will help.");
            }
            break;
        case SoloQuestBeat::ACT3_SEALED:
            break;
        case SoloQuestBeat::ACT4_WATCH:
            makeFoe("enemy_tithe_watcher", CharacterClass::ROGUE, 4, 0, /*resourceCap=*/0, /*attackStatDelta=*/-1);
            break;
        case SoloQuestBeat::ACT4_MARSH:
            makeFoe("enemy_wickthief_fen", CharacterClass::ROGUE, 2, 0, /*resourceCap=*/0, /*attackStatDelta=*/-2);
            makeFoe("enemy_tallow_rat", CharacterClass::ROGUE, 0, 0, /*resourceCap=*/0, /*attackStatDelta=*/-2);
            break;
        case SoloQuestBeat::ACT4_CAUSEWAY:
            makeFoe("enemy_ashbound", CharacterClass::FIGHTER, 6, 0, /*resourceCap=*/0, /*attackStatDelta=*/-1);
            makeFoe("enemy_ashbound", CharacterClass::FIGHTER, 4, 0, /*resourceCap=*/0, /*attackStatDelta=*/-1);
            break;
        case SoloQuestBeat::ACT4_EMBER:
            // Soft open: Search for Vigil Ember
            break;
        case SoloQuestBeat::ACT4_HERALD:
            makeFoe("boss_herald", CharacterClass::FIGHTER, 26, 2, /*resourceCap=*/2, /*attackStatDelta=*/0);
            {
                Character* ally = findNpcCompanion();
                std::string who = ally ? ally->name : "your companion";
                dmSay("The Bell-Herald of the Tithe bars the Vigil Spire. Every toll of its bell hushes the Lantern. "
                      "Keep potions ready — " + who + " will help.");
            }
            break;
        case SoloQuestBeat::ACT4_KINDLED:
        default:
            break;
    }
}

void Game::grantAshenLantern(Character* actor) {
    if (questLanternRecovered_) return;
    questLanternRecovered_ = true;
    Character* hero = actor;
    if (!hero || hero->isDead) {
        hero = nullptr;
        for (auto& p : players_) {
            if (p && !p->isDead) { hero = p.get(); break; }
        }
    }
    if (hero) {
        // Thematic quest item: modest weapon blessing without replacing class identity harshly.
        hero->equippedWeapon = Item::make("Ashen Lantern", ItemType::WEAPON, 1, ItemRarity::RARE, -1);
        hero->gold += 25;
    }
    dmSay("You lift the Ashen Lantern from the plinth. It is barely warm, and Wren is barely breathing.");
    addJournalEntry("Recovered the Ashen Lantern (questLanternRecovered).");
    addChatMessage("Quest", "Ashen Lantern recovered!");
    lastEvent_ = "Ashen Lantern recovered! Rest if needed, then carry Wren Onward to the shrine.";
}

void Game::maybeFinishQuestOnResolutionEnter() {
    if (static_cast<SoloQuestBeat>(questBeat_) != SoloQuestBeat::RESOLUTION) return;
    if (!questLanternRecovered_) {
        // Safety: if vault was skipped somehow, still grant on shrine enter.
        grantAshenLantern(players_.empty() ? nullptr : players_[0].get());
    }
    if (!questComplete_) {
        questComplete_ = true;
        dmSay("The Lantern flares. Night-things withdraw from Millhollow.");
        wrenSay("I'm still here. Don't you dare cry.");
        dmSay("In the crypt dark, a figure in a grey mask watches. It writes something in a ledger, and is gone.");
        wrenSay("Did you see that? Someone was counting us.");
        dmSay("Act 1 complete: The Kindling. Rest if you need, then Onward — Act 2, The Tithe, waits on the green.");
        addJournalEntry("Act 1 complete — Wren relit the Ashen Lantern with her own soul. A masked watcher saw it.");
        addChatMessage("Quest", "Act 1 done — The Kindling. Act 2 awaits.");
    }
}

void Game::maybeFinishAct2OnSettledEnter() {
    if (static_cast<SoloQuestBeat>(questBeat_) != SoloQuestBeat::ACT2_SETTLED) return;
    if (!questAct2LedgerFound_) {
        questAct2LedgerFound_ = true;
        addJournalEntry("Safety: the soul-ledger counted as recovered at The Tithe Broken.");
    }
    if (!questAct2Complete_) {
        questAct2Complete_ = true;
        dmSay("The soul-ledger burns in a clay bowl. The local contract breaks. The grey coats melt into the mist.");
        dmSay("Hesk, finally looking at you: \"We chose. Every generation, we chose who went into the light.\"");
        dmSay("Sallow Venn is on his knees in the wet grass. Spare him, or end it.");
        addJournalEntry("The Tithe Broken — the soul-ledger burned; Hesk confessed the elders chose who was kindled.");
        addChatMessage("Quest", "Act 2 done — The Tithe. Act 3 awaits.");
        if (players_.empty() == false && players_[0]) {
            players_[0]->gold += 40;
        }
    }
}

void Game::maybeFinishAct3OnSealedEnter() {
    if (static_cast<SoloQuestBeat>(questBeat_) != SoloQuestBeat::ACT3_SEALED) return;
    if (!questAct3SealFound_) {
        questAct3SealFound_ = true;
        addJournalEntry("Safety: the Ember Seal counted as recovered at Breach Sealed.");
    }
    if (!questAct3Complete_) {
        questAct3Complete_ = true;
        questAct2Complete_ = true;
        questComplete_ = true;
        dmSay("Before he goes still, the First Lamplighter speaks: \"I am your blood. The founder of Millhollow is "
              "Abbess Maren Vey. She bargained with the Gloam, and she drinks from lanterns to stay alive. The well "
              "is where the drunk-dry go.\"");
        dmSay("The Ember Seal will close the well. The ash-dead are still down there. Seal them in, or let them rest.");
        addJournalEntry("Emberdeep complete — the First Lamplighter named Abbess Maren Vey, founder of Millhollow.");
        addChatMessage("Quest", "Act 3 done — Emberdeep. Act 4 awaits.");
        if (!players_.empty() && players_[0]) {
            players_[0]->gold += 60;
        }
    }
}

void Game::maybeFinishAct4OnKindledEnter() {
    if (static_cast<SoloQuestBeat>(questBeat_) != SoloQuestBeat::ACT4_KINDLED) return;
    if (!questAct4EmberFound_) {
        questAct4EmberFound_ = true;
        addJournalEntry("Safety: Vigil Ember counted as recovered at Vigil Kindled.");
    }
    if (!questAct4Complete_) {
        questAct4Complete_ = true;
        questAct3Complete_ = true;
        questAct2Complete_ = true;
        questComplete_ = true;
        dmSay("The draw reverses and the light comes home. Maren Vey stands unmasked on the stair, takes a shard of "
              "the Lantern, and walks down into the Ashen Deep.");
        // Epilogue colour from the Act 3 (and Act 2) choices — flavour only.
        if (wellChoice_ == WELL_LET_REST) {
            dmSay("The Lantern is dimmer than it was before the well, and it is lighter to carry. The ash-dead below "
                  "Millhollow are resting.");
        } else {
            dmSay("The Lantern burns bright over Millhollow, and under the well the ash-dead are still sealed in.");
        }
        if (vennChoice_ == VENN_SPARED) {
            dmSay("Far off in Greyfen, an old ringmaster hears that you spared her son.");
        }
        dmSay("Follow the ash. Lamplighter's Rounds, the Ashen Deep, the Ember Ring, the Cinder Crucible and the "
              "Three Drowned Lanterns are open.");
        addJournalEntry("Ashwake Vigil complete — Maren Vey took a shard of Wren and descended. Follow the ash.");
        addChatMessage("Quest", "Act 4 done — Story complete! Endgame unlocked.");
        if (!players_.empty() && players_[0]) {
            players_[0]->gold += 80;
        }
    }
}

// --- The Kindling story choices + Wren's clear lines (v2.25) -----------------------------------

namespace {
const char* const kClearedSuffix = " The foes lie still. You may Search, take a Short Rest, or press Onward.";
}

int Game::pendingStoryChoice() const {
    if (dmOnlyTable_ || gameOver_ || !enemies_.empty()) return PROMPT_NONE;
    if (soloPlayMode_ != static_cast<int>(SoloPlayMode::STORY)) return PROMPT_NONE;
    const auto beat = static_cast<SoloQuestBeat>(questBeat_);
    if (beat == SoloQuestBeat::ACT2_SETTLED && questAct2Complete_ && vennChoice_ == VENN_UNDECIDED)
        return PROMPT_VENN;
    if (beat == SoloQuestBeat::ACT3_SEALED && questAct3Complete_ && wellChoice_ == WELL_UNDECIDED)
        return PROMPT_WELL;
    return PROMPT_NONE;
}

bool Game::chooseStoryOption(int kind, int option) {
    if (kind != pendingStoryChoice() || kind == PROMPT_NONE) return false;
    if (option != 1 && option != 2) return false;
    if (kind == PROMPT_VENN) applyVennChoice(option, false);
    else applyWellChoice(option, false);
    return true;
}

std::string Game::getStoryChoices() const {
    return std::to_string(vennChoice_) + "," + std::to_string(wellChoice_);
}

void Game::applyVennChoice(int option, bool silent) {
    vennChoice_ = (option == VENN_SPARED) ? VENN_SPARED : VENN_ENDED;
    if (silent) return;
    if (vennChoice_ == VENN_SPARED) {
        dmSay("You step back. Sallow Venn gets up slowly and walks out of Millhollow without looking back. "
              "\"The Lantern was never yours,\" he says over his shoulder. \"Ask what's under the well.\"");
        wrenSay("Good. I think. Ask me again when I've stopped shaking.");
        addJournalEntry("Spared Sallow Venn. Somewhere, Old Sallow will hear of it.");
        lastEvent_ = "You spared Sallow Venn. Rest if you need — then Onward for Act 3: Emberdeep.";
    } else {
        dmSay("It is over quickly. Sallow Venn's last words: \"The Lantern was never yours. Ask what's under the well.\"");
        dmSay("Wren is quiet for a long moment.");
        addJournalEntry("Ended Sallow Venn on the green. The Ember Ring will remember.");
        lastEvent_ = "Sallow Venn is gone. Rest if you need — then Onward for Act 3: Emberdeep.";
    }
    addChatMessage("Quest", lastEvent_);
}

void Game::applyWellChoice(int option, bool silent) {
    wellChoice_ = (option == WELL_LET_REST) ? WELL_LET_REST : WELL_SEALED_IN;
    grantMemory("mem_well_judgment", false); // body follows the choice
    if (silent) return;
    if (wellChoice_ == WELL_LET_REST) {
        dmSay("You turn the Ember Seal gently. Below, the Ashbound go still at last, and the well breathes out. "
              "The Lantern dims, and stays dimmer.");
        wrenSay("Thank you. They were so tired.");
        addJournalEntry("Let the ash-dead rest. The Lantern burns dimmer for it.");
        lastEvent_ = "The ash-dead rest. Rest if you need — then Onward for Act 4: Ashwake Vigil.";
    } else {
        dmSay("The Ember Seal locks hard. The well goes cold and quiet, and the ash-dead stay below. "
              "The Lantern burns bright.");
        wrenSay("The village is safe. I know that's what I'm supposed to want.");
        addJournalEntry("Sealed the ash-dead in. The Lantern stays bright.");
        lastEvent_ = "The well is sealed. Rest if you need — then Onward for Act 4: Ashwake Vigil.";
    }
    addChatMessage("Quest", lastEvent_);
}

void Game::onStoryRoomCleared() {
    if (!isSoloQuestScripted() || dmOnlyTable_) return;
    const QuestBeatScript* beat = findQuestBeat(questBeat_);
    if (beat && beat->wrenOnClear) wrenSay(beat->wrenOnClear);
    switch (static_cast<SoloQuestBeat>(questBeat_)) {
        case SoloQuestBeat::ACT2_HALL:
            dmSay("Sallow Venn is on his knees, not dead. Onward to the green — the village should see this.");
            break;
        case SoloQuestBeat::ACT3_WARDEN:
            dmSay("The First Lamplighter kneels, and the gate he made of himself loosens. Onward — he has something to say.");
            break;
        case SoloQuestBeat::ACT4_HERALD:
            dmSay("The bell cracks on the stair. The glass is not quiet any more. Onward — turn the Vigil Ember.");
            break;
        default:
            break;
    }
}

bool Game::refreshStoryRoomAfterLoad() {
    if (dmOnlyTable_ || soloPlayMode_ != static_cast<int>(SoloPlayMode::STORY)) return false;
    if (!isSoloQuestScripted()) return false;
    const QuestBeatScript* beat = findQuestBeat(questBeat_);
    if (!beat) return false;
    const std::string suffix(kClearedSuffix);
    const bool cleared = roomDescription_.size() >= suffix.size()
        && roomDescription_.compare(roomDescription_.size() - suffix.size(), suffix.size(), suffix) == 0;
    std::string fresh = beat->roomDescription;
    if (cleared) fresh += suffix;
    if (fresh == roomDescription_) return false;
    roomDescription_ = fresh;
    return true;
}

// --- The Memory Ledger (v2.26) ----------------------------------------------------------------
// Native decides *what* is earned; Kotlin (MemoryLedger.kt) owns the per-hero store in meta prefs
// (keyed by save slot) and drains the queue only for solo play. Host / Join tables never grant.

void Game::resetMemoryRuntime() {
    pendingMemories_.clear();
    memoryOwned_.clear();
    memoryContextKnown_ = false;
    memoryCrawlDry_ = 0;
}

void Game::setMemoryLedgerContext(const std::string& ownedCsv, int crawlDryRooms) {
    memoryOwned_.clear();
    std::string cur;
    for (char c : ownedCsv + ",") {
        if (c == ',') { if (!cur.empty() && memory::findMemory(cur)) memoryOwned_.insert(cur); cur.clear(); }
        else cur.push_back(c);
    }
    memoryCrawlDry_ = std::max(0, std::min(crawlDryRooms, 1000));
    memoryContextKnown_ = true;
}

std::string Game::drainMemoryGrants() {
    std::string out = std::to_string(memoryCrawlDry_) + "|";
    for (size_t i = 0; i < pendingMemories_.size(); ++i) {
        if (i) out += ",";
        out += pendingMemories_[i];
    }
    pendingMemories_.clear();
    return out;
}

bool Game::grantMemory(const char* id, bool voice) {
    if (dmOnlyTable_ || !id) return false;
    const memory::MemoryDef* m = memory::findMemory(id);
    if (!m || memoryOwned_.count(id)) return false;
    memoryOwned_.insert(id);
    if (pendingMemories_.size() < 64) pendingMemories_.push_back(id);
    addChatMessage("System", std::string("A soul settles in the glass — ") + m->title + ".");
    if (voice && m->wren && questComplete_) wrenSay(m->wren);
    return true;
}

void Game::memoryOnBeatEnter() {
    if (!isSoloQuestScripted() || dmOnlyTable_) return;
    for (const auto& m : memory::kMemories) {
        if (m.source != memory::SRC_STORY || m.beat != questBeat_) continue;
        if (m.trigger == memory::T_ENTER) grantMemory(m.id, false);
    }
    // Act-resolution safety: Search memories whose item flag the resolution just forced.
    switch (static_cast<SoloQuestBeat>(questBeat_)) {
        case SoloQuestBeat::ACT2_SETTLED: if (questAct2LedgerFound_) grantMemory("mem_your_name", false); break;
        case SoloQuestBeat::ACT3_SEALED:
            if (questAct3SealFound_) grantMemory("mem_first_oath", false);
            grantMemory("mem_maren_named", false); // "on defeat / entering beat 19"
            if (wellChoice_ != WELL_UNDECIDED) grantMemory("mem_well_judgment", false);
            break;
        case SoloQuestBeat::ACT4_KINDLED: if (questAct4EmberFound_) grantMemory("mem_siphon_prayer", false); break;
        default: break;
    }
}

void Game::memoryOnStoryClear() {
    if (!isSoloQuestScripted() || dmOnlyTable_) return;
    for (const auto& m : memory::kMemories) {
        if (m.source != memory::SRC_STORY || m.beat != questBeat_) continue;
        if (m.trigger == memory::T_CLEAR || m.trigger == memory::T_CLEAR_OR_SEARCH) grantMemory(m.id, false);
    }
}

void Game::memoryOnStorySearch() {
    if (!isSoloQuestScripted() || dmOnlyTable_) return;
    for (const auto& m : memory::kMemories) {
        if (m.source != memory::SRC_STORY || m.beat != questBeat_) continue;
        if (m.trigger == memory::T_SEARCH_FLAG || m.trigger == memory::T_SEARCH_ONLY
            || m.trigger == memory::T_CLEAR_OR_SEARCH) {
            grantMemory(m.id, false);
        }
    }
}

bool Game::memoryCrawlEligible() const {
    // Lamplighter's Rounds: procedural chambers once Wren is in the glass (Act 1 done).
    if (dmOnlyTable_ || gameOver_ || isMerchantRoom_ || !questComplete_) return false;
    if (isSoloQuestScripted() || isFocusedEndgameRun()) return false;
    return soloPlayMode_ == static_cast<int>(SoloPlayMode::STORY) || soloPlayMode_ == static_cast<int>(SoloPlayMode::CRAWL);
}

void Game::memoryOnCrawlEvent(bool search) {
    if (!memoryCrawlEligible() || !memoryContextKnown_) return;
    std::vector<const char*> pool;
    for (const auto& m : memory::kMemories)
        if (m.source == memory::SRC_CRAWL && !memoryOwned_.count(m.id)) pool.push_back(m.id);
    if (pool.empty()) {
        if (search && getRandomInt(1, 100) <= 18) dmSay("Ash in the corner, but no voice in it.");
        return;
    }
    // ~18% on Search, ~8% on a clear; pity after 8 cleared Rounds chambers with no new voice.
    const bool pity = memoryCrawlDry_ >= 8;
    const bool hit = pity || getRandomInt(1, 100) <= (search ? 18 : 8);
    if (!hit) {
        if (!search) memoryCrawlDry_++;
        return;
    }
    const char* id = pool[static_cast<size_t>(getRandomInt(0, static_cast<int>(pool.size()) - 1))];
    if (grantMemory(id, true)) memoryCrawlDry_ = 0;
}

void Game::memoryOnDeepDepth(int depth) {
    if (dmOnlyTable_ || !isEndlessDeep()) return;
    for (const auto& m : memory::kMemories)
        if (m.source == memory::SRC_DEEP && m.beat == depth) grantMemory(m.id, true);
}

void Game::memoryOnArenaApex() {
    if (dmOnlyTable_ || !isArena() || !memoryContextKnown_) return;
    std::vector<const char*> pool;
    for (const auto& m : memory::kMemories)
        if (m.source == memory::SRC_ARENA && !memoryOwned_.count(m.id)) pool.push_back(m.id);
    if (pool.empty()) return;
    grantMemory(pool[static_cast<size_t>(getRandomInt(0, static_cast<int>(pool.size()) - 1))], true);
}

void Game::memoryOnRaidBoss(const std::string& foeId) {
    if (dmOnlyTable_) return;
    const char* ids[3];
    memory::raidLoreFor(foeId, ids);
    for (const char* id : ids) if (id) grantMemory(id, id == ids[2]);
}

std::string Game::progressStoryMemories() const {
    const int b = questBeat_;
    const bool scripted = isScriptedSoloBeat(b);
    auto actDone = [&](int beat) {
        if (beat <= 6) return questComplete_;
        if (beat <= 13) return questAct2Complete_;
        if (beat <= 19) return questAct3Complete_;
        return questAct4Complete_;
    };
    auto itemFlag = [&](int beat) {
        switch (beat) {
            case 3: return questCryptKeyFound_;
            case 11: return questAct2LedgerFound_;
            case 17: return questAct3SealFound_;
            case 23: return questAct4EmberFound_;
            default: return false;
        }
    };
    const bool clearedHere = !gameOver_ && enemies_.empty();
    std::string ids;
    for (const auto& m : memory::kMemories) {
        if (m.source != memory::SRC_STORY) continue;
        const bool passed = actDone(m.beat) || (scripted && m.beat < b);
        const bool here = scripted && m.beat == b;
        bool got = false;
        switch (m.trigger) {
            case memory::T_ENTER: got = passed || here; break;
            case memory::T_CLEAR: got = passed || (here && clearedHere); break;
            case memory::T_CLEAR_OR_SEARCH: got = passed || (here && (clearedHere || roomSearchUsed_)); break;
            // Act 1's knife counts once Act 1 is done (approved: all six Act 1 memories).
            case memory::T_SEARCH_FLAG: got = itemFlag(m.beat) || actDone(m.beat); break;
            case memory::T_SEARCH_ONLY: got = here && roomSearchUsed_; break;
            case memory::T_WELL_CHOICE: got = wellChoice_ != WELL_UNDECIDED; break;
            default: break;
        }
        if (m.beat == 18 && !got) got = scripted && b > 18; // maren_named also on entering 19
        if (!got) continue;
        if (!ids.empty()) ids += ",";
        ids += m.id;
    }
    return getStoryChoices() + "|" + ids;
}

std::string Game::previewStoryMemories(const std::string& save) {
    try {
        Game tmp{LoadOnlyTag{}};
        tmp.parseSave(save);
        return tmp.progressStoryMemories();
    } catch (...) {
        return "";
    }
}

bool Game::endgameContentAllowed() const {
    // Focused endgame runs always treat endgame as open.
    // Act 4 is the story-complete gate; Act 3-complete saves keep endgame (grandfather).
    if (isFocusedEndgameRun()) return true;
    return questAct4Complete_ || questAct3Complete_;
}

bool Game::trySoloQuestSearch(Character* hero) {
    if (!hero || !isSoloQuestScripted()) return false;
    const auto beat = static_cast<SoloQuestBeat>(questBeat_);

    if (beat == SoloQuestBeat::MILLHOLLOW) {
        hero->gold += 8;
        lastEvent_ = hero->name + " finds a black funeral ribbon snagged on a thorn — Wren's. Her trail runs toward Hollowbarrow Crypt.";
        dmSay("The thieves ran for Hollowbarrow Crypt, and Wren ran after them. Follow.");
        addJournalEntry("Clue: Wren's funeral ribbon on the road; the thieves fled toward Hollowbarrow Crypt.");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::CRYPT_DOORS) {
        questCryptKeyFound_ = true;
        lastEvent_ = hero->name + " pries a cold iron rune-key from the ash-stained lintel.";
        dmSay("The rune-key turns. Crypt wards sigh open — the Bone Gallery waits beyond.");
        addJournalEntry("Found the Ashen Rune-Key at Hollowbarrow's doors.");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::LANTERN_VAULT && questLanternRecovered_) {
        hero->gold += 12;
        lastEvent_ = hero->name + " finds spare oil and 12 gold near the plinth.";
        dmSay("A pouch of oil and coin — useful, but the lantern itself is the true prize.");
        addJournalEntry(lastEvent_);
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::RESOLUTION) {
        lastEvent_ = hero->name + " finds a grey wax seal on the floor where the masked watcher stood.";
        dmSay("Someone was keeping count tonight. Onward leads to the green, and the Tithe.");
        addJournalEntry("Searched the Ashen Shrine — Act 1 is won.");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::ACT2_GREEN) {
        hero->gold += 10;
        lastEvent_ = hero->name + " overhears a collector mutter: \"The loft ledger above the weir seals every name.\"";
        dmSay("A clue: the Tithe keeps its book in the mill loft above the weir. Every name is in it.");
        addJournalEntry("Clue: the Tithe's ledger is in the mill loft above the weir.");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::ACT2_LOFT) {
        questAct2LedgerFound_ = true;
        hero->gold += 15;
        lastEvent_ = hero->name + " takes the soul-ledger from the shelf. Your name is the next entry, the ink still wet.";
        dmSay("You have the ledger. Onward to the Collector's Hall — Sallow Venn holds the ledger-seal.");
        wrenSay("Surplus. They wrote surplus.");
        addJournalEntry("Recovered the soul-ledger (questAct2LedgerFound). Your name was next.");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::ACT2_SETTLED) {
        lastEvent_ = hero->name + " finds only ash in the clay bowl — the soul-ledger is gone.";
        dmSay("Nothing more to take. Onward leads to the old well — Emberdeep.");
        addJournalEntry("Searched after The Tithe Broken — Act 2 already won.");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::ACT3_RUMOR) {
        hero->gold += 12;
        lastEvent_ = hero->name + " hears: \"Past the roots there's a seal — the First Lamplighter's signet. It's the only key that shuts the well kindly.\"";
        dmSay("Clue: the Ember Seal lies past the Root Labyrinth. Recover it, then face what guards the wound.");
        addJournalEntry("Clue: the First Lamplighter's Ember Seal lies past the Root Labyrinth.");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::ACT3_RELIC) {
        questAct3SealFound_ = true;
        hero->gold += 20;
        lastEvent_ = hero->name + " lifts the Ember Seal. Around the rim: \"Maren Vey bound me here. Do not let her drink again.\"";
        dmSay("You have the Ember Seal. Onward to the Breach Threshold — the First Lamplighter waits.");
        wrenSay("She has a name. The watcher from the crypt has a name.");
        addJournalEntry("Recovered the Ember Seal (questAct3SealFound): \"Maren Vey bound me here.\"");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::ACT3_SEALED) {
        lastEvent_ = hero->name + " finds cooling ash where the wound once roared — the seal holds.";
        dmSay("Nothing more to take. Onward leads west — Act 4: Ashwake Vigil.");
        addJournalEntry("Searched after Breach Sealed — Act 3 already won.");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::ACT4_WATCH) {
        hero->gold += 14;
        lastEvent_ = hero->name + " hears an elder whisper: \"Under the fen-tower there's an ember. It isn't a flame to light. It's the valve.\"";
        dmSay("Clue: the Vigil Ember is the siphon's valve. Recover it past Greyfen and reverse the draw.");
        addJournalEntry("Clue: the Vigil Ember under the fen-tower is the valve of the siphon.");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::ACT4_EMBER) {
        questAct4EmberFound_ = true;
        hero->gold += 25;
        lastEvent_ = hero->name + " lifts the Vigil Ember, and a scrap of Tithe rite in an old, careful hand.";
        dmSay("You have the Vigil Ember. Onward to the Ashwake Threshold — the Bell-Herald waits.");
        wrenSay("This isn't a light. It's a throat.");
        addJournalEntry("Recovered the Vigil Ember (questAct4EmberFound) and a siphon prayer in Maren's hand.");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::ACT4_KINDLED) {
        lastEvent_ = hero->name + " finds only ash on the stair where Maren Vey stood.";
        dmSay("Nothing more to take. Follow the ash — Onward opens endgame rooms.");
        addJournalEntry("Searched after Vigil Kindled — Act 4 already won.");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    return false; // use default Search loot/trap
}

void Game::generateRoomDescription() {
    if (isSoloQuestScripted()) {
        // Description already set by applySoloQuestRoom; keep procedural helper for post-quest / DM.
        return;
    }
    if (questAct4Complete_ || questAct3Complete_ || questAct2Complete_
        || (questComplete_ && questBeat_ == static_cast<int>(SoloQuestBeat::POST_QUEST))) {
        std::vector<std::string> adjectives = {"dark", "damp", "ancient", "dusty", "eerie", "forgotten", "cursed"};
        std::vector<std::string> rooms = {"chamber", "hallway", "library", "crypt", "vault", "shrine", "laboratory"};
        std::stringstream ss;
        const char* tag = questAct4Complete_ ? "[Endgame] "
            : (questAct3Complete_ ? "[After Act 3] "
            : (questAct2Complete_ ? "[After Act 2] " : "[Main quest done] "));
        ss << tag << "You press into a " << adjectives[static_cast<size_t>(getRandomInt(0, static_cast<int>(adjectives.size()) - 1))]
           << " " << rooms[static_cast<size_t>(getRandomInt(0, static_cast<int>(rooms.size()) - 1))] << ". ";
        roomDescription_ = ss.str();
        dmSay("Room " + std::to_string(roomCount_) + ": " + roomDescription_ + "What do you do?");
        return;
    }
    std::vector<std::string> adjectives = {"dark", "damp", "ancient", "dusty", "eerie", "forgotten", "cursed"};
    std::vector<std::string> rooms = {"chamber", "hallway", "library", "crypt", "vault", "shrine", "laboratory"};
    std::stringstream ss;
    ss << "You stand in a " << adjectives[static_cast<size_t>(getRandomInt(0, static_cast<int>(adjectives.size()) - 1))]
       << " " << rooms[static_cast<size_t>(getRandomInt(0, static_cast<int>(rooms.size()) - 1))] << ". ";
    roomDescription_ = ss.str();
    dmSay("Room " + std::to_string(roomCount_) + ": " + roomDescription_ + "What do you do?");
}

void Game::spawnRoomContent() {
    enemies_.clear();
    shopInventory_.clear();
    isMerchantRoom_ = false;
    roomSearchUsed_ = false; roomRestUsed_ = false;
    clearSetPiecePending();

    if (isSoloQuestScripted()) {
        spawnSoloQuestEnemies();
        return;
    }

    if (roomCount_ > 1 && roomCount_ % 4 == 0) {
        isMerchantRoom_ = true;
        roomDescription_ = "You find a rare pocket of safety. A weary Merchant awaits.";
        dmSay("A lantern glows ahead. A traveling merchant offers goods — and a chance for a longer rest from the grind.");
        restockShop();
        return;
    }

    // Boss chance for deep crawl / post-quest procedural rooms (gated so beginners are not soft-locked).
    maybeSpawnBossEncounter();
    if (!enemies_.empty()) return;

    // Authored set-pieces (AA step 2): bias mid-crawl / post-story; fall back to light trash.
    if (trySpawnAuthoredEncounter()) return;

    auto goblin = newEnemy("enemy_wickthief", CharacterClass::ROGUE);
    goblin->maxHp += (roomCount_ * 2);
    goblin->currentHp = goblin->maxHp;
    enemies_.push_back(std::move(goblin));

    if (roomCount_ > 5) {
        auto skeleton = newEnemy("enemy_ashbound", CharacterClass::FIGHTER);
        skeleton->maxHp += roomCount_;
        skeleton->currentHp = skeleton->maxHp;
        skeleton->armorClass += (roomCount_ / 10);
        enemies_.push_back(std::move(skeleton));
    }
}

void Game::rollInitiative() {
    syncStatRules(); // v2.31: solo heroes use the universal stat rules
    for (auto& p : players_) {
        if (!p) continue;
        p->fightShieldUsed = false;
        p->specialUsedThisCombat = false;
        // v2.31 Signature (main stat 20+): one free class-special use per battle.
        p->signatureUses = p->signatureTier() >= 1 ? 1 : 0;
    }

    turnOrder_.clear();
    if (isMerchantRoom_) return;
    bool freshFight = false;
    for (const auto& e : enemies_) if (e && e->curvePending) { freshFight = true; break; }
    applyEncounterCurve(); // v2.29: freshly spawned foes get their EL / role numbers (solo only)
    if (freshFight) {
        // v2.30: a new fight — statuses from the last room are gone, the round count restarts.
        combatRound_ = 1;
        fledFoes_ = 0;
        roomFoeTotal_ = static_cast<int>(enemies_.size());
        for (auto& p : players_) if (p) p->status = Character::Statuses{};
    }

    for (auto& p : players_) {
        if (p->isDead) continue; // fallen heroes stay out of initiative
        // v2.29: effective DEX (Legendary DEX counts — audit bug d).
        p->initiative = (getRandomInt(1, 20)) + p->initiativeModifier();
        turnOrder_.push_back(p.get());
    }
    for (auto& e : enemies_) {
        e->initiative = (getRandomInt(1, 20)) + e->initiativeModifier();
        turnOrder_.push_back(e.get());
    }
    std::sort(turnOrder_.begin(), turnOrder_.end(), [](Character* a, Character* b) {
        return a->initiative > b->initiative;
    });
    currentTurnIndex_ = 0;
    if (enemyAiActive()) {
        // v2.30: every foe shows its first intent before anyone acts.
        for (auto& e : enemies_) {
            if (!e || e->curveEL <= 0) continue;
            updateBossPhase(*e, false);
            if (e->ai.move == 0) planEnemyMove(*e);
        }
    }
}

void Game::rebuildTurnOrder() {
    // Capture identity before we clear; never touch turnOrder_ after players_/enemies_ were freed.
    const int savedIndex = currentTurnIndex_;
    std::string currentUid;
    if (!turnOrder_.empty()
        && currentTurnIndex_ >= 0
        && static_cast<size_t>(currentTurnIndex_) < turnOrder_.size()
        && turnOrder_[static_cast<size_t>(currentTurnIndex_)] != nullptr) {
        currentUid = turnOrder_[static_cast<size_t>(currentTurnIndex_)]->uid;
    }

    turnOrder_.clear();
    for (auto& p : players_) {
        if (!p->isDead) turnOrder_.push_back(p.get());
    }
    for (auto& e : enemies_) turnOrder_.push_back(e.get());

    std::sort(turnOrder_.begin(), turnOrder_.end(), [](Character* a, Character* b) {
        return a->initiative > b->initiative;
    });

    if (!currentUid.empty()) {
        for (size_t i = 0; i < turnOrder_.size(); ++i) {
            if (turnOrder_[i]->uid == currentUid) {
                currentTurnIndex_ = static_cast<int>(i);
                return;
            }
        }
    }
    // After deserialize turnOrder_ was cleared first, so fall back to the saved index.
    if (savedIndex >= 0 && static_cast<size_t>(savedIndex) < turnOrder_.size()) {
        currentTurnIndex_ = savedIndex;
    } else {
        currentTurnIndex_ = 0;
    }
}

Character* Game::findCharacter(const std::string& name) {
    for (auto& p : players_) {
        if (p->name == name) return p.get();
    }
    return nullptr;
}

void Game::restockShop() {
    shopInventory_.clear();
    int depthBonus = std::max(0, roomCount_ / 4);
    int preferA = -1, preferB = -1;
    partyPreferClasses(preferA, preferB);

    auto isPartyClass = [&](int cls) {
        if (cls < 0) return true; // Any
        return cls == preferA || cls == preferB;
    };
    auto classWeapon = [&](int cls, ItemRarity r, int bonus) {
        if (cls == 0) return Item::make("Fighter's Arming Sword", ItemType::WEAPON, bonus, r, 0);
        if (cls == 1) return Item::make("Wizard's Focus Rod", ItemType::WEAPON, bonus, r, 1);
        if (cls == 2) return Item::make("Rogue's Stiletto", ItemType::WEAPON, bonus, r, 2);
        if (cls == 3) return Item::make("Cleric's Warhammer", ItemType::WEAPON, bonus, r, 3);
        return Item::make("Bard's Stage Rapier", ItemType::WEAPON, bonus, r, 4);
    };
    auto classArmor = [&](int cls, ItemRarity r, int bonus) {
        if (cls == 0) return Item::make("Knight Plate", ItemType::ARMOR, bonus, r, 0);
        if (cls == 1) return Item::make("Scholar Robes", ItemType::ARMOR, bonus, r, 1);
        if (cls == 2) return Item::make("Veil of Shadows", ItemType::ARMOR, bonus, r, 2);
        if (cls == 3) return Item::make("Reliquary Mail", ItemType::ARMOR, bonus, r, 3);
        return Item::make("Troubadour Coat", ItemType::ARMOR, bonus, r, 4);
    };

    // Always: Any commons + potions (usable by everyone).
    shopInventory_.push_back(Item::make("Traveler Blade", ItemType::WEAPON, depthBonus, ItemRarity::COMMON, -1));
    shopInventory_.push_back(Item::make("Padded Vest", ItemType::ARMOR, depthBonus, ItemRarity::COMMON, -1));
    shopInventory_.push_back(Item::make("Healing Draught", ItemType::POTION, 8 + depthBonus, ItemRarity::COMMON, -1));
    shopInventory_.push_back(Item::make("Greater Potion", ItemType::POTION, 14 + depthBonus, ItemRarity::UNCOMMON, -1));

    // Party stock: commons/uncommons dominate; Rare/Epic gated deeper (#46). No Legendary in shop.
    if (preferA >= 0) {
        shopInventory_.push_back(classWeapon(preferA, ItemRarity::UNCOMMON, depthBonus + 1));
        if (roomCount_ >= 10)
            shopInventory_.push_back(classArmor(preferA, ItemRarity::RARE, depthBonus + 2));
        else if (roomCount_ >= 5)
            shopInventory_.push_back(classArmor(preferA, ItemRarity::UNCOMMON, depthBonus + 1));
    }
    if (preferB >= 0 && preferB != preferA) {
        shopInventory_.push_back(classWeapon(preferB, ItemRarity::UNCOMMON, depthBonus + 1));
        if (roomCount_ >= 12)
            shopInventory_.push_back(classArmor(preferB, ItemRarity::RARE, depthBonus + 2));
        else if (roomCount_ >= 6)
            shopInventory_.push_back(classArmor(preferB, ItemRarity::UNCOMMON, depthBonus + 1));
    }
    int off = -1;
    for (int c = 0; c < 5; ++c) {
        if (!isPartyClass(c)) { off = c; break; }
    }
    if (off >= 0) {
        shopInventory_.push_back(classWeapon(off, ItemRarity::UNCOMMON, depthBonus + 1));
    }
    if (roomCount_ >= 16 && preferA >= 0) {
        shopInventory_.push_back(classWeapon(preferA, ItemRarity::EPIC, depthBonus + 3));
    }
    if (roomCount_ >= 20 && preferB >= 0) {
        shopInventory_.push_back(classArmor(preferB, ItemRarity::EPIC, depthBonus + 4));
    }
}

bool Game::buyItem(const std::string& playerName, int itemIndex) {
    if (gameOver_) return false;
    if (!isMerchantRoom_ || itemIndex < 0 || static_cast<size_t>(itemIndex) >= shopInventory_.size()) return false;
    Character* hero = findCharacter(playerName);
    if (!hero || hero->isDead) return false;

    auto item = shopInventory_[static_cast<size_t>(itemIndex)];
    int cost = item->shopCost();
    if (hero->statRules) cost = stats::buyPrice(cost, hero->effMod(5)); // v2.31 Barter

    if (hero->gold >= cost) {
        hero->gold -= cost;
        if (item->type == ItemType::POTION) {
            hero->heal(item->bonus);
            lastEvent_ = hero->name + " purchased and drank " + item->name + "!";
        } else {
            hero->addToInventory(item);
            lastEvent_ = hero->name + " purchased " + item->name + " [" + item->rarityLabel() + "] — open Inventory to equip.";
        }
        addJournalEntry(hero->name + " bought " + item->name + " for " + std::to_string(cost) + " gold.");
        shopInventory_.erase(shopInventory_.begin() + itemIndex);
        return true;
    }
    lastEvent_ = "Not enough gold!";
    return false;
}

std::string Game::getShopManifest() const {
    if (!isMerchantRoom_) return "";
    std::stringstream ss;
    for (size_t i = 0; i < shopInventory_.size(); ++i) {
        const auto& it = shopInventory_[i];
        // idx:name|bonus|type|rarity|class|cost
        ss << i << ":" << it->name << "|" << it->bonus << "|"
           << (it->type == ItemType::WEAPON ? "Weapon" : (it->type == ItemType::ARMOR ? "Armor" : "Potion"))
           << "|" << it->rarityLabel() << "|" << it->classLabel() << "|"
           << ((!players_.empty() && players_[0] && players_[0]->statRules)
                   ? stats::buyPrice(it->shopCost(), players_[0]->effMod(5)) : it->shopCost()) << ";"; // v2.31 Barter
    }
    return ss.str();
}

std::string Game::getInventoryManifest(const std::string& playerName) const {
    Character* hero = nullptr;
    for (const auto& p : players_) {
        if (p && p->name == playerName) { hero = p.get(); break; }
    }
    if (!hero) return "";
    std::stringstream ss;
    auto emit = [&](const std::shared_ptr<Item>& it, const char* slot, int index) {
        if (!it) return;
        // idx:name|bonus|type|rarity|class|slot|upgradeLevel|upgradeCost|sellPrice
        // idx:name|bonus|type|rarity|class|slot|upgradeLevel|upgradeCost|sellPrice|legText
        std::string leg = (it->type == ItemType::POTION)
            ? it->potionEffectText()
            : it->legendaryBonusText();
        for (char& ch : leg) { if (ch == ';' || ch == '|' || ch == ':') ch = ','; }
        ss << index << ":" << it->name << "|" << it->bonus << "|"
           << (it->type == ItemType::WEAPON ? "Weapon" : (it->type == ItemType::ARMOR ? "Armor" : "Potion"))
           << "|" << it->rarityLabel() << "|" << it->classLabel() << "|" << slot
           << "|" << it->upgradeLevel << "|" << it->upgradeCost()
           << "|" << it->sellPrice(hero->statRules ? hero->effMod(5) : 0) << "|" << leg << ";";
    };
    // Equipped first with negative-ish slots encoded as weapon/armor indices in slot field
    emit(hero->equippedWeapon, "weapon", -1);
    emit(hero->equippedArmor, "armor", -2);
    for (size_t i = 0; i < hero->inventory.size(); ++i) {
        emit(hero->inventory[i], "bag", static_cast<int>(i));
    }
    return ss.str();
}

bool Game::equipInventoryItem(const std::string& playerName, int invIndex) {
    if (gameOver_) return false;
    Character* hero = findCharacter(playerName);
    if (!hero || hero->isDead) return false;
    if (invIndex < 0 || static_cast<size_t>(invIndex) >= hero->inventory.size()) {
        lastEvent_ = "No such item.";
        return false;
    }
    auto item = hero->inventory[static_cast<size_t>(invIndex)];
    if (!item || item->type == ItemType::POTION) {
        lastEvent_ = "Cannot equip that.";
        return false;
    }
    if (!item->canEquip(hero->characterClass)) {
        lastEvent_ = item->classLabel() + " only.";
        return false;
    }
    hero->inventory.erase(hero->inventory.begin() + invIndex);
    if (item->type == ItemType::WEAPON) {
        if (hero->equippedWeapon) hero->addToInventory(hero->equippedWeapon);
        hero->equippedWeapon = item;
        hero->refreshDerivedStats(); // Legendary DEX/CON on weapons can affect AC/HP
    } else {
        if (hero->equippedArmor) hero->addToInventory(hero->equippedArmor);
        hero->equippedArmor = item;
        hero->refreshDerivedStats();
    }
    lastEvent_ = hero->name + " equips " + item->getDescription() + ".";
    return true;
}

bool Game::unequipSlot(const std::string& playerName, int slot) {
    if (gameOver_) return false;
    Character* hero = findCharacter(playerName);
    if (!hero || hero->isDead) return false;
    if (slot == 0) {
        if (!hero->equippedWeapon) { lastEvent_ = "No weapon equipped."; return false; }
        hero->addToInventory(hero->equippedWeapon);
        lastEvent_ = hero->name + " unequips " + hero->equippedWeapon->name + ".";
        hero->equippedWeapon = nullptr;
        hero->refreshDerivedStats();
        return true;
    }
    if (slot == 1) {
        if (!hero->equippedArmor) { lastEvent_ = "No armor equipped."; return false; }
        hero->addToInventory(hero->equippedArmor);
        lastEvent_ = hero->name + " unequips " + hero->equippedArmor->name + ".";
        hero->equippedArmor = nullptr;
        hero->refreshDerivedStats();
        return true;
    }
    lastEvent_ = "Bad slot.";
    return false;
}

bool Game::upgradeInventoryItem(const std::string& playerName, int invIndex) {
    if (gameOver_) return false;
    Character* hero = findCharacter(playerName);
    if (!hero || hero->isDead) return false;
    if (invIndex < 0 || static_cast<size_t>(invIndex) >= hero->inventory.size()) {
        lastEvent_ = "No such item.";
        return false;
    }
    std::string err;
    auto& item = hero->inventory[static_cast<size_t>(invIndex)];
    int cost = item ? item->upgradeCost() : 0;
    if (!hero->upgradeOwnedItem(item, err)) {
        lastEvent_ = err;
        return false;
    }
    lastEvent_ = hero->name + " upgrades " + item->name + " to +" + std::to_string(item->bonus)
        + " for " + std::to_string(cost) + "g.";
    addJournalEntry(lastEvent_);
    return true;
}

bool Game::upgradeEquippedItem(const std::string& playerName, int slot) {
    if (gameOver_) return false;
    Character* hero = findCharacter(playerName);
    if (!hero || hero->isDead) return false;
    std::shared_ptr<Item>* ptr = (slot == 0) ? &hero->equippedWeapon : (slot == 1 ? &hero->equippedArmor : nullptr);
    if (!ptr || !*ptr) { lastEvent_ = "Nothing equipped there."; return false; }
    std::string err;
    int cost = (*ptr)->upgradeCost();
    if (!hero->upgradeOwnedItem(*ptr, err)) {
        lastEvent_ = err;
        return false;
    }
    lastEvent_ = hero->name + " upgrades " + (*ptr)->name + " to +" + std::to_string((*ptr)->bonus)
        + " for " + std::to_string(cost) + "g.";
    addJournalEntry(lastEvent_);
    return true;
}

bool Game::sellInventoryItem(const std::string& playerName, int invIndex) {
    if (gameOver_) return false;
    Character* hero = findCharacter(playerName);
    if (!hero || hero->isDead) return false;
    if (invIndex < 0 || static_cast<size_t>(invIndex) >= hero->inventory.size()) {
        lastEvent_ = "No such item.";
        return false;
    }
    auto item = hero->inventory[static_cast<size_t>(invIndex)];
    if (!item) {
        lastEvent_ = "No such item.";
        return false;
    }
    int price = item->sellPrice(hero->statRules ? hero->effMod(5) : 0); // v2.31 Barter
    std::string itemName = item->getDescription();
    hero->inventory.erase(hero->inventory.begin() + invIndex);
    hero->gold += price;
    lastEvent_ = hero->name + " sells " + itemName + " for " + std::to_string(price) + "g.";
    addJournalEntry(lastEvent_);
    return true;
}

bool Game::sellEquippedItem(const std::string& playerName, int slot) {
    if (gameOver_) return false;
    Character* hero = findCharacter(playerName);
    if (!hero || hero->isDead) return false;
    std::shared_ptr<Item>* ptr = (slot == 0) ? &hero->equippedWeapon : (slot == 1 ? &hero->equippedArmor : nullptr);
    if (!ptr || !*ptr) {
        lastEvent_ = "Nothing equipped there.";
        return false;
    }
    auto item = *ptr;
    int price = item->sellPrice(hero->statRules ? hero->effMod(5) : 0); // v2.31 Barter
    std::string itemName = item->getDescription();
    // Unequip first: clear slot so combat falls back to fists / unarmored AC.
    *ptr = nullptr;
    hero->refreshDerivedStats();
    hero->gold += price;
    lastEvent_ = hero->name + " sells " + itemName + " for " + std::to_string(price) + "g."
        + (slot == 0 ? " (fists ready)" : "");
    addJournalEntry(hero->name + " sold equipped " + itemName + " for " + std::to_string(price) + "g.");
    return true;
}

void Game::partyPreferClasses(int& outA, int& outB) const {
    outA = -1;
    outB = -1;
    if (!players_.empty() && players_[0]) {
        outA = static_cast<int>(players_[0]->characterClass);
    }
    if (Character* c = findNpcCompanion()) {
        outB = static_cast<int>(c->characterClass);
    }
}

Character* Game::findTransferAlly(const Character* from) const {
    if (!from) return nullptr;
    // Prefer NPC companion when transferring from the hero.
    if (Character* npc = findNpcCompanion()) {
        if (npc != from) return npc;
    }
    for (const auto& p : players_) {
        if (p && p.get() != from) return p.get();
    }
    return nullptr;
}

void Game::deliverItemToAlly(Character* ally, std::shared_ptr<Item> item) {
    if (!ally || !item) return;
    if (item->type == ItemType::POTION) {
        ally->addToInventory(item);
        return;
    }
    if (item->canEquip(ally->characterClass)) {
        if (item->type == ItemType::WEAPON) {
            if (ally->equippedWeapon) ally->addToInventory(ally->equippedWeapon);
            ally->equippedWeapon = item;
            ally->refreshDerivedStats();
            return;
        }
        if (item->type == ItemType::ARMOR) {
            if (ally->equippedArmor) ally->addToInventory(ally->equippedArmor);
            ally->equippedArmor = item;
            ally->refreshDerivedStats();
            return;
        }
    }
    ally->addToInventory(item);
}

bool Game::transferInventoryItemToAlly(const std::string& fromName, int invIndex) {
    if (gameOver_) return false;
    Character* from = findCharacter(fromName);
    if (!from || from->isDead) return false;
    Character* ally = findTransferAlly(from);
    if (!ally) {
        lastEvent_ = "No companion to transfer to.";
        return false;
    }
    if (invIndex < 0 || static_cast<size_t>(invIndex) >= from->inventory.size()) {
        lastEvent_ = "No such item.";
        return false;
    }
    auto item = from->inventory[static_cast<size_t>(invIndex)];
    if (!item) {
        lastEvent_ = "No such item.";
        return false;
    }
    from->inventory.erase(from->inventory.begin() + invIndex);
    std::string label = item->getDescription();
    bool equipped = item->canEquip(ally->characterClass) && item->type != ItemType::POTION;
    deliverItemToAlly(ally, item);
    if (equipped) {
        lastEvent_ = from->name + " gives " + label + " to " + ally->name + " (equipped).";
    } else {
        lastEvent_ = from->name + " gives " + label + " to " + ally->name + ".";
    }
    dmSay(lastEvent_);
    addJournalEntry(lastEvent_);
    return true;
}

bool Game::transferEquippedItemToAlly(const std::string& fromName, int slot) {
    if (gameOver_) return false;
    Character* from = findCharacter(fromName);
    if (!from || from->isDead) return false;
    Character* ally = findTransferAlly(from);
    if (!ally) {
        lastEvent_ = "No companion to transfer to.";
        return false;
    }
    std::shared_ptr<Item>* ptr = (slot == 0) ? &from->equippedWeapon : (slot == 1 ? &from->equippedArmor : nullptr);
    if (!ptr || !*ptr) {
        lastEvent_ = "Nothing equipped there.";
        return false;
    }
    auto item = *ptr;
    *ptr = nullptr;
    from->refreshDerivedStats();
    std::string label = item->getDescription();
    bool willEquip = item->canEquip(ally->characterClass);
    deliverItemToAlly(ally, item);
    if (willEquip) {
        lastEvent_ = from->name + " gives " + label + " to " + ally->name + " (equipped).";
    } else {
        lastEvent_ = from->name + " gives " + label + " to " + ally->name + ".";
    }
    dmSay(lastEvent_);
    addJournalEntry(lastEvent_);
    return true;
}

void Game::markBossSeen(const std::string& enemyId) {
    // Boss-seen save flags key off the stable enemy ID.
    if (enemyId == "boss_wick_king") bossSeenGk_ = true;
    else if (enemyId == "boss_prior") bossSeenSk_ = true;
    else if (enemyId == "boss_hollow_crown") bossSeenHollow_ = true;
    else if (enemyId == "boss_ember_hydra") bossSeenHydra_ = true;
    else if (enemyId == "boss_nightfang") bossSeenNightfang_ = true;
    // The Drake flag doubles as "a tier-3+ boss was seen" for any other high-tier boss.
    else if (enemyId == "boss_drake" || LootSystem::bossTier(enemyId) >= 3) bossSeenDrake_ = true;
}

std::unique_ptr<Character> Game::newEnemy(const std::string& enemyId, CharacterClass cl, const std::string& uidTag) {
    std::string uid = enemyId + "-" + (uidTag.empty() ? std::string() : uidTag + "-")
        + std::to_string(getRandomInt(0, 1000000));
    auto foe = std::make_unique<Character>(bestiary::displayName(enemyId), cl, uid);
    foe->enemyId = enemyId;
    foe->applyStatsForLevel(); // foes skip the hero level-1 HP floor
    foe->currentHp = foe->maxHp;
    foe->curvePending = true;  // rollInitiative() fits it to the encounter curve (solo only)
    return foe;
}

void Game::spawnNamedBoss(const std::string& enemyId) {
    markBossSeen(enemyId);
    const int tier = LootSystem::bossTier(enemyId);
    CharacterClass cl = CharacterClass::FIGHTER;
    if (tier == 1) cl = CharacterClass::ROGUE;
    else if (tier == 3) cl = CharacterClass::WIZARD;
    else if (tier >= 4) {
        if (enemyId == "boss_ember_hydra") cl = CharacterClass::WIZARD;
        else if (enemyId == "boss_nightfang") cl = CharacterClass::ROGUE;
        else cl = CharacterClass::FIGHTER;
    }
    auto foe = newEnemy(enemyId, cl);
    int hpBonus = 10 + tier * 8 + roomCount_;
    int acBonus = std::min(6, tier);
    int atkDelta = (tier >= 3) ? 0 : -1;
    if (tier >= 4) {
        hpBonus += 12 + (tier - 3) * 10;
        acBonus += 1;
        atkDelta += 1;
    }
    if (difficulty_ <= static_cast<int>(Difficulty::EASY)) {
        hpBonus = std::max(8, hpBonus - 8);
        acBonus = std::max(0, acBonus - 1);
        atkDelta -= 1;
    } else if (difficulty_ == static_cast<int>(Difficulty::MEDIUM)) {
        hpBonus = std::max(10, hpBonus - 4);
    } else if (difficulty_ >= static_cast<int>(Difficulty::HARD)) {
        hpBonus += 6 + tier * 2;
        acBonus += 1;
    }
    foe->maxHp += hpBonus;
    foe->currentHp = foe->maxHp;
    foe->armorClass += acBonus;
    foe->resources = std::min(foe->maxResources, std::max(1, tier));
    if (cl == CharacterClass::ROGUE) foe->attributes.dexterity = std::max(8, foe->attributes.dexterity + atkDelta);
    else if (cl == CharacterClass::WIZARD) foe->attributes.intelligence = std::max(8, foe->attributes.intelligence + atkDelta);
    else if (cl == CharacterClass::CLERIC) foe->attributes.wisdom = std::max(8, foe->attributes.wisdom + atkDelta);
    else if (cl == CharacterClass::BARD) foe->attributes.charisma = std::max(8, foe->attributes.charisma + atkDelta);
    else foe->attributes.strength = std::max(8, foe->attributes.strength + atkDelta);
    enemies_.push_back(std::move(foe));
}

bool Game::hasLivingBossEnemy() const {
    for (const auto& e : enemies_) {
        if (e && !e->isDead && LootSystem::isBossId(e->enemyId)) return true;
    }
    return false;
}

void Game::clearSetPiecePending() {
    activeSetPieceId_ = 0;
    pendingSetPieceXpBonus_ = 0;
    pendingSetPieceGoldBonus_ = 0;
    pendingSetPieceGuaranteedLoot_ = false;
    pendingSetPieceClearFeed_.clear();
}

bool Game::trySpawnAuthoredEncounter() {
    // Only procedural crawl / post-quest / DM rooms — never rewrite Easy early story beats.
    if (isSoloQuestScripted()) return false;
    if (isFocusedEndgameRun()) return false;
    if (roomCount_ < 3) return false;

    const bool postStory = questComplete_ || questAct2Complete_ || questAct3Complete_ || questAct4Complete_
        || questBeat_ == static_cast<int>(SoloQuestBeat::POST_QUEST);
    const bool easy = difficulty_ <= static_cast<int>(Difficulty::EASY);

    // Chance rises with depth; Easy early crawl stays mostly trash; post-story bias.
    int chance = 18;
    if (roomCount_ >= 6) chance = 32;
    if (roomCount_ >= 10) chance = 40;
    if (postStory) chance += 18;
    if (easy && roomCount_ < 6) chance = std::max(10, chance - 12);
    if (chance > 55) chance = 55;
    if (getRandomInt(1, 100) > chance) return false;

    size_t n = 0;
    const EncounterAuthorship::TemplateInfo* tab = EncounterAuthorship::table(n);
    std::vector<size_t> eligible;
    eligible.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        if (roomCount_ < tab[i].minRoomDepth) continue;
        // King's Herald: prefer mid+ depth; Ogre needs a bit more room.
        eligible.push_back(i);
        if (tab[i].preferPostStory && postStory) eligible.push_back(i); // weight
        if (roomCount_ >= tab[i].minRoomDepth + 3) eligible.push_back(i);
    }
    if (eligible.empty()) return false;

    const EncounterAuthorship::TemplateInfo& t = tab[eligible[static_cast<size_t>(getRandomInt(0, static_cast<int>(eligible.size()) - 1))]];

    auto scaleHp = [&](Character& c, int flat) {
        c.maxHp += flat + (roomCount_ / 2);
        if (easy) c.maxHp = std::max(4, c.maxHp - 4);
        c.currentHp = c.maxHp;
    };
    auto pushFoe = [&](const std::string& enemyId, CharacterClass cl, int hpFlat, int acFlat, int atkDelta) {
        auto foe = newEnemy(enemyId, cl);
        scaleHp(*foe, hpFlat);
        foe->armorClass += acFlat;
        foe->resources = 0;
        if (cl == CharacterClass::ROGUE) foe->attributes.dexterity = std::max(8, foe->attributes.dexterity + atkDelta);
        else if (cl == CharacterClass::WIZARD) foe->attributes.intelligence = std::max(8, foe->attributes.intelligence + atkDelta);
        else foe->attributes.strength = std::max(8, foe->attributes.strength + atkDelta);
        enemies_.push_back(std::move(foe));
    };

    using Tid = EncounterAuthorship::TemplateId;
    switch (t.id) {
        case Tid::SHADOW_AMBUSH:
            pushFoe("enemy_wickthief_cutpurse", CharacterClass::ROGUE, roomCount_, 0, -1);
            pushFoe("enemy_wickthief_cutpurse", CharacterClass::ROGUE, roomCount_ - 2, 0, -2);
            if (roomCount_ >= 8) pushFoe("enemy_wickthief", CharacterClass::ROGUE, roomCount_ / 2, 0, -2);
            break;
        case Tid::BONEBOUND_DUO:
            pushFoe("enemy_ashbound_bonebound", CharacterClass::FIGHTER, 6 + roomCount_, 1, -1);
            pushFoe("enemy_ashbound_bonebound", CharacterClass::FIGHTER, 4 + roomCount_, 1, -1);
            break;
        case Tid::RELIC_SNATCH:
            pushFoe("enemy_gloamhound", CharacterClass::ROGUE, 2 + roomCount_ / 2, 0, -2);
            pushFoe("enemy_gloamhound", CharacterClass::ROGUE, roomCount_ / 2, 0, -2);
            pushFoe("enemy_wickthief_cache", CharacterClass::ROGUE, 4 + roomCount_, 0, -1);
            break;
        case Tid::KINGS_HERALD: {
            // Tease deeper bosses without spawning them; pick herald flavor by depth.
            if (roomCount_ >= 9) {
                pushFoe("enemy_crier_bone", CharacterClass::FIGHTER, 10 + roomCount_, 1, -1);
                pushFoe("enemy_ashbound", CharacterClass::FIGHTER, roomCount_, 0, -2);
            } else {
                pushFoe("enemy_crier_wick", CharacterClass::ROGUE, 8 + roomCount_, 1, -1);
                pushFoe("enemy_wickthief_scout", CharacterClass::ROGUE, roomCount_, 0, -2);
            }
            break;
        }
        case Tid::WOLF_PACK:
            pushFoe("enemy_gloamhound", CharacterClass::ROGUE, 2 + roomCount_ / 2, 0, -2);
            pushFoe("enemy_gloamhound", CharacterClass::ROGUE, roomCount_ / 2, 0, -2);
            if (roomCount_ >= 7) pushFoe("enemy_gloamhound", CharacterClass::ROGUE, roomCount_ / 3, 0, -3);
            break;
        case Tid::OGRE_ROADBLOCK:
            pushFoe("enemy_brute", CharacterClass::FIGHTER, 18 + roomCount_, 2, 0);
            break;
        default:
            return false;
    }

    if (enemies_.empty()) return false;

    activeSetPieceId_ = static_cast<int>(t.id);
    pendingSetPieceXpBonus_ = t.xpBonus + (postStory ? 10 : 0);
    pendingSetPieceGoldBonus_ = t.goldBonus + (postStory ? 5 : 0);
    pendingSetPieceGuaranteedLoot_ = true;
    pendingSetPieceClearFeed_ = t.clearFeed;

    roomDescription_ = t.roomFlavor;
    lastEvent_ = std::string("SET-PIECE! ") + t.displayName + " — stand ready!";
    dmSay(t.telegraph);
    addJournalEntry(std::string("Set-piece: ") + t.displayName + ".");
    addChatMessage("System", lastEvent_);
    return true;
}

void Game::maybeSpawnBossEncounter() {
    // Never soft-lock early story beginners: only procedural crawl / post-quest rooms.
    if (isSoloQuestScripted()) return;

    const bool easy = difficulty_ <= static_cast<int>(Difficulty::EASY);
    const bool endgame = endgameContentAllowed();
    const int gkNeed = easy ? 5 : 4;
    const int skNeed = easy ? 9 : 8;
    const int dragonNeed = easy ? 14 : 12;
    const int endNeed = easy ? 22 : 18;
    const int gkChance = easy ? 32 : 34;
    const int skChance = easy ? 26 : 28;
    const int dragonChance = easy ? 14 : 16;
    const int endChance = easy ? 18 : 22;

    auto spawnGkCourt = [&]() {
        spawnNamedBoss("boss_wick_king");
        if (difficulty_ >= static_cast<int>(Difficulty::MEDIUM)) {
            auto scout = newEnemy("enemy_wickthief_scout", CharacterClass::ROGUE, "gk");
            scout->maxHp = std::max(4, scout->maxHp - 4);
            scout->currentHp = scout->maxHp;
            enemies_.push_back(std::move(scout));
        }
        roomDescription_ = "Stolen candles gutter on every spike. The Wick King rises from a throne of scrap iron, his crown a ring of melted wax.";
        lastEvent_ = "BOSS! The Wick King — stand ready!";
        dmSay("The Wick King! Original boss — richer spoils if you prevail.");
        addJournalEntry("Boss: The Wick King.");
    };

    if (roomCount_ >= 10 && !bossSeenGk_) {
        spawnGkCourt();
        return;
    }

    if (roomCount_ < gkNeed) return;

    int roll = getRandomInt(1, 100);

    // Endgame-only originals — require story Acts 1–3 complete (or Boss Raid).
    if (endgame && roomCount_ >= endNeed && roll <= endChance) {
        int pick = getRandomInt(0, 2);
        auto place = [&](const std::string& bossId, const std::string& desc) {
            spawnNamedBoss(bossId);
            const std::string nm = bestiary::displayName(bossId);
            roomDescription_ = desc;
            lastEvent_ = "BOSS! " + nm + " — stand ready!";
            dmSay("Endgame boss: " + nm + " (original). Legendary spoils possible.");
            addJournalEntry("Boss: " + nm + ".");
        };
        if (pick == 0) {
            place("boss_hollow_crown",
                  "A throne of cracked millstone and bone. The Hollow Crown — an endgame tyrant of ash — rises without a face.");
            return;
        }
        if (pick == 1) {
            place("boss_ember_hydra",
                  "Multiple ember-lit heads weave between pillars. The Ember Hydra hisses steam and bone-fire.");
            return;
        }
        place("boss_nightfang",
              "Webs thick as rope choke the vault. The Nightfang Matriarch — a vast spider-queen — descends.");
        return;
    }

    if (roomCount_ >= dragonNeed && roll <= dragonChance) {
        spawnNamedBoss("boss_drake");
        roomDescription_ = "The chamber opens into a scorched hollow. An Ashen Drake coils around a cracked pillar — heat warps the air.";
        lastEvent_ = "BOSS! Ashen Drake — stand ready!";
        dmSay("Original menace: the Ashen Drake (not from any published module). Stand ready — this is a late-game trial.");
        addJournalEntry("Boss: Ashen Drake stirs in the deep.");
        return;
    }
    if (roomCount_ >= skNeed && roll <= skChance) {
        spawnNamedBoss("boss_prior");
        roomDescription_ = "Bone thrones and rusted crowns litter the floor. The Ossuary Prior rises, a mitre of stacked skulls, empty eyes fixed on you.";
        lastEvent_ = "BOSS! The Ossuary Prior — stand ready!";
        dmSay("The Ossuary Prior claims this ossuary. Original foe — fight smart.");
        addJournalEntry("Boss: The Ossuary Prior.");
        return;
    }
    if (roomCount_ >= gkNeed && roll <= gkChance) {
        spawnGkCourt();
    }
}

void Game::noteBossDefeat(const std::string& foeName, const std::string& foeId) {
    int tier = LootSystem::bossTier(foeId);
    if (tier <= 0) return;
    dqBossesDefeated_++;
    // #46 base luck: 10+tier*8 — bosses better than trash, not BiS flood
    int xp = 40 * tier + roomCount_ * 2;
    int luck = 10 + tier * 8;
    int gold = 20 * tier + getRandomInt(5, 15);
    // Endless Deep: Legendary only from endgame apex names (depth 10/20/…); other focused
    // modes (Raid/Challenge/Arena) allow Legendary on any focused-run boss defeat.
    const bool endgameBossLoot = LootSystem::isEndgameBossId(foeId)
        || (isFocusedEndgameRun() && !isEndlessDeep());
    if (endgameBossLoot) {
        xp += 80;
        luck += 18; // XP/gold/luck bump only; Legendary gated by allowLegendary flag
        gold += 40;
        pendingBossAllowLegendary_ = true;
    }
    pendingBossXpBonus_ += xp;
    pendingBossLootLuck_ += luck;
    pendingBossGoldBonus_ += gold;
    dmSay("Boss fallen: " + foeName + "! Greater rewards await.");
    maybeGrantRaidKeyFromBoss(foeName, foeId);
    if (isBossRaid()) memoryOnRaidBoss(foeId);
}

void Game::maybeGrantRaidKeyFromBoss(const std::string& foeName, const std::string& foeId) {
    if (!endgameContentAllowed()) return;
    int tier = LootSystem::bossTier(foeId);
    if (tier < 3 && !LootSystem::isEndgameBossId(foeId) && !isFocusedEndgameRun()) return;
    int chance = isFocusedEndgameRun() ? 55 : (LootSystem::isEndgameBossId(foeId) ? 40 : 22);
    if (getRandomInt(1, 100) <= chance) {
        pendingRaidKeyDrop_ = true;
        dmSay("A Raid Key glints among the spoils — check the menu (max 3 held per calendar day).");
        addJournalEntry("Raid Key recovered from " + foeName + ".");
    }
}

void Game::advanceTurn() {
    if (turnOrder_.empty()) { currentTurnIndex_ = 0; return; }
    // v2.30: the actor's statuses tick down at the end of its own turn (Burn deals damage).
    endTurnStatuses(getCurrentActor());
    if (turnOrder_.empty()) { currentTurnIndex_ = 0; return; }
    currentTurnIndex_ = (currentTurnIndex_ + 1) % static_cast<int>(turnOrder_.size());
    if (currentTurnIndex_ == 0) ++combatRound_;
}

void Game::removeFromTurnOrder(Character* c) {
    if (!c || turnOrder_.empty()) return;
    Character* cur = nullptr;
    if (currentTurnIndex_ >= 0 && static_cast<size_t>(currentTurnIndex_) < turnOrder_.size()) {
        cur = turnOrder_[static_cast<size_t>(currentTurnIndex_)];
    }
    turnOrder_.erase(std::remove(turnOrder_.begin(), turnOrder_.end(), c), turnOrder_.end());
    if (turnOrder_.empty()) {
        currentTurnIndex_ = 0;
        return;
    }
    // Keep the same current actor when possible; otherwise clamp.
    if (cur && cur != c) {
        for (size_t i = 0; i < turnOrder_.size(); ++i) {
            if (turnOrder_[i] == cur) {
                currentTurnIndex_ = static_cast<int>(i);
                return;
            }
        }
    }
    if (currentTurnIndex_ >= static_cast<int>(turnOrder_.size())) {
        currentTurnIndex_ = 0;
    }
}

void Game::checkPartyDefeat() {
    if (players_.empty()) return;
    bool anyLiving = false;
    for (const auto& pl : players_) {
        if (!pl->isDead) { anyLiving = true; break; }
    }
    if (!anyLiving) {
        gameOver_ = true;
        const char* dname = difficultyName(difficulty_);
        lastEvent_ = std::string("Game Over — the entire party has fallen. Difficulty: ") + dname + ".";
        if (static_cast<Difficulty>(difficulty_) == Difficulty::NIGHTMARE) {
            dmSay("Silence falls. Nightmare: this run is over — return to the menu and start a new adventure.");
        } else {
            dmSay(std::string("Silence falls. Difficulty ") + dname +
                  ": choose Continue on the wipe dialog to revive (rules depend on difficulty).");
        }
        addChatMessage("System", "Game Over");
    }
}

void Game::revivePartyForDifficulty() {
    const auto d = static_cast<Difficulty>(difficulty_);
    for (auto& pl : players_) {
        if (!pl) continue;
        if (d == Difficulty::HARD) {
            pl->stripGearAndGoldToStarters();
        }
        int hp;
        if (d == Difficulty::EASY) {
            hp = pl->maxHp;
            pl->resources = pl->maxResources;
        } else if (d == Difficulty::MEDIUM) {
            hp = std::max(1, pl->maxHp / 2);
            pl->resources = std::max(0, pl->maxResources / 2);
        } else {
            hp = std::max(1, pl->maxHp / 2);
            pl->resources = std::max(1, pl->maxResources / 2);
        }
        pl->reviveAfterWipe(hp);
    }
}

void Game::rollbackOneRoomOrBeat() {
    enemies_.clear();
    shopInventory_.clear();
    isMerchantRoom_ = false;
    roomSearchUsed_ = false; roomRestUsed_ = false;

    if (isSoloQuestScripted()) {
        int prev = previousScriptedBeat(questBeat_);
        if (prev != questBeat_) {
            questBeat_ = prev;
            if (roomCount_ > 1) roomCount_--;
        } else if (roomCount_ > 1) {
            roomCount_--;
        }
        applySoloQuestRoom();
        if (!enemies_.empty()) rollInitiative();
        else { turnOrder_.clear(); currentTurnIndex_ = 0; }
        return;
    }

    if (roomCount_ > 1) roomCount_--;
    spawnRoomContent();
    const bool bossRoom = hasLivingBossEnemy();
    if (!isMerchantRoom_) {
        if (!bossRoom) generateRoomDescription();
        if (!enemies_.empty()) rollInitiative();
        else { turnOrder_.clear(); currentTurnIndex_ = 0; }
    } else {
        turnOrder_.clear();
        currentTurnIndex_ = 0;
    }
}

bool Game::beginBossRaidFromCurrent() {
    if (players_.empty()) return false;

    // Preserve party (stats, gear, inventory, gold, companion, level, Legendary upgrades).
    if (soloPlayMode_ != static_cast<int>(SoloPlayMode::RAID)
        && soloPlayMode_ != static_cast<int>(SoloPlayMode::CHALLENGE)
        && soloPlayMode_ != static_cast<int>(SoloPlayMode::ARENA)
        && soloPlayMode_ != static_cast<int>(SoloPlayMode::ENDLESS)) {
        preRaidSoloPlayMode_ = soloPlayMode_;
        preRaidRoomCount_ = roomCount_;
    }

    soloPlayMode_ = static_cast<int>(SoloPlayMode::RAID);
    gameOver_ = false;
    dmOnlyTable_ = false;
    isHost_ = true;
    dmName_.clear();
    pendingRaidKeyDrop_ = false;
    pendingBossXpBonus_ = 0;
    pendingBossLootLuck_ = 0;
    pendingBossAllowLegendary_ = false;
    pendingBossGoldBonus_ = 0;
    pendingChallengeLegendaryFound_ = false;
    arenaWave_ = 0;
    arenaScore_ = 0;
    arenaWavesCleared_ = 0;
    endlessDepth_ = 0;
    endlessBestDepthThisRun_ = 0;
    clearSetPiecePending();

    // Endgame gates active for this encounter (story must already be complete at menu).
    questBeat_ = static_cast<int>(SoloQuestBeat::NONE);
    questComplete_ = true;
    questAct2Complete_ = true;
    questAct3Complete_ = true;
    questAct4Complete_ = true;

    enemies_.clear();
    shopInventory_.clear();
    isMerchantRoom_ = false;
    roomSearchUsed_ = false; roomRestUsed_ = false;
    // Deep-room bias for raid loot/tuning without permanently rewriting adventure room.
    roomCount_ = std::max(roomCount_, 20);

    int pick = getRandomInt(0, 2);
    if (pick == 0) spawnNamedBoss("boss_hollow_crown");
    else if (pick == 1) spawnNamedBoss("boss_ember_hydra");
    else spawnNamedBoss("boss_nightfang");

    roomDescription_ = "Raid arena — a sealed endgame vault. One Raid Key spent. Defeat the raid boss for rich spoils.";
    lastEvent_ = "BOSS RAID! Stand ready!";
    dmSay("Boss Raid begins with your saved hero. Earn gold, XP, and a chance at Legendary gear and another key.");
    if (!enemies_.empty()) rollInitiative();
    else { turnOrder_.clear(); currentTurnIndex_ = 0; }
    addChatMessage("System", "Boss Raid begins (difficulty: " + std::string(difficultyName(difficulty_)) + ") — same hero preserved.");
    return true;
}

void Game::finishBossRaidKeepParty() {
    if (!isBossRaid() && preRaidSoloPlayMode_ < 0) return;

    enemies_.clear();
    shopInventory_.clear();
    isMerchantRoom_ = false;
    roomSearchUsed_ = false; roomRestUsed_ = false;
    turnOrder_.clear();
    currentTurnIndex_ = 0;
    gameOver_ = false;

    if (preRaidSoloPlayMode_ >= 0) {
        soloPlayMode_ = preRaidSoloPlayMode_;
        if (preRaidRoomCount_ > 0) roomCount_ = preRaidRoomCount_;
    } else {
        // Fallback if mid-raid save lost the snapshot: stay in story endgame.
        soloPlayMode_ = static_cast<int>(SoloPlayMode::STORY);
    }
    preRaidSoloPlayMode_ = -1;
    preRaidRoomCount_ = 0;

    // Keep endgame story flags; adventure continues with same character.
    questComplete_ = true;
    questAct2Complete_ = true;
    questAct3Complete_ = true;
    questAct4Complete_ = true;
    questBeat_ = static_cast<int>(SoloQuestBeat::NONE);

    generateRoomDescription();
    lastEvent_ = "Raid ended — your hero and gear are intact. Return to the menu or press Onward to keep exploring.";
    dmSay(lastEvent_);
    addChatMessage("System", "Boss Raid finished — story character preserved.");
    addJournalEntry("Boss Raid concluded; party preserved.");
}


bool Game::beginChallengeDungeonFromCurrent(int legendaryChancePercent) {
    if (players_.empty()) return false;

    if (soloPlayMode_ != static_cast<int>(SoloPlayMode::CHALLENGE)
        && soloPlayMode_ != static_cast<int>(SoloPlayMode::RAID)
        && soloPlayMode_ != static_cast<int>(SoloPlayMode::ARENA)
        && soloPlayMode_ != static_cast<int>(SoloPlayMode::ENDLESS)) {
        preRaidSoloPlayMode_ = soloPlayMode_;
        preRaidRoomCount_ = roomCount_;
    }

    soloPlayMode_ = static_cast<int>(SoloPlayMode::CHALLENGE);
    setChallengeLegendaryChance(legendaryChancePercent);
    gameOver_ = false;
    dmOnlyTable_ = false;
    isHost_ = true;
    dmName_.clear();
    pendingRaidKeyDrop_ = false;
    pendingBossXpBonus_ = 0;
    pendingBossLootLuck_ = 0;
    pendingBossAllowLegendary_ = false;
    pendingBossGoldBonus_ = 0;
    pendingChallengeLegendaryFound_ = false;
    arenaWave_ = 0;
    arenaScore_ = 0;
    arenaWavesCleared_ = 0;
    endlessDepth_ = 0;
    endlessBestDepthThisRun_ = 0;
    clearSetPiecePending();

    questBeat_ = static_cast<int>(SoloQuestBeat::NONE);
    questComplete_ = true;
    questAct2Complete_ = true;
    questAct3Complete_ = true;
    questAct4Complete_ = true;

    enemies_.clear();
    shopInventory_.clear();
    isMerchantRoom_ = false;
    roomSearchUsed_ = false; roomRestUsed_ = false;
    roomCount_ = std::max(roomCount_, 22);

    // Original vault: endgame apex + SRD-safe adds (no WotC module text).
    int pick = getRandomInt(0, 2);
    if (pick == 0) spawnNamedBoss("boss_hollow_crown");
    else if (pick == 1) spawnNamedBoss("boss_ember_hydra");
    else spawnNamedBoss("boss_nightfang");

    {
        auto add = newEnemy("enemy_brute", CharacterClass::FIGHTER, "cd");
        add->maxHp = std::max(18, add->maxHp + 10 + roomCount_ / 2);
        add->currentHp = add->maxHp;
        add->armorClass += 1;
        enemies_.push_back(std::move(add));
    }
    if (difficulty_ >= static_cast<int>(Difficulty::MEDIUM)) {
        auto wolf = newEnemy("enemy_gloamhound", CharacterClass::ROGUE, "cd");
        wolf->maxHp = std::max(8, wolf->maxHp + 4);
        wolf->currentHp = wolf->maxHp;
        enemies_.push_back(std::move(wolf));
    }

    roomDescription_ = "Cinder Crucible — a sealed Ashen trial vault. Higher Legendary odds await, but each Legendary found dims the forge.";
    lastEvent_ = "CHALLENGE DUNGEON! Stand ready!";
    dmSay("Challenge Dungeon begins with your saved hero. Legendary chance is "
          + std::to_string(challengeLegendaryChance_)
          + "% this run (uncapped Legendaries; chance falls after each find, rises every 50 attempts toward a cap).");
    if (!enemies_.empty()) rollInitiative();
    else { turnOrder_.clear(); currentTurnIndex_ = 0; }
    addChatMessage("System", "Challenge Dungeon begins (difficulty: "
                   + std::string(difficultyName(difficulty_)) + ") — same hero preserved.");
    addJournalEntry("Entered the Cinder Crucible (Challenge Dungeon).");
    return true;
}

void Game::finishChallengeDungeonKeepParty() {
    if (!isChallengeDungeon() && preRaidSoloPlayMode_ < 0) return;

    enemies_.clear();
    shopInventory_.clear();
    isMerchantRoom_ = false;
    roomSearchUsed_ = false; roomRestUsed_ = false;
    turnOrder_.clear();
    currentTurnIndex_ = 0;
    gameOver_ = false;

    if (preRaidSoloPlayMode_ >= 0) {
        soloPlayMode_ = preRaidSoloPlayMode_;
        if (preRaidRoomCount_ > 0) roomCount_ = preRaidRoomCount_;
    } else {
        soloPlayMode_ = static_cast<int>(SoloPlayMode::STORY);
    }
    preRaidSoloPlayMode_ = -1;
    preRaidRoomCount_ = 0;

    questComplete_ = true;
    questAct2Complete_ = true;
    questAct3Complete_ = true;
    questAct4Complete_ = true;
    questBeat_ = static_cast<int>(SoloQuestBeat::NONE);

    generateRoomDescription();
    lastEvent_ = "Challenge Dungeon ended — your hero and gear are intact. Return to the menu or press Onward to keep exploring.";
    dmSay(lastEvent_);
    addChatMessage("System", "Challenge Dungeon finished — story character preserved.");
    addJournalEntry("Challenge Dungeon concluded; party preserved.");
}

void Game::spawnArenaWave(int wave) {
    enemies_.clear();
    shopInventory_.clear();
    isMerchantRoom_ = false;
    roomSearchUsed_ = false; roomRestUsed_ = false;
    clearSetPiecePending();
    pendingBossXpBonus_ = 0;
    pendingBossLootLuck_ = 0;
    pendingBossAllowLegendary_ = false;
    pendingBossGoldBonus_ = 0;

    auto pushFoe = [&](const std::string& enemyId, CharacterClass cl, int hpFlat, int acFlat, int atkDelta) {
        auto foe = newEnemy(enemyId, cl, "ar");
        foe->maxHp = std::max(6, foe->maxHp + hpFlat);
        foe->currentHp = foe->maxHp;
        foe->armorClass += acFlat;
        if (atkDelta != 0) {
            foe->attributes.strength = std::max(1, foe->attributes.strength + atkDelta);
            foe->attributes.dexterity = std::max(1, foe->attributes.dexterity + atkDelta);
        }
        enemies_.push_back(std::move(foe));
    };

    const int w = std::max(1, std::min(wave, ARENA_MAX_WAVES));
    arenaWave_ = w;
    switch (w) {
        case 1:
            pushFoe("enemy_wickthief", CharacterClass::ROGUE, 2, 0, -2);
            pushFoe("enemy_wickthief", CharacterClass::ROGUE, 0, 0, -2);
            roomDescription_ = "Ember Ring — Wave 1. Ashen sand, roaring crowd. Prove yourself against the opener pack.";
            break;
        case 2:
            pushFoe("enemy_gloamhound", CharacterClass::ROGUE, 4, 0, -2);
            pushFoe("enemy_ashbound", CharacterClass::FIGHTER, 2, 0, -2);
            roomDescription_ = "Ember Ring — Wave 2. Bone and fang circle the ring.";
            break;
        case 3:
            pushFoe("enemy_brute", CharacterClass::FIGHTER, 14 + roomCount_ / 2, 1, 0);
            roomDescription_ = "Ember Ring — Wave 3. A Millstone Brute stomps into the ash.";
            break;
        case 4:
            pushFoe("enemy_brute", CharacterClass::FIGHTER, 16 + roomCount_ / 2, 2, 0);
            pushFoe("enemy_gloamhound", CharacterClass::ROGUE, 4, 0, -2);
            if (difficulty_ >= static_cast<int>(Difficulty::MEDIUM)) {
                pushFoe("enemy_ashbound", CharacterClass::FIGHTER, 4, 0, -1);
            }
            roomDescription_ = "Ember Ring — Wave 4. The crowd chants for blood — a heavy pack.";
            break;
        default: {
            int pick = getRandomInt(0, 2);
            if (pick == 0) spawnNamedBoss("boss_hollow_crown");
            else if (pick == 1) spawnNamedBoss("boss_ember_hydra");
            else spawnNamedBoss("boss_nightfang");
            roomDescription_ = "Ember Ring — Wave 5 (Finale). An endgame apex enters. Legendary possible (7%).";
            break;
        }
    }
    lastEvent_ = "ARENA WAVE " + std::to_string(w) + "/" + std::to_string(ARENA_MAX_WAVES) + "! Stand ready!";
    dmSay("Ember Ring Wave " + std::to_string(w) + " of " + std::to_string(ARENA_MAX_WAVES)
          + ". Score " + std::to_string(arenaScore_) + ".");
    if (!enemies_.empty()) rollInitiative();
    else { turnOrder_.clear(); currentTurnIndex_ = 0; }
}

bool Game::beginArenaFromCurrent() {
    if (players_.empty()) return false;

    if (soloPlayMode_ != static_cast<int>(SoloPlayMode::ARENA)
        && soloPlayMode_ != static_cast<int>(SoloPlayMode::RAID)
        && soloPlayMode_ != static_cast<int>(SoloPlayMode::CHALLENGE)
        && soloPlayMode_ != static_cast<int>(SoloPlayMode::ENDLESS)) {
        preRaidSoloPlayMode_ = soloPlayMode_;
        preRaidRoomCount_ = roomCount_;
    }

    soloPlayMode_ = static_cast<int>(SoloPlayMode::ARENA);
    arenaWave_ = 1;
    arenaScore_ = 0;
    arenaWavesCleared_ = 0;
    endlessDepth_ = 0;
    endlessBestDepthThisRun_ = 0;
    gameOver_ = false;
    dmOnlyTable_ = false;
    isHost_ = true;
    dmName_.clear();
    pendingRaidKeyDrop_ = false;
    pendingBossXpBonus_ = 0;
    pendingBossLootLuck_ = 0;
    pendingBossAllowLegendary_ = false;
    pendingBossGoldBonus_ = 0;
    pendingChallengeLegendaryFound_ = false;
    clearSetPiecePending();

    questBeat_ = static_cast<int>(SoloQuestBeat::NONE);
    questComplete_ = true;
    questAct2Complete_ = true;
    questAct3Complete_ = true;
    questAct4Complete_ = true;

    roomCount_ = std::max(roomCount_, 18);
    spawnArenaWave(1);
    addChatMessage("System", "Ember Ring Arena begins (difficulty: "
                   + std::string(difficultyName(difficulty_)) + ") — same hero preserved.");
    addJournalEntry("Entered the Ember Ring Arena.");
    return true;
}

void Game::finishArenaKeepParty() {
    if (!isArena() && preRaidSoloPlayMode_ < 0) return;

    enemies_.clear();
    shopInventory_.clear();
    isMerchantRoom_ = false;
    roomSearchUsed_ = false; roomRestUsed_ = false;
    turnOrder_.clear();
    currentTurnIndex_ = 0;
    gameOver_ = false;

    if (preRaidSoloPlayMode_ >= 0) {
        soloPlayMode_ = preRaidSoloPlayMode_;
        if (preRaidRoomCount_ > 0) roomCount_ = preRaidRoomCount_;
    } else {
        soloPlayMode_ = static_cast<int>(SoloPlayMode::STORY);
    }
    preRaidSoloPlayMode_ = -1;
    preRaidRoomCount_ = 0;
    // Keep arenaScore_ / arenaWavesCleared_ for Kotlin meta until next begin.

    questComplete_ = true;
    questAct2Complete_ = true;
    questAct3Complete_ = true;
    questAct4Complete_ = true;
    questBeat_ = static_cast<int>(SoloQuestBeat::NONE);

    generateRoomDescription();
    lastEvent_ = "Ember Ring ended — score " + std::to_string(arenaScore_)
        + ". Your hero and gear are intact. Return to the menu or press Onward to keep exploring.";
    dmSay(lastEvent_);
    addChatMessage("System", "Arena finished — story character preserved. Score "
                   + std::to_string(arenaScore_) + ".");
    addJournalEntry("Ember Ring Arena concluded; score " + std::to_string(arenaScore_) + ".");
}

void Game::spawnEndlessDepth(int depth) {
    memoryOnDeepDepth(depth);
    enemies_.clear();
    shopInventory_.clear();
    isMerchantRoom_ = false;
    roomSearchUsed_ = false; roomRestUsed_ = false;
    clearSetPiecePending();
    pendingBossXpBonus_ = 0;
    pendingBossLootLuck_ = 0;
    pendingBossAllowLegendary_ = false;
    pendingBossGoldBonus_ = 0;

    auto pushFoe = [&](const std::string& enemyId, CharacterClass cl, int hpFlat, int acFlat, int atkDelta) {
        auto foe = newEnemy(enemyId, cl, "ed");
        foe->maxHp = std::max(6, foe->maxHp + hpFlat);
        foe->currentHp = foe->maxHp;
        foe->armorClass += acFlat;
        if (atkDelta != 0) {
            foe->attributes.strength = std::max(1, foe->attributes.strength + atkDelta);
            foe->attributes.dexterity = std::max(1, foe->attributes.dexterity + atkDelta);
        }
        enemies_.push_back(std::move(foe));
    };

    const int d = std::max(1, depth);
    endlessDepth_ = d;
    endlessBestDepthThisRun_ = std::max(endlessBestDepthThisRun_, d);
    roomCount_ = std::max(roomCount_, 15 + d);

    const int hpScale = d * 2 + d / 3;
    const int acScale = std::min(4, d / 5);
    const int atkScale = (d >= 12) ? 1 : ((d >= 6) ? 0 : -1);

    if (d % ENDLESS_APEX_EVERY == 0) {
        int pick = getRandomInt(0, 2);
        if (pick == 0) spawnNamedBoss("boss_hollow_crown");
        else if (pick == 1) spawnNamedBoss("boss_ember_hydra");
        else spawnNamedBoss("boss_nightfang");
        // Scale apex with depth beyond first apex.
        if (!enemies_.empty() && d > ENDLESS_APEX_EVERY) {
            Character* apex = enemies_.back().get();
            const int extra = (d / ENDLESS_APEX_EVERY - 1) * 10;
            apex->maxHp += extra;
            apex->currentHp = apex->maxHp;
            apex->armorClass += std::min(3, d / 20);
        }
        if (d >= 15 || difficulty_ >= static_cast<int>(Difficulty::MEDIUM)) {
            pushFoe("enemy_gloamhound", CharacterClass::ROGUE, 4 + hpScale / 2, 0, -1);
        }
        roomDescription_ = "Ashen Deep — Depth " + std::to_string(d)
            + " (Apex). The Deep opens a throne of cinders. Legendary possible (7%).";
        lastEvent_ = "ENDLESS APEX DEPTH " + std::to_string(d) + "! Stand ready!";
        dmSay("Ashen Deep apex at depth " + std::to_string(d) + ". Endgame foe — Legendary gated at 7%.");
    } else if (d % ENDLESS_MILESTONE_EVERY == 0) {
        pushFoe("enemy_brute", CharacterClass::FIGHTER, 14 + hpScale, 1 + acScale, atkScale);
        if (d >= 10 || difficulty_ >= static_cast<int>(Difficulty::MEDIUM)) {
            pushFoe("enemy_ashbound", CharacterClass::FIGHTER, 4 + hpScale / 2, acScale, -1);
        }
        if (difficulty_ >= static_cast<int>(Difficulty::HARD)) {
            pushFoe("enemy_gloamhound", CharacterClass::ROGUE, 2 + hpScale / 2, 0, -1);
        }
        roomDescription_ = "Ashen Deep — Depth " + std::to_string(d)
            + " (Milestone). An elite pack bars the shaft. Retreat is wise — or press deeper.";
        lastEvent_ = "ENDLESS MILESTONE DEPTH " + std::to_string(d) + "!";
        dmSay("Milestone depth " + std::to_string(d) + ". You may Retreat from Settings with spoils, or Onward deeper.");
    } else {
        // Trash pack — denser with depth.
        pushFoe("enemy_wickthief", CharacterClass::ROGUE, 2 + hpScale, acScale / 2, -2 + (d >= 8 ? 1 : 0));
        pushFoe("enemy_wickthief", CharacterClass::ROGUE, hpScale, 0, -2);
        if (d >= 3) pushFoe("enemy_gloamhound", CharacterClass::ROGUE, 2 + hpScale / 2, 0, -2 + (atkScale > 0 ? 1 : 0));
        if (d >= 6) pushFoe("enemy_ashbound", CharacterClass::FIGHTER, 2 + hpScale / 2, acScale, -1);
        if (d >= 9) pushFoe("enemy_wickthief", CharacterClass::ROGUE, hpScale / 2, 0, -1);
        if (d >= 14) pushFoe("enemy_ashbound", CharacterClass::FIGHTER, 4 + hpScale / 2, acScale, atkScale);
        roomDescription_ = "Ashen Deep — Depth " + std::to_string(d)
            + ". Ashen shafts plunge without end. Foes thicken with every descent.";
        lastEvent_ = "ENDLESS DEPTH " + std::to_string(d) + "! Stand ready!";
        dmSay("Ashen Deep depth " + std::to_string(d) + ".");
    }
    if (!enemies_.empty()) rollInitiative();
    else { turnOrder_.clear(); currentTurnIndex_ = 0; }
}

bool Game::beginEndlessDeepFromCurrent() {
    if (players_.empty()) return false;

    if (soloPlayMode_ != static_cast<int>(SoloPlayMode::ENDLESS)
        && soloPlayMode_ != static_cast<int>(SoloPlayMode::RAID)
        && soloPlayMode_ != static_cast<int>(SoloPlayMode::CHALLENGE)
        && soloPlayMode_ != static_cast<int>(SoloPlayMode::ARENA)) {
        preRaidSoloPlayMode_ = soloPlayMode_;
        preRaidRoomCount_ = roomCount_;
    }

    soloPlayMode_ = static_cast<int>(SoloPlayMode::ENDLESS);
    endlessDepth_ = 1;
    endlessBestDepthThisRun_ = 1;
    arenaWave_ = 0;
    arenaScore_ = 0;
    arenaWavesCleared_ = 0;
    gameOver_ = false;
    dmOnlyTable_ = false;
    isHost_ = true;
    dmName_.clear();
    pendingRaidKeyDrop_ = false;
    pendingBossXpBonus_ = 0;
    pendingBossLootLuck_ = 0;
    pendingBossAllowLegendary_ = false;
    pendingBossGoldBonus_ = 0;
    pendingChallengeLegendaryFound_ = false;
    clearSetPiecePending();

    questBeat_ = static_cast<int>(SoloQuestBeat::NONE);
    questComplete_ = true;
    questAct2Complete_ = true;
    questAct3Complete_ = true;
    questAct4Complete_ = true;

    roomCount_ = std::max(roomCount_, 16);
    spawnEndlessDepth(1);
    addChatMessage("System", "Ashen Deep begins (difficulty: "
                   + std::string(difficultyName(difficulty_)) + ") — same hero preserved. Endless.");
    addJournalEntry("Descended into the Ashen Deep (Endless Deep).");
    return true;
}

void Game::finishEndlessDeepKeepParty() {
    if (!isEndlessDeep() && preRaidSoloPlayMode_ < 0) return;

    enemies_.clear();
    shopInventory_.clear();
    isMerchantRoom_ = false;
    roomSearchUsed_ = false; roomRestUsed_ = false;
    turnOrder_.clear();
    currentTurnIndex_ = 0;
    gameOver_ = false;

    if (preRaidSoloPlayMode_ >= 0) {
        soloPlayMode_ = preRaidSoloPlayMode_;
        if (preRaidRoomCount_ > 0) roomCount_ = preRaidRoomCount_;
    } else {
        soloPlayMode_ = static_cast<int>(SoloPlayMode::STORY);
    }
    preRaidSoloPlayMode_ = -1;
    preRaidRoomCount_ = 0;
    // Keep endlessDepth_ / endlessBestDepthThisRun_ for Kotlin meta until next begin.

    questComplete_ = true;
    questAct2Complete_ = true;
    questAct3Complete_ = true;
    questAct4Complete_ = true;
    questBeat_ = static_cast<int>(SoloQuestBeat::NONE);

    generateRoomDescription();
    lastEvent_ = "Ashen Deep ended — depth " + std::to_string(endlessBestDepthThisRun_)
        + ". Your hero and gear are intact. Return to the menu or press Onward to keep exploring.";
    dmSay(lastEvent_);
    addChatMessage("System", "Endless Deep finished — story character preserved. Best depth "
                   + std::to_string(endlessBestDepthThisRun_) + ".");
    addJournalEntry("Ashen Deep concluded; depth " + std::to_string(endlessBestDepthThisRun_) + ".");
}


bool Game::recoverFromPartyWipe() {
    if (!gameOver_) return false;
    if (static_cast<Difficulty>(difficulty_) == Difficulty::NIGHTMARE) {
        lastEvent_ = "Nightmare — no Continue. Start a new adventure from the menu.";
        return false;
    }
    gameOver_ = false;
    revivePartyForDifficulty();
    if (encounterCurveActive()) {
        // v2.29 Rekindle (comeback): the next room is -20%; Hard's gear strip gets 3 rooms.
        rekindleRooms_ = (static_cast<Difficulty>(difficulty_) == Difficulty::HARD) ? 3 : 1;
        pressure_ = 0;
        lowHpStreak_ = 0;
    }
    if (isBossRaid()) {
        // Mid-raid wipe: apply difficulty revive, then leave raid without spawning a blank run.
        finishBossRaidKeepParty();
        const char* dname = difficultyName(difficulty_);
        lastEvent_ = std::string("Continue (") + dname + ") — raid aborted; your story hero stands again.";
        dmSay(lastEvent_);
        addChatMessage("System", lastEvent_);
        addJournalEntry(std::string("Raid wipe Continue on ") + dname + " — character preserved.");
        return true;
    }
    if (isChallengeDungeon()) {
        finishChallengeDungeonKeepParty();
        const char* dname = difficultyName(difficulty_);
        lastEvent_ = std::string("Continue (") + dname + ") — Challenge Dungeon aborted; your story hero stands again.";
        dmSay(lastEvent_);
        addChatMessage("System", lastEvent_);
        addJournalEntry(std::string("Challenge Dungeon wipe Continue on ") + dname + " — character preserved.");
        return true;
    }
    if (isArena()) {
        finishArenaKeepParty();
        const char* dname = difficultyName(difficulty_);
        lastEvent_ = std::string("Continue (") + dname + ") — Ember Ring aborted; your story hero stands again.";
        dmSay(lastEvent_);
        addChatMessage("System", lastEvent_);
        addJournalEntry(std::string("Arena wipe Continue on ") + dname + " — character preserved.");
        return true;
    }
    if (isEndlessDeep()) {
        finishEndlessDeepKeepParty();
        const char* dname = difficultyName(difficulty_);
        lastEvent_ = std::string("Continue (") + dname + ") — Ashen Deep aborted; your story hero stands again.";
        dmSay(lastEvent_);
        addChatMessage("System", lastEvent_);
        addJournalEntry(std::string("Endless Deep wipe Continue on ") + dname + " — character preserved.");
        return true;
    }
    rollbackOneRoomOrBeat();
    const char* dname = difficultyName(difficulty_);
    lastEvent_ = std::string("Continue (") + dname + ") — the party rises one chamber back.";
    dmSay(lastEvent_);
    addChatMessage("System", lastEvent_);
    addJournalEntry(std::string("Party wipe Continue on ") + dname + ".");
    return true;
}

bool Game::isAllyAi(const Character* c) const {
    if (!c) return false;
    // Player-controlled companions (and joined humans) wait for UI input.
    return c->aiControlled;
}

void Game::grantKillLoot(Character* actor, const std::string& foeName, const std::string& foeId) {
    int gold = 8 + (roomCount_ * 3) + getRandomInt(0, 7);
    if (LootSystem::isBossId(foeId)) {
        noteBossDefeat(foeName, foeId);
        gold += pendingBossGoldBonus_;
        pendingBossGoldBonus_ = 0;
        gold = static_cast<int>(gold * (1.5 + 0.25 * LootSystem::bossTier(foeId)));
    }
    Character* looter = actor;
    if (!looter || looter->isDead) {
        looter = nullptr;
        for (auto& p : players_) {
            if (p && !p->isDead) { looter = p.get(); break; }
        }
    }
    if (!looter) return;
    if (encounterCurveActive() && pressure_ > 0) {
        gold = static_cast<int>(std::lround(gold * curve::pressureMultiplier(pressure_)));
    }
    gold = luckGold(gold); // v2.31 Luck of the Lantern (CHA)
    looter->gold += gold;
    // Kill beat first so UI juice can toast "Felled …" then gold line stays in the feed.
    std::string msg = "Felled " + foeName + "! " + looter->name + " loots " +
                      std::to_string(gold) + " gold.";
    addChatMessage("Combat", msg);
    const bool hasArticle = foeName.rfind("The ", 0) == 0 || foeName.rfind("Collector ", 0) == 0;
    dmSay((hasArticle ? foeName : "The " + foeName) + " falls. " + looter->name + " claims " + std::to_string(gold) + " gold.");
    addJournalEntry("Defeated " + foeName + " — " + looter->name + " loots " + std::to_string(gold) + " gold.");
    lastEvent_ = msg;
}

void Game::enterClearedRoom(Character* actor) {
    dqRoomsCleared_++;
    dqFightsWon_++;
    // Preserve kill beat ("Felled …") so UI juice can toast it when this room clears.
    const std::string priorKillBeat =
        (lastEvent_.rfind("Felled ", 0) == 0) ? lastEvent_ : std::string();
    const int setPieceXp = pendingSetPieceXpBonus_;
    const int setPieceGold = pendingSetPieceGoldBonus_;
    const bool setPieceLoot = pendingSetPieceGuaranteedLoot_;
    const std::string setPieceFeed = pendingSetPieceClearFeed_;
    const bool wasSetPiece = activeSetPieceId_ != 0;
    clearSetPiecePending();

    int xpGained = 50 + (roomCount_ * 15) + pendingBossXpBonus_ + setPieceXp;
    pendingBossXpBonus_ = 0;
    if (encounterCurveActive() && pressure_ > 0) {
        // Pressure reward: +5% XP (and gold, see grantKillLoot) per flawless-room step.
        xpGained = static_cast<int>(std::lround(xpGained * curve::pressureMultiplier(pressure_)));
    }
    if (fledFoes_ > 0 && roomFoeTotal_ > 0) {
        // v2.30: a foe that slipped away gives half its share of the room's XP.
        const double kept = 1.0 - 0.5 * std::min(fledFoes_, roomFoeTotal_) / static_cast<double>(roomFoeTotal_);
        xpGained = std::max(1, static_cast<int>(std::lround(xpGained * kept)));
    }
    fledFoes_ = 0;
    for (auto& p : players_) if (p) p->status = Character::Statuses{}; // fight over: statuses end
    noteRoomOutcomeForCurve();
    for (auto& p : players_) {
        if (p->addXp(xpGained)) {
            dmSay(p->name + " levels up! Spend your ability points from the character sheet.");
        }
    }
    dmSay("The party gains " + std::to_string(xpGained) + " XP.");
    if (setPieceGold > 0) {
        Character* purse = actor;
        if (!purse || purse->isDead) {
            purse = nullptr;
            for (auto& p : players_) {
                if (p && !p->isDead) { purse = p.get(); break; }
            }
        }
        if (purse) {
            purse->gold += setPieceGold;
            dmSay(purse->name + " claims +" + std::to_string(setPieceGold) + " gold from the set-piece cache.");
            addChatMessage("Combat", purse->name + " claims +" + std::to_string(setPieceGold) + " set-piece gold.");
        }
    }
    // Ember Ring: score the cleared wave; luck bump on 1–4 (Rare/Epic); Wave 5 uses boss Legendary gate.
    if (isArena() && arenaWave_ >= 1) {
        arenaScore_ += 100 * arenaWave_;
        arenaWavesCleared_ = std::max(arenaWavesCleared_, arenaWave_);
        if (arenaWave_ >= ARENA_MAX_WAVES) {
            arenaScore_ += 200; // finale bonus
        } else {
            pendingBossLootLuck_ += 10 + arenaWave_ * 4;
        }
        dmSay("Wave " + std::to_string(arenaWave_) + " cleared! Arena score "
              + std::to_string(arenaScore_) + ".");
        addChatMessage("Arena", "Wave " + std::to_string(arenaWave_) + "/"
                       + std::to_string(ARENA_MAX_WAVES) + " clear — score "
                       + std::to_string(arenaScore_) + ".");
    }
    // Ashen Deep: depth-scaled gold + loot luck; Legendary only via apex noteBossDefeat.
    if (isEndlessDeep() && endlessDepth_ >= 1) {
        endlessBestDepthThisRun_ = std::max(endlessBestDepthThisRun_, endlessDepth_);
        pendingBossLootLuck_ += 4 + endlessDepth_ * 2;
        int depthGold = 8 + endlessDepth_ * 3;
        if (endlessDepth_ % ENDLESS_MILESTONE_EVERY == 0) {
            depthGold += 15 + endlessDepth_; // milestone soft-exit bonus
        }
        Character* purse = actor;
        if (!purse || purse->isDead) {
            purse = nullptr;
            for (auto& p : players_) {
                if (p && !p->isDead) { purse = p.get(); break; }
            }
        }
        if (purse) {
            purse->gold += depthGold;
            dmSay(purse->name + " claims +" + std::to_string(depthGold)
                  + " gold from the Ashen Deep (depth " + std::to_string(endlessDepth_) + ").");
            addChatMessage("Endless", purse->name + " +" + std::to_string(depthGold) + " gold (depth "
                           + std::to_string(endlessDepth_) + ").");
        }
        dmSay("Depth " + std::to_string(endlessDepth_) + " cleared! Ashen Deep continues — Onward, or Retreat from Settings.");
        addChatMessage("Endless", "Depth " + std::to_string(endlessDepth_)
                       + " clear — luck/gold scale with depth.");
    }

    int luck = pendingBossLootLuck_;
    pendingBossLootLuck_ = 0;
    // Capture before lantern / early-return paths discard the pending flag.
    const bool pendingAllowLegendary = pendingBossAllowLegendary_;
    pendingBossAllowLegendary_ = false;
    std::string rareLootNote; // Rare+ for UI juice (toast + soft accent)
    if (static_cast<SoloQuestBeat>(questBeat_) == SoloQuestBeat::LANTERN_VAULT) {
        grantAshenLantern(actor);
    } else if (actor && !actor->isDead) {
        int preferA = -1, preferB = -1;
        partyPreferClasses(preferA, preferB);
        // Legendary only from endgame boss / Boss Raid / Challenge Dungeon defeat (never questAct3 alone).
        const int legPct = isChallengeDungeon() ? challengeLegendaryChance_ : 7;
        auto loot = LootSystem::generateLoot(roomCount_, luck, preferA, preferB, pendingAllowLegendary, legPct);
        if (loot) {
            actor->addToInventory(loot);
            dmSay(actor->name + " finds " + loot->getDescription() + " [" + loot->rarityLabel() + "] — check Inventory.");
            addJournalEntry(actor->name + " found loot: " + loot->getDescription() + " (" + loot->rarityLabel() + ")");
            if (loot->rarity == ItemRarity::LEGENDARY && isChallengeDungeon()) {
                pendingChallengeLegendaryFound_ = true;
                dmSay("Challenge Dungeon: Legendary found — drop chance will fall (uncapped Legendaries; chance recovers every 50 attempts).");
            }
            if (loot->rarity == ItemRarity::RARE || loot->rarity == ItemRarity::EPIC
                || loot->rarity == ItemRarity::LEGENDARY) {
                rareLootNote = actor->name + " finds " + loot->getDescription()
                    + " [" + loot->rarityLabel() + "]!";
            }
        } else if (luck > 0) {
            // #46 pity: guarantee a drop; hard-cap at Epic (never Legendary).
            auto pity = LootSystem::generateLoot(roomCount_, 30, preferA, preferB, false);
            if (!pity) pity = LootSystem::generateLoot(roomCount_, 60, preferA, preferB, false);
            if (pity) {
                actor->addToInventory(pity);
                dmSay(actor->name + " claims a boss trophy: " + pity->getDescription() + " [" + pity->rarityLabel() + "].");
                addJournalEntry(actor->name + " claimed boss loot: " + pity->getDescription());
                if (pity->rarity == ItemRarity::RARE || pity->rarity == ItemRarity::EPIC
                    || pity->rarity == ItemRarity::LEGENDARY) {
                    rareLootNote = actor->name + " claims " + pity->getDescription()
                        + " [" + pity->rarityLabel() + "]!";
                }
            }
        }
        // Set-piece: guaranteed Common or Uncommon (never Legendary; respects drop nerfs).
        if (setPieceLoot) {
            ItemRarity r = (roomCount_ >= 8 || getRandomInt(0, 1) == 1)
                ? ItemRarity::UNCOMMON : ItemRarity::COMMON;
            int bonus = (roomCount_ / 5) + static_cast<int>(r);
            if (bonus < 0) bonus = 0;
            bool weapon = (getRandomInt(0, 1) == 0);
            int cls = (preferA >= 0) ? preferA : preferB;
            std::shared_ptr<Item> forced;
            if (weapon) {
                if (cls == 0) forced = Item::make("Fighter's Arming Sword", ItemType::WEAPON, bonus, r, 0);
                else if (cls == 1) forced = Item::make("Wizard's Focus Rod", ItemType::WEAPON, bonus, r, 1);
                else if (cls == 2) forced = Item::make("Rogue's Stiletto", ItemType::WEAPON, bonus, r, 2);
                else if (cls == 3) forced = Item::make("Cleric's Warhammer", ItemType::WEAPON, bonus, r, 3);
                else if (cls == 4) forced = Item::make("Bard's Stage Rapier", ItemType::WEAPON, bonus, r, 4);
                else forced = Item::make("Traveler's Blade", ItemType::WEAPON, bonus, r, -1);
            } else {
                if (cls == 0) forced = Item::make("Fighter's Mail", ItemType::ARMOR, 3 + bonus, r, 0);
                else if (cls == 1) forced = Item::make("Wizard's Robe", ItemType::ARMOR, bonus, r, 1);
                else if (cls == 2) forced = Item::make("Rogue's Leathers", ItemType::ARMOR, 1 + bonus, r, 2);
                else if (cls == 3) forced = Item::make("Cleric's Scale", ItemType::ARMOR, 4 + bonus, r, 3);
                else if (cls == 4) forced = Item::make("Performer's Leathers", ItemType::ARMOR, 1 + bonus, r, 4);
                else forced = Item::make("Sturdy Cloak", ItemType::ARMOR, bonus, r, -1);
            }
            if (forced) {
                actor->addToInventory(forced);
                dmSay(actor->name + " claims set-piece spoils: " + forced->getDescription()
                      + " [" + forced->rarityLabel() + "] (Common/Uncommon guarantee).");
                addJournalEntry(actor->name + " set-piece loot: " + forced->getDescription());
                addChatMessage("Combat", actor->name + " set-piece loot: " + forced->getDescription()
                               + " [" + forced->rarityLabel() + "]");
            }
        }
    }
    // Stay in this chamber so Short Rest / one Search / Onward are available.
    turnOrder_.clear();
    currentTurnIndex_ = 0;
    roomSearchUsed_ = false; roomRestUsed_ = false;
    roomDescription_ += kClearedSuffix;
    if (!rareLootNote.empty()) {
        // Prefer Rare+ loot beat so UI can toast a restrained accent.
        lastEvent_ = rareLootNote;
        addChatMessage("Combat", rareLootNote);
        if (!priorKillBeat.empty()) addChatMessage("Combat", priorKillBeat);
        if (wasSetPiece && !setPieceFeed.empty()) {
            addChatMessage("Combat", setPieceFeed + " Search, Rest, or Onward.");
        } else {
            addChatMessage("Combat", "Room cleared! Search, Rest, or Onward.");
        }
    } else if (!priorKillBeat.empty()) {
        // Keep Felled line as lastEvent_ for kill-pop toast; also note the clear.
        lastEvent_ = priorKillBeat;
        addChatMessage("Combat", priorKillBeat);
        if (wasSetPiece && !setPieceFeed.empty()) {
            addChatMessage("Combat", setPieceFeed + " Search, Rest, or Onward.");
        } else {
            addChatMessage("Combat", "Room cleared! Search, Rest, or Onward.");
        }
    } else if (wasSetPiece && !setPieceFeed.empty()) {
        lastEvent_ = setPieceFeed + " Search, Rest, or Onward.";
        addChatMessage("Combat", lastEvent_);
    } else {
        lastEvent_ = "Room cleared! Search, Rest, or Onward.";
        addChatMessage("Combat", lastEvent_);
    }
    if (questLanternRecovered_ && static_cast<SoloQuestBeat>(questBeat_) == SoloQuestBeat::LANTERN_VAULT) {
        dmSay("The vault is clear. Carry the Ashen Lantern and Wren Onward to the shrine — or Search and Rest first.");
    } else if (wasSetPiece) {
        dmSay("Set-piece cleared. Search for more, take a Short Rest, or press Onward.");
    } else {
        dmSay("The chamber is clear. Search for loot, take a Short Rest, or press Onward.");
    }
    onStoryRoomCleared();
    memoryOnStoryClear();
    if (isArena() && arenaWave_ >= ARENA_MAX_WAVES) memoryOnArenaApex();
    memoryOnCrawlEvent(false);
}

void Game::purgeDownedEnemies() {
    // Drop 0-HP foes out of the encounter without touching a dangling turnOrder_ pointer.
    bool removed = false;
    Character* looter = getCurrentActor();
    for (auto it = enemies_.begin(); it != enemies_.end(); ) {
        if ((*it)->currentHp <= 0 || (*it)->isDowned) {
            returnStolenGold(**it, looter);
            grantKillLoot(looter, (*it)->name, (*it)->enemyId);
            removeFromTurnOrder(it->get());
            it = enemies_.erase(it);
            removed = true;
        } else {
            ++it;
        }
    }
    if (!removed) return;
    if (enemies_.empty()) {
        enterClearedRoom(looter);
    } else if (!turnOrder_.empty()) {
        if (currentTurnIndex_ >= static_cast<int>(turnOrder_.size())) {
            currentTurnIndex_ = 0;
        }
    }
}

void Game::allyTurn() {
    if (turnOrder_.empty()) return;
    Character* actor = getCurrentActor(); // M1: bounds-checked
    if (!actor || actor->isDowned || actor->isDead) {
        advanceTurn();
        return;
    }
    if (enemies_.empty()) {
        advanceTurn();
        return;
    }

    const std::string actorUid = actor->uid;
    const int roomBefore = roomCount_;

    // 1) Heal a downed / critically hurt ally when we can (potion or Cleric Healing Word).
    const int healIdx = pickAllyAiHealTarget();
    const bool canWord = actor->characterClass == CharacterClass::CLERIC && actor->status.shackled <= 0
                         && specialAvailable(*actor); // v2.31: a Signature free use counts
    if (healIdx >= 0 && (actor->resources > 0 || canWord)) {
        if (canWord) {
            dmSay(actor->name + " calls a Healing Word for a wounded ally.");
            playerSpecialAction(0); // Cleric special ignores enemy index; heals lowest HP
        } else {
            dmSay(actor->name + " spends a potion on a wounded ally.");
            playerHeal(healIdx);
        }
        if (roomCount_ != roomBefore) return;
        Character* now = getCurrentActor();
        if (now && now->uid == actorUid) advanceTurn();
        return;
    }

    // 2) Pick a living foe — prefer finishing blows / focus fire.
    const int target = pickAllyAiEnemyTarget();
    if (target < 0) {
        purgeDownedEnemies();
        return;
    }

    Character& enemy = *enemies_[static_cast<size_t>(target)];

    // 3) Class special when it clearly beats a basic swing and we have a use left.
    // v2.30: a Fighter's Action Surge is once per fight — don't burn the turn retrying it.
    if (specialAvailable(*actor) && actor->characterClass != CharacterClass::CLERIC
        && actor->status.shackled <= 0 && allySpecialBeatsBasic(*actor, enemy)) {
        dmSay(actor->name + " uses " + actor->getSpecialAbilityName() + "!");
        playerSpecialAction(target);
        if (roomCount_ != roomBefore) return;
        Character* now = getCurrentActor();
        if (now && now->uid == actorUid) advanceTurn();
        return;
    }

    // 4) Basic weapon attack — only with a validated living target (no playerAttack no-ops).
    playerAttack(target);
    if (roomCount_ != roomBefore) return;
    Character* now = getCurrentActor();
    if (now && now->uid == actorUid) {
        advanceTurn();
    }
}

void Game::processTurn() {
    if (gameOver_ || isMerchantRoom_ || !isHost_) return;
    syncStatRules();
    // Cleared chamber: wait for Search / Short Rest / Onward — do not AI-spin allies.
    if (enemies_.empty()) return;
    if (turnOrder_.empty()) return;

    Character* current = getCurrentActor(); // M1: bounds-checked
    if (!current) { advanceTurn(); return; }

    // Dead / zero-HP heroes must never take a normal action turn.
    if (!current->isDead && current->currentHp <= 0) current->isDowned = true;
    if (current->isDead) {
        removeFromTurnOrder(current);
        checkPartyDefeat();
        return;
    }

    bool isEnemy = std::find_if(enemies_.begin(), enemies_.end(),
        [&](auto& e){ return e.get() == current; }) != enemies_.end();

    // 0-HP foes still sitting in initiative look like "NPC turns that do nothing".
    if (isEnemy && (current->isDowned || current->currentHp <= 0)) {
        purgeDownedEnemies();
        return;
    }

    if (current->isDowned) {
        if (!isEnemy) {
            // Stable PCs skip death saves until healed or damaged again.
            if (current->isStable) {
                addChatMessage("System", current->name + " remains stable (unconscious).");
                advanceTurn();
                return;
            }
            int roll = getRandomInt(1, 20);
            // v2.31 Grit: +floor(CONmod / 2) on death saves (natural 1 and 20 as before).
            const int grit = current->statRules ? stats::deathSaveBonus(current->effMod(2)) : 0;
            if (roll == 20) {
                current->heal(1);
                addChatMessage("System", current->name + " stood back up with a Natural 20!");
            } else if (roll != 1 && roll + grit >= 10) {
                current->deathSaveSuccesses++;
                if (current->deathSaveSuccesses >= 3) {
                    current->currentHp = 0;
                    current->isStable = true;
                    current->deathSaveSuccesses = 3;
                    addChatMessage("System", current->name + " is now stable.");
                    dmSay(current->name + " stabilizes — still down, but no longer making death saves.");
                } else {
                    addChatMessage("System", current->name + " death save success (" +
                                  std::to_string(current->deathSaveSuccesses) + "/3).");
                }
            } else {
                current->deathSaveFailures += (roll == 1 ? 2 : 1);
                if (current->deathSaveFailures >= 3) {
                    current->isDead = true;
                    current->isDowned = true;
                    current->isStable = false;
                    current->deathSaveFailures = 3;
                    addChatMessage("System", current->name + " has died.");
                    lastEvent_ = current->name + " has fallen permanently.";
                    dmSay(current->name + " breathes their last. The rest of the party fights on.");
                    removeFromTurnOrder(current);
                    checkPartyDefeat();
                    return;
                } else {
                    addChatMessage("System", current->name + " death save failure (" +
                                  std::to_string(current->deathSaveFailures) + "/3).");
                }
            }
        }
        advanceTurn();
        return;
    }

    if (isEnemy) {
        enemyTurn();
        if (!gameOver_ && getCurrentActor() == current) {
            advanceTurn();
        }
        return;
    }

    if (isAllyAi(current)) {
        allyTurn();
        return;
    }
    // Otherwise it's a human-controlled hero — wait for UI input.
}


void Game::applyLegendaryOnHit(Character* actor, int damageDealt) {
    if (!actor || damageDealt <= 0 || !actor->equippedWeapon) return;
    auto& w = actor->equippedWeapon;
    if (!w->subEffectUnlocked()) return;
    int heal = 0;
    if (w->subEffect == static_cast<int>(LegendarySubEffect::LIFESTEAL)) {
        heal = 1 + (w->upgradeLevel / 10);
    } else if (w->subEffect == static_cast<int>(LegendarySubEffect::ON_HIT_HEAL)) {
        heal = 2;
    }
    if (heal > 0) {
        actor->heal(heal);
        dmSay(actor->name + "'s Legendary weapon restores " + std::to_string(heal) + " HP.");
    }
}

void Game::tryFightShield(Character* defender, int& incomingDamage) {
    if (!defender || incomingDamage <= 0 || !defender->equippedArmor) return;
    auto& a = defender->equippedArmor;
    if (!a->subEffectUnlocked()) return;
    if (a->subEffect != static_cast<int>(LegendarySubEffect::FIGHT_SHIELD)) return;
    if (defender->fightShieldUsed) return;
    defender->fightShieldUsed = true;
    int absorb = 5 + (a->upgradeLevel / 5);
    int blocked = std::min(incomingDamage, absorb);
    incomingDamage -= blocked;
    dmSay(defender->name + "'s Legendary armor projects a once-per-fight shield (" + std::to_string(blocked) + " absorbed).");
}

void Game::dmSay(const std::string& line) {
    addChatMessage("Keeper", line); // narrator = the Lanternkeeper
}

void Game::wrenSay(const std::string& line) {
    // Wren has spoken from inside the Lantern since the Ashen Shrine (beat 6). Never online / DM.
    if (dmOnlyTable_) return;
    addChatMessage("Lantern", line);
}

int Game::proficiencyBonus() const {
    int level = players_.empty() ? 1 : players_[0]->level;
    return dnd::CombatSystem::proficiencyBonusForLevel(level);
}

bool Game::performWeaponAttack(Character* actor, Character& target, int atkVisIndex, bool targetIsEnemy, int targetIndex, bool sneakAttack,
                               bool special, int flatBonus) {
    if (!actor) return false;
    pushVisualEvent(targetIsEnemy ? VisualEventType::PLAYER_ATTACK : VisualEventType::ENEMY_ATTACK, atkVisIndex);

    // v2.31: INT Weak point widens the crit range; a special at Signature tier 3 one more.
    RollResult result = CombatSystem::performAttackRoll(*actor, special && targetIsEnemy);
    int mod = CombatSystem::getPrimaryModifier(*actor);
    int pb = CombatSystem::proficiencyBonusForLevel(actor->level);
    int gearBonus = actor->equippedWeapon ? actor->equippedWeapon->bonus : 0;
    int mockPenalty = 0;
    if (actor->nextAttackPenalty > 0) {
        mockPenalty = actor->nextAttackPenalty;
        actor->nextAttackPenalty = 0;
        result.total -= mockPenalty;
    }
    const int blindPenalty = (targetIsEnemy && actor->status.blind > 0) ? ai::kBlindToHit : 0;
    result.total -= blindPenalty; // v2.30: Crown flare
    // v2.31 Inspire: a companion gets +floor(CHAmod / 4) to hit from the party's best CHA.
    int inspireBonus = 0;
    if (targetIsEnemy && actor->statRules && !players_.empty() && players_[0].get() != actor) {
        inspireBonus = stats::inspire(partyChaMod(actor));
        result.total += inspireBonus;
    }

    std::stringstream ss;
    ss << actor->name << " attacks " << target.name << "! d20=" << result.dieRoll
       << " + " << mod << " (ability) + " << pb << " (prof) + " << gearBonus
       << " (magic)";
    if (mockPenalty > 0) ss << " - " << mockPenalty << " (rattled)";
    if (blindPenalty > 0) ss << " - " << blindPenalty << " (blinded)";
    if (inspireBonus > 0) ss << " + " << inspireBonus << " (inspired)";
    ss << " = " << result.total << " vs AC " << target.armorClass << ". ";

    bool hit = result.total >= target.armorClass || result.isCriticalHit;
    if (result.isCriticalFail) hit = false;
    if (encounterCurveActive()) {
        // v2.29 hit clamp: always needs 3..18 on the die (15%–90%).
        const curve::HitCheck hc = curve::attackHits(result.dieRoll, result.total - result.dieRoll, target.armorClass);
        hit = hc.hit;
        if (hc.clamped) ss << "(needs " << hc.need << "+ — hit clamp) ";
    }

    if (hit) {
        int extraDice = sneakAttack ? actor->sneakAttackDice() : 0;
        int dmg = CombatSystem::calculateDamage(*actor, result.isCriticalHit, extraDice, 6);
        dmg += std::max(0, flatBonus); // v2.31: Action Surge's second swing (+Heft)
        if (special && targetIsEnemy && actor->signatureTier() >= 2) // Signature: potency +25%
            dmg = static_cast<int>(std::lround(dmg * stats::kSignaturePotency));
        // v2.31 Break guard: STR mod +3 strips a foe's shield-wall.
        if (targetIsEnemy && actor->statRules && target.status.guarded > 0 && stats::breaksGuard(actor->effMod(0))) {
            target.status.guarded = 0;
            target.armorClass -= ai::kGuardAc;
            ss << "(guard broken) ";
        }
        if (!targetIsEnemy) {
            tryFightShield(&target, dmg);
        }
        target.takeDamage(dmg);
        if (targetIsEnemy) {
            noteDailyQuestDamage(dmg);
            applyLegendaryOnHit(actor, dmg);
            noteFoeDamaged(target, dmg, actor, result.isCriticalHit); // v2.30: stagger / phases
        }
        pushVisualEvent(targetIsEnemy ? VisualEventType::ENEMY_DAMAGE : VisualEventType::PLAYER_DAMAGE, targetIndex);
        ss << (result.isCriticalHit ? "CRITICAL HIT! " : "Hit! ") << "Damage " << dmg;
        if (sneakAttack) ss << " (includes Sneak Attack)";
        ss << ".";
        if (result.isCriticalHit) dmSay("The strike lands true — a critical hit!");
    } else {
        ss << "Miss!";
        if (result.isCriticalFail) {
            ss << " (natural 1)";
            dmSay(actor->name + " swings wildly and nearly drops their weapon.");
        }
    }
    lastEvent_ = ss.str();
    addChatMessage("Combat", lastEvent_);
    return hit && target.currentHp <= 0;
}

void Game::resolveEnemyDefeated(Character* actor, int /*targetEnemyIndex*/) {
    // Remove dead enemies; pause in a cleared chamber (do not auto-spawn next room).
    // Strip turnOrder_ entries BEFORE destroying the Character (no dangling pointers).
    for (auto it = enemies_.begin(); it != enemies_.end(); ) {
        if ((*it)->currentHp <= 0) {
            returnStolenGold(**it, actor);
            grantKillLoot(actor, (*it)->name, (*it)->enemyId); // sets Felled … lastEvent_ + journal/DM
            removeFromTurnOrder(it->get());
            it = enemies_.erase(it);
        } else ++it;
    }

    if (enemies_.empty()) {
        enterClearedRoom(actor);
    } else {
        rebuildTurnOrder();
        // The attacker already spent their action on the killing blow — move on.
        advanceTurn();
    }
}

void Game::playerAttack(int targetEnemyIndex) {
    if (gameOver_ || enemies_.empty() || targetEnemyIndex < 0 || static_cast<size_t>(targetEnemyIndex) >= enemies_.size()) return;
    if (turnOrder_.empty()) return;
    Character* actor = getCurrentActor(); // M1: bounds-checked
    if (!actor || actor->isDowned || actor->isDead) return;

    // Only party members (hero + NPC allies) use this attack path
    bool isPlayer = false;
    int playerIdx = 0;
    for (size_t i = 0; i < players_.size(); ++i) {
        if (players_[i].get() == actor) { isPlayer = true; playerIdx = static_cast<int>(i); break; }
    }
    if (!isPlayer) return;

    Character& enemy = *enemies_[static_cast<size_t>(targetEnemyIndex)];
    bool killed = performWeaponAttack(actor, enemy, playerIdx, true, targetEnemyIndex, false);
    if (killed || enemy.currentHp <= 0) {
        resolveEnemyDefeated(actor, targetEnemyIndex);
    } else {
        advanceTurn();
    }
}

void Game::playerHeal(int targetPlayerIndex) {
    if (gameOver_) return;
    // Potion of Healing (SRD): 2d4+2. Anyone can drink one by spending a resource (supply).
    if (turnOrder_.empty()) return;
    Character* actor = getCurrentActor(); // M1: bounds-checked
    if (!actor || actor->isDowned || actor->isDead || actor->currentHp <= 0) {
        lastEvent_ = "You're down — you can't use a potion until you're back up.";
        return;
    }
    // M2: only a party member on their own turn may drink (mirrors playerAttack's guard).
    if (!isPartyMember(actor)) return;
    if (actor->resources <= 0 || targetPlayerIndex < 0 || static_cast<size_t>(targetPlayerIndex) >= players_.size()) {
        lastEvent_ = "No potions left (need a resource/supply).";
        return;
    }

    actor->resources--;
    Character& target = *players_[static_cast<size_t>(targetPlayerIndex)];
    int amount = getRandomInt(1, 4) + getRandomInt(1, 4) + 2;
    amount = target.heal(amount); // v2.31: Mending (WIS)
    if (&target != actor) actor->status.healedRecently = 2; // v2.30: foes notice healers
    lastEvent_ = actor->name + " drinks a Potion of Healing on " + target.name + " for " + std::to_string(amount) + " HP.";
    dmSay(target.name + " feels vitality return as the potion takes hold.");
    addChatMessage("Combat", lastEvent_);
    advanceTurn();
}

void Game::playerSpecialAction(int targetEnemyIndex) {
    if (gameOver_ || turnOrder_.empty()) return;
    Character* actor = getCurrentActor(); // M1: bounds-checked
    if (!actor || actor->isDowned || actor->isDead || actor->currentHp <= 0) return;
    // M2: enemies never run the hero special path (an enemy "Action Surge" would hit enemies_).
    if (!isPartyMember(actor)) return;

    int playerIdx = 0;
    for (size_t i = 0; i < players_.size(); ++i) if (players_[i].get() == actor) playerIdx = static_cast<int>(i);

    if (actor->status.shackled > 0) {
        // v2.30: Shackle / Rattle — no class feature this turn (the turn is not spent).
        lastEvent_ = actor->name + " is shackled — no " + actor->getSpecialAbilityName() + " this turn.";
        addChatMessage("Combat", lastEvent_);
        return;
    }

    // Cleric Healing Word can target allies without an enemy
    if (actor->characterClass == CharacterClass::CLERIC) {
        if (!specialAvailable(*actor)) { lastEvent_ = "No spell slots left."; return; }
        const bool usedSignature = actor->signatureUses > 0;
        spendSpecialUse(*actor);
        // Heals the lowest-HP living ally (M2: never a dead ally; the UI target index is unused here).
        int best = -1;
        for (size_t i = 0; i < players_.size(); ++i) {
            if (!players_[i] || players_[i]->isDead) continue;
            if (best < 0 || players_[i]->currentHp < players_[static_cast<size_t>(best)]->currentHp) best = static_cast<int>(i);
        }
        if (best < 0) {
            if (usedSignature) actor->signatureUses++; else actor->resources++;
            lastEvent_ = "No ally to heal."; return;
        }
        Character& ally = *players_[static_cast<size_t>(best)];
        int wis = actor->healingWordModifier(); // v2.29: effective WIS (audit bug d)
        int amount = getRandomInt(1, 4) + wis;
        if (amount < 1) amount = 1;
        if (actor->signatureTier() >= 2) amount = static_cast<int>(std::lround(amount * stats::kSignaturePotency));
        amount = ally.heal(amount); // v2.31: the ally's Mending (WIS) adds to it
        if (&ally != actor) actor->status.healedRecently = 2; // v2.30: foes notice healers
        lastEvent_ = actor->name + " casts Healing Word on " + ally.name + " for " + std::to_string(amount) + " HP.";
        dmSay("A soft glow knits " + ally.name + "'s wounds — Healing Word.");
        addChatMessage("Combat", lastEvent_);
        advanceTurn();
        return;
    }

    if (enemies_.empty() || targetEnemyIndex < 0 || static_cast<size_t>(targetEnemyIndex) >= enemies_.size()) return;
    // Fighter Action Surge: once per combat (still spends a resource/supply); a Signature free use
    // (main stat 20+, solo) allows one more.
    if (actor->characterClass == CharacterClass::FIGHTER && actor->specialUsedThisCombat && actor->signatureUses <= 0) {
        lastEvent_ = "Action Surge already used this fight — rest or finish the battle for another chance.";
        dmSay(actor->name + " is still catching their breath from the last surge.");
        return;
    }
    if (!specialAvailable(*actor)) { lastEvent_ = "No uses remaining for that feature."; return; }
    spendSpecialUse(*actor);
    if (actor->characterClass == CharacterClass::FIGHTER) actor->specialUsedThisCombat = true;

    Character& enemy = *enemies_[static_cast<size_t>(targetEnemyIndex)];

    if (actor->characterClass == CharacterClass::FIGHTER) {
        dmSay(actor->name + " shouts and surges forward — Action Surge!");
        // Extra attack action: two weapon attacks
        performWeaponAttack(actor, enemy, playerIdx, true, targetEnemyIndex, false, true);
        if (enemy.currentHp > 0) {
            dmSay("Still standing! " + actor->name + " swings again.");
            // v2.31: the second swing adds +floor(STRmod / 2).
            const int extra = actor->statRules ? stats::heft(actor->effMod(0)) : 0;
            performWeaponAttack(actor, enemy, playerIdx, true, targetEnemyIndex, false, true, extra);
        }
        if (enemy.currentHp <= 0) resolveEnemyDefeated(actor, targetEnemyIndex);
        else advanceTurn();
        return;
    }

    if (actor->characterClass == CharacterClass::ROGUE) {
        dmSay(actor->name + " slips into a flank and strikes — Sneak Attack!");
        performWeaponAttack(actor, enemy, playerIdx, true, targetEnemyIndex, true, true);
        if (enemy.currentHp <= 0) resolveEnemyDefeated(actor, targetEnemyIndex);
        else advanceTurn();
        return;
    }

    if (actor->characterClass == CharacterClass::WIZARD) {
        // Magic Missile: 3 darts of 1d4+1, auto-hit (SRD)
        // v2.31: each dart is 1d4 + 1 + floor(INTmod / 3) in solo play; Signature potency +25%.
        const int dartFlat = actor->statRules ? stats::dartBonus(actor->effMod(3)) : 1;
        int total = 0;
        for (int i = 0; i < 3; ++i) total += getRandomInt(1, 4) + dartFlat;
        if (actor->signatureTier() >= 2) total = static_cast<int>(std::lround(total * stats::kSignaturePotency));
        enemy.takeDamage(total);
        noteDailyQuestDamage(total);
        noteFoeDamaged(enemy, total, actor, true); // a spell scorches like a crit (Hydra regrowth)
        pushVisualEvent(VisualEventType::PLAYER_ATTACK, playerIdx);
        pushVisualEvent(VisualEventType::ENEMY_DAMAGE, targetEnemyIndex);
        lastEvent_ = actor->name + " casts Magic Missile! Three darts deal " + std::to_string(total) + " force damage to " + enemy.name + ".";
        dmSay("Glowing darts streak unerringly to their mark.");
        addChatMessage("Combat", lastEvent_);
        if (enemy.currentHp <= 0) resolveEnemyDefeated(actor, targetEnemyIndex);
        else advanceTurn();
        return;
    }

    if (actor->characterClass == CharacterClass::BARD) {
        // Cutting Quip: CHA attack vs AC; psychic sting + rattle (-2 next attack). Original flavor.
        dmSay(actor->name + " fires a Cutting Quip — a barbed line that bites deeper than steel.");
        RollResult result = CombatSystem::performAttackRoll(*actor, true);
        int mod = CombatSystem::getPrimaryModifier(*actor);
        int pb = CombatSystem::proficiencyBonusForLevel(actor->level);
        if (actor->status.blind > 0) result.total -= ai::kBlindToHit; // v2.30: Crown flare
        bool hit = result.total >= enemy.armorClass || result.isCriticalHit;
        if (result.isCriticalFail) hit = false;
        if (encounterCurveActive()) {
            hit = curve::attackHits(result.dieRoll, result.total - result.dieRoll, enemy.armorClass).hit;
        }
        pushVisualEvent(VisualEventType::PLAYER_ATTACK, playerIdx);
        std::stringstream ss;
        ss << actor->name << " Cutting Quip vs " << enemy.name << "! d20=" << result.dieRoll
           << " + " << mod << " (CHA) + " << pb << " (prof) = " << result.total
           << " vs AC " << enemy.armorClass << ". ";
        if (hit) {
            int cha = Attributes::getModifier(actor->effectiveAttr(5));
            int dmg = getRandomInt(1, 4) + cha;
            if (result.isCriticalHit) dmg += getRandomInt(1, 4);
            const bool potent = actor->signatureTier() >= 2; // v2.31 Signature: +25% sting and rattle
            if (potent) dmg = static_cast<int>(std::lround(dmg * stats::kSignaturePotency));
            if (dmg < 1) dmg = 1;
            enemy.takeDamage(dmg);
            noteDailyQuestDamage(dmg);
            noteFoeDamaged(enemy, dmg, actor, result.isCriticalHit);
            enemy.nextAttackPenalty = potent ? 3 : 2;
            pushVisualEvent(VisualEventType::ENEMY_DAMAGE, targetEnemyIndex);
            ss << (result.isCriticalHit ? "CRITICAL! " : "Hit! ")
               << "Psychic sting " << dmg << ". " << enemy.name << " is rattled (-" << enemy.nextAttackPenalty << " next attack).";
            dmSay(enemy.name + " flinches at the cutting words.");
        } else {
            ss << "Miss!";
            if (result.isCriticalFail) ss << " (natural 1)";
            dmSay("The quip falls flat — " + enemy.name + " shrugs it off.");
        }
        lastEvent_ = ss.str();
        addChatMessage("Combat", lastEvent_);
        if (enemy.currentHp <= 0) resolveEnemyDefeated(actor, targetEnemyIndex);
        else advanceTurn();
        return;
    }
}

void Game::playerRest(bool force) {
    if (gameOver_) { lastEvent_ = "Game Over — start a new adventure."; return; }
    if (getCurrentActor() && getCurrentActor()->isDowned) {
        lastEvent_ = "You're dying — make death saves, you can't rest now.";
        return;
    }
    // Block Short Rest while foes are still in the room (mid-combat).
    // DM grant can pass force=true to allow a story beat.
    if (!force && !isMerchantRoom_ && !enemies_.empty()) {
        lastEvent_ = "Can't Short Rest in the middle of a fight!";
        dmSay("Steel yourselves — rest when the chamber is clear.");
        return;
    }
    if (isMerchantRoom_) {
        roomCount_++;
        advanceLongRestProgress();
        spawnRoomContent();
        const bool bossRoom = hasLivingBossEnemy();
        const std::string bossToast = lastEvent_;
        if (!bossRoom) generateRoomDescription();
        if (!enemies_.empty()) rollInitiative();
        else { turnOrder_.clear(); currentTurnIndex_ = 0; }
        if (bossRoom && bossToast.rfind("BOSS!", 0) == 0) {
            lastEvent_ = bossToast;
        } else {
            lastEvent_ = "You bid the merchant farewell and press deeper.";
            dmSay("The merchant nods. \"Luck in the dark, friends.\"");
        }
        return;
    }
    if (!force && roomRestUsed_) {
        lastEvent_ = "Already rested here — press Onward for the next rest opportunity.";
        dmSay("You've already taken your rest in this chamber. Move on before resting again.");
        return;
    }
    // Short Rest (5e-inspired): spend hit dice vibe — recover half missing HP + some features
    for (auto& p : players_) {
        if (p->isDead) continue; // dead heroes stay down
        int missing = p->maxHp - p->currentHp;
        int recover = std::max(p->hitDie() + Attributes::getModifier(p->effectiveAttr(2)), missing / 2);
        if (recover < 1) recover = 1;
        if (recover > missing && missing > 0) recover = missing;
        if (missing == 0) recover = 0;
        p->heal(recover);
        int back = std::max(1, p->maxResources / 2);
        if (p->statRules && stats::restResourceBonus(p->effMod(4))) ++back; // v2.31: WIS +4
        p->resources = std::min(p->maxResources, p->resources + back);
        p->isDowned = false;
        p->isStable = false;
        p->deathSaveSuccesses = 0;
        p->deathSaveFailures = 0;
    }
    if (!force) roomRestUsed_ = true;
    lastEvent_ = "Short Rest complete. Wounds bind; some power returns.";
    dmSay("You catch your breath in a quiet alcove. Short Rest taken — one rest per clear (Short or Long).");
    addJournalEntry("The party took a short rest.");
    addChatMessage("Combat", lastEvent_);
    if (enemies_.empty()) {
        // Remain in the cleared chamber so Search / Onward stay available.
        turnOrder_.clear();
        currentTurnIndex_ = 0;
        return;
    }
    rollInitiative();
}



void Game::advanceLongRestProgress() {
    longRestProgress_++;
}

void Game::playerLongRest(bool force) {
    if (gameOver_) { lastEvent_ = "Game Over — start a new adventure."; return; }
    if (getCurrentActor() && getCurrentActor()->isDowned) {
        lastEvent_ = "You're dying — make death saves, you can't rest now.";
        return;
    }
    if (!force && !isMerchantRoom_ && !enemies_.empty()) {
        lastEvent_ = "Can't Long Rest in the middle of a fight!";
        dmSay("Steel yourselves — rest when the chamber is clear.");
        return;
    }
    if (isMerchantRoom_) {
        lastEvent_ = "Leave the merchant stall before settling in for a Long Rest.";
        return;
    }
    if (!force && roomRestUsed_) {
        lastEvent_ = "Already rested here — press Onward for the next rest opportunity.";
        dmSay("You've already taken your rest in this chamber. Move on before resting again.");
        return;
    }
    if (!force && longRestProgress_ < longRestNextAvailableRoom_) {
        const int left = longRestNextAvailableRoom_ - longRestProgress_;
        lastEvent_ = "Long Rest on cooldown — clear " + std::to_string(left) +
                     " more room" + (left == 1 ? "" : "s") + " first (every 5 rooms).";
        dmSay("A full Long Rest takes time — push through a few more chambers, or take a Short Rest now.");
        return;
    }
    // Long Rest (5e-inspired): full HP, full resources/specials/supplies; clear downed state.
    for (auto& p : players_) {
        if (p->isDead) continue;
        p->currentHp = p->maxHp;
        p->resources = p->maxResources;
        p->isDowned = false;
        p->isStable = false;
        p->deathSaveSuccesses = 0;
        p->deathSaveFailures = 0;
    }
    if (!force) {
        roomRestUsed_ = true;
        longRestNextAvailableRoom_ = longRestProgress_ + 5; // Long Rest every 5 rooms (all modes)
    }
    lastEvent_ = "Long Rest complete. Full strength and supplies restored.";
    dmSay("Safe enough to settle in. A Long Rest restores your hit points and special uses in full.");
    addJournalEntry("The party took a long rest.");
    addChatMessage("Combat", lastEvent_);
    if (enemies_.empty()) {
        turnOrder_.clear();
        currentTurnIndex_ = 0;
        return;
    }
    rollInitiative();
}

bool Game::useInventoryPotion(const std::string& playerName, int invIndex) {
    if (gameOver_) return false;
    Character* hero = findCharacter(playerName);
    if (!hero || hero->isDead) {
        lastEvent_ = "Cannot use a potion right now.";
        return false;
    }
    if (hero->isDowned) {
        lastEvent_ = "You're down — an ally must help before you can drink.";
        return false;
    }
    if (invIndex < 0 || static_cast<size_t>(invIndex) >= hero->inventory.size()) {
        lastEvent_ = "No such item.";
        return false;
    }
    auto item = hero->inventory[static_cast<size_t>(invIndex)];
    if (!item || item->type != ItemType::POTION) {
        lastEvent_ = "That is not a potion.";
        return false;
    }
    const std::string potionName = item->name;
    const int healAmt = std::max(1, item->bonus);
    const bool wayfarer = (potionName == "Wayfarer's Tonic");
    hero->inventory.erase(hero->inventory.begin() + invIndex);
    hero->heal(healAmt);
    if (wayfarer) {
        hero->resources = std::min(hero->maxResources, hero->resources + 1);
        lastEvent_ = hero->name + " drinks Wayfarer's Tonic — +" + std::to_string(healAmt)
            + " HP and +1 special/supply.";
        dmSay("The tonic warms " + hero->name + "'s veins; wounds ease and a spark of power returns.");
    } else {
        lastEvent_ = hero->name + " drinks " + potionName + " for " + std::to_string(healAmt) + " HP.";
        dmSay(hero->name + " downs the potion and feels steadier.");
    }
    addJournalEntry(lastEvent_);
    addChatMessage("Combat", lastEvent_);
    return true;
}


void Game::playerInteract(const std::string& playerName) {
    if (gameOver_) { lastEvent_ = "Game Over — start a new adventure."; return; }
    Character* hero = findCharacter(playerName);
    if (!hero || hero->isDead || hero->isDowned) return;

    // Search only when the chamber is clear (not mid-fight).
    if (!enemies_.empty()) {
        lastEvent_ = "Too dangerous to Search while foes remain!";
        dmSay("Clear the chamber first — then Search.");
        return;
    }
    if (roomSearchUsed_) {
        lastEvent_ = "You've already searched this room.";
        dmSay("Nothing more turns up here — press Onward when ready.");
        return;
    }
    // One meaningful Search per room (success or fail locks further Search).
    roomSearchUsed_ = true;
    dqSearches_++;

    const bool storySearch = trySoloQuestSearch(hero);
    if (storySearch) {
        // Story Search handled (clue / rune-key / shrine).
        memoryOnStorySearch();
    } else if (hero->performSavingThrow(hero->searchScore(), 12)) { // v2.29: effective INT (bug d)
        int found = luckGold(15 + roomCount_ * 2 + getRandomInt(0, 10)); // v2.31 Luck (CHA)
        hero->gold += found;
        lastEvent_ = hero->name + " searched and found " + std::to_string(found) + " gold pieces!";
        addChatMessage("Combat", lastEvent_);
        dmSay("A successful Investigation check reveals a hidden pouch.");
        addJournalEntry(lastEvent_);
    } else {
        hero->takeDamage(3);
        int idx = 0;
        for(size_t i=0; i<players_.size(); ++i) if(players_[i].get() == hero) idx = static_cast<int>(i);
        pushVisualEvent(VisualEventType::PLAYER_DAMAGE, idx);
        lastEvent_ = "Fail! A trap hit " + hero->name + " for 3 damage!";
        addChatMessage("Combat", lastEvent_);
        dmSay("A pressure plate clicks — poison darts!");
        if (hero->isDead) {
            removeFromTurnOrder(hero);
            checkPartyDefeat();
            return;
        }
    }
    if (!storySearch) memoryOnCrawlEvent(true);
    // Interact spends your turn like Attack/Heal when initiative is active.
    if (!turnOrder_.empty()) {
        advanceTurn();
    }
}

void Game::playerAdvanceFromCleared() {
    if (gameOver_) { lastEvent_ = "Game Over — start a new adventure."; return; }
    if (isMerchantRoom_) {
        lastEvent_ = "Use Leave to depart the merchant.";
        return;
    }
    if (!enemies_.empty()) {
        lastEvent_ = "Can't press Onward — foes remain!";
        dmSay("Steel yourselves — finish the fight first.");
        return;
    }

    // Boss Raid / Challenge Dungeon clear: keep spoils on the same hero, restore prior adventure mode.
    if (isBossRaid()) {
        advanceLongRestProgress(); // cleared vault counts as a room for Long Rest
        finishBossRaidKeepParty();
        lastEvent_ = "Raid victorious! Spoils kept on your story hero. Onward resumes your adventure, or use Menu.";
        dmSay(lastEvent_);
        addChatMessage("Quest", "Boss Raid cleared — character preserved.");
        return;
    }
    if (isChallengeDungeon()) {
        advanceLongRestProgress(); // cleared vault counts as a room for Long Rest
        finishChallengeDungeonKeepParty();
        lastEvent_ = "Challenge Dungeon cleared! Spoils kept on your story hero. Onward resumes your adventure, or use Menu.";
        dmSay(lastEvent_);
        addChatMessage("Quest", "Challenge Dungeon cleared — character preserved.");
        return;
    }
    if (isArena()) {
        if (arenaWave_ < ARENA_MAX_WAVES) {
            const int next = arenaWave_ + 1;
            advanceLongRestProgress(); // wave clear counts as a room for Long Rest
            spawnArenaWave(next);
            lastEvent_ = "Onward — Ember Ring Wave " + std::to_string(next)
                + "/" + std::to_string(ARENA_MAX_WAVES) + " (score " + std::to_string(arenaScore_) + ").";
            addChatMessage("Arena", lastEvent_);
            addJournalEntry("Advanced to Arena Wave " + std::to_string(next) + ".");
            return;
        }
        advanceLongRestProgress(); // final wave clear counts for Long Rest
        finishArenaKeepParty();
        lastEvent_ = "Ember Ring cleared! Final score " + std::to_string(arenaScore_)
            + ". Spoils kept on your story hero. Onward resumes your adventure, or use Menu.";
        dmSay(lastEvent_);
        addChatMessage("Quest", "Arena cleared — character preserved. Score "
                       + std::to_string(arenaScore_) + ".");
        return;
    }
    if (isEndlessDeep()) {
        const int next = endlessDepth_ + 1;
        advanceLongRestProgress(); // depth clear counts as a room for Long Rest
        spawnEndlessDepth(next);
        lastEvent_ = "Onward — Ashen Deep Depth " + std::to_string(next)
            + " (best this run " + std::to_string(endlessBestDepthThisRun_) + ").";
        addChatMessage("Endless", lastEvent_);
        addJournalEntry("Descended to Endless Deep depth " + std::to_string(next) + ".");
        return;
    }

    // Solo story: Act 1 → Act 2 → Act 3 → Act 4 → procedural endgame.
    if (isSoloQuestScripted()) {
        if (static_cast<SoloQuestBeat>(questBeat_) == SoloQuestBeat::CRYPT_DOORS && !questCryptKeyFound_) {
            dmSay("The doors yield grudgingly — you force them without the rune-key. Dust and bone-scent spill out.");
            addJournalEntry("Forced Hollowbarrow's doors without the Ashen Rune-Key.");
        }
        if (static_cast<SoloQuestBeat>(questBeat_) == SoloQuestBeat::ACT2_LOFT && !questAct2LedgerFound_) {
            dmSay("You leave without the ledger — the Tithe will still have copies. Steel yourselves.");
            addJournalEntry("Left the Ledger Loft without the soul-ledger.");
        }
        if (static_cast<SoloQuestBeat>(questBeat_) == SoloQuestBeat::ACT3_RELIC && !questAct3SealFound_) {
            dmSay("You leave without the Ember Seal — the well will not close kindly.");
            addJournalEntry("Left Ember Seal Niche without the seal.");
        }
        if (static_cast<SoloQuestBeat>(questBeat_) == SoloQuestBeat::ACT4_EMBER && !questAct4EmberFound_) {
            dmSay("You leave without the Vigil Ember — the siphon will not turn cleanly.");
            addJournalEntry("Left Vigil Niche without the Vigil Ember.");
        }

        if (questBeat_ >= static_cast<int>(SoloQuestBeat::MILLHOLLOW)
            && questBeat_ < static_cast<int>(SoloQuestBeat::RESOLUTION)) {
            questBeat_++;
            roomCount_++;
            advanceLongRestProgress();
            roomSearchUsed_ = false; roomRestUsed_ = false;
            applySoloQuestRoom();
            if (!enemies_.empty()) rollInitiative();
            lastEvent_ = std::string("Onward — ") + soloQuestBeatName(questBeat_) + ".";
            addChatMessage("Quest", lastEvent_);
            addJournalEntry("The party advanced to " + std::string(soloQuestBeatName(questBeat_)) + ".");
            return;
        }
        if (questBeat_ == static_cast<int>(SoloQuestBeat::RESOLUTION)) {
            questComplete_ = true;
            questBeat_ = static_cast<int>(SoloQuestBeat::ACT2_GREEN);
            roomCount_++;
            advanceLongRestProgress();
            roomSearchUsed_ = false; roomRestUsed_ = false;
            applySoloQuestRoom();
            if (!enemies_.empty()) rollInitiative();
            lastEvent_ = "Onward — Act 2: The Tithe begins.";
            addChatMessage("Quest", lastEvent_);
            addJournalEntry("Act 2 begins — The Tithe, on the green.");
            return;
        }
        if (questBeat_ >= static_cast<int>(SoloQuestBeat::ACT2_GREEN)
            && questBeat_ < static_cast<int>(SoloQuestBeat::ACT2_SETTLED)) {
            questBeat_++;
            roomCount_++;
            advanceLongRestProgress();
            roomSearchUsed_ = false; roomRestUsed_ = false;
            applySoloQuestRoom();
            if (!enemies_.empty()) rollInitiative();
            lastEvent_ = std::string("Onward — ") + soloQuestBeatName(questBeat_) + ".";
            addChatMessage("Quest", lastEvent_);
            addJournalEntry("The party advanced to " + std::string(soloQuestBeatName(questBeat_)) + ".");
            return;
        }
        if (questBeat_ == static_cast<int>(SoloQuestBeat::ACT2_SETTLED)) {
            if (vennChoice_ == VENN_UNDECIDED) applyVennChoice(VENN_ENDED, false); // no answer: old ending
            questAct2Complete_ = true;
            questComplete_ = true;
            questBeat_ = static_cast<int>(SoloQuestBeat::ACT3_RUMOR);
            roomCount_++;
            advanceLongRestProgress();
            roomSearchUsed_ = false; roomRestUsed_ = false;
            applySoloQuestRoom();
            if (!enemies_.empty()) rollInitiative();
            lastEvent_ = "Onward — Act 3: Emberdeep begins.";
            addChatMessage("Quest", lastEvent_);
            addJournalEntry("Act 3 begins — Emberdeep, at the old well.");
            return;
        }
        if (questBeat_ >= static_cast<int>(SoloQuestBeat::ACT3_RUMOR)
            && questBeat_ < static_cast<int>(SoloQuestBeat::ACT3_SEALED)) {
            questBeat_++;
            roomCount_++;
            advanceLongRestProgress();
            roomSearchUsed_ = false; roomRestUsed_ = false;
            applySoloQuestRoom();
            if (!enemies_.empty()) rollInitiative();
            lastEvent_ = std::string("Onward — ") + soloQuestBeatName(questBeat_) + ".";
            addChatMessage("Quest", lastEvent_);
            addJournalEntry("The party advanced to " + std::string(soloQuestBeatName(questBeat_)) + ".");
            return;
        }
        if (questBeat_ == static_cast<int>(SoloQuestBeat::ACT3_SEALED)) {
            if (wellChoice_ == WELL_UNDECIDED) applyWellChoice(WELL_SEALED_IN, false); // no answer: old ending
            questAct3Complete_ = true;
            questAct2Complete_ = true;
            questComplete_ = true;
            questBeat_ = static_cast<int>(SoloQuestBeat::ACT4_WATCH);
            roomCount_++;
            advanceLongRestProgress();
            roomSearchUsed_ = false; roomRestUsed_ = false;
            applySoloQuestRoom();
            if (!enemies_.empty()) rollInitiative();
            lastEvent_ = "Onward — Act 4: Ashwake Vigil begins.";
            addChatMessage("Quest", lastEvent_);
            addJournalEntry("Act 4 begins — Ashwake Vigil on Millhollow's green.");
            return;
        }
        if (questBeat_ >= static_cast<int>(SoloQuestBeat::ACT4_WATCH)
            && questBeat_ < static_cast<int>(SoloQuestBeat::ACT4_KINDLED)) {
            questBeat_++;
            roomCount_++;
            advanceLongRestProgress();
            roomSearchUsed_ = false; roomRestUsed_ = false;
            applySoloQuestRoom();
            if (!enemies_.empty()) rollInitiative();
            lastEvent_ = std::string("Onward — ") + soloQuestBeatName(questBeat_) + ".";
            addChatMessage("Quest", lastEvent_);
            addJournalEntry("The party advanced to " + std::string(soloQuestBeatName(questBeat_)) + ".");
            return;
        }
        // ACT4_KINDLED → post-story endgame procedural
        questBeat_ = static_cast<int>(SoloQuestBeat::POST_QUEST);
        questAct4Complete_ = true;
        questAct3Complete_ = true;
        questAct2Complete_ = true;
        questComplete_ = true;
        roomCount_++;
            advanceLongRestProgress();
        roomSearchUsed_ = false; roomRestUsed_ = false;
        spawnRoomContent();
        const bool bossRoom = hasLivingBossEnemy();
        const std::string bossToast = lastEvent_;
        if (!isMerchantRoom_) {
            if (!bossRoom) generateRoomDescription();
            if (!enemies_.empty()) rollInitiative();
            else { turnOrder_.clear(); currentTurnIndex_ = 0; }
        } else {
            turnOrder_.clear();
            currentTurnIndex_ = 0;
        }
        if (bossRoom && bossToast.rfind("BOSS!", 0) == 0) {
            lastEvent_ = bossToast;
            addChatMessage("Quest", lastEvent_);
        } else {
            lastEvent_ = "Story complete — endgame rooms await. Room " + std::to_string(roomCount_) + ".";
            addChatMessage("Quest", "Acts 1–4 complete — endgame unlocked.");
            dmSay("Follow the ash. Lamplighter's Rounds, the Ashen Deep, the Ember Ring, the Cinder Crucible and the Three Drowned Lanterns await.");
        }
        addJournalEntry("Post-story endgame exploration begins (Acts 1–4 done).");
        return;
    }

    // Legacy / mid-save: finished Act 2 into procedural before Act 3 shipped → start Act 3.
    if (!isFocusedEndgameRun() && !isSoloCrawl()
        && questAct2Complete_ && !questAct3Complete_
        && !isSoloQuestScripted()) {
        questBeat_ = static_cast<int>(SoloQuestBeat::ACT3_RUMOR);
        roomCount_++;
            advanceLongRestProgress();
        roomSearchUsed_ = false; roomRestUsed_ = false;
        applySoloQuestRoom();
        if (!enemies_.empty()) rollInitiative();
        lastEvent_ = "Onward — Act 3: Emberdeep begins (story continues).";
        addChatMessage("Quest", lastEvent_);
        addJournalEntry("Act 3 begins from post–Act 2 save.");
        return;
    }

    // Legacy / mid-save: finished Act 3 into procedural before Act 4 shipped → start Act 4.
    if (!isFocusedEndgameRun() && !isSoloCrawl()
        && questAct3Complete_ && !questAct4Complete_
        && !isSoloQuestScripted()) {
        questBeat_ = static_cast<int>(SoloQuestBeat::ACT4_WATCH);
        roomCount_++;
            advanceLongRestProgress();
        roomSearchUsed_ = false; roomRestUsed_ = false;
        applySoloQuestRoom();
        if (!enemies_.empty()) rollInitiative();
        lastEvent_ = "Onward — Act 4: Ashwake Vigil begins (story continues).";
        addChatMessage("Quest", lastEvent_);
        addJournalEntry("Act 4 begins from post–Act 3 save.");
        return;
    }

    roomCount_++;
    advanceLongRestProgress();
    roomSearchUsed_ = false; roomRestUsed_ = false;
    spawnRoomContent();
    const bool bossRoom = hasLivingBossEnemy();
    const std::string bossToast = lastEvent_;
    if (!isMerchantRoom_) {
        if (!bossRoom) generateRoomDescription();
        if (!enemies_.empty()) rollInitiative();
        else { turnOrder_.clear(); currentTurnIndex_ = 0; }
    } else {
        turnOrder_.clear();
        currentTurnIndex_ = 0;
    }
    if (bossRoom && bossToast.rfind("BOSS!", 0) == 0) {
        lastEvent_ = bossToast;
        addChatMessage("Combat", lastEvent_);
        addJournalEntry("The party advanced to room " + std::to_string(roomCount_) + " (boss).");
    } else {
        lastEvent_ = "You press deeper into the dungeon… Room " + std::to_string(roomCount_) + ".";
        addChatMessage("Combat", lastEvent_);
        dmSay("Onward — a new chamber opens before you.");
        addJournalEntry("The party advanced to room " + std::to_string(roomCount_) + ".");
    }
}

void Game::playerIncreaseStat(const std::string& playerName, int statIndex) {
    if (gameOver_) return;
    syncStatRules();
    Character* hero = findCharacter(playerName);
    if (!hero) return;
    if (!hero->increaseAttribute(statIndex) && hero->pendingStatPoints > 0 && statIndex >= 0 && statIndex <= 5) {
        const std::string statName = std::string("STRDEXCONINTWISCHA").substr(static_cast<size_t>(statIndex) * 3, 3);
        lastEvent_ = statName + " is at its cap (" + std::to_string(hero->attrCapFor(statIndex))
            + " at level " + std::to_string(hero->level) + ") — spend the point elsewhere.";
    }
}

int Game::scoreHeroThreat(const Character& hero, bool partyHasDowned) const {
    if (hero.isDead) return -100000;
    int score = 0;
    if (hero.isDowned) {
        // Only chosen when no upright heroes remain — finish unstable first.
        score = 20 + hero.deathSaveFailures * 25;
        if (!hero.isStable) score += 15;
        return score;
    }
    // Living threats: prefer low HP (focus fire / finish) and pressure when someone is already down.
    score = 100;
    score += (hero.maxHp - hero.currentHp) * 4;
    if (hero.maxHp > 0 && hero.currentHp * 3 <= hero.maxHp) score += 35; // critically low
    if (hero.currentHp <= 4) score += 25; // easy finish
    if (partyHasDowned) score += 20; // "downed-adjacent" pressure on remaining fighters
    return score;
}

int Game::pickEnemyAiTarget() const {
    bool partyHasDowned = false;
    for (const auto& p : players_) {
        if (p && !p->isDead && p->isDowned) { partyHasDowned = true; break; }
    }

    int bestUp = -1, bestUpScore = -100000;
    int bestDown = -1, bestDownScore = -100000;
    for (size_t i = 0; i < players_.size(); ++i) {
        const Character* p = players_[i].get();
        if (!p || p->isDead) continue;
        int s = scoreHeroThreat(*p, partyHasDowned);
        if (p->isDowned) {
            if (s > bestDownScore) { bestDownScore = s; bestDown = static_cast<int>(i); }
        } else {
            if (s > bestUpScore) { bestUpScore = s; bestUp = static_cast<int>(i); }
        }
    }
    if (bestUp >= 0) return bestUp;
    return bestDown;
}

int Game::pickAllyAiEnemyTarget() const {
    int best = -1;
    int bestScore = -100000;
    for (size_t i = 0; i < enemies_.size(); ++i) {
        const Character* e = enemies_[i].get();
        if (!e || e->currentHp <= 0 || e->isDowned || e->isDead) continue;
        // Prefer finishing weak foes (focus fire).
        int score = 50 + (e->maxHp - e->currentHp) * 5;
        if (e->currentHp <= 6) score += 40;
        if (e->maxHp > 0 && e->currentHp * 3 <= e->maxHp) score += 20;
        // Slight bias toward lower AC (easier to hit with a basic attack).
        score += std::max(0, 16 - e->armorClass);
        if (enemyAiActive() && e->curveEL > 0) {
            // v2.30 (2f): casters and supports first, and break a telegraphed Slam / heavy.
            const int role = e->curveRole;
            if (role == static_cast<int>(curve::Role::CASTER) || role == static_cast<int>(curve::Role::SUPPORT)) score += 30;
            if (ai::intentOf(ai::moveFromInt(e->ai.move)) == ai::Intent::HEAVY) score += 25;
            if (ai::intentOf(ai::moveFromInt(e->ai.move)) == ai::Intent::HEAL) score += 20;
        }
        if (score > bestScore) {
            bestScore = score;
            best = static_cast<int>(i);
        }
    }
    return best;
}

int Game::pickAllyAiHealTarget() const {
    int best = -1;
    int bestScore = -100000;
    // During Ashen Lantern (beats 1–5), bias heals earlier so beginners aren't left bloodied.
    const bool earlySoloHeal = isSoloQuestScripted()
        && questBeat_ <= static_cast<int>(SoloQuestBeat::LANTERN_VAULT);
    for (size_t i = 0; i < players_.size(); ++i) {
        const Character* p = players_[i].get();
        if (!p || p->isDead) continue;
        int score = 0;
        if (p->isDowned) {
            score = 200 + p->deathSaveFailures * 30; // revive / stabilize urgency
        } else if (earlySoloHeal && p->maxHp > 0 && p->currentHp * 2 <= p->maxHp) {
            score = 95 + (p->maxHp - p->currentHp); // bloodied — heal sooner on solo quest
        } else if (p->maxHp > 0 && p->currentHp * 3 <= p->maxHp) {
            score = 80 + (p->maxHp - p->currentHp); // critically low
        } else if (p->currentHp <= 4 && p->currentHp < p->maxHp) {
            score = 60;
        } else if (earlySoloHeal && p->currentHp <= 6 && p->currentHp < p->maxHp) {
            score = 55; // soft early-quest cushion
        } else {
            continue; // healthy enough — do not burn a heal
        }
        if (score > bestScore) {
            bestScore = score;
            best = static_cast<int>(i);
        }
    }
    return best;
}

bool Game::allySpecialBeatsBasic(const Character& actor, const Character& enemy) const {
    if (actor.resources <= 0) return false;
    // Expected basic hit damage is modest and can miss; specials are stronger or auto-hit.
    const int hp = enemy.currentHp;
    switch (actor.characterClass) {
        case CharacterClass::WIZARD:
            // Magic Missile auto-hits (~10.5 avg) — clearly better than a staff swing, especially to finish or vs high AC.
            return hp <= 12 || enemy.armorClass >= 13 || actor.resources >= 2;
        case CharacterClass::FIGHTER:
            // Action Surge = two attacks; worth it vs chunky foes or to secure a finish.
            return hp > 8 || (hp > 4 && actor.resources >= 2);
        case CharacterClass::ROGUE:
            // Sneak Attack adds dice — almost always better than a plain swing when a use remains.
            return true;
        case CharacterClass::CLERIC:
            // Healing Word is a heal, not an attack special.
            return false;
        case CharacterClass::BARD:
            // Cutting Quip: CHA attack + rattle — good vs high AC or to finish.
            return hp <= 10 || enemy.armorClass >= 13 || actor.resources >= 2;
    }
    return false;
}

void Game::enemyTurn() {
    if (enemies_.empty() || players_.empty()) return;
    Character* enemy = getCurrentActor(); // M1: bounds-checked
    if (!enemy || enemy->isDowned || enemy->isDead || enemy->currentHp <= 0) return;

    int enemyIdx = 0;
    for (size_t i = 0; i < enemies_.size(); ++i) {
        if (enemies_[i].get() == enemy) enemyIdx = static_cast<int>(i);
    }

    const int targetIdx = pickEnemyAiTarget();
    if (targetIdx < 0) {
        checkPartyDefeat();
        return;
    }
    // v2.29 telegraph: a surprise boss spends its first turn winding up (1-turn warning).
    if (enemy->curveWarnTurns > 0) {
        enemy->curveWarnTurns--;
        lastEvent_ = "WARNING! " + telegraphLine(*enemy);
        addChatMessage("Combat", lastEvent_);
        if (enemyAiActive() && enemy->curveEL > 0 && enemy->ai.move == 0) planEnemyMove(*enemy);
        return;
    }
    if (enemyAiActive() && enemy->curveEL > 0) {
        // v2.30: roles, intents and boss phases (EnemyAi.h). Legacy / online foes fall through.
        runEnemyMove(*enemy, enemyIdx);
        return;
    }
    // Bell-Herald (story beat 24): flavour only — the toll hushes Wren; no mechanical effect.
    if (enemy->enemyId == "boss_herald" && static_cast<SoloQuestBeat>(questBeat_) == SoloQuestBeat::ACT4_HERALD
        && (heraldTolls_++ % 2) == 0) {
        addChatMessage("Combat", "The bell tolls. The glass goes quiet.");
    }

    Character& hero = *players_[static_cast<size_t>(targetIdx)];
    // Light DM color when the AI focuses a wounded or dying target.
    if (!hero.isDead && (hero.isDowned || hero.currentHp * 3 <= hero.maxHp)) {
        dmSay(enemy->name + " smells blood and focuses on " + hero.name + "!");
    }

    // Simple specials: spend a resource when it clearly beats a basic swing.
    const bool canSpecial = enemy->resources > 0;
    const bool finishThreat = hero.isDowned || hero.currentHp * 2 <= hero.maxHp;
    const bool fighterBurst = enemy->characterClass == CharacterClass::FIGHTER
        && hero.currentHp > 6 && !hero.isDowned;
    const bool rogueSneak = enemy->characterClass == CharacterClass::ROGUE
        && (finishThreat || hero.armorClass >= 14);
    const bool useSpecial = canSpecial && (finishThreat || fighterBurst || rogueSneak);

    auto doOneAttack = [&](bool sneak) {
        pushVisualEvent(VisualEventType::ENEMY_ATTACK, enemyIdx);
        RollResult result = CombatSystem::performAttackRoll(*enemy);
        if (enemy->nextAttackPenalty > 0) {
            result.total -= enemy->nextAttackPenalty;
            enemy->nextAttackPenalty = 0;
        }
        bool hit = (result.total >= hero.armorClass || result.isCriticalHit) && !result.isCriticalFail;
        const bool curved = encounterCurveActive();
        std::string clampNote;
        if (curved) {
            const curve::HitCheck hc = curve::attackHits(result.dieRoll, result.total - result.dieRoll, hero.armorClass);
            hit = hc.hit;
            if (hc.clamped) clampNote = " (needs " + std::to_string(hc.need) + "+ — hit clamp)";
        }
        if (hit) {
            int dmg = 0;
            if (enemy->curveEL > 0) {
                dmg = curve::rollFoeDamage(enemy->curveAvgDmgX10, enemy->curveEL, result.isCriticalHit, sneak);
            } else {
                int extra = sneak ? enemy->sneakAttackDice() : 0;
                dmg = CombatSystem::calculateDamage(*enemy, result.isCriticalHit, extra, 6);
            }
            if (curved && !hero.isDowned && !hero.isStable) {
                // v2.29 overkill guard: one hit takes at most 45% of max HP (telegraphed boss
                // heavies will be exempt in PR 2; there are none yet).
                const int capped = curve::overkillCap(dmg, hero.maxHp);
                if (capped < dmg) {
                    dmg = capped;
                    clampNote += " (guarded: max 45% of " + hero.name + "'s HP)";
                }
            }
            tryFightShield(&hero, dmg);
            bool wasDowned = hero.isDowned || hero.isStable;
            hero.takeDamage(dmg, wasDowned && result.isCriticalHit);
            pushVisualEvent(VisualEventType::PLAYER_DAMAGE, targetIdx);
            lastEvent_ = enemy->name + " attacks " + hero.name + "! d20=" + std::to_string(result.dieRoll)
                + " = " + std::to_string(result.total) + " vs AC " + std::to_string(hero.armorClass)
                + clampNote + ". " + (result.isCriticalHit ? "CRITICAL HIT! " : "Hit! ")
                + "Damage " + std::to_string(dmg) + ".";
            if (sneak) lastEvent_ += " (Sneak Attack)";
            if (!wasDowned && hero.isDowned) heroDroppedThisRoom_ = true;
            addChatMessage("Combat", lastEvent_);
            if (hero.isDead) {
                dmSay(hero.name + " is slain!");
                removeFromTurnOrder(&hero);
                checkPartyDefeat();
            } else if (!wasDowned && hero.isDowned) {
                dmSay(hero.name + " drops! Death saving throws will follow on their turns.");
            } else if (wasDowned && hero.isDowned) {
                dmSay(hero.name + " takes a death-save failure from the blow" +
                     (result.isCriticalHit ? " (critical — two failures)!" : "."));
            }
        } else {
            lastEvent_ = enemy->name + " attacks " + hero.name + "! d20=" + std::to_string(result.dieRoll)
                + " = " + std::to_string(result.total) + " vs AC " + std::to_string(hero.armorClass) + clampNote + ". Miss!";
            addChatMessage("Combat", lastEvent_);
        }
        return hero.isDead;
    };

    if (useSpecial && enemy->characterClass == CharacterClass::FIGHTER) {
        enemy->resources--;
        dmSay(enemy->name + " surges with a brutal flurry!");
        if (!doOneAttack(false) && hero.currentHp > 0 && !hero.isDead) {
            doOneAttack(false);
        }
        return;
    }
    if (useSpecial && enemy->characterClass == CharacterClass::ROGUE) {
        enemy->resources--;
        dmSay(enemy->name + " strikes from an angle — Sneak Attack!");
        doOneAttack(true);
        return;
    }

    doOneAttack(false);
}

void Game::addChatMessage(const std::string& sender, const std::string& message) {
    if (sender == "System" || sender == "Combat" || sender == "Lantern") {
        chatHistory_.push_back("[" + sender + "]: " + message);
    } else {
        chatHistory_.push_back(message);
    }
    if (chatHistory_.size() > 30) chatHistory_.pop_front();
}

std::string Game::getChatHistory() const {
    std::stringstream ss;
    for (const auto& msg : chatHistory_) ss << msg << "\n";
    return ss.str();
}

void Game::addJournalEntry(const std::string& entry) {
    journalEntries_.push_back(entry);
    if (journalEntries_.size() > 30) journalEntries_.pop_front();
}

std::string Game::getJournal() const {
    std::stringstream ss;
    ss << "--- ADVENTURE LOG ---\n";
    for (const auto& entry : journalEntries_) ss << "- " << entry << "\n";
    return ss.str();
}

std::string Game::getPartyStatus() const {
    std::stringstream ss;
    Character* current = getCurrentActor();

    const bool exploreBeat = isMerchantRoom_ || (enemies_.empty() && roomCount_ >= 1 && !dmOnlyTable_);
    ss << "Room " << roomCount_ << " | Turn: " << (current ? current->name : (exploreBeat ? "Safe" : "None")) << "\n";
    if (isBossRaid()) ss << "Mode: Boss Raid\n";
    else if (isChallengeDungeon()) ss << "Mode: Challenge Dungeon (Legendary chance " << challengeLegendaryChance_ << "%)\n";
    else if (isArena()) ss << "Mode: Ember Ring Arena — Wave " << arenaWave_ << "/"
                           << ARENA_MAX_WAVES << " · Score " << arenaScore_ << "\n";
    else if (isEndlessDeep()) ss << "Mode: Ashen Deep (Endless) — Depth " << endlessDepth_
                                 << " · Best " << endlessBestDepthThisRun_ << "\n";
    else if (isSoloCrawl()) ss << "Mode: Dungeon Crawl\n";
    else if (questAct4Complete_) ss << "Story complete: Acts 1–4 (endgame unlocked)\n";
    else if (isAct4Beat(questBeat_)) ss << "Quest: Ashwake Vigil — " << soloQuestBeatName(questBeat_) << "\n";
    else if (questAct3Complete_ && !isSoloQuestScripted()) ss << "Acts 1–3 done — Act 4 pending or onward\n";
    else if (isAct3Beat(questBeat_)) ss << "Quest: " << soloQuestActTitle(questBeat_) << " — " << soloQuestBeatName(questBeat_) << "\n";
    else if (questAct2Complete_ && !isSoloQuestScripted()) ss << "Acts 1–2 done — Act 3 pending or onward\n";
    else if (isAct2Beat(questBeat_)) ss << "Quest: " << soloQuestActTitle(questBeat_) << " — " << soloQuestBeatName(questBeat_) << "\n";
    else if (questComplete_ && !isSoloQuestScripted()) ss << "Quest complete: Ashen Lantern recovered\n";
    else if (isAct1Beat(questBeat_)) ss << "Quest: The Kindling — " << soloQuestBeatName(questBeat_) << "\n";
    ss << "Difficulty: " << difficultyName(difficulty_) << "\n";
    if (encounterCurveActive() && !players_.empty()) {
        // v2.29: "Curve: EL 7 · Pressure 2/4 · Rekindle 1" — Kotlin shows the pressure pips.
        int el = 0;
        for (const auto& e : enemies_) if (e && e->curveEL > 0) { el = e->curveEL; break; }
        if (el <= 0) el = currentEncounterLevel();
        ss << "Curve: EL " << el << " · Pressure " << pressure_ << "/" << curve::kPressureMaxSteps
           << " · Rekindle " << rekindleRooms_ << "\n";
    }
    if (gameOver_) ss << "Game Over\n";
    if (dmOnlyTable_ && !dmName_.empty()) ss << "Lanternkeeper: " << dmName_ << "\n";
    if (Character* companion = findNpcCompanion()) {
        ss << "Companion: " << companion->name
           << (companion->aiControlled ? " [Auto]" : " [Player]") << "\n";
    }
    for (const auto& p : players_) {
        ss << p->name << " | HP: " << p->currentHp << "/" << p->maxHp;
        if (p->isDead) ss << " [DEAD]";
        else if (p->isStable) ss << " [STABLE]";
        else if (p->isDowned) ss << " [DOWN]";
        ss << "\n";
    }
    if (!enemies_.empty()) {
        ss << "--- Foes ---\n";
        for (const auto& e : enemies_) ss << e->name << " | HP: " << e->currentHp << "/" << e->maxHp << "\n";
    }
    return ss.str();
}


std::string Game::getBattleRoster() const {
    std::stringstream ss;
    for (size_t i = 0; i < players_.size(); ++i) {
        if (i) ss << ";";
        ss << "P," << savecodec::listSafe(players_[i]->name) << ","
           << static_cast<int>(players_[i]->characterClass) << ","
           << players_[i]->currentHp << "," << players_[i]->maxHp;
    }
    ss << "|";
    for (size_t i = 0; i < enemies_.size(); ++i) {
        if (i) ss << ";";
        ss << "E," << savecodec::listSafe(enemies_[i]->name) << ","
           << static_cast<int>(enemies_[i]->characterClass) << ","
           << enemies_[i]->currentHp << "," << enemies_[i]->maxHp << ","
           << enemies_[i]->enemyId; // sprite lookup keys off this ID (MainActivity.spriteForEnemyId)
    }
    return ss.str();
}

std::string Game::getSpecialActionName() const {
    Character* actor = getCurrentActor();
    if (actor) return actor->getSpecialAbilityName();
    return players_.empty() ? "Special" : players_[0]->getSpecialAbilityName();
}

// serialize() / deserialize() / parseSave() live in GameSave.cpp (Code Auditor L10).


std::string Game::getDailyQuestCounters() const {
    std::ostringstream ss;
    ss << dqRoomsCleared_ << ',' << dqFightsWon_ << ',' << dqSearches_ << ','
       << dqDamageDealt_ << ',' << dqBossesDefeated_;
    return ss.str();
}

void Game::noteDailyQuestDamage(int dmg) {
    if (dmg > 0) dqDamageDealt_ += dmg;
}

bool Game::grantDailyQuestSpoils(int gold, bool giveLoot) {
    Character* purse = nullptr;
    for (auto& p : players_) {
        if (p && !p->isDead) { purse = p.get(); break; }
    }
    if (!purse) return false;
    if (gold > 0) {
        purse->gold += gold;
        dmSay(purse->name + " claims " + std::to_string(gold) + " gold from today's errands.");
        addChatMessage("Quest", purse->name + " claims " + std::to_string(gold) + " daily-quest gold.");
        addJournalEntry("Daily quests: +" + std::to_string(gold) + " gold.");
    }
    if (giveLoot) {
        auto potion = Item::make("Wayfarer's Tonic", ItemType::POTION, 10, ItemRarity::UNCOMMON, -1);
        purse->inventory.push_back(potion);
        dmSay(purse->name + " tucks away a Wayfarer's Tonic (Uncommon potion).");
        addChatMessage("Quest", purse->name + " receives Wayfarer's Tonic.");
        addJournalEntry("Daily quests: Wayfarer's Tonic.");
        lastEvent_ = purse->name + " claimed daily-quest spoils.";
    } else if (gold > 0) {
        lastEvent_ = purse->name + " claimed " + std::to_string(gold) + " daily-quest gold.";
    }
    return true;
}

// ---------------------------------------------------------------------------------------------
// v2.29 combat curve (PR 1 of the combat/progression revamp). Numbers live in CombatCurve.h.
// ---------------------------------------------------------------------------------------------

void Game::resetCombatCurveState() {
    pressure_ = 0;
    rekindleRooms_ = 0;
    lowHpStreak_ = 0;
    lastRoomOverBudget_ = false;
    heroDroppedThisRoom_ = false;
    pendingSurprise_ = false;
    combatRound_ = 0;
    roomFoeTotal_ = 0;
    fledFoes_ = 0;
}

int Game::encounterModeBonus() const {
    if (isBossRaid()) return 2;
    if (isChallengeDungeon()) return 3;
    if (isArena()) return arenaWave_ >= ARENA_MAX_WAVES ? 2 : 0;
    if (isEndlessDeep()) return std::max(0, endlessDepth_) / 5;
    return 0;
}

int Game::currentEncounterLevel() const {
    std::vector<curve::PartyMember> party;
    for (const auto& p : players_) if (p && !p->isDead) party.push_back(curve::memberOf(*p));
    if (party.empty()) for (const auto& p : players_) if (p) party.push_back(curve::memberOf(*p));
    return curve::encounterLevel(party, encounterModeBonus());
}

double Game::currentRoomType() const {
    if (isSoloQuestScripted()) {
        switch (static_cast<SoloQuestBeat>(questBeat_)) {
            case SoloQuestBeat::MILLHOLLOW:   // Act openers stay soft
            case SoloQuestBeat::THORNPATH:
            case SoloQuestBeat::ACT2_GREEN:
            case SoloQuestBeat::ACT3_RUMOR:
            case SoloQuestBeat::ACT4_WATCH:
                return curve::kRoomSoftOpen;
            default:
                return curve::kRoomNormal;
        }
    }
    if (activeSetPieceId_ != 0) return curve::kRoomSetPiece;
    if (isEndlessDeep() && endlessDepth_ > 0 && endlessDepth_ % ENDLESS_MILESTONE_EVERY == 0
        && endlessDepth_ % ENDLESS_APEX_EVERY != 0) {
        return curve::kRoomDeepMilestone;
    }
    if (isSoloCrawl() && roomCount_ <= 2) return curve::kRoomSoftOpen;
    return curve::kRoomNormal;
}

void Game::applyEncounterCurve() {
    bool anyPending = false;
    for (const auto& e : enemies_) if (e && e->curvePending) { anyPending = true; break; }
    if (!anyPending) { pendingSurprise_ = false; return; }
    if (!encounterCurveActive()) {
        // Online Keeper tables / joiners: old rules, untouched.
        for (auto& e : enemies_) if (e) e->curvePending = false;
        pendingSurprise_ = false;
        return;
    }
    heroDroppedThisRoom_ = false;
    const curve::DifficultyInfo& di = curve::difficultyInfo(difficulty_);

    // No more than 4 foes on the field (5 on Nightmare); the budget fit turns the rest into HP.
    auto isPending = [](const std::unique_ptr<Character>& e) { return e && e->curvePending; };
    int pendingCount = static_cast<int>(std::count_if(enemies_.begin(), enemies_.end(), isPending));
    while (pendingCount > di.maxFoes) {
        for (auto it = enemies_.end(); it != enemies_.begin();) {
            --it;
            if (isPending(*it) && bestiary::role((*it)->enemyId) != static_cast<int>(curve::Role::BOSS)) {
                enemies_.erase(it);
                break;
            }
        }
        const int now = static_cast<int>(std::count_if(enemies_.begin(), enemies_.end(), isPending));
        if (now == pendingCount) break; // only bosses left
        pendingCount = now;
    }

    int party = 0;
    for (const auto& p : players_) if (p && !p->isDead) ++party;
    party = std::max(1, party);
    const int el = currentEncounterLevel();
    // Mode bonuses (Raid +2, Challenge +3, Arena finale +2, Deep +depth/5) raise HP and damage to
    // the bonus EL; hit odds (AC / to-hit) stay at the party's own EL so the 15–90% clamp and the
    // ~60/50% targets hold in every mode.
    int accuracyEl = el;
    {
        std::vector<curve::PartyMember> pm;
        for (const auto& p : players_) if (p && !p->isDead) pm.push_back(curve::memberOf(*p));
        if (!pm.empty()) accuracyEl = curve::encounterLevel(pm, 0);
    }
    const double budget = curve::roomBudget(party, difficulty_, currentRoomType());

    // Comeback / pressure.
    double mod = 1.0;
    bool rekindled = false;
    if (rekindleRooms_ > 0) {
        mod = curve::kRekindleBudget;
        rekindleRooms_--;
        rekindled = true;
    } else if (pressure_ > 0) {
        mod = curve::pressureMultiplier(pressure_);
    }
    // Variance: +-15%, never two over-budget rooms back to back.
    double jitter = 1.0 + getRandomInt(-15, 15) / 100.0;
    if (lastRoomOverBudget_ && jitter > 1.0) jitter = 2.0 - jitter;

    const bool surprise = pendingSurprise_;
    pendingSurprise_ = false;
    bool hasBoss = false;
    double addsCost = 0, cost = 0;
    for (const auto& e : enemies_) {
        if (!isPending(e)) continue;
        const curve::Role r = curve::roleFromInt(bestiary::role(e->enemyId));
        if (r == curve::Role::BOSS) { hasBoss = true; continue; }
        cost += curve::roleInfo(r).cost;
    }
    addsCost = cost;

    double scaleBoss = 1.0, scaleOthers = 1.0;
    if (surprise) {
        // Early surprise boss: Elite numbers, capped at 1.5x this room's budget (no pressure
        // on top of the cap; Rekindle still lowers it). The boss spends its first turn winding up.
        const double cap = curve::kSurpriseBudgetCap * budget * std::min(1.0, mod);
        scaleBoss = cap / curve::roleInfo(curve::Role::ELITE).cost;
        jitter = std::min(jitter, 1.0);
        scaleOthers = addsCost > 0 ? curve::fitScale(budget * std::min(1.0, mod) * jitter, addsCost) : 1.0;
    } else if (hasBoss) {
        double bossMod = mod * jitter;
        if (isSoloQuestScripted()) bossMod = std::max(bossMod, curve::kRekindleStoryBossFloor);
        scaleBoss = bossMod;
        if (addsCost > 0) {
            scaleOthers = std::clamp(curve::kBossAddsShare * budget * mod * jitter / addsCost, 0.4, 1.0);
        }
    } else {
        scaleOthers = curve::fitScale(budget * mod * jitter, cost);
    }
    lastRoomOverBudget_ = jitter > 1.0;

    for (auto& e : enemies_) {
        if (!isPending(e)) continue;
        curve::Role r = curve::roleFromInt(bestiary::role(e->enemyId));
        double s = scaleOthers;
        if (r == curve::Role::BOSS) {
            s = scaleBoss;
            if (surprise) {
                r = curve::Role::ELITE;
                e->curveSurprise = true;
                e->curveWarnTurns = 1;
            }
        }
        if (universalStatsActive()) s *= curve::statsToughness(el, r == curve::Role::BOSS, addsCost > 0); // v2.31
        const curve::FoeStats st = curve::foeStats(r, el, difficulty_, party, s, accuracyEl);
        e->maxHp = st.maxHp;
        e->currentHp = st.maxHp;
        e->armorClass = st.ac;
        e->curveEL = el;
        e->curveRole = static_cast<int>(r);
        e->curveToHit = st.toHit;
        e->curveAvgDmgX10 = st.avgDmgX10;
        e->curvePending = false;
    }

    std::string line = "Encounter Level " + std::to_string(el);
    if (pressure_ > 0 && !rekindled) line += " · Pressure " + std::to_string(pressure_) + "/" + std::to_string(curve::kPressureMaxSteps);
    addChatMessage("Combat", line + ".");
    if (rekindled) dmSay("The dark draws back.");
}

void Game::noteRoomOutcomeForCurve() {
    if (!encounterCurveActive()) return;
    bool low = false, flawless = true;
    for (const auto& p : players_) {
        if (!p || p->isDead) { flawless = false; continue; }
        if (p->maxHp > 0 && p->currentHp * 4 <= p->maxHp) low = true;
        if (p->maxHp > 0 && p->currentHp * 10 < p->maxHp * 7) flawless = false;
        if (p->isDowned) flawless = false;
    }
    if (heroDroppedThisRoom_) {
        pressure_ = 0;
        flawless = false;
    }
    if (flawless) pressure_ = std::min(curve::kPressureMaxSteps, pressure_ + 1);
    lowHpStreak_ = low ? lowHpStreak_ + 1 : 0;
    if (lowHpStreak_ >= 2) {
        rekindleRooms_ = std::max(rekindleRooms_, 1);
        lowHpStreak_ = 0;
        pressure_ = 0;
    }
    heroDroppedThisRoom_ = false;
}

std::string Game::telegraphLine(const Character& foe) const {
    if (foe.enemyId == "boss_prior") return "Bones knit into a mitre — the Ossuary Prior rises. It strikes next turn.";
    if (foe.enemyId == "boss_wick_king") return "Molten wax hardens into a crown — the Wick King rises. He strikes next turn.";
    return foe.name + " gathers itself. It strikes next turn.";
}

// ======================================================================================
// v2.30 enemy AI (PR 2): intents, roles, statuses, boss phases. Solo only (enemyAiActive).
// ======================================================================================

int Game::heroIndexOf(const Character* c) const {
    for (size_t i = 0; i < players_.size(); ++i) if (players_[i].get() == c) return static_cast<int>(i);
    return -1;
}

int Game::enemyIndexOf(const Character* c) const {
    for (size_t i = 0; i < enemies_.size(); ++i) if (enemies_[i].get() == c) return static_cast<int>(i);
    return -1;
}

int Game::livingFoeCount() const {
    int n = 0;
    for (const auto& e : enemies_) if (e && !e->isDead && !e->isDowned && e->currentHp > 0) ++n;
    return n;
}

bool Game::heroHasIntBonus() const {
    for (const auto& p : players_) {
        if (p && !p->isDead && p->effectiveAttr(3) >= ai::kIntentNumbersInt) return true;
    }
    return false;
}

void Game::announceBanner(const std::string& text) {
    ++bannerSeq_;
    bannerText_ = text;
    addChatMessage("Combat", text);
}

void Game::returnStolenGold(Character& foe, Character* looter) {
    if (foe.ai.stolenGold <= 0) return;
    Character* to = looter;
    if (!to || to->isDead || to->isFoe()) {
        to = nullptr;
        for (auto& p : players_) if (p && !p->isDead) { to = p.get(); break; }
    }
    if (to) {
        to->gold += foe.ai.stolenGold;
        addChatMessage("Combat", to->name + " takes back " + std::to_string(foe.ai.stolenGold) + " stolen gold.");
    }
    foe.ai.stolenGold = 0;
}

namespace {
bool moveNeedsHero(ai::Move m) {
    using ai::Intent;
    const ai::MoveInfo& mi = ai::moveInfo(m);
    if (mi.aoe) return false;
    switch (mi.intent) {
        case Intent::NONE: case Intent::GUARD: case Intent::BUFF: case Intent::HEAL:
        case Intent::SUMMON: case Intent::FLEE:
            return false;
        default:
            return true;
    }
}
bool isStanding(const Character* c) {
    return c && !c->isDead && !c->isDowned && c->currentHp > 0;
}
} // namespace

int Game::pickAiHeroTarget(const Character& foe, int behaviour) {
    const ai::Tier tier = ai::tierFor(difficulty_);
    std::vector<ai::HeroView> hv;
    hv.reserve(players_.size());
    for (size_t i = 0; i < players_.size(); ++i) {
        const Character* p = players_[i].get();
        ai::HeroView v;
        v.alive = p && !p->isDead;
        if (p) {
            v.downed = p->isDowned || p->currentHp <= 0;
            v.hp = p->currentHp; v.maxHp = p->maxHp; v.ac = p->armorClass;
            v.healer = p->characterClass == CharacterClass::CLERIC || p->characterClass == CharacterClass::BARD;
            v.healedRecently = p->status.healedRecently > 0;
            v.marked = p->status.marked > 0;
            v.hitMe = foe.ai.lastHitBy == static_cast<int>(i);
            for (const auto& e : enemies_) {
                if (!e || e.get() == &foe || !isStanding(e.get())) continue;
                if (e->ai.target == static_cast<int>(i) && moveNeedsHero(ai::moveFromInt(e->ai.move))) ++v.focus;
            }
        }
        hv.push_back(v);
    }
    // Hollow Crown (phase 2+): never the same hero twice in a row while another stands.
    if (foe.enemyId == "boss_hollow_crown" && foe.ai.phase >= 2 && foe.ai.lastTarget >= 0
        && static_cast<size_t>(foe.ai.lastTarget) < hv.size()) {
        int standing = 0;
        for (const auto& v : hv) if (v.alive && !v.downed) ++standing;
        if (standing > 1) hv[static_cast<size_t>(foe.ai.lastTarget)].alive = false;
    }
    std::vector<double> noise;
    for (size_t i = 0; i < hv.size(); ++i) noise.push_back(getRandomInt(0, 9999) / 10000.0);
    const double roll = getRandomInt(0, 9999) / 10000.0;
    return ai::pickTarget(hv, tier, behaviour, noise, roll);
}

void Game::planEnemyMove(Character& foe) {
    using ai::Move;
    auto& A = foe.ai;
    const ai::Tier tier = ai::tierFor(difficulty_);
    const int beh = bestiary::behaviour(foe.enemyId);
    const bool sloppy = tier == ai::Tier::SLOPPY;
    auto rnd01 = [this]() { return getRandomInt(0, 9999) / 10000.0; };
    // Sloppy foes use their trick only when it's obvious (about half the time it's ready).
    auto ready = [&]() { return A.cooldown == 0 && (!sloppy || rnd01() < 0.5); };
    const double hpFrac = foe.maxHp > 0 ? static_cast<double>(foe.currentHp) / foe.maxHp : 1.0;
    A.target = -1;

    if (beh == bestiary::BEH_BOSS) {
        planBossMove(foe, static_cast<int>(tier));
        A.lastHitBy = -1;
        return;
    }
    Move m = Move::STRIKE;
    // Wickthieves and hounds slip away under 25% (Medium and up). Never undead or bosses.
    // v2.31 Intimidate: the party's best CHA makes them break sooner (+2% per mod).
    const double fleeAt = ai::kFleeBelow + (universalStatsActive() ? stats::intimidate(partyChaMod()) : 0.0);
    if (bestiary::canFlee(foe.enemyId) && tier >= ai::Tier::TACTICAL && hpFrac < fleeAt) {
        A.move = static_cast<int>(Move::SLIP_AWAY);
        A.lastHitBy = -1;
        dmSay(foe.name + " glances at the way out — it will slip away next turn!");
        return;
    }
    // Nightmare only: a Ruthless brute may go for a downed hero — always telegraphed first.
    if (tier == ai::Tier::RUTHLESS && bestiary::role(foe.enemyId) == static_cast<int>(curve::Role::BRUTE)) {
        for (size_t i = 0; i < players_.size(); ++i) {
            const Character* p = players_[i].get();
            if (p && !p->isDead && (p->isDowned || p->currentHp <= 0) && rnd01() < 0.5) {
                A.move = static_cast<int>(Move::FINISH_DOWNED);
                A.target = static_cast<int>(i);
                A.lastHitBy = -1;
                announceBanner("WARNING! " + foe.name + " eyes the fallen " + p->name + " — it will strike them next turn!");
                return;
            }
        }
    }
    switch (beh) {
        case bestiary::BEH_SNATCH: m = Move::SNATCH; break;
        case bestiary::BEH_BACKSTAB:
            if (ready()) { m = Move::BACKSTAB; A.cooldown = ai::cooldownFor(tier, 2); }
            break;
        case bestiary::BEH_SHIELDWALL: {
            int best = -1; double bestFrac = sloppy ? 0.25 : 0.5;
            for (size_t i = 0; i < enemies_.size(); ++i) {
                const Character* e = enemies_[i].get();
                if (!isStanding(e) || e == &foe || e->status.guarded > 0 || e->maxHp <= 0) continue;
                const double f = static_cast<double>(e->currentHp) / e->maxHp;
                if (f < bestFrac) { bestFrac = f; best = static_cast<int>(i); }
            }
            for (const auto& e : enemies_) // one shield-wall per ally at a time
                if (e.get() != &foe && ai::moveFromInt(e->ai.move) == Move::SHIELD_WALL && e->ai.target == 100 + best) best = -1;
            if (best >= 0 && A.cooldown == 0) {
                m = Move::SHIELD_WALL; A.target = 100 + best; A.cooldown = ai::cooldownFor(tier, 3);
            }
            break;
        }
        case bestiary::BEH_PACK: m = Move::PACK_BITE; break;
        case bestiary::BEH_SWARM: m = Move::SWARM_BITE; break;
        case bestiary::BEH_KNIT:
        case bestiary::BEH_RALLY: {
            if (beh == bestiary::BEH_KNIT && A.cooldown == 0) {
                int best = -1; double bestFrac = ai::healBelow(tier);
                for (size_t i = 0; i < enemies_.size(); ++i) {
                    const Character* e = enemies_[i].get();
                    if (!isStanding(e) || !bestiary::isUndead(e->enemyId) || e->maxHp <= 0) continue;
                    const double f = static_cast<double>(e->currentHp) / e->maxHp;
                    if (f < bestFrac) { bestFrac = f; best = static_cast<int>(i); }
                }
                if (best >= 0) {
                    m = Move::KNIT; A.target = 100 + best; A.cooldown = ai::cooldownFor(tier, 2);
                    break;
                }
            }
            bool someoneToRally = false, brute = false;
            for (const auto& e : enemies_) {
                if (!isStanding(e.get()) || e.get() == &foe) continue;
                if (e->status.inspired <= 0) someoneToRally = true;
                if (e->curveRole == static_cast<int>(curve::Role::BRUTE)) brute = true;
            }
            // Coordinated+: always buff before the brutes swing; otherwise when it's ready.
            if (someoneToRally && A.cooldown == 0 && (brute && tier >= ai::Tier::COORDINATED ? true : ready())) {
                m = Move::RALLY; A.cooldown = ai::cooldownFor(tier, 3);
            }
            break;
        }
        case bestiary::BEH_SLAM:
            if (ready()) { m = Move::SLAM; A.stagger = 0; A.cooldown = ai::cooldownFor(tier, 3); }
            break;
        case bestiary::BEH_SHACKLE:
            if (ready()) { m = Move::SHACKLE; A.cooldown = ai::cooldownFor(tier, 3); }
            break;
        case bestiary::BEH_FINISHER: m = Move::FINISH; break;
        case bestiary::BEH_MARKER: {
            m = Move::CINDER_BOLT; // casters actually cast
            if (tier >= ai::Tier::COORDINATED && A.cooldown == 0) {
                // Coordinated: interrupt a healer (Rattle -> no class feature next turn).
                for (size_t i = 0; i < players_.size(); ++i) {
                    const Character* p = players_[i].get();
                    if (isStanding(p) && p->status.shackled <= 0
                        && (p->characterClass == CharacterClass::CLERIC || p->status.healedRecently > 0)) {
                        m = Move::RATTLE_HEALER; A.target = static_cast<int>(i); A.cooldown = ai::cooldownFor(tier, 2);
                        break;
                    }
                }
                if (m == Move::RATTLE_HEALER) break;
            }
            bool anyMarked = false;
            for (const auto& p : players_) if (p && p->status.marked > 0) anyMarked = true;
            if (!anyMarked && ready()) { m = Move::MARK; A.cooldown = ai::cooldownFor(tier, 3); }
            break;
        }
        default: break;
    }
    A.move = static_cast<int>(m);
    if (moveNeedsHero(m) && A.target < 0) A.target = pickAiHeroTarget(foe, beh);
    A.lastHitBy = -1;
}

void Game::planBossMove(Character& foe, int tierInt) {
    using ai::Move;
    auto& A = foe.ai;
    const ai::Tier tier = static_cast<ai::Tier>(tierInt);
    if (A.phase <= 0) A.phase = 1;
    const int ph = A.phase;
    const std::string& id = foe.enemyId;
    const bool room = livingFoeCount() < curve::difficultyInfo(difficulty_).maxFoes;
    Move m = Move::STRIKE;
    int target = -1;
    auto standingHero = [this](int i) {
        return i >= 0 && static_cast<size_t>(i) < players_.size() && isStanding(players_[static_cast<size_t>(i)].get());
    };
    if (id == "boss_sexton") {
        if (ph >= 2 && !(A.flags & ai::F_CALL2) && room) m = Move::SEXTON_CALL;
        else if (ph >= 3 && A.cooldown == 0) {
            m = Move::SNUFF; A.cooldown = ai::cooldownFor(tier, 3);
            if (standingHero(0)) target = 0; // the lantern-bearer
        }
    } else if (id == "boss_collector") {
        if (ph >= 2 && !(A.flags & ai::F_CALL2)) {
            m = Move::TALLY;
            int rich = -1;
            for (size_t i = 0; i < players_.size(); ++i)
                if (isStanding(players_[i].get()) && (rich < 0 || players_[i]->gold > players_[static_cast<size_t>(rich)]->gold)) rich = static_cast<int>(i);
            target = rich;
        } else if (ph >= 3 && !(A.flags & ai::F_CALL3)) {
            m = Move::VENN_CALL;
        }
    } else if (id == "boss_warden") {
        if (ph >= 3) m = (A.alt % 2 == 0) ? Move::WARDEN_GUARD : Move::TWIN_STRIKE;
        else if (ph >= 2 && A.cooldown == 0) { m = Move::EMBER_WOUND; A.cooldown = ai::cooldownFor(tier, 4); }
    } else if (id == "boss_herald") {
        if (ph >= 3 && !(A.flags & ai::F_CALL3)) m = Move::BELL_CRACK;
        else if (ph >= 2 && A.cooldown == 0) { m = Move::TOLL; A.cooldown = 2; } // every 3rd turn
    } else if (id == "boss_wick_king") {
        if (ph >= 2 && !(A.flags & ai::F_CALL2) && room) m = Move::KING_CALL;
        else if (ph >= 3 && A.cooldown == 0) { m = Move::CROWN_FLARE; A.cooldown = ai::cooldownFor(tier, 3); }
    } else if (id == "boss_prior") {
        if (ph >= 2 && !(A.flags & ai::F_CALL2)) m = Move::RELIQUARY;
        else if (ph >= 3 && !(A.flags & ai::F_CALL3) && room) m = Move::RAISE_DEAD;
    } else if (id == "boss_drake") {
        if (ph >= 3) m = (A.alt % 2 == 0) ? Move::TAKE_FLIGHT : Move::DIVE;
        else if (ph >= 2 && A.cooldown == 0) { m = Move::CINDER_BREATH; A.cooldown = ai::cooldownFor(tier, 3); }
    } else if (id == "boss_hollow_crown") {
        if (ph >= 3 && A.cooldown == 0) { m = Move::TYRANTS_DUE; A.cooldown = ai::cooldownFor(tier, 3); }
    } else if (id == "boss_ember_hydra") {
        if (ph >= 2) m = Move::HEADS;
        else m = (A.alt % 2 == 0) ? Move::HYDRA_FIRE : Move::STRIKE; // the Hydra casts
    } else if (id == "boss_nightfang") {
        if (ph >= 3 && !(A.flags & ai::F_CALL3) && room) m = Move::BROOD;
        else if (ph >= 2 && A.cooldown == 0) { m = Move::WEBS; A.cooldown = ai::cooldownFor(tier, 3); }
    }
    A.move = static_cast<int>(m);
    A.target = target;
    if (moveNeedsHero(m) && A.target < 0) A.target = pickAiHeroTarget(foe, bestiary::BEH_BOSS);
}

bool Game::foeStrike(Character& foe, int foeIdx, int heroIdx, double dmgMult, int hitBonus,
                     bool exempt, bool autoHit, const std::string& moveLabel, int* dealt) {
    if (dealt) *dealt = 0;
    if (heroIdx < 0 || static_cast<size_t>(heroIdx) >= players_.size() || !players_[static_cast<size_t>(heroIdx)]) return false;
    Character& hero = *players_[static_cast<size_t>(heroIdx)];
    if (hero.isDead) return false;
    pushVisualEvent(VisualEventType::ENEMY_ATTACK, foeIdx);
    RollResult result = CombatSystem::performAttackRoll(foe);
    std::string notes;
    if (foe.nextAttackPenalty > 0) {
        result.total -= foe.nextAttackPenalty;
        notes += " -" + std::to_string(foe.nextAttackPenalty) + " (rattled)";
        foe.nextAttackPenalty = 0;
    }
    int bonus = hitBonus;
    if (foe.status.inspired > 0) bonus += ai::kInspiredToHit;
    if (hero.status.marked > 0) bonus += ai::kMarkToHit;
    if (bonus != 0) notes += std::string(" ") + (bonus > 0 ? "+" : "") + std::to_string(bonus) + " (allies / marks)";
    result.total += bonus;
    bool hit = true, crit = false;
    if (!autoHit) {
        const curve::HitCheck hc = curve::attackHits(result.dieRoll, result.total - result.dieRoll, hero.armorClass);
        hit = hc.hit;
        crit = result.isCriticalHit && hit && !exempt; // a telegraphed heavy is already the big hit
        if (hc.clamped) notes += " (needs " + std::to_string(hc.need) + "+ — hit clamp)";
    }
    const std::string head = autoHit
        ? foe.name + " — " + moveLabel + " on " + hero.name + " (no roll)"
        : foe.name + " — " + moveLabel + " on " + hero.name + "! d20=" + std::to_string(result.dieRoll)
          + " = " + std::to_string(result.total) + " vs AC " + std::to_string(hero.armorClass) + notes;
    if (!hit) {
        lastEvent_ = head + ". Miss!";
        addChatMessage("Combat", lastEvent_);
        return false;
    }
    if (foe.curveRole == static_cast<int>(curve::Role::BOSS)) dmgMult *= ai::kBossDamageScale;
    const int avgX10 = std::max(10, static_cast<int>(std::lround(foe.curveAvgDmgX10 * dmgMult)));
    int dmg = curve::rollFoeDamage(avgX10, foe.curveEL, crit, false);
    std::string guard;
    // v2.31 Evade: DEX cuts telegraphed heavies and area attacks by 3% per mod (max 24%).
    {
        const ai::MoveInfo& cur = ai::moveInfo(ai::moveFromInt(foe.ai.move));
        if (hero.statRules && (exempt || cur.aoe || cur.intent == ai::Intent::HEAVY)) {
            const double cut = stats::evade(hero.effMod(1));
            if (cut > 0 && dmg > 1) {
                const int after = std::max(1, static_cast<int>(std::lround(dmg * (1.0 - cut))));
                if (after < dmg) guard += " (evaded " + std::to_string(dmg - after) + ")";
                dmg = after;
            }
        }
    }
    if (!hero.isDowned && !hero.isStable) {
        const int capped = curve::overkillCap(dmg, hero.maxHp, exempt);
        if (capped < dmg) {
            dmg = capped;
            guard = " (guarded: max 45% of " + hero.name + "'s HP)";
        } else if (exempt && curve::overkillCap(dmg, hero.maxHp) < dmg) {
            guard = " (telegraphed heavy — not guarded)";
        }
    }
    tryFightShield(&hero, dmg);
    const bool wasDowned = hero.isDowned || hero.isStable;
    hero.takeDamage(dmg, wasDowned && crit);
    if (dealt) *dealt = dmg;
    pushVisualEvent(VisualEventType::PLAYER_DAMAGE, heroIdx);
    lastEvent_ = head + guard + ". " + (crit ? "CRITICAL HIT! " : "Hit! ") + "Damage " + std::to_string(dmg) + ".";
    addChatMessage("Combat", lastEvent_);
    if (!wasDowned && hero.isDowned) heroDroppedThisRoom_ = true;
    if (hero.isDowned) hero.status = Character::Statuses{};
    if (hero.isDead) {
        dmSay(hero.name + " is slain!");
        removeFromTurnOrder(&hero);
        checkPartyDefeat();
    } else if (!wasDowned && hero.isDowned) {
        dmSay(hero.name + " drops! Death saving throws will follow on their turns.");
    } else if (wasDowned && hero.isDowned) {
        dmSay(hero.name + " takes a death-save failure from the blow" + (crit ? std::string(" (critical — two failures)!") : std::string(".")));
    }
    return true;
}

bool Game::summonFoe(const std::string& enemyId, CharacterClass cl, const Character& summoner) {
    if (livingFoeCount() >= curve::difficultyInfo(difficulty_).maxFoes) return false;
    auto f = newEnemy(enemyId, cl, "call");
    int party = 0;
    std::vector<curve::PartyMember> pm;
    for (const auto& p : players_) if (p && !p->isDead) { ++party; pm.push_back(curve::memberOf(*p)); }
    party = std::max(1, party);
    const int accuracyEl = pm.empty() ? summoner.curveEL : curve::encounterLevel(pm, 0);
    const curve::Role r = curve::roleFromInt(bestiary::role(enemyId));
    const curve::FoeStats st = curve::foeStats(r, std::max(1, summoner.curveEL), difficulty_, party,
                                               ai::kSummonScale, accuracyEl);
    f->maxHp = st.maxHp; f->currentHp = st.maxHp; f->armorClass = st.ac;
    f->curveEL = std::max(1, summoner.curveEL);
    f->curveRole = static_cast<int>(r);
    f->curveToHit = st.toHit;
    f->curveAvgDmgX10 = std::max(10, static_cast<int>(std::lround(st.avgDmgX10 * ai::kSummonDmgScale)));
    f->curvePending = false;
    f->resources = 0;
    f->initiative = getRandomInt(1, 20) + f->initiativeModifier();
    Character* raw = f.get();
    enemies_.push_back(std::move(f));
    ++roomFoeTotal_;
    rebuildTurnOrder();
    planEnemyMove(*raw);
    return true;
}

void Game::foeSlipsAway(Character& foe) {
    std::string line = foe.name + " slips away into the dark";
    if (foe.ai.stolenGold > 0) line += " with " + std::to_string(foe.ai.stolenGold) + " of your gold";
    line += "! (no loot; half XP for it)";
    lastEvent_ = line;
    addChatMessage("Combat", line);
    dmSay(foe.name + " is gone.");
    ++fledFoes_;
    Character* looter = nullptr;
    for (auto& p : players_) if (p && !p->isDead) { looter = p.get(); break; }
    removeFromTurnOrder(&foe);
    for (auto it = enemies_.begin(); it != enemies_.end(); ++it) {
        if (it->get() == &foe) { enemies_.erase(it); break; }
    }
    // Drop any 0-HP foes left behind, then clear the room if nobody remains.
    bool anyLeft = false;
    for (const auto& e : enemies_) if (isStanding(e.get())) anyLeft = true;
    if (!anyLeft) {
        for (auto it = enemies_.begin(); it != enemies_.end();) {
            returnStolenGold(**it, looter);
            grantKillLoot(looter, (*it)->name, (*it)->enemyId);
            removeFromTurnOrder(it->get());
            it = enemies_.erase(it);
        }
        enterClearedRoom(looter);
    }
}

void Game::updateBossPhase(Character& foe, bool announce) {
    if (bestiary::behaviour(foe.enemyId) != bestiary::BEH_BOSS || foe.maxHp <= 0) return;
    if (foe.currentHp <= 0 || foe.isDead) return;
    const int want = (foe.currentHp * 3 <= foe.maxHp) ? 3 : ((foe.currentHp * 3 <= foe.maxHp * 2) ? 2 : 1);
    if (foe.ai.phase <= 0) {
        // First look (spawn or a loaded v2.29 boss): set silently from HP.
        foe.ai.phase = announce ? 1 : want;
        if (!announce) return;
    }
    if (want <= foe.ai.phase) return;
    foe.ai.phase = want;
    struct Bark { const char* id; const char* p2; const char* p3; };
    static const Bark kBarks[] = {
        {"boss_sexton", "The Sexton rings for help — the ash stirs.", "The Sexton turns on the lantern-bearer: \"Snuff it out.\""},
        {"boss_collector", "Venn opens his ledger: \"Let's settle the tally.\"", "Venn whistles for his enforcers."},
        {"boss_warden", "Embers bleed from the First Lamplighter's wounds.", "The First Lamplighter braces — guard, then two blows."},
        {"boss_herald", "The toll is real now. Every third bell, hold steady.", "The bell cracks — the Herald grows reckless."},
        {"boss_wick_king", "The Wick King calls his thieves.", "The Wick King's crown flares white-hot."},
        {"boss_prior", "The Prior lifts the reliquary.", "The Prior raises the fallen."},
        {"boss_drake", "Cinders gather in the Drake's throat.", "The Drake takes to the air."},
        {"boss_hollow_crown", "Crownless — the Hollow Crown turns from foe to foe.", "The Hollow Crown demands its due."},
        {"boss_ember_hydra", "The Hydra splits — three heads strike.", "Severed heads regrow unless the blow lands true."},
        {"boss_nightfang", "The Matriarch spins her webs.", "The brood answers its mother."},
    };
    std::string bark = "It changes.";
    for (const auto& b : kBarks) if (foe.enemyId == b.id) bark = want >= 3 ? b.p3 : b.p2;
    announceBanner("PHASE " + std::to_string(want) + " — " + foe.name);
    dmSay(bark);
    planEnemyMove(foe); // the chip shows the phase move a turn ahead
}

void Game::noteFoeDamaged(Character& foe, int dmg, const Character* by, bool crit) {
    if (!enemyAiActive() || foe.curveEL <= 0 || dmg <= 0) return;
    const int hi = heroIndexOf(by);
    if (hi >= 0) foe.ai.lastHitBy = hi;
    if (crit) foe.ai.critThisRound = true;
    if (ai::moveFromInt(foe.ai.move) == ai::Move::SLAM && foe.currentHp > 0) {
        const int need = static_cast<int>(std::ceil(ai::kStaggerShare * foe.maxHp));
        const bool before = foe.ai.stagger >= need;
        foe.ai.stagger += dmg;
        if (!before && foe.ai.stagger >= need) dmSay(foe.name + " staggers — the Millstone Slam is broken!");
    }
    if (foe.currentHp > 0) updateBossPhase(foe, true);
}

void Game::endTurnStatuses(Character* c) {
    if (!c || !enemyAiActive()) return;
    auto& st = c->status;
    if (st.burn > 0) {
        if (!c->isDead && !c->isDowned && c->currentHp > 0 && st.burnDmg > 0) {
            const bool wasUp = c->currentHp > 0;
            c->takeDamage(st.burnDmg);
            addChatMessage("Combat", c->name + " burns for " + std::to_string(st.burnDmg) + ".");
            if (!c->isFoe() && wasUp && c->isDowned) {
                heroDroppedThisRoom_ = true;
                dmSay(c->name + " drops! Death saving throws will follow on their turns.");
            }
        }
        if (--st.burn <= 0) { st.burn = 0; st.burnDmg = 0; }
    }
    if (st.marked > 0) --st.marked;
    if (st.blind > 0) --st.blind;
    if (st.shackled > 0) --st.shackled;
    if (st.inspired > 0) --st.inspired;
    if (st.healedRecently > 0) --st.healedRecently;
    if (st.guarded > 0 && --st.guarded == 0) c->armorClass -= ai::kGuardAc;
    if (st.slowed > 0 && --st.slowed == 0) {
        c->initiative = st.slowedFromInit;
        st.slowedFromInit = 0;
        rebuildTurnOrder();
    }
    if (c->isFoe() && c->ai.critThisRound && c->enemyId != "boss_ember_hydra") c->ai.critThisRound = false;
}

void Game::runEnemyMove(Character& foe, int foeIdx) {
    using ai::Move;
    auto& A = foe.ai;
    if (A.phase == 0) updateBossPhase(foe, false); // a loaded v2.29 boss: phase from its HP, silently
    if (A.cooldown > 0) A.cooldown--;
    if (A.move == 0) planEnemyMove(foe);
    Move m = ai::moveFromInt(A.move);
    const ai::MoveInfo& mi = ai::moveInfo(m);
    const int beh = bestiary::behaviour(foe.enemyId);
    const std::string foeUid = foe.uid;
    auto stillHere = [this, &foeUid]() {
        for (const auto& e : enemies_) if (e && e->uid == foeUid) return isStanding(e.get());
        return false;
    };
    auto heroAt = [this](int i) -> Character* {
        return (i >= 0 && static_cast<size_t>(i) < players_.size()) ? players_[static_cast<size_t>(i)].get() : nullptr;
    };
    auto foeAt = [this](int i) -> Character* {
        return (i >= 0 && static_cast<size_t>(i) < enemies_.size()) ? enemies_[static_cast<size_t>(i)].get() : nullptr;
    };
    auto standingHeroes = [this]() {
        std::vector<int> v;
        for (size_t i = 0; i < players_.size(); ++i) if (isStanding(players_[i].get())) v.push_back(static_cast<int>(i));
        return v;
    };
    const int el = std::max(1, foe.curveEL);

    // Resolve the target (a hero who fell or died since the plan: pick again, same move).
    int t = A.target;
    if (moveNeedsHero(m)) {
        if (m == Move::FINISH_DOWNED) {
            Character* h = heroAt(t);
            if (!h || h->isDead || !(h->isDowned || h->currentHp <= 0)) { m = Move::STRIKE; t = -1; }
        } else if (!isStanding(heroAt(t))) {
            t = pickAiHeroTarget(foe, beh);
        }
        if (t < 0 || !heroAt(t) || heroAt(t)->isDead) { checkPartyDefeat(); return; }
        if (m == Move::SWARM_BITE && !isStanding(heroAt(t))) {
            addChatMessage("Combat", foe.name + " circles, hissing — it won't touch the fallen.");
            A.lastTarget = t;
            planEnemyMove(foe);
            return;
        }
        // Only Nightmare's telegraphed "finish the fallen" may strike a downed hero while others stand.
        if (m != Move::FINISH_DOWNED && !isStanding(heroAt(t)) && !standingHeroes().empty()) {
            t = standingHeroes().front();
        }
    }
    if (mi.aoe && standingHeroes().empty()) {
        // An area move with nobody standing: strike a fallen hero instead (never stall the fight).
        t = pickAiHeroTarget(foe, beh);
        if (t < 0) { checkPartyDefeat(); return; }
        m = Move::STRIKE;
    }
    const ai::MoveInfo& mv = ai::moveInfo(m);
    const std::string label = mv.label;

    switch (m) {
        case Move::STRIKE: case Move::FINISH: case Move::PACK_BITE: case Move::SWARM_BITE:
        case Move::SNATCH: case Move::BACKSTAB: case Move::SHACKLE: case Move::FINISH_DOWNED:
        case Move::SNUFF: case Move::DIVE: case Move::TYRANTS_DUE: case Move::CINDER_BOLT:
        case Move::HYDRA_FIRE: case Move::SLAM: {
            if (m == Move::SLAM && A.stagger >= static_cast<int>(std::ceil(ai::kStaggerShare * foe.maxHp))) {
                lastEvent_ = foe.name + " is staggered — the Millstone Slam never lands.";
                addChatMessage("Combat", lastEvent_);
                A.stagger = 0;
                break;
            }
            int bonus = 0;
            double mult = mv.dmgMult;
            if (m == Move::PACK_BITE) {
                for (const auto& e : enemies_)
                    if (e.get() != &foe && isStanding(e.get()) && e->enemyId == foe.enemyId && e->ai.target == t) ++bonus;
            }
            if (m == Move::SWARM_BITE) {
                int rats = 0;
                for (const auto& e : enemies_) if (e.get() != &foe && isStanding(e.get()) && e->enemyId == foe.enemyId) ++rats;
                mult += rats * 10.0 / std::max(10, foe.curveAvgDmgX10); // +1 damage per other rat
            }
            const int goldBefore = heroAt(t) ? heroAt(t)->gold : 0;
            const bool hit = foeStrike(foe, foeIdx, t, mult, bonus, mv.heavyExempt, mv.autoHit, label);
            Character* h = heroAt(t);
            if (hit && h && m == Move::SNATCH && goldBefore > 0) {
                const int steal = std::max(1, static_cast<int>(std::lround(goldBefore * ai::kSnatchShare)));
                const int took = std::min(steal, h->gold);
                h->gold -= took;
                A.stolenGold += took;
                addChatMessage("Combat", foe.name + " snatches " + std::to_string(took) + " gold from " + h->name + "!");
            }
            if (hit && h && m == Move::SHACKLE && isStanding(h)) {
                const bool strSave = h->statRules; // v2.31: STR resists Shackle (was CON)
                if (ai::savePasses(getRandomInt(1, 20), h->effectiveAttr(strSave ? 0 : 2), el)) {
                    addChatMessage("Combat", h->name + std::string(" shrugs off the shackle (") + (strSave ? "STR" : "CON") + " save).");
                } else {
                    h->status.shackled = 1;
                    addChatMessage("Combat", h->name + " is shackled — no class feature next turn.");
                }
            }
            if (m == Move::SLAM) A.stagger = 0;
            break;
        }
        case Move::TWIN_STRIKE: {
            A.alt++;
            dmSay(foe.name + " strikes twice!");
            foeStrike(foe, foeIdx, t, 1.0, 0, false, false, label);
            if (!gameOver_) {
                int t2 = isStanding(heroAt(t)) ? t : pickAiHeroTarget(foe, beh);
                if (t2 >= 0 && heroAt(t2) && !heroAt(t2)->isDead) foeStrike(foe, foeIdx, t2, 1.0, 0, false, false, label);
            }
            break;
        }
        case Move::HEADS: {
            dmSay(foe.name + "'s three heads strike!");
            for (int k = 0; k < 3 && !gameOver_; ++k) {
                const std::vector<int> up = standingHeroes();
                // Nobody standing: the heads go for the fallen, as every foe always has.
                const int th = up.empty() ? pickAiHeroTarget(foe, beh)
                                          : up[static_cast<size_t>(getRandomInt(0, static_cast<int>(up.size()) - 1))];
                if (th < 0) break;
                foeStrike(foe, foeIdx, th, mv.dmgMult, 0, false, false, label);
            }
            break;
        }
        case Move::SHIELD_WALL: {
            Character* ally = foeAt(t - 100);
            if (ally && isStanding(ally)) {
                if (ally->status.guarded <= 0) ally->armorClass += ai::kGuardAc;
                ally->status.guarded = 1;
                lastEvent_ = foe.name + " raises a shield-wall over " + (ally == &foe ? std::string("itself") : ally->name) + " (+3 AC).";
            } else {
                lastEvent_ = foe.name + " holds its ground.";
            }
            addChatMessage("Combat", lastEvent_);
            break;
        }
        case Move::WARDEN_GUARD: case Move::TAKE_FLIGHT: {
            A.alt++;
            if (foe.status.guarded <= 0) foe.armorClass += ai::kGuardAc;
            foe.status.guarded = 1;
            lastEvent_ = m == Move::TAKE_FLIGHT ? foe.name + " takes flight (+3 AC) — it will dive next turn!"
                                                : foe.name + " guards (+3 AC).";
            addChatMessage("Combat", lastEvent_);
            break;
        }
        case Move::RALLY: {
            int n = 0;
            for (auto& e : enemies_) if (e.get() != &foe && isStanding(e.get())) { e->status.inspired = 2; ++n; }
            lastEvent_ = foe.name + " lets out a rally cry — " + std::to_string(n) + " allies +2 to hit for 2 rounds.";
            addChatMessage("Combat", lastEvent_);
            break;
        }
        case Move::KNIT: case Move::RELIQUARY: {
            Character* who = (m == Move::RELIQUARY) ? &foe : foeAt(t - 100);
            if (m == Move::RELIQUARY) A.flags |= ai::F_CALL2;
            if (who && isStanding(who)) {
                const int amt = std::max(1, static_cast<int>(std::lround(who->maxHp * (m == Move::KNIT ? ai::kKnitHeal : ai::kReliquaryHeal))));
                who->heal(amt);
                lastEvent_ = foe.name + (m == Move::KNIT ? " knits bones: " : " lifts the reliquary: ") + who->name
                    + " heals " + std::to_string(amt) + ".";
                addChatMessage("Combat", lastEvent_);
            }
            break;
        }
        case Move::MARK: case Move::TALLY: {
            Character* h = heroAt(t);
            if (m == Move::TALLY) A.flags |= ai::F_CALL2;
            if (h && isStanding(h) && h->statRules && ai::savePasses(getRandomInt(1, 20), h->effectiveAttr(4), el)) {
                lastEvent_ = h->name + " stays unseen (WIS save) — " + foe.name + "'s mark slides off.";
                addChatMessage("Combat", lastEvent_);
            } else if (h && isStanding(h)) {
                h->status.marked = (m == Move::TALLY) ? 3 : 2;
                lastEvent_ = foe.name + (m == Move::TALLY ? " tallies " : " marks ") + h->name + " — foes +2 to hit them.";
                addChatMessage("Combat", lastEvent_);
            }
            break;
        }
        case Move::RATTLE_HEALER: case Move::CROWN_FLARE: case Move::WEBS: {
            Character* h = heroAt(t);
            if (!h || !isStanding(h)) break;
            if (m == Move::RATTLE_HEALER) {
                if (ai::savePasses(getRandomInt(1, 20), h->effectiveAttr(4), el)) {
                    lastEvent_ = h->name + " keeps their focus (WIS save) against " + foe.name + "'s rattle.";
                } else {
                    h->status.shackled = 1;
                    lastEvent_ = foe.name + " rattles " + h->name + " — no class feature next turn.";
                }
            } else if (m == Move::CROWN_FLARE) {
                h->status.blind = 1;
                lastEvent_ = foe.name + "'s crown flares — " + h->name + " is blinded (-4 to hit next turn).";
            } else if (h->statRules && ai::savePasses(getRandomInt(1, 20), h->effectiveAttr(4), el)) {
                lastEvent_ = h->name + " tears free of " + foe.name + "'s webs (WIS save).";
            } else {
                if (h->status.slowed <= 0) {
                    h->status.slowedFromInit = h->initiative;
                    int lowest = h->initiative;
                    for (Character* c : turnOrder_) if (c) lowest = std::min(lowest, c->initiative);
                    h->initiative = lowest - 1;
                    rebuildTurnOrder();
                }
                h->status.slowed = 2;
                lastEvent_ = foe.name + " webs " + h->name + " — slowed, acts last for 2 rounds.";
            }
            addChatMessage("Combat", lastEvent_);
            break;
        }
        case Move::EMBER_WOUND: {
            for (int i : standingHeroes()) {
                Character* h = heroAt(i);
                const bool saved = ai::savePasses(getRandomInt(1, 20), h->effectiveAttr(2), el);
                int per = std::max(1, static_cast<int>(std::lround(foe.curveAvgDmgX10 / 10.0 * 0.35 * ai::kBossDamageScale)));
                if (saved) per = std::max(1, per / 2);
                h->status.burn = 2;
                h->status.burnDmg = per;
                addChatMessage("Combat", h->name + " burns (" + std::to_string(per) + " a turn for 2 turns" + (saved ? ", halved by CON save" : "") + ").");
            }
            lastEvent_ = foe.name + " — Ember wound!";
            break;
        }
        case Move::TOLL: {
            int rattled = 0;
            for (int i : standingHeroes()) {
                Character* h = heroAt(i);
                if (!ai::savePasses(getRandomInt(1, 20), h->effectiveAttr(4), el)) {
                    h->nextAttackPenalty = std::max(h->nextAttackPenalty, ai::kRattle);
                    ++rattled;
                }
            }
            lastEvent_ = "The bell tolls — " + std::to_string(rattled) + " hero(es) rattled (-2 next attack; WIS save).";
            addChatMessage("Combat", lastEvent_);
            if (!dmOnlyTable_) wrenSay("…");
            break;
        }
        case Move::BELL_CRACK: {
            A.flags |= ai::F_CALL3;
            foe.curveToHit += 2;
            foe.armorClass = std::max(1, foe.armorClass - 2);
            lastEvent_ = "The bell cracks! " + foe.name + " +2 to hit, -2 AC.";
            addChatMessage("Combat", lastEvent_);
            break;
        }
        case Move::CINDER_BREATH: {
            dmSay(foe.name + " breathes cinders over the party!");
            for (int i : standingHeroes()) {
                if (gameOver_) break;
                Character* h = heroAt(i);
                const bool saved = ai::savePasses(getRandomInt(1, 20), h->effectiveAttr(1), el);
                foeStrike(foe, foeIdx, i, saved ? mv.dmgMult / 2 : mv.dmgMult, 0, true, true,
                          saved ? std::string("Cinder breath (DEX save, half)") : label);
            }
            break;
        }
        case Move::SEXTON_CALL: case Move::RAISE_DEAD: case Move::KING_CALL: case Move::BROOD: case Move::VENN_CALL: {
            if (m == Move::SEXTON_CALL || m == Move::KING_CALL) A.flags |= ai::F_CALL2;
            else A.flags |= ai::F_CALL3;
            int called = 0;
            if (m == Move::VENN_CALL && vennChoice_ == VENN_SPARED) {
                lastEvent_ = foe.name + " reaches for a whistle — and lets it fall. No one answers.";
                addChatMessage("Combat", lastEvent_);
                break;
            }
            const char* id = "enemy_ashbound";
            CharacterClass cl = CharacterClass::FIGHTER;
            int count = 1;
            if (m == Move::KING_CALL) { id = "enemy_wickthief"; cl = CharacterClass::ROGUE; count = 2; }
            else if (m == Move::BROOD) { id = "enemy_gloamhound"; cl = CharacterClass::ROGUE; count = 2; }
            else if (m == Move::VENN_CALL) { id = "enemy_tithe_enforcer"; }
            for (int k = 0; k < count; ++k) if (summonFoe(id, cl, foe)) ++called;
            lastEvent_ = called > 0 ? foe.name + " — " + label + " (" + std::to_string(called) + " joins the fight)."
                                    : foe.name + " calls out, but no one comes.";
            addChatMessage("Combat", lastEvent_);
            break;
        }
        case Move::SLIP_AWAY: {
            foeSlipsAway(foe); // foe is gone after this
            return;
        }
        default:
            foeStrike(foe, foeIdx, t, 1.0, 0, false, false, "Attack");
            break;
    }
    if (gameOver_ || !stillHere()) return;
    if (moveNeedsHero(m)) A.lastTarget = t;
    // Hydra, phase 3: regrows 10% at the end of its turn unless a crit landed this round.
    if (foe.enemyId == "boss_ember_hydra" && A.phase >= 3) {
        if (A.critThisRound) {
            addChatMessage("Combat", "The stumps are seared — no regrowth this round.");
        } else {
            const int amt = std::max(1, static_cast<int>(std::lround(foe.maxHp * ai::kHydraRegrow)));
            foe.heal(amt);
            addChatMessage("Combat", foe.name + " regrows " + std::to_string(amt) + " HP.");
        }
        A.critThisRound = false;
    } else if (foe.enemyId == "boss_ember_hydra" && m == Move::HYDRA_FIRE) {
        A.alt++;
    } else if (foe.enemyId == "boss_ember_hydra" && m == Move::STRIKE) {
        A.alt++;
    }
    planEnemyMove(foe); // next turn's intent is on the chip right away
}

std::string Game::moveChipLabel(const Character& foe) const {
    const ai::Move m = ai::moveFromInt(foe.ai.move);
    std::string label = ai::moveInfo(m).label;
    const int t = foe.ai.target;
    if (t >= 100) {
        const int fi = t - 100;
        if (fi >= 0 && static_cast<size_t>(fi) < enemies_.size() && enemies_[static_cast<size_t>(fi)])
            label += " " + (enemies_[static_cast<size_t>(fi)].get() == &foe ? std::string("self") : enemies_[static_cast<size_t>(fi)]->name);
    } else if (t >= 0 && static_cast<size_t>(t) < players_.size() && players_[static_cast<size_t>(t)]) {
        label += " → " + players_[static_cast<size_t>(t)]->name;
    } else if (ai::moveInfo(m).aoe) {
        label += " → everyone";
    }
    if (m == ai::Move::SLAM || m == ai::Move::SNUFF || m == ai::Move::DIVE || m == ai::Move::TYRANTS_DUE)
        label += " (next turn)";
    return savecodec::listSafe(label);
}

int Game::moveChipValue(const Character& foe) const {
    const ai::Move m = ai::moveFromInt(foe.ai.move);
    const ai::MoveInfo& mi = ai::moveInfo(m);
    if (m == ai::Move::KNIT || m == ai::Move::RELIQUARY) {
        const int fi = foe.ai.target - 100;
        const Character* who = m == ai::Move::RELIQUARY ? &foe
            : ((fi >= 0 && static_cast<size_t>(fi) < enemies_.size()) ? enemies_[static_cast<size_t>(fi)].get() : nullptr);
        if (!who) return 0;
        return std::max(1, static_cast<int>(std::lround(who->maxHp * (m == ai::Move::KNIT ? ai::kKnitHeal : ai::kReliquaryHeal))));
    }
    const double bs = foe.curveRole == static_cast<int>(curve::Role::BOSS) ? ai::kBossDamageScale : 1.0;
    if (m == ai::Move::EMBER_WOUND) return std::max(1, static_cast<int>(std::lround(foe.curveAvgDmgX10 / 10.0 * 0.35 * bs)));
    if (mi.dmgMult <= 0) return 0;
    return std::max(1, static_cast<int>(std::lround(foe.curveAvgDmgX10 / 10.0 * mi.dmgMult * bs)));
}

std::string Game::getEnemyIntents() const {
    std::ostringstream ss;
    ss << (heroHasIntBonus() ? 1 : 0) << "|" << bannerSeq_ << "|" << savecodec::listSafe(bannerText_) << "|";
    if (!enemyAiActive()) return ss.str();
    bool first = true;
    for (size_t i = 0; i < enemies_.size(); ++i) {
        const Character* e = enemies_[i].get();
        if (!e || e->curveEL <= 0 || !isStanding(e)) continue;
        int intent = static_cast<int>(ai::intentOf(ai::moveFromInt(e->ai.move)));
        std::string label = moveChipLabel(*e);
        int value = moveChipValue(*e);
        int side = -1, tgt = -1;
        if (e->curveWarnTurns > 0) {
            intent = static_cast<int>(ai::Intent::HEAVY);
            label = "Rising — strikes next turn";
            value = 0;
        }
        const ai::Move m = ai::moveFromInt(e->ai.move);
        if (e->ai.target >= 100) { side = 1; tgt = e->ai.target - 100; if (enemies_.size() > static_cast<size_t>(tgt) && enemies_[static_cast<size_t>(tgt)].get() == e) side = 2; }
        else if (e->ai.target >= 0) { side = 0; tgt = e->ai.target; }
        else if (ai::moveInfo(m).aoe) side = 3;
        else if (ai::intentOf(m) == ai::Intent::GUARD || ai::intentOf(m) == ai::Intent::HEAL) side = 2;
        if (!first) ss << ";";
        first = false;
        ss << i << "," << intent << "," << side << "," << tgt << "," << label << "," << value << ","
           << ai::statusCodes(*e);
    }
    return ss.str();
}

std::string Game::getStatusEffects() const {
    std::ostringstream ss;
    if (!enemyAiActive()) return "";
    bool first = true;
    for (size_t i = 0; i < players_.size(); ++i) {
        const Character* p = players_[i].get();
        if (!p) continue;
        const std::string codes = ai::statusCodes(*p);
        if (codes.empty()) continue;
        if (!first) ss << ";";
        first = false;
        ss << i << "," << codes;
    }
    return ss.str();
}

void Game::simBeginFight(const std::vector<std::string>& enemyIds, bool surprise) {
    for (auto& e : enemies_) removeFromTurnOrder(e.get());
    enemies_.clear();
    isMerchantRoom_ = false;
    gameOver_ = false;
    for (const auto& id : enemyIds) {
        CharacterClass cl = CharacterClass::FIGHTER;
        if (id == "boss_ember_hydra" || id == "enemy_tithe_watcher") cl = CharacterClass::WIZARD;
        else if (id.rfind("enemy_wickthief", 0) == 0 || id == "enemy_gloamhound" || id == "boss_nightfang") cl = CharacterClass::ROGUE;
        enemies_.push_back(newEnemy(id, cl));
    }
    pendingSurprise_ = surprise;
    rollInitiative();
}

} // namespace dnd



namespace dnd {

// =============================================================================================
// v2.31 universal stats (PR 3). Pure numbers live in StatRules.h; docs/STATS.md.
// =============================================================================================

void Game::syncStatRules() {
    const bool on = universalStatsActive();
    std::string note;
    int refunded = 0;
    for (auto& p : players_) {
        if (!p) continue;
        const bool was = p->statRules;
        p->statRules = on;
        if (on) {
            std::string one;
            const int r = p->clampAttributesToCaps(&one);
            if (r > 0) {
                refunded += r;
                if (!note.empty()) note += "; ";
                note += p->name + ": " + one + " (" + std::to_string(r) + " point" + (r == 1 ? "" : "s") + " refunded)";
            }
        }
        if (was != on) {
            // AC uses the DEX cap only under the new rules; HP formula is unchanged.
            const int hp = p->currentHp;
            p->calculateAC();
            p->recomputeMaxHp();
            p->currentHp = std::min(p->maxHp, hp);
        }
    }
    if (refunded > 0) {
        statMigrationNote_ = "Stat caps (v2.31): " + note + ". Spend the points on another stat.";
        addChatMessage("System", statMigrationNote_);
        lastEvent_ = statMigrationNote_;
    }
}

bool Game::specialAvailable(const Character& c) const {
    if (c.characterClass == CharacterClass::FIGHTER) {
        if (!c.specialUsedThisCombat) return c.resources > 0 || c.signatureUses > 0;
        return c.signatureUses > 0; // the Signature second surge
    }
    return c.resources > 0 || c.signatureUses > 0;
}

void Game::spendSpecialUse(Character& c) {
    // Fighter: the first surge spends a supply as before; the Signature use is the extra one.
    if (c.characterClass == CharacterClass::FIGHTER && !c.specialUsedThisCombat && c.resources > 0) {
        c.resources--;
        return;
    }
    if (c.signatureUses > 0) { c.signatureUses--; return; }
    if (c.resources > 0) c.resources--;
}

int Game::partyChaMod(const Character* except) const {
    int best = 0;
    bool any = false;
    for (const auto& p : players_) {
        if (!p || p.get() == except || p->isDead || !p->statRules) continue;
        const int m = p->effMod(5);
        if (!any || m > best) { best = m; any = true; }
    }
    return any ? best : 0;
}

int Game::luckGold(int gold) const {
    if (gold <= 0 || !universalStatsActive()) return gold;
    const double b = stats::goldBonus(partyChaMod());
    return b > 0 ? static_cast<int>(std::lround(gold * (1.0 + b))) : gold;
}

std::string Game::getStatGuide(const std::string& playerName) const {
    const Character* h = nullptr;
    for (const auto& p : players_) if (p && p->name == playerName) { h = p.get(); break; }
    if (!h) return "";
    const bool on = universalStatsActive();
    const int main = h->mainStatIndex();
    const std::string cls = h->getClassName();
    auto sgn = [](int v) { return (v >= 0 ? "+" : "") + std::to_string(v); };
    auto pct = [](double v) { return std::to_string(static_cast<int>(std::lround(v * 100))) + "%"; };
    std::ostringstream ss;
    ss << (on ? 1 : 0) << "|" << main << "|";
    for (int i = 0; i < 6; ++i) ss << (i ? "," : "") << (on ? h->attrCapFor(i) : 0);
    ss << "|";
    for (int i = 0; i < 6; ++i) ss << (i ? "," : "") << h->baseAttr(i);
    ss << "|";
    for (int i = 0; i < 6; ++i) ss << (i ? "," : "") << h->effectiveAttr(i);
    const std::string special = h->getSpecialAbilityName();
    std::string lines[6];
    const int strM = h->effMod(0), dexM = h->effMod(1), conM = h->effMod(2), intM = h->effMod(3),
              wisM = h->effMod(4), chaM = h->effMod(5);
    auto mainLine = [&](int i) -> std::string {
        if (i != main) return "";
        return "Main for " + cls + ": hit, damage and " + special + ". ";
    };
    if (!on) {
        // Online tables keep the old rules.
        lines[0] = mainLine(0) + "Online: main stat only.";
        lines[1] = mainLine(1) + "AC " + sgn(dexM) + ", initiative " + sgn(dexM) + ".";
        lines[2] = "HP " + sgn(conM) + " per level; Short Rest.";
        lines[3] = mainLine(3) + "Search " + sgn(intM) + ".";
        lines[4] = mainLine(4) + (h->characterClass == CharacterClass::CLERIC ? "Healing Word " + sgn(wisM) + "." : "Online: main stat only.");
        lines[5] = mainLine(5) + "Online: main stat only.";
    } else {
        lines[0] = mainLine(0) + "Everyone: Heft +" + std::to_string(stats::heft(strM)) + " weapon damage"
                 + (stats::breaksGuard(strM) ? "; your hits break shield-walls" : "; breaks shield-walls at STR 16")
                 + "; STR saves resist Shackle.";
        lines[1] = mainLine(1) + "Everyone: AC " + sgn(stats::acFromDex(dexM)) + " (max +6), initiative " + sgn(dexM)
                 + "; Evade " + pct(stats::evade(dexM)) + " off heavy and area hits; DEX saves.";
        lines[2] = mainLine(2) + "Everyone: HP " + sgn(conM) + " per level and on Short Rest; Grit: CON saves vs Burn; death saves +"
                 + std::to_string(stats::deathSaveBonus(conM)) + ".";
        const int cm = stats::critMin(intM, false);
        lines[3] = mainLine(3) + "Everyone: Search " + sgn(intM) + "; Weak point: crits on "
                 + (cm < 20 ? std::to_string(cm) + "–20" : std::string("20 (19+ at INT 18)"))
                 + "; Read intent: " + (intM >= stats::kReadIntentMod ? "exact numbers on foe chips." : "rough ranges on foe chips (exact at INT 14).");
        lines[4] = mainLine(4) + "Everyone: Mending +" + pct(stats::mending(wisM)) + " healing received; WIS saves resist Rattled, Marked and Slowed"
                 + (stats::restResourceBonus(wisM) ? "; +1 use on Short Rest." : "; +1 use on Short Rest at WIS 18.");
        lines[5] = mainLine(5) + "Everyone: Barter −" + pct(stats::buyDiscount(chaM)) + " shop prices, +" + std::to_string(stats::sellBonusPct(chaM))
                 + "% sell (cap 75%); Luck +" + pct(stats::goldBonus(chaM)) + " gold; Inspire +" + std::to_string(stats::inspire(chaM))
                 + " companion hit; Intimidate: foes flee " + pct(stats::intimidate(chaM)) + " sooner.";
        if (h->characterClass == CharacterClass::WIZARD)
            lines[3] += " Magic Missile darts 1d4+" + std::to_string(stats::dartBonus(intM)) + ".";
        if (h->characterClass == CharacterClass::FIGHTER)
            lines[0] += " Action Surge's second swing +" + std::to_string(stats::heft(strM)) + ".";
    }
    auto pipeSafe = [](std::string v) { for (char& c : v) if (c == '|' || c == '\n') c = ' '; return v; };
    for (const auto& l : lines) ss << "|" << pipeSafe(l);
    std::string sig;
    if (on) {
        const int tier = h->signatureTier();
        sig = "Signature (main " + std::to_string(h->effectiveAttr(main)) + "): ";
        sig += std::string(tier >= 1 ? "✓" : "·") + " 20 +1 " + special + " per battle  ";
        sig += std::string(tier >= 2 ? "✓" : "·") + " 24 " + special + " +25%  ";
        sig += std::string(tier >= 3 ? "✓" : "·") + " 28 special crits 19–20  ";
        sig += std::string(tier >= 4 ? "✓" : "·") + " 32 Lantern Arts −1 cooldown (coming soon)";
    }
    ss << "|" << pipeSafe(sig);
    return ss.str();
}

} // namespace dnd
