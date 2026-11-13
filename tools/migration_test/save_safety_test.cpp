// Save / sync safety test (Code Auditor C1, H1, H2, M2, M5). Run tools/migration_test/run.sh.
// Reproduces the auditor's harness cases: a hero named "Bob, Jr", chat containing '|',
// names with ; and ~, a bad class ordinal, and a corrupted save that must not half-load.
#include "Game.h"
#include "SaveCodec.h"
#include <iostream>
#include <string>
std::ostream aout(std::cout.rdbuf());
using namespace dnd;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n"; ++failures; } } while (0)

static const Character* findPlayer(const Game& g, const std::string& name) {
    for (const auto& p : g.getPlayers()) if (p && p->name == name) return p.get();
    return nullptr;
}

static std::string journalOf(const Game& g) { return g.getJournal(); }

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

    if (failures) { std::cerr << failures << " check(s) failed\n"; return 1; }
    std::cout << "save_safety_test OK\n";
    return 0;
}
