#include "Game.h"
#include "EncounterAuthorship.h"
#include "AndroidOut.h"
#include "SaveCodec.h"
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
        p->maxHp += 2;
        p->currentHp = p->maxHp;
        p->maxResources += 1;
        p->resources = p->maxResources;
    }
}

void Game::setDifficulty(int d) {
    if (d < 0 || d > 3) d = static_cast<int>(Difficulty::EASY);
    difficulty_ = d;
}

void Game::startDmSession(const std::string& dmName) {
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
                else if (cl == CharacterClass::ROGUE) neu->equippedArmor = Item::make("Leather Armor", ItemType::ARMOR, 1, ItemRarity::COMMON, cls);
                else if (cl == CharacterClass::WIZARD) neu->equippedArmor = Item::make("Traveler Clothes", ItemType::ARMOR, 0, ItemRarity::COMMON, -1);
                else if (cl == CharacterClass::BARD) neu->equippedArmor = Item::make("Leather Armor", ItemType::ARMOR, 1, ItemRarity::COMMON, cls);
                else neu->equippedArmor = Item::make("Scale Mail", ItemType::ARMOR, 4, ItemRarity::COMMON, cls);
            }
        }
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

    spawnSoloQuestEnemies();
    maybeFinishQuestOnResolutionEnter();
    maybeFinishAct2OnSettledEnter();
    maybeFinishAct3OnSealedEnter();
    maybeFinishAct4OnKindledEnter();

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
                lastEvent_ = "BOSS! The Ossuary Prior — stand ready!";
                dmSay("A mitred horror stirs among the bones — the Ossuary Prior (original foe).");
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
                lastEvent_ = "BOSS! The Wick King — stand ready!";
                dmSay("A crown of melted candles by the millrace — the Wick King (original) bars the path!");
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
                dmSay("Sallow Venn, the Collector, wears a false mill seal. Keep a Potion ready — " + who + " will help.");
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
                dmSay("The Breach Warden — the First Lamplighter, bound as a gate — bars the ember wound. Keep potions ready — " + who + " will help.");
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
                dmSay("The Bell-Herald of the Tithe bars the Vigil Spire. Keep potions ready — " + who + " will help.");
            }
            break;
        case SoloQuestBeat::ACT4_KINDLED:
            break;
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
        dmSay("From inside the glass, Wren's voice: \"I'm still here. Don't you dare cry.\" Night-things withdraw from Millhollow.");
        dmSay("In the crypt dark, a figure in a grey mask watches. It writes something in a ledger, and is gone.");
        dmSay("Act 1 complete: The Kindling. Rest if you need, then Onward — Millhollow's Debt waits on the green.");
        addJournalEntry("Act 1 complete — Wren relit the Ashen Lantern with her own soul. A masked watcher saw it.");
        addChatMessage("Quest", "Act 1 done — The Kindling. Act 2 awaits.");
    }
}

void Game::maybeFinishAct2OnSettledEnter() {
    if (static_cast<SoloQuestBeat>(questBeat_) != SoloQuestBeat::ACT2_SETTLED) return;
    if (!questAct2LedgerFound_) {
        questAct2LedgerFound_ = true;
        addJournalEntry("Safety: coerced ledger counted as recovered at Debt Settled.");
    }
    if (!questAct2Complete_) {
        questAct2Complete_ = true;
        dmSay("The coerced ledger burns. The false mill seal cracks. Collectors melt into the mist.");
        dmSay("Millhollow's Debt is settled. Rest if you need — then Onward for Act 3: Emberdeep Breach.");
        addJournalEntry("Millhollow's Debt complete — ledger destroyed; false seal broken.");
        addChatMessage("Quest", "Act 2 done — Millhollow's Debt. Act 3 awaits.");
        if (players_.empty() == false && players_[0]) {
            players_[0]->gold += 40;
        }
    }
}

