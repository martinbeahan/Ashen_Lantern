// Save-migration test: pre-v2.20 saves (no enemyId, old names), v2.20 Phase-1 saves
// (enemyId + Phase-1 display names) and the v2.21 Phase-2 renames. Run tools/migration_test/run.sh.
#include "Game.h"
#include <iostream>
#include <set>
#include <string>
std::ostream aout(std::cout.rdbuf());
using namespace dnd;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n"; ++failures; } } while (0)

static const char* kHeroes =
    "Hero,0,2,10,20,20,1,1,0,16,12,14,8,10,10,5,3,local-player,None:0:W:0:-1:0,None:0:A:0:-1:0,0,0,0,0,0,0,;"
    "Melf (NPC),1,2,10,12,12,3,3,0,8,14,12,16,10,10,0,2,ally-1,None:0:W:0:-1:0,None:0:A:0:-1:0,0,0,0,0,0,1,;|";

static std::string foe(const std::string& name, const std::string& uid, const std::string& id = "") {
    std::string s = name + ",0,1,0,9,9,0,0,0,10,10,10,10,10,10,0,1," + uid;
    if (!id.empty()) s += "," + id; // v2.20+ trailing enemyId
    return s + ";";
}

static std::string save(const std::string& foes) {
    return std::string("solo,5,0,0,0,0,,0,5,0,0,1|room~ev|") + kHeroes + foes + "|c1~|j1~";
}

static bool has(const std::string& roster, const std::string& needle) {
    return roster.find(needle) != std::string::npos;
}

int main() {
    // 1) Pre-v2.20 save: no enemy id field, legacy names, legacy companion.
    {
        Game g;
        g.deserialize(save(
            "Skeleton Champion,0,1,0,20,20,1,1,0,14,10,14,8,10,10,0,5,sc-1;"
            "Goblin Scout,2,1,0,5,5,0,0,0,10,16,12,12,10,10,0,4,gs-1;"
            "Goblin Scout,2,1,0,5,5,0,0,0,10,16,12,12,10,10,0,4,gs-2;"
            "Hollow Crown,0,1,0,50,50,1,1,0,14,10,14,8,10,10,0,1,hc-1;"
            "Mystery Thing,0,1,0,5,5,0,0,0,10,10,10,10,10,10,0,1,mt-1;"));
        const std::string roster = g.getBattleRoster();
        std::cout << "LEGACY ROSTER: " << roster << "\n";
        Game g2; g2.deserialize(g.serialize());
        CHECK(roster == g2.getBattleRoster());
        CHECK(has(roster, "Corvin (NPC)"));
        CHECK(has(roster, "E,The Lantern Sexton,0,20,20,boss_sexton"));
        CHECK(has(roster, "E,Wickthief Scout,2,5,5,enemy_wickthief_scout"));
        CHECK(has(roster, "boss_hollow_crown"));
        CHECK(has(roster, "E,Mystery Thing,0,5,5,"));
    }

    // 2) Phase-2 renames from a pre-v2.20 save (old names, no id).
    struct Rename { const char* oldName; const char* newName; const char* id; };
    const Rename renames[] = {
        {"Giant Rat",     "Tallow Rat",      "enemy_tallow_rat"},
        {"Ogre",          "Millstone Brute", "enemy_brute"},
        {"Debt Enforcer", "Tithe Enforcer",  "enemy_tithe_enforcer"},
        {"Ashwake Scout", "Tithe Watcher",   "enemy_tithe_watcher"},
        {"Bone Herald",   "Bone Crier",      "enemy_crier_bone"},
    };
    {
        std::string foes;
        int k = 0;
        for (const auto& r : renames) foes += foe(r.oldName, "old-" + std::to_string(k++));
        Game g; g.deserialize(save(foes));
        const std::string roster = g.getBattleRoster();
        std::cout << "PRE-2.20 RENAMES: " << roster << "\n";
        for (const auto& r : renames) {
            CHECK(has(roster, std::string("E,") + r.newName + ",0,9,9," + r.id));
            CHECK(!has(roster, std::string("E,") + r.oldName + ","));
        }
        Game g2; g2.deserialize(g.serialize());
        CHECK(roster == g2.getBattleRoster());
    }

    // 3) v2.20 (Phase 1) save: carries enemyId plus the Phase-1 display name.
    {
        std::string foes;
        int k = 0;
        for (const auto& r : renames) foes += foe(r.oldName, "p1-" + std::to_string(k++), r.id);
        foes += foe("Wickthief Herald", "p1-wh", "enemy_crier_wick");
        Game g; g.deserialize(save(foes));
        const std::string roster = g.getBattleRoster();
        std::cout << "V2.20 RENAMES: " << roster << "\n";
        for (const auto& r : renames)
            CHECK(has(roster, std::string("E,") + r.newName + ",0,9,9," + r.id));
        CHECK(has(roster, "E,Wickthief Herald,0,9,9,enemy_crier_wick"));
    }

    // 4) Name -> ID migration for every name a save may hold.
    {
        size_t n = 0;
        const auto* t = bestiary::table(n);
        std::set<std::string> ids, names;
        for (size_t i = 0; i < n; ++i) {
            CHECK(bestiary::migrateLegacyName(t[i].legacyName) == t[i].id);
            CHECK(bestiary::migrateLegacyName(t[i].displayName) == t[i].id);
            CHECK(std::string(t[i].displayName).find_first_of(",|;~") == std::string::npos);
            ids.insert(t[i].id);
            names.insert(t[i].displayName);
        }
        CHECK(n == 27 && ids.size() == n && names.size() == n);
        size_t na = 0;
        const auto* al = bestiary::aliases(na);
        for (size_t i = 0; i < na; ++i) {
            CHECK(bestiary::migrateLegacyName(al[i].name) == al[i].id);
            CHECK(bestiary::find(al[i].id) != nullptr);
        }
        for (const auto& r : renames) {
            CHECK(bestiary::migrateLegacyName(r.oldName) == r.id);
            CHECK(bestiary::migrateLegacyName(r.newName) == r.id);
            CHECK(bestiary::displayName(r.id) == r.newName);
        }
        // Numbered / decorated legacy names fall back to substring rules.
        CHECK(bestiary::migrateLegacyName("Giant Rat 2") == "enemy_tallow_rat");
        CHECK(bestiary::migrateLegacyName("Elite Ogre") == "enemy_brute");
        CHECK(bestiary::migrateLegacyName("Debt Enforcer #2") == "enemy_tithe_enforcer");
        CHECK(bestiary::migrateLegacyName("Ashwake Scout 3") == "enemy_tithe_watcher");
        CHECK(bestiary::migrateLegacyName("Bone Herald #1") == "enemy_crier_bone");
        CHECK(bestiary::migrateLegacyName("Ashwake Herald") == "boss_herald");
        std::cout << "entries=" << n << " aliases=" << na << "\n";
    }

    // 5) Boss tiers unchanged.
    CHECK(LootSystem::bossTier("boss_hollow_crown") == 4 && LootSystem::isEndgameBossId("boss_nightfang"));
    CHECK(LootSystem::bossTier("boss_warden") == 3 && LootSystem::bossTier("boss_sexton") == 0);
    CHECK(LootSystem::bossTier("enemy_brute") == 0);

    if (failures) { std::cerr << failures << " check(s) failed\n"; return 1; }
    std::cout << "migration_test OK\n";
    return 0;
}
