// Save / sync safety test (Code Auditor C1, H1, H2, M2, M5). Run tools/migration_test/run.sh.
// Reproduces the auditor's harness cases: a hero named "Bob, Jr", chat containing '|',
// names with ; and ~, a bad class ordinal, and a corrupted save that must not half-load.
#include "Game.h"
#include "SaveCodec.h"
#include <iostream>
#include <string>
#include <vector>
#include "CombatCurve.h"
std::ostream aout(std::cout.rdbuf());
using namespace dnd;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n"; ++failures; } } while (0)

static const Character* findPlayer(const Game& g, const std::string& name) {
    for (const auto& p : g.getPlayers()) if (p && p->name == name) return p.get();
    return nullptr;
}

static std::string journalOf(const Game& g) { return g.getJournal(); }

namespace dnd {
struct GameTestAccess {
    static void clearRoom(Game& g) {
        for (auto& e : g.enemies_) g.removeFromTurnOrder(e.get());
        g.enemies_.clear();
        g.enterClearedRoom(g.players_[0].get());
    }
    static void killLoot(Game& g, const char* id) { g.grantKillLoot(g.players_[0].get(), "Foe", id); }
};
} // namespace dnd

// ---- v2.36 #91 helpers: edit one numeric field of a save ----
static std::vector<std::string> splitKeep(const std::string& s, char d) {
    std::vector<std::string> out; std::string cur;
    for (char c : s) { if (c == d) { out.push_back(cur); cur.clear(); } else cur.push_back(c); }
    out.push_back(cur);
    return out;
}
static std::string joinWith(const std::vector<std::string>& v, char d) {
    std::string s;
    for (size_t i = 0; i < v.size(); ++i) { if (i) s += d; s += v[i]; }
    return s;
}
/** section 0 = header, 2 = players, 3 = enemies; rec = record index; field = ',' index; sub = ':' index or -1. */
static std::string setField(const std::string& save, size_t section, size_t rec, size_t field, const std::string& v, int sub = -1) {
    auto sec = splitKeep(save, '|');
    if (section >= sec.size()) return save;
    std::vector<std::string> recs = section == 0 ? std::vector<std::string>{sec[0]} : splitKeep(sec[section], ';');
    if (rec >= recs.size()) return save;
    auto f = splitKeep(recs[rec], ',');
    if (field >= f.size()) return save;
    if (sub < 0) f[field] = v;
    else {
        auto parts = splitKeep(f[field], ':');
        if (static_cast<size_t>(sub) >= parts.size()) return save;
        parts[static_cast<size_t>(sub)] = v;
        f[field] = joinWith(parts, ':');
    }
    recs[rec] = joinWith(f, ',');
    sec[section] = section == 0 ? recs[0] : joinWith(recs, ';');
    return joinWith(sec, '|');
}
static bool sane(const Game& g) {
    namespace lim = savecodec::limits;
    if (g.getRoomCount() < 0 || g.getRoomCount() > lim::kMaxRoomCount + 1000) { std::cerr << "insane: g.getRoomCount() < 0 || g.getRoomCount() > lim::kMaxRoomCount + 1000\n"; return false; }
    for (const auto& p : g.getPlayers()) {
        if (!p) { std::cerr << "insane: !p\n"; return false; }
        if (p->level < 1 || p->level > Character::kMaxLevel) { std::cerr << "insane: p->level < 1 || p->level > Character::kMaxLevel\n"; return false; }
        if (p->xp < 0 || p->xp > lim::kMaxXp) { std::cerr << "insane: p->xp < 0 || p->xp > lim::kMaxXp\n"; return false; }
        if (p->gold < 0 || p->gold > lim::kMaxGold) { std::cerr << "insane: p->gold < 0 || p->gold > lim::kMaxGold\n"; return false; }
        if (p->maxHp < 1 || p->maxHp > 2 * lim::kMaxHp + 5000) { std::cerr << "insane: p->maxHp < 1 || p->maxHp > 2 * lim::kMaxHp + 5000\n"; return false; }
        if (p->currentHp < 0 || p->currentHp > p->maxHp) { std::cerr << "insane: p->currentHp < 0 || p->currentHp > p->maxHp\n"; return false; }
        if (p->maxResources < 0 || p->maxResources > 2 * lim::kMaxResources) { std::cerr << "insane: maxResources\n"; return false; }
        if (p->resources < 0 || p->resources > p->maxResources + 10) { std::cerr << "insane: p->resources < 0 || p->resources > p->maxResources + 10\n"; return false; }
        if (p->initiative < lim::kMinInitiative || p->initiative > lim::kMaxInitiative) { std::cerr << "insane: p->initiative < lim::kMinInitiative || p->initiative > lim::kMaxInitiative\n"; return false; }
        for (const auto& it : {p->equippedWeapon, p->equippedArmor}) {
            if (!it) continue;
            if (it->bonus < lim::kMinItemBonus || it->bonus > lim::kMaxItemBonus) { std::cerr << "insane: it->bonus < lim::kMinItemBonus || it->bonus > lim::kMaxItemBonus\n"; return false; }
            if (it->upgradeLevel < 0 || it->upgradeLevel > lim::kMaxUpgradeLevel) { std::cerr << "insane: it->upgradeLevel < 0 || it->upgradeLevel > lim::kMaxUpgradeLevel\n"; return false; }
            if (it->classTag < -1 || it->classTag > 4) { std::cerr << "insane: it->classTag < -1 || it->classTag > 4\n"; return false; }
        }
    }
    for (const auto& e : g.getEnemies()) {
        if (!e || e->maxHp < 1 || e->maxHp > lim::kMaxHp || e->gold < 0 || e->gold > lim::kMaxGold) { std::cerr << "insane: !e || e->maxHp < 1 || e->maxHp > lim::kMaxHp || e->gold < 0 || e->gold > lim::kMaxGold\n"; return false; }
    }
    return true;
}

