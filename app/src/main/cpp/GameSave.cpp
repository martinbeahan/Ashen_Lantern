// Save / sync serialization for Game (split out of Game.cpp — Code Auditor L10).
// Format: six '|' sections — header, descriptions, players, enemies, chat, journal.
// Free text is percent-encoded (SaveCodec.h) when the header carries the "E2" marker.
#include "Game.h"
#include "AndroidOut.h"
#include "SaveCodec.h"
#include "CombatCurve.h"
#include "EnemyAi.h"
#include <sstream>
#include <algorithm>
#include <memory>
#include <vector>

namespace dnd {

namespace {
/** v2.29 header field 37: marks the combat-curve fields that follow. */
constexpr const char* kCurveSchemaTag = "S3";

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
/** Fields 0..17 shared by player and enemy records: name, class, stat block, uid. */
void writeRecordHead(std::ostream& ss, const Character& c) {
    using savecodec::encode;
    ss << encode(c.name) << "," << static_cast<int>(c.characterClass) << "," << c.level << "," << c.xp
       << "," << c.currentHp << "," << c.maxHp << "," << c.resources << "," << c.maxResources
       << "," << c.pendingStatPoints << "," << c.attributes.strength << "," << c.attributes.dexterity
       << "," << c.attributes.constitution << "," << c.attributes.intelligence << "," << c.attributes.wisdom
       << "," << c.attributes.charisma << "," << c.gold << "," << c.initiative << "," << encode(c.uid);
}

/** Inverse of writeRecordHead: one ';' record -> fields, plus a Character built from 0..17. */
template <typename TextFn>
std::unique_ptr<Character> parseRecordHead(const std::string& rec, std::vector<std::string>& f,
                                           const char* what, TextFn&& text, const std::string* rename = nullptr) {
    f = splitAll(rec, ',');
    if (f.size() < 18) throw savecodec::ParseError(std::string("short ") + what + " record");
    const std::string n = rename ? *rename : text(f[0]);
    auto c = std::make_unique<Character>(n, parseClassField(f[1]), text(f[17]));
    parseStatBlock(*c, f);
    return c;
}
} // namespace

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
       << "," << savecodec::kEncodedMarker
       // v2.25: story choices go AFTER the E2 marker (index 34 stays the marker, so v2.23/v2.24
       // readers still detect encoding and simply ignore the extra fields).
       << "," << vennChoice_ << "," << wellChoice_
       // v2.29 combat curve (append-only): 37 schema tag, 38 pressure, 39 Rekindle rooms,
       // 40 reserved for the Drowned Vigil chamber (PR 6), 41 low-HP streak, 42 last room over
       // budget, 43 a hero dropped this room.
       << "," << kCurveSchemaTag << "," << pressure_ << "," << rekindleRooms_ << ",0"
       << "," << lowHpStreak_ << "," << (lastRoomOverBudget_ ? 1 : 0) << "," << (heroDroppedThisRoom_ ? 1 : 0)
       // v2.30 enemy AI (append-only): 44 combat round, 45 foes this room, 46 foes that fled.
       << "," << combatRound_ << "," << roomFoeTotal_ << "," << fledFoes_
       << "|";
    // Section 1: Descriptions
    ss << encode(roomDescription_) << "~" << encode(lastEvent_) << "|";
    // Section 2: Players
    for (const auto& p : players_) {
        writeRecordHead(ss, *p);
        ss << ",";
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
        // v2.29: 27 prestige rank (0 until PR 5), 28 kept bonus max HP (story padding + CON points).
        ss << "," << p->prestigeRank << "," << p->bonusMaxHp;
        // v2.30: 29 kept bonus max resources (story +1), 30 statuses (EnemyAi.h codec).
        ss << "," << p->bonusMaxResources << "," << ai::encodeStatuses(p->status);
        ss << ";";
    }
    ss << "|";
    // Section 3: Enemies
    for (const auto& e : enemies_) {
        writeRecordHead(ss, *e);
        ss << "," << e->enemyId; // v2.20+: stable enemy ID (older readers ignore it)
        // v2.29 (audit bug a): 19 AC (spawn bonuses survive a mid-fight reload), 20 role,
        // 21 EL, 22 to-hit, 23 avg damage x10, 24 boss phase (PR 2; 0 now), 25 telegraph turns
        // left, 26 surprise-boss flag.
        // v2.30: 24 is now the boss phase; 27 statuses, 28 AI state (planned move, cooldowns...).
        ss << "," << e->armorClass << "," << e->curveRole << "," << e->curveEL << "," << e->curveToHit
           << "," << e->curveAvgDmgX10 << "," << e->ai.phase << "," << e->curveWarnTurns << "," << (e->curveSurprise ? 1 : 0)
           << "," << ai::encodeStatuses(e->status) << "," << ai::encodeFoeAi(e->ai) << ";";
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


bool Game::describeSave(const std::string& data, SaveSummary* out, std::string* error) {
    if (data.empty() || data.find('|') == std::string::npos) {
        if (error) *error = "empty save";
        return false;
    }
    Game tmp{LoadOnlyTag{}};
    try {
        tmp.parseSave(data);
    } catch (const std::exception& ex) {
        if (error) *error = ex.what();
        return false;
    } catch (...) {
        if (error) *error = "unknown parse error";
        return false;
    }
    if (tmp.players_.empty() || !tmp.players_.front()) {
        if (error) *error = "no hero in save";
        return false;
    }
    if (out) {
        const Character& hero = *tmp.players_.front();
        out->heroName = hero.name;
        out->classId = static_cast<int>(hero.characterClass);
        out->level = hero.level;
        out->questBeat = tmp.questBeat_;
        out->soloPlayMode = tmp.soloPlayMode_;
        out->difficulty = tmp.difficulty_;
        out->gameOver = tmp.gameOver_;
        out->roomCount = tmp.roomCount_;
        out->storyComplete = tmp.questAct4Complete_;
    }
    return true;
}

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
    resetMemoryRuntime(); // a loaded hero gets its Ledger context pushed again by Kotlin

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
    vennChoice_ = tmp.vennChoice_;
    wellChoice_ = tmp.wellChoice_;
    pressure_ = tmp.pressure_;
    rekindleRooms_ = tmp.rekindleRooms_;
    lowHpStreak_ = tmp.lowHpStreak_;
    lastRoomOverBudget_ = tmp.lastRoomOverBudget_;
    heroDroppedThisRoom_ = tmp.heroDroppedThisRoom_;
    combatRound_ = tmp.combatRound_;
    roomFoeTotal_ = tmp.roomFoeTotal_;
    fledFoes_ = tmp.fledFoes_;
    pendingSurprise_ = false;

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
    // v2.25 story choices (fields 35/36, after the E2 marker). Older saves have none: an act that
    // is already complete keeps the old script's outcome (Venn ended, ash-dead sealed in) and is
    // never re-prompted; an act still in progress stays undecided and asks on its resolution beat.
    if (has(35)) {
        vennChoice_ = savecodec::clampInt(num(35, VENN_UNDECIDED, "vennChoice"), VENN_UNDECIDED, VENN_ENDED);
    } else {
        vennChoice_ = questAct2Complete_ ? VENN_ENDED : VENN_UNDECIDED;
    }
    if (has(36)) {
        wellChoice_ = savecodec::clampInt(num(36, WELL_UNDECIDED, "wellChoice"), WELL_UNDECIDED, WELL_LET_REST);
    } else {
        wellChoice_ = questAct3Complete_ ? WELL_SEALED_IN : WELL_UNDECIDED;
    }

    // v2.29 combat curve fields (37+). Older saves: all zero — fresh curve state.
    if (has(37) && h[37] == kCurveSchemaTag) {
        pressure_ = savecodec::clampInt(num(38, 0, "pressure"), 0, curve::kPressureMaxSteps);
        rekindleRooms_ = savecodec::clampInt(num(39, 0, "rekindle"), 0, 9);
        lowHpStreak_ = savecodec::clampInt(num(41, 0, "lowHpStreak"), 0, 9);
        lastRoomOverBudget_ = flag(42);
        heroDroppedThisRoom_ = flag(43);
        // v2.30 (44..46); a v2.29 save has none: round 1, no fled foes.
        combatRound_ = savecodec::clampInt(num(44, 1, "combatRound"), 0, 100000);
        roomFoeTotal_ = savecodec::clampInt(num(45, 0, "roomFoes"), 0, 99);
        fledFoes_ = savecodec::clampInt(num(46, 0, "fledFoes"), 0, 99);
    } else {
        combatRound_ = 1; roomFoeTotal_ = 0; fledFoes_ = 0;
        pressure_ = 0; rekindleRooms_ = 0; lowHpStreak_ = 0;
        lastRoomOverBudget_ = false; heroDroppedThisRoom_ = false;
    }

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
            std::vector<std::string> f;
            // Legacy companion rename (trademark hygiene): "Melf (NPC)" -> "Corvin (NPC)".
            const std::string first = text(rec.substr(0, rec.find(',')));
            const std::string corvin = "Corvin (NPC)";
            auto p = parseRecordHead(rec, f, "player", text, first == "Melf (NPC)" ? &corvin : nullptr);

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
            // v2.29: prestige rank (27) and kept bonus HP (28). Legacy saves have neither: the
            // bonus is whatever the saved max HP holds above the level formula (story padding,
            // odd CON points), so the next level-up no longer drops it (audit bug b). Max HP is
            // never lowered by the upgrade (it can rise: Legendary CON, level-1 floor).
            int pv = 0;
            p->prestigeRank = (f.size() > 27 && savecodec::tryParseInt(f[27], pv)) ? savecodec::clampInt(pv, 0, 10) : 0;
            const int savedMax = p->maxHp;
            if (f.size() > 28 && savecodec::tryParseInt(f[28], pv)) {
                p->bonusMaxHp = savecodec::clampInt(pv, 0, 100000);
            } else {
                p->bonusMaxHp = std::max(0, savedMax - p->formulaMaxHp());
            }
            p->recomputeMaxHp();
            if (p->maxHp < savedMax) { // never shrink a hero on load; keep the difference
                p->bonusMaxHp += savedMax - p->maxHp;
                p->recomputeMaxHp();
            }
            // v2.30: kept bonus resources (29) and statuses (30). Older saves derive the bonus
            // from the saved max (story +1 padding); max resources never shrink on load.
            const int savedMaxRes = p->maxResources;
            if (f.size() > 29 && savecodec::tryParseInt(f[29], pv)) {
                p->bonusMaxResources = savecodec::clampInt(pv, 0, 1000);
            } else {
                p->bonusMaxResources = std::max(0, savedMaxRes - p->formulaMaxResources());
            }
            p->maxResources = p->formulaMaxResources() + p->bonusMaxResources;
            if (p->maxResources < savedMaxRes) {
                p->bonusMaxResources += savedMaxRes - p->maxResources;
                p->maxResources = savedMaxRes;
            }
            if (p->resources > p->maxResources + 10) p->resources = p->maxResources;
            if (f.size() > 30) p->status = ai::decodeStatuses(f[30]);
            players_.push_back(std::move(p));
        }
    }

    // Section 3: Enemies
    enemies_.clear();
    if (std::getline(ss, section, '|')) {
        for (const std::string& rec : splitAll(section, ';')) {
            if (rec.empty()) continue;
            std::vector<std::string> f;
            auto e = parseRecordHead(rec, f, "enemy", text);
            const std::string& n = e->name;
            // v2.20+: stable enemy ID. Older saves / older hosts have none — migrate from the
            // legacy display name once, then show the current bestiary display name.
            if (f.size() > 18 && bestiary::find(f[18])) {
                e->enemyId = f[18];
            } else {
                e->enemyId = bestiary::migrateLegacyName(n);
            }
            if (bestiary::find(e->enemyId)) e->name = bestiary::displayName(e->enemyId);
            e->calculateAC();
            // v2.29 (audit bug a): the saved AC keeps every spawn bonus. Older saves have no
            // field 19 and fall back to the class formula, as before.
            // Lenient: only a record whose field 19 is a number carries the v2.29 block (very old
            // hosts wrote other shapes there — those keep the legacy behaviour, never an error).
            auto intAt = [&f](size_t i, int& out) { return f.size() > i && savecodec::tryParseInt(f[i], out); };
            int v = 0;
            if (intAt(19, v)) {
                e->armorClass = savecodec::clampInt(v, 1, 200);
                if (intAt(20, v)) e->curveRole = savecodec::clampInt(v, 0, 7);
                int el = 0, hit = 0, dmg = 0;
                if (intAt(21, el) && intAt(22, hit) && intAt(23, dmg) && el > 0) {
                    e->curveEL = savecodec::clampInt(el, 1, curve::kMaxEL);
                    e->curveToHit = savecodec::clampInt(hit, -20, 200);
                    e->curveAvgDmgX10 = savecodec::clampInt(dmg, 0, 100000);
                }
                if (intAt(25, v)) e->curveWarnTurns = savecodec::clampInt(v, 0, 3);
                e->curveSurprise = (f.size() > 26 && f[26] == "1");
                // v2.30: phase (24), statuses (27), AI state (28). v2.29 foes plan on their turn.
                int phase = 0;
                if (intAt(24, v)) phase = v;
                if (f.size() > 27) e->status = ai::decodeStatuses(f[27]);
                e->ai = ai::decodeFoeAi(f.size() > 28 ? f[28] : std::string(), phase);
            }
            e->curvePending = false; // a loaded foe is never re-fitted
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

} // namespace dnd
