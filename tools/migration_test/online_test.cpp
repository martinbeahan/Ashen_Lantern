// v2.41 online Keeper-table fairness (docs/ONLINE_FAIRNESS.md). Repro of the report "a joining
// player on Easy died in the first round": the host's game runs online fights with the classic
// rules, so a fresh level-1 joiner (12 HP) could lose 9-27 HP to one Wickthief hit (crit + Sneak
// Attack), Easy did nothing to ordinary foes, and the table kept the difficulty of the host's last
// solo run. Checks: no hit over 45% of a standing hero's max HP, soft crits below level 5, Easy
// applied, the table difficulty is the host's choice (never a stale solo one), the host's or a
// veteran's level / gear / Mythical never changes the foes, clients and solo are untouched.
// Uses only the public Game API + test access, so it also builds (and fails) on v2.40 master.
// Run tools/migration_test/run.sh.
#include "Game.h"
#include "CombatCurve.h"
#include <cstdio>
#include <iostream>
#include <string>
std::ostream aout(nullptr);
using namespace dnd;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n"; ++failures; } } while (0)

namespace dnd {
struct GameTestAccess {
    static std::vector<std::unique_ptr<Character>>& party(Game& g) { return g.players_; }
    static int& room(Game& g) { return g.roomCount_; }
    static void spawn(Game& g) { g.spawnRoomContent(); if (!g.enemies_.empty()) g.rollInitiative(); }
    static size_t foes(Game& g) { return g.enemies_.size(); }
    static Character* foe(Game& g, size_t i) { return g.enemies_[i].get(); }
    static void seed(Game& g, unsigned s) { g.rng_.seed(s); rng::engine().seed(s); }
    static const std::deque<std::string>& chat(const Game& g) { return g.chatHistory_; }
};
} // namespace dnd
using T = GameTestAccess;

static bool foeStanding(Game& g) {
    for (size_t i = 0; i < T::foes(g); ++i) { auto* f = T::foe(g, i); if (f && !f->isDead && f->currentHp > 0) return true; }
    return false;
}
static int firstFoe(Game& g) {
    for (size_t i = 0; i < T::foes(g); ++i) { auto* f = T::foe(g, i); if (f && !f->isDead && f->currentHp > 0) return static_cast<int>(i); }
    return 0;
}
/** A veteran like the report's host: level 50, +32 Mythical weapon and armor. */
static void addVeteran(Game& g) {
    g.addAlly("Veteran", CharacterClass::FIGHTER);
    Character& v = *T::party(g).back();
    v.level = 50; v.applyStatsForLevel(); v.recomputeMaxHp(); v.currentHp = v.maxHp;
    v.equippedWeapon = Item::make("Kindlewrath", ItemType::WEAPON, 40, ItemRarity::MYTHICAL, 0);
    v.equippedArmor = Item::make("Hearthwall Plate", ItemType::ARMOR, 40, ItemRarity::MYTHICAL, 0);
    v.equippedWeapon->upgradeLevel = v.equippedArmor->upgradeLevel = 32;
    v.calculateAC();
}

struct Stats { int fights = 0, downBy2nd = 0, dead = 0, wipes = 0, overHits = 0, fullToZero = 0; double worstShare = 0; };

/** One online table fight: the joiner (and the veteran) attack the first foe, foes act via processTurn. */
static Stats run(int n, int room, int difficulty, bool veteran, unsigned seed0) {
    Stats st;
    for (int t = 0; t < n; ++t) {
        Game g; T::seed(g, seed0 + t);
        g.startDmSession("Keeper");
        g.setDifficulty(difficulty);
        if (veteran) addVeteran(g);
        g.addAlly("Joiner", static_cast<CharacterClass>(t % 5));
        Character* j = T::party(g).back().get();
        g.dmBeginDungeon();
        if (room > 1) { T::room(g) = room; T::spawn(g); }
        if (T::foes(g) == 0) continue;
        ++st.fights;
        int joinerTurns = 0; bool down = false;
        for (int step = 0; step < 3000 && !g.isGameOver() && foeStanding(g); ++step) {
            Character* a = g.getCurrentActor();
            if (a && !a->isFoe() && !a->isDowned && !a->isDead) {
                if (a == j && ++joinerTurns == 2 && (j->isDowned || j->isDead)) down = true;
                g.playerAttack(firstFoe(g));
                continue;
            }
            const bool standing = !j->isDowned && !j->isDead;
            const int before = j->currentHp;
            g.processTurn();
            if (standing && j->currentHp < before) {
                const int dmg = before - j->currentHp;
                const double share = static_cast<double>(dmg) / j->maxHp;
                st.worstShare = std::max(st.worstShare, share);
                if (dmg > static_cast<int>(curve::kOverkillShare * j->maxHp)) st.overHits++;
                if (before == j->maxHp && j->currentHp == 0) st.fullToZero++;
            }
            if (joinerTurns < 2 && (j->isDowned || j->isDead)) down = true;
        }
        st.downBy2nd += down; st.dead += j->isDead; st.wipes += g.isGameOver();
    }
    return st;
}
static double pct(int a, int b) { return b ? 100.0 * a / b : 0.0; }

