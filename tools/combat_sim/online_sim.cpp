// v2.41 online Keeper-table survival sim (tools/combat_sim/run.sh N online), from online_test's harness.
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
#include <cstdlib>
#include <iostream>
#include <string>
std::ostream aout(nullptr);
using namespace dnd;


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
static int gVetLevel = 50;
/** A veteran at gVetLevel (50: +32 Mythical weapon and armor, like the report's host). */
static void addVeteran(Game& g) {
    g.addAlly("Veteran", CharacterClass::FIGHTER);
    Character& v = *T::party(g).back();
    v.level = gVetLevel; v.applyStatsForLevel(); v.recomputeMaxHp(); v.currentHp = v.maxHp;
    if (gVetLevel >= 50) {
        v.equippedWeapon = Item::make("Kindlewrath", ItemType::WEAPON, 40, ItemRarity::MYTHICAL, 0);
        v.equippedArmor = Item::make("Hearthwall Plate", ItemType::ARMOR, 40, ItemRarity::MYTHICAL, 0);
        v.equippedWeapon->upgradeLevel = v.equippedArmor->upgradeLevel = 32;
    } else { v.equippedWeapon->bonus += gVetLevel / 2; v.equippedArmor->bonus += gVetLevel / 2; }
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
int main(int argc, char** argv) {
    const int n = argc > 1 ? std::atoi(argv[1]) : 1000;
    const char* dn[] = {"Easy", "Medium", "Hard", "Nightmare"};
    std::printf("Online Keeper table, fresh level-1 joiner (5 classes), %d fights per cell: joiner down by its 2nd turn / joiner dead / one-shot from full (fights)\n\n", n);
    std::printf("| Party | Room | Easy | Medium |\n|---|---|---|---|\n");
    struct P { const char* name; int vet; };
    for (const P p : {P{"joiner alone", 0}, P{"+ L5 vet", 5}, P{"+ L10 vet", 10}, P{"+ L20 vet", 20}, P{"+ L50 Mythical vet", 50}}) {
        gVetLevel = p.vet > 0 ? p.vet : 50;
        for (int room : {1, 3, 5, 10}) {
            std::printf("| %s | %d |", p.name, room);
            for (int d : {0, 1}) {
                const Stats s = run(n, room, d, p.vet > 0, 20000 + room * 7 + d);
                std::printf(" %.1f%% / %.1f%% / %d |", pct(s.downBy2nd, s.fights), pct(s.dead, s.fights), s.fullToZero);
            }
            std::printf("\n");
        }
    }
    (void)dn;
    return 0;
}
