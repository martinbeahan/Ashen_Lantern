// Combat Monte Carlo, built on the SHIPPED game code: every fight is a real Game (Game.cpp +
// GameSave.cpp) with the whole party on its own AI (attacks, specials, potions and heals, exactly
// as the companion plays), foes fitted to the encounter curve, and the full turn loop (death saves,
// statuses, boss phases, summons, flee). Enemy AI is on. v2.32 (PR 4): each scenario runs with the
// same seeds under the v2.31 level rules, the v2.32 rules without perks, and v2.32 with a Kindled
// Path loadout (Arts on the companion AI, Wards, Steady Flames) at levels 20 / 30 / 40 / 50.
// Run tools/combat_sim/run.sh.
#include "Game.h"
#include "CombatCurve.h"
#include "EnemyAi.h"
#include "Perks.h"
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
    static void story(Game& g) { g.questAct4Complete_ = true; }
    static void clearRoomAt(Game& g, int room) { g.roomCount_ = room; g.enemies_.clear(); g.enterClearedRoom(g.players_[0].get()); }
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
    int rank = 0;      // v2.33 Rekindling rank (hero full, companion half)
    bool raw = false;  // v2.34 rebalance: under-geared + unspent (starter gear, class base stats, no Embers)
    bool debugJump = false; // v2.33: the debug-build level-50 shortcut on a fresh hero (debug code, not in release)
    int gear = 0;      // v2.40: 0 reference gear, 1 Legendary +20 set, 2 Mythical +25 (Ember Echo + Rekindle),
                       //        3 Mythical +25 (Cinder Brand + Hearth Aura)
};
struct Res { double win = 0, lost = 0, rounds = 0; int el = 0; };

/** Rules for one column: v2.31 (2 points a level, proficiency grows) or v2.32 (cap 50, perks). */
enum class Rules { V231, V232_BARE, V232 };

/** v2.32 sim loadout: Arts when unlocked, two Wards, then the main-stat and CON flames, Bright Edge, Thick Wax. */
static void giveLoadout(Character& p) {
    using namespace perks;
    const int cls = static_cast<int>(p.characterClass);
    static const int kArtsPick[5][3] = {{0, 2, 3}, {0, 3, 1}, {1, 3, 0}, {0, 1, 2}, {0, 2, 3}};
    static const int kWardPick[5][3] = {{ASH_VEIL, LAST_LIGHT, SECOND_WICK}, {EMBER_WARD, LAST_LIGHT, ASH_VEIL},
                                         {KINDLE_RUSH, STEADY_HAND, LAST_LIGHT}, {SECOND_WICK, LAST_LIGHT, ASH_VEIL},
                                         {LAST_LIGHT, SPITE_SPARK, ASH_VEIL}};
    p.flames.fill(0); p.wardsLearned = 0; p.artsLearned = 0;
    p.wardEq = {{-1, -1, -1}}; p.artEq = {{-1, -1, -1}};
    auto learnArt = [&](int i) {
        if (p.level < kArtUnlock[i] || p.embersFree() < kArtCost || (p.artsLearned & (1 << i))) return;
        p.artsLearned |= (1 << i);
        for (int k = 0; k < p.artSlots(); ++k) if (p.artEq[static_cast<size_t>(k)] < 0) { p.artEq[static_cast<size_t>(k)] = i; break; }
    };
    auto learnWard = [&](int w) {
        if (p.embersFree() < kWardCost || (p.wardsLearned & (1 << w))) return;
        p.wardsLearned |= (1 << w);
        for (int k = 0; k < p.wardSlots(); ++k) if (p.wardEq[static_cast<size_t>(k)] < 0) { p.wardEq[static_cast<size_t>(k)] = w; break; }
    };
    learnArt(kArtsPick[cls][0]);
    learnWard(kWardPick[cls][0]);
    learnWard(kWardPick[cls][1]);
    learnArt(kArtsPick[cls][1]);
    // v2.33: a Rekindled hero fills the 3rd slots (rank III Ward, rank V Art) from the rank's Embers.
    if (p.artSlots() > 2) learnArt(kArtsPick[cls][2]);
    if (p.wardSlots() > 2) learnWard(kWardPick[cls][2]);
    const int order[] = {curve::primaryIndex(p.characterClass), DEEP_TALLOW, BRIGHT_EDGE, THICK_WAX, QUICKWICK};
    for (int f : order)
        while (p.embersFree() >= 1 && p.flames[static_cast<size_t>(f)] < p.flameCap()) p.flames[static_cast<size_t>(f)]++;
}

