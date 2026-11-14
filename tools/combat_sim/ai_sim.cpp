// Combat Monte Carlo, built on the SHIPPED game code: every fight is a real Game (Game.cpp +
// GameSave.cpp) with the whole party on its own AI (attacks, specials, potions and heals, exactly
// as the companion plays), foes fitted to the encounter curve, and the full turn loop (death saves,
// statuses, boss phases, summons, flee). Enemy AI is on. Each scenario runs twice with the same
// seeds: v2.30 stat rules (uncapped model hero, main stat only) and v2.31 universal stats (caps,
// every stat helps, Signature tiers).
// Run tools/combat_sim/run.sh.
#include "Game.h"
#include "CombatCurve.h"
#include "EnemyAi.h"
#include <cstdio>
#include <string>
#include <vector>
std::ostream aout(std::cout.rdbuf());
using namespace dnd;

namespace dnd {
struct GameTestAccess {
    static void setMode(Game& g, int mode, int depth, int room) {
        g.soloPlayMode_ = mode;
        g.endlessDepth_ = depth;
        g.roomCount_ = room;
    }
    static Character& hero(Game& g, size_t i) { return *g.players_[i]; }
    static size_t heroes(const Game& g) { return g.players_.size(); }
};
} // namespace dnd
using T = GameTestAccess;

struct Scn {
    std::string name;
    int level = 1;
    int diff = 1;
    std::vector<std::string> foes;
    bool surprise = false;
    int mode = static_cast<int>(SoloPlayMode::CRAWL);
    int depth = 0;
    int extraGear = 0;
};
struct Res { double win = 0, lost = 0, rounds = 0; int el = 0; };

static void buildParty(Game& g, int level, int extraGear) {
    for (size_t i = 0; i < T::heroes(g); ++i) {
        Character& p = T::hero(g, i);
        const Character ref = curve::makeReferenceHero(p.characterClass, level);
        p.level = ref.level; p.xp = 0; p.attributes = ref.attributes; p.pendingStatPoints = 0;
        p.equippedWeapon = Item::make(curve::kRefWeaponName, ItemType::WEAPON, ref.equippedWeapon->bonus + extraGear,
                                      ItemRarity::COMMON, static_cast<int>(p.characterClass));
        p.equippedArmor = Item::make(curve::kRefArmorName, ItemType::ARMOR, ref.equippedArmor->bonus + extraGear,
                                     ItemRarity::COMMON, static_cast<int>(p.characterClass));
        p.bonusMaxHp = 0; p.bonusMaxResources = 0;
        p.applyStatsForLevel();
        p.calculateAC();
        p.currentHp = p.maxHp; p.resources = p.maxResources;
        p.isDowned = p.isDead = p.isStable = false;
        p.aiControlled = true;
    }
}

static Res run(const Scn& s, bool v231, int n) {
    curve::legacyReferenceForSim() = !v231;
    Res r;
    double hpSum = 0, maxSum = 0;
    for (int i = 0; i < n; ++i) {
        Game g;
        g.seedForTesting(2900u + static_cast<unsigned>(i) * 7919u);
        g.startNewGame(CharacterClass::FIGHTER, "Mira", static_cast<int>(SoloPlayMode::CRAWL), s.diff,
                       CharacterClass::WIZARD, true);
        g.setAsHost(true);
        g.setStatRulesEnabledForTesting(v231);
        T::setMode(g, s.mode, s.depth, 6);
        buildParty(g, s.level, s.extraGear);
        g.simBeginFight(s.foes, s.surprise);
        if (!g.getEnemies().empty()) r.el = g.getEnemies().front()->curveEL;
        int steps = 0;
        while (!g.getEnemies().empty() && !g.isGameOver() && steps < 20000) { g.processTurn(); ++steps; }
        const bool won = g.getEnemies().empty() && !g.isGameOver();
        if (won) r.win += 1;
        r.rounds += g.getCombatRound();
        for (const auto& p : g.getPlayers()) { hpSum += std::max(0, p->currentHp); maxSum += p->maxHp; }
    }
    r.win = 100.0 * r.win / n;
    r.rounds /= n;
    r.lost = maxSum > 0 ? 100.0 * (1.0 - hpSum / maxSum) : 0;
    return r;
}

