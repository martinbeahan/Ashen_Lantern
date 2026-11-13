// Save / sync serialization for Game (split out of Game.cpp — Code Auditor L10).
// Format: six '|' sections — header, descriptions, players, enemies, chat, journal.
// Free text is percent-encoded (SaveCodec.h) when the header carries the "E2" marker.
#include "Game.h"
#include "AndroidOut.h"
#include "SaveCodec.h"
#include <sstream>
#include <algorithm>
#include <memory>
#include <vector>

namespace dnd {

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
       << "," << savecodec::kEncodedMarker << "|";
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
        ss << ";";
    }
    ss << "|";
    // Section 3: Enemies
    for (const auto& e : enemies_) {
        writeRecordHead(ss, *e);
        ss << "," << e->enemyId << ";"; // v2.20+: stable enemy ID (older readers ignore it)
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