/** v2.34 rebalance: the "debug shortcut" hero — level L, but the class base stats with every point unspent,
 *  starter gear (+0 / armor base) and no Embers spent. */
static void buildRaw(Game& g, int level) {
    for (size_t i = 0; i < T::heroes(g); ++i) {
        Character& p = T::hero(g, i);
        p.prestigeRank = 0; p.prestigeHalf = i != 0;
        p.statRules = true;
        const Character fresh(p.name, p.characterClass, "raw");
        p.level = std::clamp(level, 1, Character::kMaxLevel); p.xp = 0; p.attributes = fresh.attributes;
        p.pendingStatPoints = perks::statPointsThrough(p.level);
        p.equippedWeapon = Item::make(curve::kRefWeaponName, ItemType::WEAPON, 0, ItemRarity::COMMON, static_cast<int>(p.characterClass));
        p.equippedArmor = Item::make(curve::kRefArmorName, ItemType::ARMOR, curve::starterArmorBase(p.characterClass),
                                     ItemRarity::COMMON, static_cast<int>(p.characterClass));
        p.flames.fill(0); p.wardsLearned = 0; p.artsLearned = 0;
        p.wardEq = {{-1, -1, -1}}; p.artEq = {{-1, -1, -1}};
        p.bonusMaxHp = 0; p.bonusMaxResources = 0;
        p.applyStatsForLevel();
        p.calculateAC();
        p.currentHp = p.maxHp; p.resources = p.maxResources;
        p.isDowned = p.isDead = p.isStable = false;
        p.aiControlled = true;
    }
}

static void buildParty(Game& g, int level, int extraGear, bool perksOn, int rank = 0, int gear = 0) {
    for (size_t i = 0; i < T::heroes(g); ++i) {
        Character& p = T::hero(g, i);
        p.prestigeRank = rank; p.prestigeHalf = i != 0; // v2.33: the companion has half the rank
        p.statRules = true;
        const Character ref = curve::makeReferenceHero(p.characterClass, level);
        p.level = ref.level; p.xp = 0; p.attributes = ref.attributes; p.pendingStatPoints = 0;
        p.equippedWeapon = Item::make(curve::kRefWeaponName, ItemType::WEAPON, ref.equippedWeapon->bonus + extraGear,
                                      ItemRarity::COMMON, static_cast<int>(p.characterClass));
        p.equippedArmor = Item::make(curve::kRefArmorName, ItemType::ARMOR, ref.equippedArmor->bonus + extraGear,
                                     ItemRarity::COMMON, static_cast<int>(p.characterClass));
        if (gear > 0) {
            // v2.40: the same bonus as the reference piece (+3 for Mythical: depth/5 + 4 + 5 vs + 4 + 2),
            // Legendary at +20 (3 stats + sub-effect) or Mythical at +25 (Mythic effect unlocked).
            const bool myth = gear >= 2;
            const ItemRarity r = myth ? ItemRarity::MYTHICAL : ItemRarity::LEGENDARY;
            const int cls = static_cast<int>(p.characterClass);
            auto w = Item::make(myth ? LootSystem::kMythicalWeapons[cls].name : LootSystem::kLegendaryWeapons[cls].name,
                                ItemType::WEAPON, p.equippedWeapon->bonus + (myth ? 3 : 0), r, cls);
            auto a = Item::make(myth ? LootSystem::kMythicalArmor[cls].name : LootSystem::kLegendaryArmor[cls].name,
                                ItemType::ARMOR, p.equippedArmor->bonus + (myth ? 3 : 0), r, cls);
            w->upgradeLevel = a->upgradeLevel = myth ? 25 : 20;
            if (gear == 2) { w->mythicEffect = static_cast<int>(MythicEffect::EMBER_ECHO); a->mythicEffect = static_cast<int>(MythicEffect::REKINDLE); }
            if (gear == 3) { w->mythicEffect = static_cast<int>(MythicEffect::CINDER_BRAND); a->mythicEffect = static_cast<int>(MythicEffect::HEARTH_AURA); }
            p.equippedWeapon = w; p.equippedArmor = a;
        }
        p.bonusMaxHp = 0; p.bonusMaxResources = 0;
        if (perksOn) giveLoadout(p);
        p.statRules = true; // the stat rules are on in every column here (v2.31+)
        p.applyStatsForLevel();
        p.calculateAC();
        p.currentHp = p.maxHp; p.resources = p.maxResources;
        p.isDowned = p.isDead = p.isStable = false;
        p.aiControlled = true;
    }
}

