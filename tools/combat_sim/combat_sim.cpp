// Headless Monte Carlo of the v2.29 combat curve, built from the SHIPPED code: heroes are real
// Characters (GameLogic.h: HP formula + level-1 floor, AC, to-hit, damage), foes come from
// CombatCurve.h (Reference Lamplighter, roles, budgets), and every swing goes through the same
// hit clamp / overkill guard / crit softening the game uses. Basic attacks only (no specials,
// potions or the companion's heals), heroes go down at 0 HP (no death saves): a conservative
// floor — real fights come out easier. Run tools/combat_sim/run.sh.
#include "CombatCurve.h"
#include <cstdio>
#include <string>
#include <vector>
std::ostream aout(std::cout.rdbuf());
using namespace dnd;

struct Unit {
    // hero
    const Character* hero = nullptr;
    // foe
    curve::FoeStats foe;
    int el = 1;
    bool isFoe = false;
    int warn = 0;
    int hp = 1, max = 1, ac = 10;
};

struct Result { double win = 0, rounds = 0, lost = 0; };

static Unit heroUnit(const Character& c) {
    Unit u; u.hero = &c; u.hp = u.max = c.maxHp; u.ac = c.armorClass; return u;
}
static int gAccEl = 0; // accuracy EL for mode-bonus scenarios (0 = same as el)
static Unit foeUnit(curve::Role r, int el, int diff, int party, double scale, int warn = 0) {
    Unit u; u.isFoe = true; u.el = el; u.warn = warn;
    u.foe = curve::foeStats(r, el, diff, party, scale, gAccEl);
    u.hp = u.max = u.foe.maxHp; u.ac = u.foe.ac; return u;
}

static Result run(const std::vector<Character>& party, const std::vector<Unit>& foesIn, int n = 4000) {
    Result r;
    for (int i = 0; i < n; ++i) {
        std::vector<Unit> P; for (const auto& c : party) P.push_back(heroUnit(c));
        std::vector<Unit> F = foesIn;
        int rnd = 1; bool won = false, lost = false;
        for (; rnd < 80; ++rnd) {
            for (auto& p : P) {
                if (p.hp <= 0) continue;
                Unit* t = nullptr;
                for (auto& f : F) if (f.hp > 0 && (!t || f.hp < t->hp)) t = &f;
                if (!t) break;
                const RollResult roll = CombatSystem::performAttackRoll(*p.hero);
                const auto hc = curve::attackHits(roll.dieRoll, roll.total - roll.dieRoll, t->ac);
                if (hc.hit) t->hp -= CombatSystem::calculateDamage(*p.hero, roll.isCriticalHit);
            }
            bool anyFoe = false; for (auto& f : F) if (f.hp > 0) anyFoe = true;
            if (!anyFoe) { won = true; break; }
            for (auto& f : F) {
                if (f.hp <= 0) continue;
                if (f.warn > 0) { f.warn--; continue; } // telegraph turn
                Unit* t = nullptr;
                for (auto& p : P) if (p.hp > 0 && (!t || (p.max - p.hp) > (t->max - t->hp))) t = &p;
                if (!t) break;
                const int die = rng::below(20) + 1;
                const auto hc = curve::attackHits(die, f.foe.toHit, t->ac);
                if (hc.hit) {
                    int d = curve::rollFoeDamage(f.foe.avgDmgX10, f.el, die == 20, false);
                    t->hp -= curve::overkillCap(d, t->max);
                }
            }
            bool anyHero = false; for (auto& p : P) if (p.hp > 0) anyHero = true;
            if (!anyHero) { lost = true; break; }
        }
        if (won && !lost) r.win += 1;
        r.rounds += rnd;
        double tot = 0, gone = 0;
        for (auto& p : P) { tot += p.max; gone += p.max - std::max(0, p.hp); }
        r.lost += gone / tot;
    }
    r.win = 100 * r.win / n; r.rounds /= n; r.lost = 100 * r.lost / n;
    return r;
}

static std::vector<Character> refParty(int L) {
    return {curve::makeReferenceHero(CharacterClass::FIGHTER, L), curve::makeReferenceHero(CharacterClass::WIZARD, L)};
}