int main() {
    // 1) "Bob, Jr" (unsanitized, e.g. an old host or a direct native call) round-trips exactly.
    {
        Game g;
        g.startNewGame(CharacterClass::ROGUE, "Bob, Jr");
        const size_t party = g.getPlayers().size();
        const std::string s = g.serialize();
        Game g2;
        std::string err;
        CHECK(g2.deserialize(s, &err));
        CHECK(err.empty());
        CHECK(g2.getPlayers().size() == party);
        CHECK(findPlayer(g2, "Bob, Jr") != nullptr);
        CHECK(findPlayer(g2, "Bob, Jr") && findPlayer(g2, "Bob, Jr")->characterClass == CharacterClass::ROGUE);
        CHECK(g2.getCurrentActor() != nullptr || g2.getEnemies().empty());
        CHECK(g2.serialize() == s); // stable re-encode
        std::cout << "BOB JR: party=" << g2.getPlayers().size() << " name='" << g2.getPlayers()[0]->name << "'\n";
    }

    // 2) Names with ; ~ | ^ : % all survive; the roster list never sees raw separators.
    {
        Game g;
        g.startNewGame(CharacterClass::BARD, "A;B~C|D^E:F%G");
        Game g2;
        CHECK(g2.deserialize(g.serialize()));
        CHECK(findPlayer(g2, "A;B~C|D^E:F%G") != nullptr);
        const std::string roster = g2.getBattleRoster();
        CHECK(roster.find("A B~C D^E:F%G") != std::string::npos);
        CHECK(roster.find("A;B") == std::string::npos);
    }

    // 3) Chat "hi | there" no longer overwrites the journal.
    {
        Game g;
        g.startNewGame(CharacterClass::FIGHTER, "Hero");
        g.addJournalEntry("Found the crypt key.");
        g.addChatMessage("Remote", "[Bob]: hi | there ~ friend");
        const std::string before = journalOf(g);
        Game g2;
        CHECK(g2.deserialize(g.serialize()));
        CHECK(journalOf(g2) == before);
        CHECK(journalOf(g2).find("Found the crypt key.") != std::string::npos);
        CHECK(g2.getChatHistory().find("[Bob]: hi | there ~ friend") != std::string::npos);
        std::cout << "CHAT PIPE: journal kept (" << journalOf(g2).size() << " chars)\n";
    }

    // 4) Bad class ordinals (save / host state / direct calls) fall back to Fighter, no UB.
    {
        const std::string bad =
            "solo,3,0,0,0,0,,0,0,0,0,0|room~ev|"
            "Hero,7,1,0,10,10,1,1,0,16,12,14,8,10,10,0,3,local-player,None:0:W:0:-1:0,None:0:A:0:-1:0,0,0,0,0,0,0,;"
            "|Foe,-3,1,0,5,5,0,0,0,10,10,10,10,10,10,0,1,f-1,enemy_wickthief;|c~|j~";
        Game g;
        CHECK(g.deserialize(bad));
        CHECK(g.getPlayers().size() == 1);
        CHECK(g.getPlayers()[0]->characterClass == CharacterClass::FIGHTER);
        CHECK(g.getEnemies().size() == 1 && g.getEnemies()[0]->characterClass == CharacterClass::FIGHTER);
        Character c("Odd", static_cast<CharacterClass>(9));
        CHECK(c.characterClass == CharacterClass::FIGHTER);
        CHECK(c.maxHp > 0 && c.armorClass > 0 && c.maxResources >= 1 && c.attributes.strength == 16);
        g.addAlly("Joiner", static_cast<CharacterClass>(42));
        CHECK(findPlayer(g, "Joiner") && findPlayer(g, "Joiner")->characterClass == CharacterClass::FIGHTER);
        CHECK(savecodec::isValidClassId(4) && !savecodec::isValidClassId(5) && !savecodec::isValidClassId(-1));
        std::cout << "BAD CLASS: hero=" << g.getPlayers()[0]->getClassName() << " hp=" << g.getPlayers()[0]->maxHp << "\n";
    }

    // 5) H1: a corrupted save fails cleanly and leaves the running game untouched.
    {
        Game g;
        g.startNewGame(CharacterClass::CLERIC, "Keeper of Ash");
        const std::string beforeRoster = g.getBattleRoster();
        const std::string beforeSave = g.serialize();
        std::string err;
        // Legacy (unencoded) writer + "Bob, Jr" = the auditor's T1: fields shift, hp is "Jr".
        const std::string broken =
            "solo,5,0,0,0,0,,0,5,0,0,1|room~ev|"
            "Bob, Jr,0,2,10,20,20,1,1,0,16,12,14,8,10,10,5,3,local-player;|Foe,0,1,0,9,9,0,0,0,10,10,10,10,10,10,0,1,f-1;|c~|j~";
        CHECK(!g.deserialize(broken, &err));
        CHECK(!err.empty());
        CHECK(g.getBattleRoster() == beforeRoster);
        CHECK(g.serialize() == beforeSave);
        CHECK(findPlayer(g, "Keeper of Ash") != nullptr);
        std::cout << "BROKEN SAVE: rejected (" << err << "), party intact\n";
        CHECK(!g.deserialize("", &err));
        CHECK(!g.deserialize("garbage-without-sections", &err));
        CHECK(!g.deserialize("AL-1,2,0|room~ev", &err)); // truncated: no party section
        CHECK(findPlayer(g, "Keeper of Ash") != nullptr);
        CHECK(g.serialize() == beforeSave);
    }

    // 6) Legacy (pre-v2.23, no "E2" marker) text is read literally — a '%' is not decoded.
    {
        const std::string legacy =
            "solo,2,0,0,0,0,,0,0,0,0,0|Room 100%2C damp~Took 5%|"
            "Hero,0,1,0,10,10,1,1,0,16,12,14,8,10,10,0,3,local-player,None:0:W:0:-1:0,None:0:A:0:-1:0,0,0,0,0,0,0,;|"
            "|c%7C1~|j%2C1~";
        Game g;
        CHECK(g.deserialize(legacy));
        CHECK(g.getRoomDescription() == "Room 100%2C damp");
        CHECK(g.getLastEvent() == "Took 5%");
        CHECK(g.getChatHistory().find("c%7C1") != std::string::npos);
    }

    // 7) Name sanitizer (mirrors TextSafety.sanitizeName in Kotlin).
    {
        CHECK(savecodec::sanitizeName("Bob, Jr") == "Bob Jr");
        CHECK(savecodec::sanitizeName("A;B~C|D^E:F%G") == "ABCDEFG");
        CHECK(savecodec::sanitizeName("Corvin (NPC)") == "Corvin");
        CHECK(savecodec::sanitizeName("  tab\tname  ") == "tab name");
        CHECK(savecodec::sanitizeName(",;|", "Hero") == "Hero");
        CHECK(savecodec::sanitizeName(std::string(200, 'x')).size() == 96);
        CHECK(savecodec::encode("a,b|c%") == "a%2Cb%7Cc%25");
        CHECK(savecodec::decode(savecodec::encode("x;y~z^w:v\n")) == "x;y~z^w:v\n");
        CHECK(savecodec::decode("100%") == "100%");
        int v = 0;
        CHECK(!savecodec::tryParseInt("12abc", v));
        CHECK(!savecodec::tryParseInt("", v));
        CHECK(savecodec::tryParseInt(" 42 ", v) && v == 42);
        CHECK(!savecodec::tryParseInt("99999999999", v));
    }

    // 8) M2: hero-only actions do nothing on an enemy's turn.
    {
        const std::string enemyFirst =
            "solo,3,0,0,0,0,,0,0,0,0,0|room~ev|"
            "Hero,0,1,0,10,10,1,1,0,16,12,14,8,10,10,0,1,local-player,None:0:W:0:-1:0,None:0:A:0:-1:0,0,0,0,0,0,0,;"
            "|Brute,0,1,0,9,9,2,2,0,10,10,10,10,10,10,0,99,f-1,enemy_brute;"
            "Rat,0,1,0,9,9,0,0,0,10,10,10,10,10,10,0,98,f-2,enemy_tallow_rat;|c~|j~";
        Game g;
        CHECK(g.deserialize(enemyFirst));
        const Character* cur = g.getCurrentActor();
        if (cur && !cur->enemyId.empty()) {
            const int resBefore = cur->resources;
            const int hp0 = g.getEnemies()[0]->currentHp, hp1 = g.getEnemies()[1]->currentHp;
            g.playerSpecialAction(1);
            g.playerHeal(0);
            CHECK(g.getCurrentActor() == cur);
            CHECK(cur->resources == resBefore);
            CHECK(g.getEnemies()[0]->currentHp == hp0 && g.getEnemies()[1]->currentHp == hp1);
        } else {
            std::cout << "M2: (enemy not first in initiative; skipped)\n";
        }
    }

    // 9) M5: seeded RNG helper stays in range.
    {
        bool inRange = true;
        for (int i = 0; i < 2000; ++i) { int r = rng::below(20); if (r < 0 || r > 19) inRange = false; }
        CHECK(inRange);
        CHECK(rng::below(1) == 0 && rng::below(0) == 0);
    }

    // 10) v2.24 (L6): ability modifiers floor like the rules — 9 -> -1, 7 -> -2, 1 -> -5.
    {
        CHECK(Attributes::getModifier(10) == 0 && Attributes::getModifier(11) == 0);
        CHECK(Attributes::getModifier(9) == -1 && Attributes::getModifier(8) == -1);
        CHECK(Attributes::getModifier(7) == -2 && Attributes::getModifier(1) == -5);
        CHECK(Attributes::getModifier(20) == 5 && Attributes::getModifier(3) == -4);
    }
    // 11) v2.24 (L8): item names with ':' round-trip; a bad rarity falls back to Common.
    {
        Item it; it.name = "Key: Mill Hall"; it.type = ItemType::WEAPON; it.bonus = 1;
        auto back = Item::fromToken(it.toToken());
        CHECK(back && back->name == "Key: Mill Hall" && back->bonus == 1);
        auto bad = Item::fromToken("Plain Dagger:1:0:zz");
        CHECK(bad && bad->name == "Plain Dagger" && bad->rarity == ItemRarity::COMMON);
    }
    // 12) v2.24 (L10): table-driven loot keeps the old contract — legendary only when allowed,
    //     class tags in -1..4, armor of a known class gets its table bonus.
    {
        bool ok = true; int drops = 0, legendary = 0;
        for (int i = 0; i < 20000; ++i) {
            const bool allow = (i % 2) == 0;
            auto it = LootSystem::generateLoot(10, 30, 0, 3, allow, 50);
            if (!it) continue;
            ++drops;
            if (it->rarity == ItemRarity::LEGENDARY) { ++legendary; if (!allow) ok = false; }
            if (it->classTag < -1 || it->classTag > 4 || it->name.empty()) ok = false;
            if (it->name == "Bulwark Mail" && it->bonus != 2 + 2 + static_cast<int>(it->rarity)) ok = false;
        }
        CHECK(ok && drops > 0 && legendary > 0);
        CHECK(!LootSystem::generateLoot(0, -1000)); // below the drop gate: never drops
    }
    // 13) v2.24 (L10): player/enemy records share one writer/reader — enemy IDs and stats survive.
    {
        Game g;
        g.startNewGame(CharacterClass::BARD, "Wren");
        Game h;
        CHECK(h.deserialize(g.serialize()));
        CHECK(h.serialize() == g.serialize());
    }

    // 14) v2.36 #91: numbers at the edge of int — every one is pulled in on load, later math can't overflow.
    {
        namespace lim = savecodec::limits;
        const std::string IMAX = "2147483647", IMIN = "-2147483648";
        Game base;
        base.seedForTesting(11);
        base.startNewGame(CharacterClass::FIGHTER, "Edge", 2, 1, CharacterClass::WIZARD, true);
        base.setAsHost(true);
        GameTestAccess::clearRoom(base);
        const std::string s0 = base.serialize();
        // roomCount = INT_MAX: used to wrap negative and jump a level-1 hero to 50 with 1.4 billion gold.
        {
            Game g; CHECK(g.deserialize(setField(s0, 0, 0, 1, IMAX)));
            CHECK(g.getRoomCount() == lim::kMaxRoomCount);
            const int lv = g.getPlayers()[0]->level;
            for (int i = 0; i < 3; ++i) { GameTestAccess::killLoot(g, "enemy_ashbound"); GameTestAccess::clearRoom(g); }
            CHECK(g.getRoomCount() > 0 && sane(g));
            CHECK(g.getPlayers()[0]->gold <= lim::kMaxGold);
            std::cout << "INT_MAX roomCount: room " << g.getRoomCount() << ", level " << lv << " -> " << g.getPlayers()[0]->level
                      << ", gold " << g.getPlayers()[0]->gold << "\n";
        }
        // xp / gold / maxHp / maxResources / initiative = INT_MAX (hero, field 3/15/5/7/16).
        for (size_t fld : {3u, 15u, 5u, 7u, 16u, 4u, 6u}) {
            Game g; CHECK(g.deserialize(setField(s0, 2, 0, fld, IMAX)));
            CHECK(sane(g));
            GameTestAccess::killLoot(g, "boss_hollow_crown");
            GameTestAccess::clearRoom(g);
            CHECK(sane(g));
            Game n; CHECK(n.deserialize(setField(s0, 2, 0, fld, IMIN))); // negative: floored
            CHECK(sane(n));
        }
        { Game g; CHECK(g.deserialize(setField(s0, 2, 0, 15, IMAX))); CHECK(g.getPlayers()[0]->gold == lim::kMaxGold);
          GameTestAccess::killLoot(g, "boss_hollow_crown"); CHECK(g.getPlayers()[0]->gold == lim::kMaxGold); }
        { Game g; CHECK(g.deserialize(setField(s0, 2, 0, 3, IMAX))); CHECK(g.getPlayers()[0]->xp >= 0 && g.getPlayers()[0]->level <= 50); }
        { Game g; CHECK(g.deserialize(setField(s0, 2, 0, 7, IMAX))); CHECK(g.getPlayers()[0]->resources <= lim::kMaxResources + 10); }
        // Item token fields: bonus, classTag, upgradeLevel, legStats, subEffect (weapon = player field 18).
        const char* junk[] = {"2147483647", "-2147483648", "99999999999", "abc", "", "1e9", "+7", " 3 ", "-1", "0x10"};
        for (int sub : {1, 4, 5, 6, 7, 8, 9}) {
            for (const char* v : junk) {
                Game g; std::string err;
                CHECK(g.deserialize(setField(s0, 2, 0, 18, v, sub), &err)); // item fields are lenient: default + clamp
                CHECK(sane(g));
            }
        }
        { Game g; CHECK(g.deserialize(setField(s0, 2, 0, 18, IMAX, 1)));
          CHECK(g.getPlayers()[0]->equippedWeapon && g.getPlayers()[0]->equippedWeapon->bonus == lim::kMaxItemBonus);
          CHECK(g.currentEncounterLevel() <= curve::kMaxEL); }
        // Strict fields (a stat that isn't a number at all, or overflows int) still reject the whole save
        // and leave the running game untouched (transactional load, M5) — never an exception or crash.
        for (const char* v : {"abc", "99999999999", "", "1e9"}) {
            Game g; g.startNewGame(CharacterClass::CLERIC, "Keep");
            const std::string before = g.serialize();
            std::string err;
            CHECK(!g.deserialize(setField(s0, 2, 0, 15, v), &err) && !err.empty());
            CHECK(g.serialize() == before);
        }
        // Enemy record (section 3) at INT_MAX: HP, gold, initiative.
        {
            Game f; f.seedForTesting(3);
            f.startNewGame(CharacterClass::ROGUE, "Foe", 2, 1, CharacterClass::BARD, true);
            const std::string sf = f.serialize();
            for (size_t fld : {4u, 5u, 15u, 16u, 3u}) {
                Game g; CHECK(g.deserialize(setField(sf, 3, 0, fld, IMAX)));
                CHECK(sane(g));
            }
        }
        // Fuzz: 3000 random numeric edits across header / heroes / foes / item tokens.
        {
            Game f; f.seedForTesting(5);
            f.startNewGame(CharacterClass::BARD, "Fuzz", 2, 2, CharacterClass::CLERIC, true);
            const std::string seeds[] = {s0, f.serialize()};
            const char* vals[] = {"2147483647", "-2147483648", "2147483646", "-1", "0", "1", "999999999", "1000001",
                                  "abc", "", "99999999999", "1e9", "7", "50", "100000000"};
            unsigned x = 12345;
            auto rnd = [&](unsigned n) { x = x * 1103515245u + 12345u; return (x >> 8) % n; };
            int accepted = 0, rejected = 0;
            for (int it = 0; it < 3000; ++it) {
                const std::string& sd = seeds[rnd(2)];
                std::string m = sd;
                for (int k = 0; k < 3; ++k) {
                    const size_t section = std::vector<size_t>{0, 2, 3}[rnd(3)];
                    const size_t field = section == 0 ? 1 + rnd(46) : 2 + rnd(38);
                    const int sub = (section == 2 && (field == 18 || field == 19)) ? static_cast<int>(1 + rnd(9)) : -1;
                    m = setField(m, section, rnd(2), field, vals[rnd(15)], sub);
                }
                Game g; std::string err;
                if (g.deserialize(m, &err)) {
                    ++accepted;
                    CHECK(sane(g));
                    if (!g.getPlayers().empty()) { GameTestAccess::killLoot(g, "enemy_ashbound"); GameTestAccess::clearRoom(g); CHECK(sane(g)); }
                    Game r; CHECK(r.deserialize(g.serialize())); // whatever loads, saves and reloads
                } else {
                    ++rejected;
                }
            }
            CHECK(accepted > 500);
            std::cout << "FUZZ #91: 3000 edited saves, " << accepted << " loaded within limits, " << rejected << " rejected cleanly\n";
        }
        // A real save (several modes / classes / rooms) loads and re-saves byte for byte.
        for (int c = 0; c < 5; ++c) {
            Game g; g.seedForTesting(20 + c);
            g.startNewGame(static_cast<CharacterClass>(c), "Same", 1 + c % 3, c % 4, static_cast<CharacterClass>((c + 2) % 5), true);
            g.setAsHost(true);
            for (int r = 0; r < 4; ++r) { GameTestAccess::clearRoom(g); g.dmAdvanceRoom(); }
            const std::string sv = g.serialize();
            Game h; CHECK(h.deserialize(sv)); CHECK(h.serialize() == sv);
        }
    }
    if (failures) { std::cerr << failures << " check(s) failed\n"; return 1; }
    std::cout << "save_safety_test OK\n";
    return 0;
}