static Res run(const Scn& s, Rules rules, int n, CharacterClass a = CharacterClass::FIGHTER,
               CharacterClass b = CharacterClass::WIZARD) {
    curve::legacyReferenceForSim() = false;
    perks::legacyLevelRulesForSim() = rules == Rules::V231;
    Res r;
    double hpSum = 0, maxSum = 0;
    for (int i = 0; i < n; ++i) {
        Game g;
        g.seedForTesting(2900u + static_cast<unsigned>(i) * 7919u);
        g.startNewGame(a, "Mira", static_cast<int>(SoloPlayMode::CRAWL), s.diff, b, true);
        g.setAsHost(true);
        g.setStatRulesEnabledForTesting(true);
        T::setMode(g, s.mode, s.depth, 6);
        if (s.debugJump) {
            g.syncStatRules();
            T::clearRoomAt(g, 6); // out of the opening fight, as the shortcut needs
            for (size_t k = 0; k < T::heroes(g); ++k) T::hero(g, k).aiControlled = true;
            if (!g.debugJumpToRekindleReady()) std::printf("(debug shortcut refused)\n");
        } else if (s.raw) buildRaw(g, s.level); else buildParty(g, s.level, s.extraGear, rules == Rules::V232, s.rank, s.gear);
        g.simBeginFight(s.foes, s.surprise);
        if (!g.getEnemies().empty()) r.el = g.getEnemies().front()->curveEL;
        int steps = 0;
        while (!g.getEnemies().empty() && !g.isGameOver() && steps < 20000) { g.processTurn(); ++steps; }
        const bool won = g.getEnemies().empty() && !g.isGameOver();
        if (won) r.win += 1;
        r.rounds += g.getCombatRound();
        for (const auto& p : g.getPlayers()) { hpSum += std::max(0, p->currentHp); maxSum += p->maxHp; }
    }
    perks::legacyLevelRulesForSim() = false;
    r.win = 100.0 * r.win / n;
    r.rounds /= n;
    r.lost = maxSum > 0 ? 100.0 * (1.0 - hpSum / maxSum) : 0;
    return r;
}