static int partyEl(const std::vector<Character>& party, int modeBonus) {
    std::vector<curve::PartyMember> m;
    for (const auto& c : party) m.push_back(curve::memberOf(c));
    return curve::encounterLevel(m, modeBonus);
}

static std::vector<Unit> room(int el, int diff, int party = 2) {
    using R = curve::Role;
    std::vector<std::pair<R, double>> comp = {{R::BRUTE, 1.5}, {R::SKIRMISHER, 1.0}};
    if (diff >= 3) comp.push_back({R::MINION, 0.5});
    double cost = 0; for (auto& c : comp) cost += c.second;
    const double s = curve::fitScale(curve::roomBudget(party, diff, curve::kRoomNormal), cost);
    std::vector<Unit> out; for (auto& c : comp) out.push_back(foeUnit(c.first, el, diff, party, s));
    return out;
}
static std::vector<Unit> bossRoom(int el, int diff, int party = 2) {
    const double budget = curve::roomBudget(party, diff, curve::kRoomNormal);
    const double addS = std::clamp(curve::kBossAddsShare * budget / 0.5, 0.4, 1.0);
    return {foeUnit(curve::Role::BOSS, el, diff, party, 1.0), foeUnit(curve::Role::MINION, el, diff, party, addS)};
}

int main() {
    rng::engine().seed(29);
    std::printf("| Level | Ref hero to-hit / AC / HP / dmg | Standard foe AC / to-hit / HP / dmg | You hit / foe hits | Medium room: win, HP lost | Medium boss: win, HP lost | Hard room win | Nightmare room (3 foes) win |\n");
    std::printf("|---|---|---|---|---|---|---|---|\n");
    for (int L : {1, 3, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50}) {
        const auto party = refParty(L);
        const int el = partyEl(party, 0);
        const auto& ref = curve::reference(el);
        const auto s = curve::foeStats(curve::Role::SKIRMISHER, el, 1, 2);
        const Character& f = party[0];
        const int fAtk = CombatSystem::getPrimaryModifier(f) + CombatSystem::proficiencyBonusForLevel(L) + f.equippedWeapon->bonus;
        const Result m = run(party, room(el, 1));
        const Result b = run(party, bossRoom(el, 1));
        const Result h = run(party, room(el, 2));
        const Result nm = run(party, room(el, 3));
        std::printf("| %d | +%.0f / %.0f / %.0f / %.0f | %d / +%d / %d / %.0f | %.0f%% / %.0f%% | %.0f%%, %.0f%% | %.0f%%, %.0f%% | %.0f%% | %.0f%% |\n",
                    L, ref.atk, ref.ac, ref.hp, ref.dmg, s.ac, s.toHit, s.maxHp, s.avgDmgX10 / 10.0,
                    100 * curve::hitChance(fAtk, s.ac), 100 * curve::hitChance(s.toHit, f.armorClass),
                    m.win, m.lost, b.win, b.lost, h.win, nm.win);
    }

    std::printf("\n| Scenario | EL | Win | Rounds | Party HP lost |\n|---|---|---|---|---|\n");
    auto line = [](const char* name, int el, const Result& r) {
        std::printf("| %s | %d | %.1f%% | %.1f | %.0f%% |\n", name, el, r.win, r.rounds, r.lost);
    };
    {   // Bone Gallery surprise: Ossuary Prior as Elite at 1.5x the Medium room budget, 1-turn warning.
        const auto party = refParty(2); const int el = partyEl(party, 0);
        const double s = curve::kSurpriseBudgetCap * curve::roomBudget(2, 1, 1.0) / curve::roleInfo(curve::Role::ELITE).cost;
        line("Act 1 Bone Gallery, L2 vs Ossuary Prior surprise (Elite, 1.5x cap, warning) — was 65%", el,
             run(party, {foeUnit(curve::Role::ELITE, el, 1, 2, s, 1)}));
        line("Act 1 Bone Gallery, L2 vs 2 Ashbound (normal room)", el,
             run(party, [&] { double sc = curve::fitScale(2.4, 3.0); return std::vector<Unit>{foeUnit(curve::Role::BRUTE, el, 1, 2, sc), foeUnit(curve::Role::BRUTE, el, 1, 2, sc)}; }()));
        const double hs = curve::kSurpriseBudgetCap * curve::roomBudget(2, 2, 1.0) / 2.5;
        line("Same surprise on Hard", el, run(party, {foeUnit(curve::Role::ELITE, el, 2, 2, hs, 1)}));
    }
    {   // Story bosses as the Boss role.
        const auto p3 = refParty(3); const int e3 = partyEl(p3, 0);
        line("Act 1 Lantern Sexton, L3 (Boss role)", e3, run(p3, {foeUnit(curve::Role::BOSS, e3, 1, 2, 1.0)}));
        const auto p9 = refParty(9); const int e9 = partyEl(p9, 0);
        line("Act 4 Bell-Herald, L9 (Boss role) — was 100% / 4% lost", e9, run(p9, {foeUnit(curve::Role::BOSS, e9, 1, 2, 1.0)}));
    }
    {   // Endgame: Raid +2 EL.
        const auto p12 = refParty(12); const int e12 = partyEl(p12, 2);
        gAccEl = partyEl(p12, 0);
        line("Boss Raid Ember Hydra, L12 (+2 EL) — was 100% / 4% lost", e12, run(p12, bossRoom(e12, 1)));
        auto over = refParty(30);
        for (auto& c : over) {
            c.equippedWeapon->bonus = 20;
            c.equippedArmor->bonus = curve::starterArmorBase(c.characterClass) + 20;
            c.calculateAC();
        }
        const int eo = partyEl(over, 2);
        gAccEl = partyEl(over, 0);
        line("Boss Raid, L30 with +20/+20 gear (gear followed 50%) — was 100% / 0% lost", eo, run(over, bossRoom(eo, 1)));
        auto g20 = refParty(20);
        for (auto& c : g20) {
            c.equippedWeapon->bonus = 20;
            c.equippedArmor->bonus = curve::starterArmorBase(c.characterClass) + 20;
            c.calculateAC();
        }
        const int eg = partyEl(g20, 0);
        gAccEl = 0;
        line("L20 with +20/+20 gear vs Medium boss", eg, run(g20, bossRoom(eg, 1)));
        line("Same party if gear were NOT followed (EL 20)", 20, run(g20, bossRoom(20, 1)));
        const auto p14 = refParty(14); const int ed = partyEl(p14, 14 / 5);
        gAccEl = partyEl(p14, 0);
        line("Ashen Deep depth 14, L14 (+2 EL), 6 foes trimmed to 4 and fitted", ed,
             run(p14, [&] {
                 using R = curve::Role; const double sc = curve::fitScale(2.4, 1.0 + 1.0 + 1.0 + 1.5);
                 return std::vector<Unit>{foeUnit(R::SKIRMISHER, ed, 1, 2, sc), foeUnit(R::SKIRMISHER, ed, 1, 2, sc),
                                          foeUnit(R::SKIRMISHER, ed, 1, 2, sc), foeUnit(R::BRUTE, ed, 1, 2, sc)}; }()));
        const auto p50 = refParty(50); const int e50 = partyEl(p50, 2);
        gAccEl = partyEl(p50, 0);
        line("Boss Raid at level 50 (+2 EL)", e50, run(p50, bossRoom(e50, 1)));
    }
    gAccEl = 0;
    {   // Comeback / pressure on a Medium L10 room.
        const auto p = refParty(10); const int el = partyEl(p, 0);
        auto fitted = [&](double mod) {
            const double sc = curve::fitScale(2.4 * mod, 2.5);
            return std::vector<Unit>{foeUnit(curve::Role::BRUTE, el, 1, 2, sc), foeUnit(curve::Role::SKIRMISHER, el, 1, 2, sc)};
        };
        line("L10 Medium room with Rekindle (-20%)", el, run(p, fitted(curve::kRekindleBudget)));
        line("L10 Medium room at full Pressure (+20%)", el, run(p, fitted(curve::pressureMultiplier(4))));
    }
    return 0;
}