int main(int argc, char** argv) {
    const int n = argc > 1 ? std::atoi(argv[1]) : 1000;
    const std::vector<std::string> room = {"enemy_wickthief", "enemy_ashbound", "enemy_tithe_watcher"};
    const std::vector<std::string> boss = {"boss_sexton", "enemy_ashbound"};
    const std::vector<std::string> trio = {"enemy_gloamhound", "enemy_gloamhound", "enemy_crier_wick"};
    std::printf("| Level | Medium room (Wickthief, Ashbound, Watcher): v2.30 → v2.31 win, HP lost | Medium boss (Sexton + Ashbound) | Hard room | Nightmare (2 hounds + crier) |\n");
    std::printf("|---|---|---|---|---|\n");
    for (int L : {1, 3, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50}) {
        auto cell = [&](const std::vector<std::string>& f, int d) {
            Scn s; s.level = L; s.diff = d; s.foes = f;
            const Res a = run(s, false, n), b = run(s, true, n);
            char buf[160];
            std::snprintf(buf, sizeof buf, "%.0f%% → **%.0f%%**, %.0f%% → %.0f%%", a.win, b.win, a.lost, b.lost);
            return std::string(buf);
        };
        std::printf("| %d | %s | %s | %s | %s |\n", L, cell(room, 1).c_str(), cell(boss, 1).c_str(),
                    cell(room, 2).c_str(), cell(trio, 3).c_str());
    }
    std::printf("\n| Scenario | EL | v2.30 stats: win · HP lost · rounds | v2.31 universal stats: win · HP lost · rounds |\n|---|---|---|---|\n");
    const int raid = static_cast<int>(SoloPlayMode::RAID), deep = static_cast<int>(SoloPlayMode::ENDLESS);
    std::vector<Scn> list;
    auto add = [&](const std::string& name, int L, int d, std::vector<std::string> f, bool surprise = false,
                   int mode = static_cast<int>(SoloPlayMode::CRAWL), int depth = 0, int gear = 0) {
        Scn s; s.name = name; s.level = L; s.diff = d; s.foes = std::move(f); s.surprise = surprise;
        s.mode = mode; s.depth = depth; s.extraGear = gear; list.push_back(s);
    };
    add("L2 Bone Gallery surprise: Ossuary Prior (Elite, warned)", 2, 1, {"boss_prior"}, true);
    add("L3 Lantern Sexton", 3, 1, {"boss_sexton"});
    add("L5 Wick King", 5, 1, {"boss_wick_king"});
    add("L6 Collector Sallow Venn", 6, 1, {"boss_collector"});
    add("L8 First Lamplighter (Warden)", 8, 1, {"boss_warden"});
    add("L9 Bell-Herald", 9, 1, {"boss_herald"});
    add("L10 Ashen Drake", 10, 1, {"boss_drake"});
    add("L10 Millstone Brute + Enforcer + Watcher (Hard)", 10, 2, {"enemy_brute", "enemy_tithe_enforcer", "enemy_tithe_watcher"});
    add("L10 4 Wickthieves (flee + snatch)", 10, 1, {"enemy_wickthief", "enemy_wickthief_scout", "enemy_wickthief_cutpurse", "enemy_wickthief_river"});
    add("L10 Bone Crier + 2 Ashbound (heals)", 10, 1, {"enemy_crier_bone", "enemy_ashbound", "enemy_ashbound_bonebound"});
    add("Raid L12 Ember Hydra (+2 EL)", 12, 1, {"boss_ember_hydra"}, false, raid);
    add("Raid L20 Hollow Crown", 20, 1, {"boss_hollow_crown"}, false, raid);
    add("Raid L20 Nightfang Matriarch", 20, 1, {"boss_nightfang"}, false, raid);
    add("Raid L30 +20/+20 gear, Ember Hydra", 30, 1, {"boss_ember_hydra"}, false, raid, 0, 20);
    add("Ashen Deep depth 14, L14, 6 foes trimmed to 4", 14, 1, {"enemy_wickthief", "enemy_ashbound", "enemy_gloamhound", "enemy_tithe_watcher", "enemy_brute", "enemy_crier_bone"}, false, deep, 14);
    add("Raid L50 Hollow Crown", 50, 1, {"boss_hollow_crown"}, false, raid);
    add("Nightmare L20 Brute + 2 Ashbound (finish the fallen)", 20, 3, {"enemy_brute", "enemy_ashbound", "enemy_ashbound"});
    for (const auto& s : list) {
        const Res a = run(s, false, n), b = run(s, true, n);
        std::printf("| %s | %d | %.1f%% · %.0f%% · %.1f | **%.1f%%** · %.0f%% · %.1f |\n", s.name.c_str(), b.el,
                    a.win, a.lost, a.rounds, b.win, b.lost, b.rounds);
    }
    return 0;
}