int main(int argc, char** argv) {
    const int n = argc > 1 ? std::atoi(argv[1]) : 600;
    const int raid = static_cast<int>(SoloPlayMode::RAID);
    const std::vector<std::string> room = {"enemy_wickthief", "enemy_ashbound", "enemy_tithe_watcher"};
    const std::vector<std::string> boss = {"boss_sexton", "enemy_ashbound"};
    const std::vector<std::string> trio = {"enemy_gloamhound", "enemy_gloamhound", "enemy_crier_wick"};
    struct Col { std::string name; std::vector<std::string> foes; int diff; int mode; };
    const std::vector<Col> cols = {
        {"Medium room", room, 1, static_cast<int>(SoloPlayMode::CRAWL)},
        {"Boss + escort (Sexton + Ashbound)", boss, 1, static_cast<int>(SoloPlayMode::CRAWL)},
        {"Nightmare (2 hounds + crier)", trio, 3, static_cast<int>(SoloPlayMode::CRAWL)},
        {"Raid: Hollow Crown", {"boss_hollow_crown"}, 1, raid},
        {"Raid: Ember Hydra", {"boss_ember_hydra"}, 1, raid},
        {"Raid: Nightfang Matriarch", {"boss_nightfang"}, 1, raid},
    };
    // v2.34 rebalance: every difficulty, levels 1-50, a built hero (model stats + gear + perks) and the
    // under-geared, unspent one (what the debug shortcut used to make). Arg 2 "grid" prints only this.
    const bool mythicOnly = argc > 2 && std::string(argv[2]) == "mythic";
    if (!mythicOnly) {
        static const char* kDiffName[] = {"Easy", "Medium", "Hard", "Nightmare"};
        std::printf("| Difficulty | Level | room: built | room: unspent | boss + escort: built | boss + escort: unspent |\n|---|---|---|---|---|---|\n");
        for (int d = 0; d < 4; ++d) {
            for (int L : {1, 10, 20, 30, 40, 50}) {
                std::printf("| %s | %d |", kDiffName[d], L);
                for (const auto* foes : {&room, &boss}) {
                    for (bool raw : {false, true}) {
                        Scn s; s.level = L; s.diff = d; s.foes = *foes; s.raw = raw;
                        const Res p = run(s, Rules::V232, n);
                        std::printf(" %.0f%% · %.0f%% · %.1f r · EL %d |", p.win, p.lost, p.rounds, p.el);
                    }
                }
                std::printf("\n");
            }
        }
        std::printf("\n| Debug shortcut (fresh hero → level 50) | room | boss + escort |\n|---|---|---|\n");
        for (int d = 0; d < 4; ++d) {
            std::printf("| %s |", kDiffName[d]);
            for (const auto* foes : {&room, &boss}) {
                Scn s; s.level = 50; s.diff = d; s.foes = *foes; s.debugJump = true;
                const Res p = run(s, Rules::V232, n);
                std::printf(" %.0f%% · %.0f%% · %.1f r · EL %d |", p.win, p.lost, p.rounds, p.el);
            }
            std::printf("\n");
        }
        std::printf("\n");
        if (argc > 2 && std::string(argv[2]) == "grid") return 0;
    }
    // v2.40 Mythical: level 50, Rekindled I / V / X, the same seeds with Legendary +20 gear and two
    // Mythical +25 loadouts. Arg 2 "mythic" prints only this.
    {
        static const char* kDiffName[] = {"Easy", "Medium", "Hard", "Nightmare"};
        const int vigil = static_cast<int>(SoloPlayMode::VIGIL);
        struct M { std::string name; std::vector<std::string> foes; int diff; int mode; };
        const std::vector<std::string> vc1 = {"enemy_ashbound_bonebound", "enemy_tithe_enforcer", "enemy_gloamhound", "enemy_crier_bone"};
        const std::vector<std::string> vc2 = {"enemy_brute", "enemy_tithe_enforcer"};
        std::vector<M> ms;
        for (int d : {1, 2, 3}) ms.push_back({std::string("Boss + escort (") + kDiffName[d] + ")", boss, d, static_cast<int>(SoloPlayMode::CRAWL)});
        ms.push_back({"Raid: Hollow Crown (Medium)", {"boss_hollow_crown"}, 1, raid});
        ms.push_back({"Raid: Ember Hydra (Hard)", {"boss_ember_hydra"}, 2, raid});
        for (int d : {1, 2, 3}) {
            ms.push_back({std::string("Vigil 1/3 pack (") + kDiffName[d] + ")", vc1, d, vigil});
            ms.push_back({std::string("Vigil 2/3 pair (") + kDiffName[d] + ")", vc2, d, vigil});
            ms.push_back({std::string("Vigil 3/3 Drowned Nightfang + 2 (") + kDiffName[d] + ")",
                          {"boss_nightfang", "enemy_ashbound_bonebound", "enemy_crier_bone"}, d, vigil});
        }
        std::printf("v2.40 Mythical, L50 Fighter + Wizard, %d fights per cell: win %% · HP lost %% · EL. "
                    "Gear: Legendary +20 → **Mythical +25 (Echo + Rekindle)** → Mythical +25 (Brand + Hearth Aura).\n\n", n);
        std::printf("| Scenario | rank I | rank V | rank X |\n|---|---|---|---|\n");
        for (const auto& m : ms) {
            std::printf("| %s |", m.name.c_str());
            for (int rank : {1, 5, 10}) {
                Res r[3];
                for (int gi = 0; gi < 3; ++gi) {
                    Scn s; s.level = 50; s.diff = m.diff; s.foes = m.foes; s.mode = m.mode; s.rank = rank; s.gear = gi + 1;
                    r[gi] = run(s, Rules::V232, n);
                }
                std::printf(" %.0f%% → **%.0f%%** → %.0f%% · %.0f/%.0f/%.0f%% · EL %d/%d/%d |", r[0].win, r[1].win, r[2].win,
                            r[0].lost, r[1].lost, r[2].lost, r[0].el, r[1].el, r[2].el);
            }
            std::printf("\n");
        }
        std::printf("\n");
        if (argc > 2 && std::string(argv[2]) == "mythic") return 0;
    }
    std::printf("Fighter + Wizard party, %d fights per cell. Each cell: v2.31 rules → v2.32 without perks → **v2.32 with the sim loadout** (win %%, HP lost %%, rounds).\n\n", n);
    std::printf("| Level | Scenario | EL | v2.31 | v2.32, no perks | v2.32 + perks |\n|---|---|---|---|---|---|\n");
    for (int L : {20, 30, 40, 50}) {
        for (const auto& c : cols) {
            Scn s; s.level = L; s.diff = c.diff; s.foes = c.foes; s.mode = c.mode;
            const Res a = run(s, Rules::V231, n), b = run(s, Rules::V232_BARE, n), p = run(s, Rules::V232, n);
            std::printf("| %d | %s | %d | %.0f%% · %.0f%% · %.1f | %.0f%% · %.0f%% · %.1f | **%.0f%%** · %.0f%% · %.1f |\n",
                        L, c.name.c_str(), p.el, a.win, a.lost, a.rounds, b.win, b.lost, b.rounds, p.win, p.lost, p.rounds);
        }
    }
    // Other classes, boss + escort, to show every class's Arts pull their weight.
    std::printf("\n| Party (boss + escort, Medium) | Level | v2.31 | v2.32 + perks |\n|---|---|---|---|\n");
    const std::pair<CharacterClass, CharacterClass> parties[] = {
        {CharacterClass::ROGUE, CharacterClass::CLERIC}, {CharacterClass::BARD, CharacterClass::FIGHTER},
        {CharacterClass::CLERIC, CharacterClass::WIZARD}};
    const char* pname[] = {"Rogue + Cleric", "Bard + Fighter", "Cleric + Wizard"};
    for (int pi = 0; pi < 3; ++pi) {
        for (int L : {30, 50}) {
            Scn s; s.level = L; s.diff = 1; s.foes = boss;
            const Res a = run(s, Rules::V231, n, parties[pi].first, parties[pi].second);
            const Res p = run(s, Rules::V232, n, parties[pi].first, parties[pi].second);
            std::printf("| %s | %d | %.0f%% · %.0f%% · %.1f | **%.0f%%** · %.0f%% · %.1f |\n", pname[pi], L,
                        a.win, a.lost, a.rounds, p.win, p.lost, p.rounds);
        }
    }
    // Below the cap nothing changed: the v2.31 checkpoints, same seeds.
    std::printf("\n| Level 1-20 check (Medium room) | v2.31 | v2.32 |\n|---|---|---|\n");
    for (int L : {1, 5, 10, 15, 20}) {
        Scn s; s.level = L; s.diff = 1; s.foes = room;
        const Res a = run(s, Rules::V231, n), b = run(s, Rules::V232, n);
        std::printf("| L%d | %.0f%% · %.0f%% | %.0f%% · %.0f%% |\n", L, a.win, a.lost, b.win, b.lost);
    }
    // v2.33 Rekindling: level 50 at ranks 0 / I / V / X (perks spent, the extra Embers and slots used).
    std::printf("\n| L50, v2.32 + perks | Scenario | rank 0 | I | V | X |\n|---|---|---|---|---|---|\n");
    for (const auto& c : cols) {
        std::printf("| 50 | %s |", c.name.c_str());
        for (int rank : {0, 1, 5, 10}) {
            Scn s; s.level = 50; s.diff = c.diff; s.foes = c.foes; s.mode = c.mode; s.rank = rank;
            const Res p = run(s, Rules::V232, n);
            std::printf(" %.0f%% · %.0f%% · EL %d |", p.win, p.lost, p.el);
        }
        std::printf("\n");
    }
    // The climb after a Rekindling: level-50 gear (attuned to floor(level / 2) + 3), the rank's bonuses.
    std::printf("\n| Climb after Rekindling (L50 gear, attuned to floor(level / 2) + 3) | Level | first climb (rank 0, normal gear) | I | V | X |\n|---|---|---|---|---|---|\n");
    for (const auto& c : {cols[0], cols[1], cols[2]}) {
        for (int L : {1, 10, 20, 30, 40}) {
            Scn s0; s0.level = L; s0.diff = c.diff; s0.foes = c.foes; s0.mode = c.mode;
            const Res base = run(s0, Rules::V232, n);
            std::printf("| %s | %d | %.0f%% · %.0f%% |", c.name.c_str(), L, base.win, base.lost);
            for (int rank : {1, 5, 10}) {
                Scn s = s0; s.rank = rank;
                s.extraGear = curve::makeReferenceHero(CharacterClass::FIGHTER, 50).equippedWeapon->bonus
                    - curve::makeReferenceHero(CharacterClass::FIGHTER, L).equippedWeapon->bonus;
                const Res p = run(s, Rules::V232, n);
                std::printf(" %.0f%% · %.0f%% · EL %d |", p.win, p.lost, p.el);
            }
            std::printf("\n");
        }
    }
    // Rooms (cleared, flawless pressure ignored) to climb 1 -> 50 again, the room counter carrying on.
    std::printf("\n| Rooms to level 50 | first climb (from room 1) | rank I (from room 60) | IV | X |\n|---|---|---|---|---|\n");
    {
        auto climb = [](int rank, int startRoom) {
            Game g;
            g.seedForTesting(77);
            g.startNewGame(CharacterClass::FIGHTER, "Mira", static_cast<int>(SoloPlayMode::CRAWL), 1, CharacterClass::CLERIC, true);
            g.setAsHost(true);
            g.setStatRulesEnabledForTesting(true);
            g.syncStatRules();
            T::story(g);
            for (int r = 0; r < rank; ++r) { g.debugJumpToRekindleReady(false); T::clearRoomAt(g, 0); g.rekindle(); }
            int rooms = 0, room = startRoom, to20 = 0;
            while (T::hero(g, 0).level < 50 && rooms < 5000) {
                T::clearRoomAt(g, room++);
                ++rooms;
                if (!to20 && T::hero(g, 0).level >= 20) to20 = rooms;
            }
            return std::make_pair(to20, rooms);
        };
        const auto a = climb(0, 1), b = climb(1, 60), c = climb(4, 60), d = climb(10, 60);
        std::printf("| to 20 / to 50 | %d / %d | %d / %d | %d / %d | %d / %d |\n", a.first, a.second, b.first, b.second,
                    c.first, c.second, d.first, d.second);
    }
    return 0;
}