int main() {
    // ---------------------------------------------------------------- the report: fresh joiner, Easy, room 1
    {
        const Stats s = run(3000, 1, 0, false, 1000);
        std::printf("Repro (Easy, room 1, lone fresh joiner, %d fights): down by its 2nd turn %.1f%%, dead %.1f%%, "
                    "worst hit %.0f%% of max HP, hits over 45%%: %d, full HP -> 0 in one hit: %d\n",
                    s.fights, pct(s.downBy2nd, s.fights), pct(s.dead, s.fights), 100 * s.worstShare, s.overHits, s.fullToZero);
        CHECK(s.fights == 3000);
        CHECK(s.overHits == 0);                    // master: hundreds (Wickthief crit / Sneak Attack up to 27)
        CHECK(s.fullToZero == 0);                  // never one-shot from full
        CHECK(s.worstShare <= curve::kOverkillShare + 1e-9);
        CHECK(pct(s.downBy2nd, s.fights) < 3.0);   // master: ~27%
        CHECK(pct(s.dead, s.fights) < 25.0);       // master: ~54%
    }
    // ---------------------------------------------------------------- mismatched levels: L50 Mythical veteran + fresh joiner
    for (int room : {1, 5, 10, 15}) {
        const Stats s = run(1500, room, 0, true, 5000 + room);
        CHECK(s.overHits == 0 && s.fullToZero == 0);
        CHECK(pct(s.downBy2nd, s.fights) < 3.0);
    }
    // Medium / Hard / Nightmare tables get the hit cap too (no one-shots on any difficulty).
    for (int d : {1, 2, 3}) {
        const Stats s = run(1500, 1, d, false, 9000 + d);
        CHECK(s.overHits == 0 && s.fullToZero == 0);
    }
    // ---------------------------------------------------------------- foes never follow the host's / a veteran's level or gear
    for (int room : {1, 3, 5, 10, 15}) {
        for (unsigned sd = 0; sd < 40; ++sd) {
            auto foesOf = [&](bool vet) {
                Game g; T::seed(g, 777 + sd);
                g.startDmSession("Keeper"); g.setDifficulty(0);
                g.addAlly("Joiner", CharacterClass::WIZARD);
                if (vet) addVeteran(g);
                T::seed(g, 4242 + sd); // same spawn rolls either way
                T::room(g) = room; T::spawn(g);
                std::string sig;
                for (size_t i = 0; i < T::foes(g); ++i) {
                    auto* f = T::foe(g, i);
                    sig += f->enemyId + ":" + std::to_string(f->maxHp) + ":" + std::to_string(f->armorClass) + ":"
                         + std::to_string(f->level) + ":" + std::to_string(f->attributes.strength) + ":"
                         + std::to_string(f->attributes.dexterity) + ";";
                }
                return sig;
            };
            CHECK(foesOf(false) == foesOf(true));
        }
    }
    // ---------------------------------------------------------------- the table difficulty is the host's choice
    {
        Game g;
        g.startNewGame(CharacterClass::FIGHTER, "Solo", 0, /*difficulty=*/3, CharacterClass::WIZARD, true);
        CHECK(g.getDifficulty() == 3);
        g.startDmSession("Keeper");
        CHECK(g.getDifficulty() == 0);             // master: still 3 (Nightmare) from the solo run
        g.setDifficulty(1);
        CHECK(g.getDifficulty() == 1);
        bool told = false;
        for (const auto& l : T::chat(g)) if (l.find("Table difficulty: ") != std::string::npos) told = true;
        CHECK(told);
        // It travels to joiners in the normal state sync.
        Game c; c.prepareClientJoin();
        CHECK(c.deserialize(g.serialize()));
        CHECK(c.getDifficulty() == 1);
    }
    // ---------------------------------------------------------------- Easy online foes: -15% HP
    {
        int easyHp = 0, medHp = 0;
        for (unsigned sd = 0; sd < 200; ++sd) {
            for (int d : {0, 1}) {
                Game g; T::seed(g, 31 + sd);
                g.startDmSession("Keeper"); g.setDifficulty(d);
                g.addAlly("Joiner", CharacterClass::FIGHTER);
                T::seed(g, 99 + sd);
                g.dmBeginDungeon();
                for (size_t i = 0; i < T::foes(g); ++i) (d == 0 ? easyHp : medHp) += T::foe(g, i)->maxHp;
            }
        }
        CHECK(medHp > 0 && easyHp < medHp);
        std::printf("Easy vs Medium online foe HP (200 room-1 spawns): %d vs %d\n", easyHp, medHp);
    }
    // ---------------------------------------------------------------- solo play: no Keeper-table rules
    {
        Game g;
        g.startNewGame(CharacterClass::WIZARD, "Solo", 1, 0, CharacterClass::FIGHTER, true);
        bool told = false;
        g.setDifficulty(0);
        for (const auto& l : T::chat(g)) if (l.find("Table difficulty: ") != std::string::npos) told = true;
        CHECK(!told);
    }

    if (failures) { std::cerr << failures << " failure(s)\n"; return 1; }
    std::cout << "online_test OK\n";
    return 0;
}