void Game::maybeFinishAct3OnSealedEnter() {
    if (static_cast<SoloQuestBeat>(questBeat_) != SoloQuestBeat::ACT3_SEALED) return;
    if (!questAct3SealFound_) {
        questAct3SealFound_ = true;
        addJournalEntry("Safety: Ember Seal counted as recovered at Breach Sealed.");
    }
    if (!questAct3Complete_) {
        questAct3Complete_ = true;
        questAct2Complete_ = true;
        questComplete_ = true;
        dmSay("The Ember Seal locks. Ember-light dies. Cool air returns up the well to Millhollow.");
        dmSay("Act 3 complete. Rest if you need — then Onward for Act 4: Ashwake Vigil.");
        addJournalEntry("Emberdeep Breach complete — Act 4 Ashwake Vigil awaits.");
        addChatMessage("Quest", "Act 3 done — Emberdeep Breach. Act 4 awaits.");
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
        dmSay("The Vigil Ember seats. Twin ash-light answers Millhollow across Greyfen. The Ashwake thins.");
        dmSay("Story complete (Acts 1–4). Endgame crawl bosses, Legendary gear, and Boss Raids unlock.");
        addJournalEntry("Ashwake Vigil complete — story finished; endgame unlocked.");
        addChatMessage("Quest", "Act 4 done — Story complete! Endgame unlocked.");
        if (!players_.empty() && players_[0]) {
            players_[0]->gold += 80;
        }
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
        dmSay("Someone was keeping count tonight. Onward leads to Millhollow's Debt.");
        addJournalEntry("Searched the Ashen Shrine — Act 1 is won.");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::ACT2_GREEN) {
        hero->gold += 10;
        lastEvent_ = hero->name + " overhears a collector mutter: \"The loft ledger above the weir seals every name.\"";
        dmSay("A clue: the coerced debt ledger is kept in the mill loft above the weir.");
        addJournalEntry("Clue: coerced ledger is in the mill loft above the weir.");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::ACT2_LOFT) {
        questAct2LedgerFound_ = true;
        hero->gold += 15;
        lastEvent_ = hero->name + " pulls the coerced debt ledger from the shelf — fresh ink, false mill seal stamped on every page.";
        dmSay("You have the ledger. Onward to Collector's Hall — confront the shakedown.");
        addJournalEntry("Recovered the coerced debt ledger (questAct2LedgerFound).");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::ACT2_SETTLED) {
        lastEvent_ = hero->name + " finds only ash in the clay bowl — the debt is already settled.";
        dmSay("Nothing more to take. Onward leads to the old well — Emberdeep Breach.");
        addJournalEntry("Searched after Debt Settled — Act 2 already won.");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::ACT3_RUMOR) {
        hero->gold += 12;
        lastEvent_ = hero->name + " hears: \"The Ember Seal sleeps in a niche past the root maze — only it can close the wound.\"";
        dmSay("Clue: recover the Ember Seal, then face whatever guards the breach.");
        addJournalEntry("Clue: Ember Seal lies past the Root Labyrinth.");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::ACT3_RELIC) {
        questAct3SealFound_ = true;
        hero->gold += 20;
        lastEvent_ = hero->name + " lifts the Ember Seal — iron disc, still warm, etched with ash-runes.";
        dmSay("You have the Ember Seal. Onward to the Breach Threshold — the Warden waits.");
        addJournalEntry("Recovered the Ember Seal (questAct3SealFound).");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::ACT3_SEALED) {
        lastEvent_ = hero->name + " finds cooling ash where the breach once roared — the seal holds.";
        dmSay("Nothing more to take. Onward leads west — Act 4: Ashwake Vigil.");
        addJournalEntry("Searched after Breach Sealed — Act 3 already won.");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::ACT4_WATCH) {
        hero->gold += 14;
        lastEvent_ = hero->name + " hears an elder whisper: \"The Vigil Ember sleeps in a niche under the fen-tower — only it can kindle the Spire.\"";
        dmSay("Clue: recover the Vigil Ember past Greyfen, then face whatever guards the Ashwake Threshold.");
        addJournalEntry("Clue: Vigil Ember lies beneath the Vigil Spire in Greyfen.");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::ACT4_EMBER) {
        questAct4EmberFound_ = true;
        hero->gold += 25;
        lastEvent_ = hero->name + " lifts the Vigil Ember — iron cinder, still warm, etched with twin ash-runes.";
        dmSay("You have the Vigil Ember. Onward to the Ashwake Threshold — the Herald waits.");
        addJournalEntry("Recovered the Vigil Ember (questAct4EmberFound).");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::ACT4_KINDLED) {
        lastEvent_ = hero->name + " finds twin ash-light steady in the brazier — the Vigil holds.";
        dmSay("Nothing more to take. Story is complete — Onward opens endgame rooms.");
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
    for (auto& p : players_) {
        if (!p) continue;
        p->fightShieldUsed = false;
        p->specialUsedThisCombat = false;
    }

    turnOrder_.clear();
    if (isMerchantRoom_) return;

    for (auto& p : players_) {
        if (p->isDead) continue; // fallen heroes stay out of initiative
        p->initiative = (getRandomInt(1, 20)) + Attributes::getModifier(p->attributes.dexterity);
        turnOrder_.push_back(p.get());
    }
    for (auto& e : enemies_) {
        e->initiative = (getRandomInt(1, 20)) + Attributes::getModifier(e->attributes.dexterity);
        turnOrder_.push_back(e.get());
    }
    std::sort(turnOrder_.begin(), turnOrder_.end(), [](Character* a, Character* b) {
        return a->initiative > b->initiative;
    });
    currentTurnIndex_ = 0;
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
           << "|" << it->rarityLabel() << "|" << it->classLabel() << "|" << it->shopCost() << ";";
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
           << "|" << it->sellPrice() << "|" << leg << ";";
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
        hero->calculateAC(); // Legendary DEX on weapons can affect AC
    } else {
        if (hero->equippedArmor) hero->addToInventory(hero->equippedArmor);
        hero->equippedArmor = item;
        hero->calculateAC();
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
        hero->calculateAC();
        return true;
    }
    if (slot == 1) {
        if (!hero->equippedArmor) { lastEvent_ = "No armor equipped."; return false; }
        hero->addToInventory(hero->equippedArmor);
        lastEvent_ = hero->name + " unequips " + hero->equippedArmor->name + ".";
        hero->equippedArmor = nullptr;
        hero->calculateAC();
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
    int price = item->sellPrice();
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
    int price = item->sellPrice();
    std::string itemName = item->getDescription();
    // Unequip first: clear slot so combat falls back to fists / unarmored AC.
    *ptr = nullptr;
    if (slot == 1) hero->calculateAC();
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
            return;
        }
        if (item->type == ItemType::ARMOR) {
            if (ally->equippedArmor) ally->addToInventory(ally->equippedArmor);
            ally->equippedArmor = item;
            ally->calculateAC();
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
    if (slot == 1) from->calculateAC();
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
    else if (enemyId == "boss_drake") bossSeenDrake_ = true;
    else if (enemyId == "boss_hollow_crown") bossSeenHollow_ = true;
    else if (enemyId == "boss_ember_hydra") bossSeenHydra_ = true;
    else if (enemyId == "boss_nightfang") bossSeenNightfang_ = true;
    else if (LootSystem::bossTier(enemyId) >= 3) bossSeenDrake_ = true;
}

std::unique_ptr<Character> Game::newEnemy(const std::string& enemyId, CharacterClass cl, const std::string& uidTag) {
    std::string uid = enemyId + "-" + (uidTag.empty() ? std::string() : uidTag + "-")
        + std::to_string(getRandomInt(0, 1000000));
    auto foe = std::make_unique<Character>(bestiary::displayName(enemyId), cl, uid);
    foe->enemyId = enemyId;
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
    currentTurnIndex_ = (currentTurnIndex_ + 1) % static_cast<int>(turnOrder_.size());
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
    roomDescription_ += " The foes lie still. You may Search, take a Short Rest, or press Onward.";
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
}

void Game::purgeDownedEnemies() {
    // Drop 0-HP foes out of the encounter without touching a dangling turnOrder_ pointer.
    bool removed = false;
    Character* looter = getCurrentActor();
    for (auto it = enemies_.begin(); it != enemies_.end(); ) {
        if ((*it)->currentHp <= 0 || (*it)->isDowned) {
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
    if (healIdx >= 0 && actor->resources > 0) {
        if (actor->characterClass == CharacterClass::CLERIC) {
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
    if (actor->resources > 0 && actor->characterClass != CharacterClass::CLERIC
        && allySpecialBeatsBasic(*actor, enemy)) {
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
            if (roll == 20) {
                current->heal(1);
                addChatMessage("System", current->name + " stood back up with a Natural 20!");
            } else if (roll >= 10) {
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

int Game::proficiencyBonus() const {
    int level = players_.empty() ? 1 : players_[0]->level;
    return dnd::CombatSystem::proficiencyBonusForLevel(level);
}

bool Game::performWeaponAttack(Character* actor, Character& target, int atkVisIndex, bool targetIsEnemy, int targetIndex, bool sneakAttack) {
    if (!actor) return false;
    pushVisualEvent(targetIsEnemy ? VisualEventType::PLAYER_ATTACK : VisualEventType::ENEMY_ATTACK, atkVisIndex);

    RollResult result = CombatSystem::performAttackRoll(*actor);
    int mod = CombatSystem::getPrimaryModifier(*actor);
    int pb = CombatSystem::proficiencyBonusForLevel(actor->level);
    int gearBonus = actor->equippedWeapon ? actor->equippedWeapon->bonus : 0;
    int mockPenalty = 0;
    if (actor->nextAttackPenalty > 0) {
        mockPenalty = actor->nextAttackPenalty;
        actor->nextAttackPenalty = 0;
        result.total -= mockPenalty;
    }

    std::stringstream ss;
    ss << actor->name << " attacks " << target.name << "! d20=" << result.dieRoll
       << " + " << mod << " (ability) + " << pb << " (prof) + " << gearBonus
       << " (magic)";
    if (mockPenalty > 0) ss << " - " << mockPenalty << " (rattled)";
    ss << " = " << result.total << " vs AC " << target.armorClass << ". ";

    bool hit = result.total >= target.armorClass || result.isCriticalHit;
    if (result.isCriticalFail) hit = false;

    if (hit) {
        int extraDice = sneakAttack ? actor->sneakAttackDice() : 0;
        int dmg = CombatSystem::calculateDamage(*actor, result.isCriticalHit, extraDice, 6);
        if (!targetIsEnemy) {
            tryFightShield(&target, dmg);
        }
        target.takeDamage(dmg);
        if (targetIsEnemy) {
            noteDailyQuestDamage(dmg);
            applyLegendaryOnHit(actor, dmg);
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
        currentTurnIndex_ = (currentTurnIndex_ + 1) % static_cast<int>(turnOrder_.size());
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
    target.heal(amount);
    lastEvent_ = actor->name + " drinks a Potion of Healing on " + target.name + " for " + std::to_string(amount) + " HP.";
    dmSay(target.name + " feels vitality return as the potion takes hold.");
    addChatMessage("Combat", lastEvent_);
    currentTurnIndex_ = (currentTurnIndex_ + 1) % static_cast<int>(turnOrder_.size());
}

void Game::playerSpecialAction(int targetEnemyIndex) {
    if (gameOver_ || turnOrder_.empty()) return;
    Character* actor = getCurrentActor(); // M1: bounds-checked
    if (!actor || actor->isDowned || actor->isDead || actor->currentHp <= 0) return;
    // M2: enemies never run the hero special path (an enemy "Action Surge" would hit enemies_).
    if (!isPartyMember(actor)) return;

    int playerIdx = 0;
    for (size_t i = 0; i < players_.size(); ++i) if (players_[i].get() == actor) playerIdx = static_cast<int>(i);

    // Cleric Healing Word can target allies without an enemy
    if (actor->characterClass == CharacterClass::CLERIC) {
        if (actor->resources <= 0) { lastEvent_ = "No spell slots left."; return; }
        actor->resources--;
        // Heals the lowest-HP living ally (M2: never a dead ally; the UI target index is unused here).
        int best = -1;
        for (size_t i = 0; i < players_.size(); ++i) {
            if (!players_[i] || players_[i]->isDead) continue;
            if (best < 0 || players_[i]->currentHp < players_[static_cast<size_t>(best)]->currentHp) best = static_cast<int>(i);
        }
        if (best < 0) { actor->resources++; lastEvent_ = "No ally to heal."; return; }
        Character& ally = *players_[static_cast<size_t>(best)];
        int wis = Attributes::getModifier(actor->attributes.wisdom);
        int amount = getRandomInt(1, 4) + wis;
        if (amount < 1) amount = 1;
        ally.heal(amount);
        lastEvent_ = actor->name + " casts Healing Word on " + ally.name + " for " + std::to_string(amount) + " HP.";
        dmSay("A soft glow knits " + ally.name + "'s wounds — Healing Word.");
        addChatMessage("Combat", lastEvent_);
        currentTurnIndex_ = (currentTurnIndex_ + 1) % static_cast<int>(turnOrder_.size());
        return;
    }

    if (enemies_.empty() || targetEnemyIndex < 0 || static_cast<size_t>(targetEnemyIndex) >= enemies_.size()) return;
    if (actor->resources <= 0) { lastEvent_ = "No uses remaining for that feature."; return; }
    // Fighter Action Surge: once per combat (still spends a resource/supply).
    if (actor->characterClass == CharacterClass::FIGHTER && actor->specialUsedThisCombat) {
        lastEvent_ = "Action Surge already used this fight — rest or finish the battle for another chance.";
        dmSay(actor->name + " is still catching their breath from the last surge.");
        return;
    }
    actor->resources--;
    if (actor->characterClass == CharacterClass::FIGHTER) actor->specialUsedThisCombat = true;

    Character& enemy = *enemies_[static_cast<size_t>(targetEnemyIndex)];

    if (actor->characterClass == CharacterClass::FIGHTER) {
        dmSay(actor->name + " shouts and surges forward — Action Surge!");
        // Extra attack action: two weapon attacks
        performWeaponAttack(actor, enemy, playerIdx, true, targetEnemyIndex, false);
        if (enemy.currentHp > 0) {
            dmSay("Still standing! " + actor->name + " swings again.");
            performWeaponAttack(actor, enemy, playerIdx, true, targetEnemyIndex, false);
        }
        if (enemy.currentHp <= 0) resolveEnemyDefeated(actor, targetEnemyIndex);
        else currentTurnIndex_ = (currentTurnIndex_ + 1) % static_cast<int>(turnOrder_.size());
        return;
    }

    if (actor->characterClass == CharacterClass::ROGUE) {
        dmSay(actor->name + " slips into a flank and strikes — Sneak Attack!");
        performWeaponAttack(actor, enemy, playerIdx, true, targetEnemyIndex, true);
        if (enemy.currentHp <= 0) resolveEnemyDefeated(actor, targetEnemyIndex);
        else currentTurnIndex_ = (currentTurnIndex_ + 1) % static_cast<int>(turnOrder_.size());
        return;
    }

    if (actor->characterClass == CharacterClass::WIZARD) {
        // Magic Missile: 3 darts of 1d4+1, auto-hit (SRD)
        int total = 0;
        for (int i = 0; i < 3; ++i) total += getRandomInt(1, 4) + 1;
        enemy.takeDamage(total);
        noteDailyQuestDamage(total);
        pushVisualEvent(VisualEventType::PLAYER_ATTACK, playerIdx);
        pushVisualEvent(VisualEventType::ENEMY_DAMAGE, targetEnemyIndex);
        lastEvent_ = actor->name + " casts Magic Missile! Three darts deal " + std::to_string(total) + " force damage to " + enemy.name + ".";
        dmSay("Glowing darts streak unerringly to their mark.");
        addChatMessage("Combat", lastEvent_);
        if (enemy.currentHp <= 0) resolveEnemyDefeated(actor, targetEnemyIndex);
        else currentTurnIndex_ = (currentTurnIndex_ + 1) % static_cast<int>(turnOrder_.size());
        return;
    }

    if (actor->characterClass == CharacterClass::BARD) {
        // Cutting Quip: CHA attack vs AC; psychic sting + rattle (-2 next attack). Original flavor.
        dmSay(actor->name + " fires a Cutting Quip — a barbed line that bites deeper than steel.");
        RollResult result = CombatSystem::performAttackRoll(*actor);
        int mod = CombatSystem::getPrimaryModifier(*actor);
        int pb = CombatSystem::proficiencyBonusForLevel(actor->level);
        bool hit = result.total >= enemy.armorClass || result.isCriticalHit;
        if (result.isCriticalFail) hit = false;
        pushVisualEvent(VisualEventType::PLAYER_ATTACK, playerIdx);
        std::stringstream ss;
        ss << actor->name << " Cutting Quip vs " << enemy.name << "! d20=" << result.dieRoll
           << " + " << mod << " (CHA) + " << pb << " (prof) = " << result.total
           << " vs AC " << enemy.armorClass << ". ";
        if (hit) {
            int cha = Attributes::getModifier(actor->effectiveAttr(5));
            int dmg = getRandomInt(1, 4) + cha;
            if (result.isCriticalHit) dmg += getRandomInt(1, 4);
            if (dmg < 1) dmg = 1;
            enemy.takeDamage(dmg);
            noteDailyQuestDamage(dmg);
            enemy.nextAttackPenalty = 2;
            pushVisualEvent(VisualEventType::ENEMY_DAMAGE, targetEnemyIndex);
            ss << (result.isCriticalHit ? "CRITICAL! " : "Hit! ")
               << "Psychic sting " << dmg << ". " << enemy.name << " is rattled (-2 next attack).";
            dmSay(enemy.name + " flinches at the cutting words.");
        } else {
            ss << "Miss!";
            if (result.isCriticalFail) ss << " (natural 1)";
            dmSay("The quip falls flat — " + enemy.name + " shrugs it off.");
        }
        lastEvent_ = ss.str();
        addChatMessage("Combat", lastEvent_);
        if (enemy.currentHp <= 0) resolveEnemyDefeated(actor, targetEnemyIndex);
        else currentTurnIndex_ = (currentTurnIndex_ + 1) % static_cast<int>(turnOrder_.size());
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
        int recover = std::max(p->hitDie() + Attributes::getModifier(p->attributes.constitution), missing / 2);
        if (recover < 1) recover = 1;
        if (recover > missing && missing > 0) recover = missing;
        if (missing == 0) recover = 0;
        p->heal(recover);
        p->resources = std::min(p->maxResources, p->resources + std::max(1, p->maxResources / 2));
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

    if (trySoloQuestSearch(hero)) {
        // Story Search handled (clue / rune-key / shrine).
    } else if (hero->performSavingThrow(hero->attributes.intelligence, 12)) {
        int found = 15 + roomCount_ * 2 + getRandomInt(0, 10);
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
            dmSay("You leave without the ledger — the Collector will still have copies. Steel yourselves.");
            addJournalEntry("Left Ledger Loft without recovering the coerced ledger.");
        }
        if (static_cast<SoloQuestBeat>(questBeat_) == SoloQuestBeat::ACT3_RELIC && !questAct3SealFound_) {
            dmSay("You leave without the Ember Seal — the breach will not close cleanly.");
            addJournalEntry("Left Ember Seal Niche without the seal.");
        }
        if (static_cast<SoloQuestBeat>(questBeat_) == SoloQuestBeat::ACT4_EMBER && !questAct4EmberFound_) {
            dmSay("You leave without the Vigil Ember — the Spire will not kindle cleanly.");
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
            lastEvent_ = "Onward — Act 2: Millhollow's Debt begins.";
            addChatMessage("Quest", lastEvent_);
            addJournalEntry("Act 2 begins — Millhollow's Debt on the green.");
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
            questAct2Complete_ = true;
            questComplete_ = true;
            questBeat_ = static_cast<int>(SoloQuestBeat::ACT3_RUMOR);
            roomCount_++;
            advanceLongRestProgress();
            roomSearchUsed_ = false; roomRestUsed_ = false;
            applySoloQuestRoom();
            if (!enemies_.empty()) rollInitiative();
            lastEvent_ = "Onward — Act 3: Emberdeep Breach begins.";
            addChatMessage("Quest", lastEvent_);
            addJournalEntry("Act 3 begins — Emberdeep Breach at the old well.");
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
            dmSay("Millhollow's twin lights hold. Endgame crawl bosses, Legendary gear, and Boss Raids await the brave.");
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
        lastEvent_ = "Onward — Act 3: Emberdeep Breach begins (story continues).";
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
    Character* hero = findCharacter(playerName);
    if (hero) hero->increaseAttribute(statIndex);
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
        if (hit) {
            int extra = sneak ? enemy->sneakAttackDice() : 0;
            int dmg = CombatSystem::calculateDamage(*enemy, result.isCriticalHit, extra, 6);
            tryFightShield(&hero, dmg);
            bool wasDowned = hero.isDowned || hero.isStable;
            hero.takeDamage(dmg, wasDowned && result.isCriticalHit);
            pushVisualEvent(VisualEventType::PLAYER_DAMAGE, targetIdx);
            lastEvent_ = enemy->name + " attacks " + hero.name + "! d20=" + std::to_string(result.dieRoll)
                + " = " + std::to_string(result.total) + " vs AC " + std::to_string(hero.armorClass)
                + ". " + (result.isCriticalHit ? "CRITICAL HIT! " : "Hit! ")
                + "Damage " + std::to_string(dmg) + ".";
            if (sneak) lastEvent_ += " (Sneak Attack)";
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
                + " = " + std::to_string(result.total) + " vs AC " + std::to_string(hero.armorClass) + ". Miss!";
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
    if (sender == "System" || sender == "Combat") {
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
    else if (isAct3Beat(questBeat_)) ss << "Quest: Emberdeep Breach — " << soloQuestBeatName(questBeat_) << "\n";
    else if (questAct2Complete_ && !isSoloQuestScripted()) ss << "Acts 1–2 done — Act 3 pending or onward\n";
    else if (isAct2Beat(questBeat_)) ss << "Quest: Millhollow's Debt — " << soloQuestBeatName(questBeat_) << "\n";
    else if (questComplete_ && !isSoloQuestScripted()) ss << "Quest complete: Ashen Lantern recovered\n";
    else if (isAct1Beat(questBeat_)) ss << "Quest: The Kindling — " << soloQuestBeatName(questBeat_) << "\n";
    ss << "Difficulty: " << difficultyName(difficulty_) << "\n";
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

std::string Game::serialize() {
    using savecodec::encode;
    std::stringstream ss;
    // Section 0: Header. Free-text fields are percent-encoded (SaveCodec.h); the trailing
    // "E2" marker tells readers to decode. Readers before v2.23 ignore the extra field.
    ss << encode(sessionId_) << "," << roomCount_ << "," << (gameOver_ ? 1 : 0) << "," << currentTurnIndex_ << "," << (isMerchantRoom_ ? 1 : 0)
       << "," << (dmOnlyTable_ ? 1 : 0) << "," << encode(dmName_) << "," << (roomSearchUsed_ ? 1 : 0)
       << "," << questBeat_ << "," << (questLanternRecovered_ ? 1 : 0) << "," << (questComplete_ ? 1 : 0)
       << "," << (questCryptKeyFound_ ? 1 : 0)
       << "," << (questAct2Complete_ ? 1 : 0) << "," << (questAct2LedgerFound_ ? 1 : 0)
       << "," << soloPlayMode_ << "," << difficulty_
       << "," << (bossSeenGk_ ? 1 : 0) << "," << (bossSeenSk_ ? 1 : 0) << "," << (bossSeenDrake_ ? 1 : 0)
       << "," << (questAct3Complete_ ? 1 : 0) << "," << (questAct3SealFound_ ? 1 : 0)
       << "," << (bossSeenHollow_ ? 1 : 0) << "," << (bossSeenHydra_ ? 1 : 0) << "," << (bossSeenNightfang_ ? 1 : 0)
       << "," << arenaWave_ << "," << arenaScore_ << "," << arenaWavesCleared_
       << "," << endlessDepth_ << "," << endlessBestDepthThisRun_
       << "," << (questAct4Complete_ ? 1 : 0) << "," << (questAct4EmberFound_ ? 1 : 0)
       << "," << (roomRestUsed_ ? 1 : 0) << "," << longRestNextAvailableRoom_ << "," << longRestProgress_
       << "," << savecodec::kEncodedMarker << "|";
    // Section 1: Descriptions
    ss << encode(roomDescription_) << "~" << encode(lastEvent_) << "|";
    // Section 2: Players
    for (const auto& p : players_) {
        ss << encode(p->name) << "," << static_cast<int>(p->characterClass) << "," << p->level << "," << p->xp << "," << p->currentHp << "," << p->maxHp << "," << p->resources << "," << p->maxResources << "," << p->pendingStatPoints << "," << p->attributes.strength << "," << p->attributes.dexterity << "," << p->attributes.constitution << "," << p->attributes.intelligence << "," << p->attributes.wisdom << "," << p->attributes.charisma << "," << p->gold << "," << p->initiative << "," << encode(p->uid) << ",";
        if (p->equippedWeapon) ss << p->equippedWeapon->toToken(); else ss << "None:0:W:0:-1:0";
        ss << ",";
        if (p->equippedArmor) ss << p->equippedArmor->toToken(); else ss << "None:0:A:0:-1:0";
        ss << "," << (p->isDowned ? 1 : 0) << "," << p->deathSaveSuccesses << "," << p->deathSaveFailures
           << "," << (p->isStable ? 1 : 0) << "," << (p->isDead ? 1 : 0)
           << "," << (p->aiControlled ? 1 : 0);
        // Inventory bag (hero required; companion included): tokens joined by ^
        ss << ",";
        for (size_t ii = 0; ii < p->inventory.size(); ++ii) {
            if (ii) ss << "^";
            if (p->inventory[ii]) ss << p->inventory[ii]->toToken();
        }
        ss << ";";
    }
    ss << "|";
    // Section 3: Enemies
    for (const auto& e : enemies_) {
        ss << encode(e->name) << "," << static_cast<int>(e->characterClass) << "," << e->level << "," << e->xp << "," << e->currentHp << "," << e->maxHp << "," << e->resources << "," << e->maxResources << "," << e->pendingStatPoints << "," << e->attributes.strength << "," << e->attributes.dexterity << "," << e->attributes.constitution << "," << e->attributes.intelligence << "," << e->attributes.wisdom << "," << e->attributes.charisma << "," << e->gold << "," << e->initiative << "," << encode(e->uid)
           << "," << e->enemyId << ";"; // v2.20+: stable enemy ID (older readers ignore it)
    }
    ss << "|";
    // Section 4: Chat History
    for (const auto& msg : chatHistory_) {
        ss << encode(msg) << "~";
    }
    ss << "|";
    // Section 5: Journal
    for (const auto& entry : journalEntries_) {
        ss << encode(entry) << "~";
    }
    return ss.str();
}

namespace {
/** Split keeping empty tokens (unlike getline, a trailing delimiter yields a final ""). */
std::vector<std::string> splitAll(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == delim) { out.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    out.push_back(cur);
    return out;
}

/** Common stat block shared by player and enemy records (fields 1..16 after the name). */
void parseStatBlock(Character& c, const std::vector<std::string>& f) {
    using savecodec::parseInt;
    using savecodec::clampInt;
    c.level = clampInt(parseInt(f[2], "level"), 1, 100);
    c.xp = std::max(0, parseInt(f[3], "xp"));
    c.maxHp = std::max(1, parseInt(f[5], "maxHp"));
    c.currentHp = clampInt(parseInt(f[4], "hp"), 0, c.maxHp);
    c.maxResources = std::max(0, parseInt(f[7], "maxResources"));
    c.resources = clampInt(parseInt(f[6], "resources"), 0, std::max(c.maxResources, 0) + 10);
    c.pendingStatPoints = clampInt(parseInt(f[8], "statPoints"), 0, 1000);
    c.attributes = {clampInt(parseInt(f[9], "str"), 1, 60), clampInt(parseInt(f[10], "dex"), 1, 60),
                    clampInt(parseInt(f[11], "con"), 1, 60), clampInt(parseInt(f[12], "int"), 1, 60),
                    clampInt(parseInt(f[13], "wis"), 1, 60), clampInt(parseInt(f[14], "cha"), 1, 60)};
    c.gold = std::max(0, parseInt(f[15], "gold"));
    c.initiative = parseInt(f[16], "initiative");
}

CharacterClass parseClassField(const std::string& s) {
    int cl = savecodec::parseInt(s, "class");
    if (!savecodec::isValidClassId(cl)) {
        aout << "deserialize: class " << cl << " out of range, using Fighter" << std::endl;
        cl = 0;
    }
    return static_cast<CharacterClass>(cl);
}
} // namespace

bool Game::deserialize(const std::string& data, std::string* error) {
    if (data.empty()) {
        if (error) *error = "empty save";
        return false;
    }
    // H1: parse into a scratch Game; only a fully parsed save is adopted into *this.
    Game tmp{LoadOnlyTag{}};
    tmp.dmName_ = dmName_; // legacy headers without a Keeper name keep the current one
    bool hasDescriptions = false;
    try {
        hasDescriptions = tmp.parseSave(data);
    } catch (const std::exception& ex) {
        if (error) *error = ex.what();
        return false;
    } catch (...) {
        if (error) *error = "unknown parse error";
        return false;
    }

    // Commit. Drop turn pointers BEFORE destroying Character unique_ptrs (avoids use-after-free).
    turnOrder_.clear();
    pendingBossXpBonus_ = 0;
    pendingBossLootLuck_ = 0;
    pendingBossAllowLegendary_ = false;
    pendingBossGoldBonus_ = 0;
    pendingChallengeLegendaryFound_ = false;
    clearSetPiecePending();
    pendingRaidKeyDrop_ = false;
    shopInventory_.clear();

    players_ = std::move(tmp.players_);
    enemies_ = std::move(tmp.enemies_);
    chatHistory_ = std::move(tmp.chatHistory_);
    journalEntries_ = std::move(tmp.journalEntries_);
    if (hasDescriptions) {
        roomDescription_ = std::move(tmp.roomDescription_);
        lastEvent_ = std::move(tmp.lastEvent_);
    }
    sessionId_ = tmp.sessionId_;
    roomCount_ = tmp.roomCount_;
    gameOver_ = tmp.gameOver_;
    currentTurnIndex_ = tmp.currentTurnIndex_;
    isMerchantRoom_ = tmp.isMerchantRoom_;
    dmOnlyTable_ = tmp.dmOnlyTable_;
    dmName_ = tmp.dmName_;
    roomSearchUsed_ = tmp.roomSearchUsed_;
    roomRestUsed_ = tmp.roomRestUsed_;
    questBeat_ = tmp.questBeat_;
    questLanternRecovered_ = tmp.questLanternRecovered_;
    questComplete_ = tmp.questComplete_;
    questCryptKeyFound_ = tmp.questCryptKeyFound_;
    questAct2Complete_ = tmp.questAct2Complete_;
    questAct2LedgerFound_ = tmp.questAct2LedgerFound_;
    soloPlayMode_ = tmp.soloPlayMode_;
    difficulty_ = tmp.difficulty_;
    bossSeenGk_ = tmp.bossSeenGk_;
    bossSeenSk_ = tmp.bossSeenSk_;
    bossSeenDrake_ = tmp.bossSeenDrake_;
    questAct3Complete_ = tmp.questAct3Complete_;
    questAct3SealFound_ = tmp.questAct3SealFound_;
    bossSeenHollow_ = tmp.bossSeenHollow_;
    bossSeenHydra_ = tmp.bossSeenHydra_;
    bossSeenNightfang_ = tmp.bossSeenNightfang_;
    arenaWave_ = tmp.arenaWave_;
    arenaScore_ = tmp.arenaScore_;
    arenaWavesCleared_ = tmp.arenaWavesCleared_;
    endlessDepth_ = tmp.endlessDepth_;
    endlessBestDepthThisRun_ = tmp.endlessBestDepthThisRun_;
    questAct4Complete_ = tmp.questAct4Complete_;
    questAct4EmberFound_ = tmp.questAct4EmberFound_;
    longRestNextAvailableRoom_ = tmp.longRestNextAvailableRoom_;
    longRestProgress_ = tmp.longRestProgress_;

    rebuildTurnOrder();
    return true;
}

bool Game::parseSave(const std::string& data) {
    using savecodec::parseInt;
    using savecodec::ParseError;
    std::stringstream ss(data);
    std::string section;

    // Section 0: Header
    if (!std::getline(ss, section, '|') || section.empty()) throw ParseError("missing header");
    const std::vector<std::string> h = splitAll(section, ',');
    const bool enc = h.size() > 34 && h[34] == savecodec::kEncodedMarker;
    auto text = [enc](const std::string& s) { return enc ? savecodec::decode(s) : s; };
    auto has = [&h](size_t i) { return i < h.size(); };
    auto flag = [&](size_t i) { return has(i) && h[i] == "1"; };
    auto num = [&](size_t i, int def, const char* name) { return has(i) ? parseInt(h[i], name) : def; };

    sessionId_ = text(h[0]);
    roomCount_ = std::max(0, num(1, 0, "roomCount"));
    gameOver_ = flag(2);
    currentTurnIndex_ = std::max(0, num(3, 0, "turnIndex"));
    isMerchantRoom_ = flag(4);
    dmOnlyTable_ = flag(5);
    if (has(6)) dmName_ = text(h[6]);
    else if (!dmOnlyTable_) dmName_.clear();
    roomSearchUsed_ = flag(7);
    questBeat_ = num(8, 0, "questBeat");
    if (questBeat_ < 0 || questBeat_ > static_cast<int>(SoloQuestBeat::ACT4_KINDLED)) questBeat_ = 0;
    questLanternRecovered_ = flag(9);
    questComplete_ = flag(10);
    questCryptKeyFound_ = flag(11);
    questAct2Complete_ = flag(12);
    questAct2LedgerFound_ = flag(13);
    soloPlayMode_ = num(14, static_cast<int>(SoloPlayMode::STORY), "playMode");
    if (soloPlayMode_ < 0 || soloPlayMode_ > static_cast<int>(SoloPlayMode::ENDLESS))
        soloPlayMode_ = static_cast<int>(SoloPlayMode::STORY);
    difficulty_ = num(15, static_cast<int>(Difficulty::EASY), "difficulty");
    if (difficulty_ < 0 || difficulty_ > 3) difficulty_ = static_cast<int>(Difficulty::EASY);
    bossSeenGk_ = flag(16);
    bossSeenSk_ = flag(17);
    bossSeenDrake_ = flag(18);
    questAct3Complete_ = flag(19);
    questAct3SealFound_ = flag(20);
    bossSeenHollow_ = flag(21);
    bossSeenHydra_ = flag(22);
    bossSeenNightfang_ = flag(23);
    arenaWave_ = std::max(0, num(24, 0, "arenaWave"));
    arenaScore_ = std::max(0, num(25, 0, "arenaScore"));
    arenaWavesCleared_ = std::max(0, num(26, 0, "arenaCleared"));
    endlessDepth_ = std::max(0, num(27, 0, "endlessDepth"));
    endlessBestDepthThisRun_ = std::max(0, num(28, endlessDepth_, "endlessBest"));
    questAct4Complete_ = flag(29);
    questAct4EmberFound_ = flag(30);
    roomRestUsed_ = flag(31);
    longRestNextAvailableRoom_ = std::max(0, num(32, 0, "longRestNext"));
    // Mode-safe Long Rest progress (backward compatible: fall back to roomCount_).
    longRestProgress_ = std::max(0, num(33, roomCount_, "longRestProgress"));

    // Section 1: Descriptions
    bool hasDescriptions = false;
    if (!std::getline(ss, section, '|')) throw ParseError("missing descriptions section");
    {
        size_t pos = section.find('~');
        if (pos != std::string::npos) {
            roomDescription_ = text(section.substr(0, pos));
            lastEvent_ = text(section.substr(pos + 1));
            hasDescriptions = true;
        }
    }

    // Section 2: Players
    players_.clear();
    // Every writer emits all six sections; a save without the party section is truncated.
    if (!std::getline(ss, section, '|')) throw ParseError("missing party section");
    {
        for (const std::string& rec : splitAll(section, ';')) {
            if (rec.empty()) continue;
            const std::vector<std::string> f = splitAll(rec, ',');
            if (f.size() < 18) throw ParseError("short player record");
            std::string n = text(f[0]);
            // Legacy companion rename (trademark hygiene): "Melf (NPC)" -> "Corvin (NPC)".
            if (n == "Melf (NPC)") n = "Corvin (NPC)";
            auto p = std::make_unique<Character>(n, parseClassField(f[1]), text(f[17]));
            parseStatBlock(*p, f);

            // "None:..." keeps the class starter gear from the constructor (pre-v2.23 behaviour).
            if (f.size() > 18) { auto w = Item::fromToken(f[18]); if (w) p->equippedWeapon = w; }
            if (f.size() > 19) { auto a = Item::fromToken(f[19]); if (a) p->equippedArmor = a; }
            // Optional trailing death-save fields (backward compatible with older saves).
            if (f.size() > 20) {
                p->isDowned = (f[20] == "1") || (p->currentHp <= 0);
                if (f.size() > 21) p->deathSaveSuccesses = savecodec::clampInt(parseInt(f[21], "deathSaveOk"), 0, 3);
                if (f.size() > 22) p->deathSaveFailures = savecodec::clampInt(parseInt(f[22], "deathSaveFail"), 0, 3);
                if (f.size() > 23) p->isStable = (f[23] == "1");
                if (f.size() > 24) p->isDead = (f[24] == "1");
                if (f.size() > 25) {
                    p->aiControlled = (f[25] == "1");
                } else {
                    // Legacy saves: NPC-named allies were always AI-controlled.
                    p->aiControlled = (p->name.find("(NPC)") != std::string::npos);
                }
                // Optional inventory bag (v2.2+): ^-joined tokens
                if (f.size() > 26) {
                    p->inventory.clear();
                    if (!f[26].empty()) {
                        for (const std::string& tok : splitAll(f[26], '^')) {
                            auto it = Item::fromToken(tok);
                            if (it) p->inventory.push_back(it);
                        }
                    }
                }
            } else {
                p->isDowned = (p->currentHp <= 0);
                p->aiControlled = (p->name.find("(NPC)") != std::string::npos);
            }
            if (p->isDead) {
                p->isDowned = true;
                p->isStable = false;
                p->currentHp = 0;
            }
            p->calculateAC();
            players_.push_back(std::move(p));
        }
    }

    // Section 3: Enemies
    enemies_.clear();
    if (std::getline(ss, section, '|')) {
        for (const std::string& rec : splitAll(section, ';')) {
            if (rec.empty()) continue;
            const std::vector<std::string> f = splitAll(rec, ',');
            if (f.size() < 18) throw ParseError("short enemy record");
            const std::string n = text(f[0]);
            auto e = std::make_unique<Character>(n, parseClassField(f[1]), text(f[17]));
            parseStatBlock(*e, f);
            // v2.20+: stable enemy ID. Older saves / older hosts have none — migrate from the
            // legacy display name once, then show the current bestiary display name.
            if (f.size() > 18 && bestiary::find(f[18])) {
                e->enemyId = f[18];
            } else {
                e->enemyId = bestiary::migrateLegacyName(n);
            }
            if (bestiary::find(e->enemyId)) e->name = bestiary::displayName(e->enemyId);
            e->calculateAC();
            enemies_.push_back(std::move(e));
        }
    }

    // Section 4: Chat
    chatHistory_.clear();
    if (std::getline(ss, section, '|')) {
        for (const std::string& msg : splitAll(section, '~')) {
            if (!msg.empty()) chatHistory_.push_back(text(msg));
        }
    }

    // Section 5: Journal (old unencoded saves: a '|' in chat split the journal — read what is left)
    journalEntries_.clear();
    if (std::getline(ss, section, '|')) {
        for (const std::string& entry : splitAll(section, '~')) {
            if (!entry.empty()) journalEntries_.push_back(text(entry));
        }
    }
    return hasDescriptions;
}


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

} // namespace dnd
