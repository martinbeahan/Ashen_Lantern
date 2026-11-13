// v2.30 enemy AI (PR 2 of the combat/progression revamp): roles + behaviours for every enemy
// ID, AI tiers, target scoring (healers / wounded), casters cast, supports heal / buff, flee
// rules, Nightmare's telegraphed downed-hero strike, 10 boss phases at 66% / 33%, the
// telegraphed-heavy exemption from the 45% guard, Slam stagger, statuses, the story +1 resource
// fix, save round trips (v2.29 mid-fight saves load), and online unchanged. Seeded RNG.
// Run tools/migration_test/run.sh.
#include "Game.h"
#include "CombatCurve.h"
#include "EnemyAi.h"
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
std::ostream aout(std::cout.rdbuf());
using namespace dnd;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n"; ++failures; } } while (0)
static bool contains(const std::string& s, const std::string& sub) { return s.find(sub) != std::string::npos; }

namespace dnd {
struct GameTestAccess {
    static Character& foe(Game& g, size_t i) { return *g.enemies_[i]; }
    static Character& hero(Game& g, size_t i) { return *g.players_[i]; }
    static void plan(Game& g, size_t i) { g.planEnemyMove(*g.enemies_[i]); }
    static void run(Game& g, size_t i) { g.runEnemyMove(*g.enemies_[i], static_cast<int>(i)); }
    static void damage(Game& g, size_t i, int dmg, size_t by, bool crit = false) {
        Character& f = *g.enemies_[i];
        f.takeDamage(dmg);
        g.noteFoeDamaged(f, dmg, g.players_[by].get(), crit);
    }
    static int fled(const Game& g) { return g.fledFoes_; }
    static int bannerSeq(const Game& g) { return g.bannerSeq_; }
    static void endTurn(Game& g, Character* c) { g.endTurnStatuses(c); }
    static int pick(Game& g, size_t i) { return g.pickAiHeroTarget(*g.enemies_[i], bestiary::behaviour(g.enemies_[i]->enemyId)); }
};
} // namespace dnd
using T = GameTestAccess;

static ai::Move moveOf(Game& g, size_t i) { return ai::moveFromInt(T::foe(g, i).ai.move); }

/** Crawl party (hero + companion), difficulty d, seeded; foes replaced by ids. */
static void setup(Game& g, int difficulty, const std::vector<std::string>& ids, unsigned seed,
                  CharacterClass hero = CharacterClass::FIGHTER, CharacterClass comp = CharacterClass::CLERIC) {
    g.seedForTesting(seed);
    g.startNewGame(hero, "Mira", 1, difficulty, comp, true);
    g.setAsHost(true);
    g.simBeginFight(ids);
}

static int partyGains(const Game& g) {
    const std::string c = g.getChatHistory();
    const size_t p = c.rfind("The party gains ");
    if (p == std::string::npos) return -1;
    return std::atoi(c.c_str() + p + 16);
}

int main() {
    // ------------------------------------------------------------ every enemy ID has a role + behaviour
    {
        size_t n = 0;
        const bestiary::EnemyArchetype* t = bestiary::table(n);
        int roles[8] = {0};
        for (size_t i = 0; i < n; ++i) {
            const std::string id = t[i].id;
            CHECK(t[i].role >= 1 && t[i].role <= 7);
            roles[t[i].role]++;
            CHECK(t[i].behaviour >= bestiary::BEH_STRIKE && t[i].behaviour <= bestiary::BEH_BOSS);
            const bool boss = id.rfind("boss_", 0) == 0;
            CHECK(boss == (t[i].behaviour == bestiary::BEH_BOSS));
            CHECK(boss == (t[i].role == 7));
            const bool fleer = id.rfind("enemy_wickthief", 0) == 0 || id == "enemy_gloamhound";
            CHECK(bestiary::canFlee(id) == fleer);
            if (t[i].undead || boss) CHECK(!bestiary::canFlee(id));
        }
        CHECK(n == 27);
        CHECK(roles[1] && roles[2] && roles[3] && roles[4] && roles[5] && roles[7]); // Elite = surprise bosses
        CHECK(ai::tierFor(0) == ai::Tier::SLOPPY && ai::tierFor(3) == ai::Tier::RUTHLESS);
        std::cout << "ROLES: 27 enemy IDs -> role + behaviour; only wickthieves / hounds may flee\n";
    }

    // ------------------------------------------------------------ target scoring
    {
        ai::HeroView fighter; fighter.hp = 30; fighter.maxHp = 30; fighter.ac = 16;
        ai::HeroView cleric = fighter; cleric.healer = true; cleric.ac = 16;
        ai::HeroView hurt = fighter; hurt.hp = 8;
        std::vector<double> flat(3, 0.5);
        CHECK(ai::pickTarget({fighter, cleric}, ai::Tier::TACTICAL, 0, flat, 0.9) == 1); // healer focus
        CHECK(ai::scoreTarget(cleric, ai::Tier::SLOPPY, 0) == ai::scoreTarget(fighter, ai::Tier::SLOPPY, 0)); // never on Easy
        CHECK(ai::pickTarget({fighter, hurt}, ai::Tier::COORDINATED, 0, flat, 0.9) == 1); // wounded
        ai::HeroView down = fighter; down.downed = true; down.hp = 0;
        CHECK(ai::pickTarget({down, fighter}, ai::Tier::RUTHLESS, 0, flat, 0.9) == 1); // standing first
        CHECK(ai::pickTarget({down}, ai::Tier::RUTHLESS, 0, flat, 0.9) == 0);
        ai::HeroView lowAc = fighter; lowAc.ac = 11;
        CHECK(ai::pickTarget({fighter, lowAc}, ai::Tier::TACTICAL, bestiary::BEH_BACKSTAB, flat, 0.9) == 1);
        CHECK(ai::targetNoise(ai::Tier::SLOPPY) == 0.30 && ai::targetNoise(ai::Tier::TACTICAL) == 0.15
              && ai::targetNoise(ai::Tier::COORDINATED) == 0.0);
        // In a real fight on Medium, a skirmisher goes for the Cleric companion over the full-HP Fighter.
        int clericPicks = 0;
        for (unsigned s = 1; s <= 40; ++s) {
            Game g; setup(g, 1, {"enemy_wickthief"}, s);
            if (T::pick(g, 0) == 1) ++clericPicks;
        }
        CHECK(clericPicks >= 30);
        std::cout << "TARGETING: healers (" << clericPicks << "/40) and wounded heroes first; Easy is noisy\n";
    }

    // ------------------------------------------------------------ intents on the chips
    {
        Game g; setup(g, 1, {"enemy_wickthief", "enemy_tithe_watcher"}, 7, CharacterClass::WIZARD);
        CHECK(g.enemyAiActive());
        for (size_t i = 0; i < g.getEnemies().size(); ++i) CHECK(T::foe(g, i).ai.move != 0);
        const std::string s = g.getEnemyIntents();
        // Wizard INT 16 -> numbers shown.
        CHECK(s.rfind("1|", 0) == 0);
        const std::string entries = s.substr(s.find('|', s.find('|', s.find('|') + 1) + 1) + 1);
        CHECK(std::count(entries.begin(), entries.end(), ';') == 1);
        CHECK(contains(entries, "Snatch") && (contains(entries, "Cinder bolt") || contains(entries, "Mark")));
        Game f; setup(f, 1, {"enemy_wickthief"}, 7, CharacterClass::FIGHTER, CharacterClass::FIGHTER);
        CHECK(f.getEnemyIntents().rfind("0|", 0) == 0); // INT 8 -> icons only
        std::cout << "INTENTS: every foe shows a planned move; numbers only with INT 14+\n";
    }

    // ------------------------------------------------------------ casters cast (incl. the Ember Hydra)
    {
        Game g; setup(g, 1, {"enemy_tithe_watcher"}, 3);
        bool bolt = false, mark = false;
        for (int k = 0; k < 8; ++k) {
            const ai::Move m = moveOf(g, 0);
            if (m == ai::Move::CINDER_BOLT) bolt = true;
            if (m == ai::Move::MARK) mark = true;
            T::run(g, 0);
            if (g.getEnemies().empty() || g.isGameOver()) break;
            for (auto& p : g.getPlayers()) const_cast<Character&>(*p).currentHp = p->maxHp;
        }
        CHECK(bolt && mark);
        CHECK(contains(g.getChatHistory(), "Cinder bolt on") && contains(g.getChatHistory(), "(no roll)"));
        Game h; setup(h, 1, {"boss_ember_hydra"}, 4);
        CHECK(moveOf(h, 0) == ai::Move::HYDRA_FIRE);
        T::run(h, 0);
        CHECK(contains(h.getChatHistory(), "Ember spit on"));
        std::cout << "CASTERS: the Watcher marks and casts Cinder bolt; the Ember Hydra spits fire\n";
    }

    // ------------------------------------------------------------ supports heal / buff
    {
        Game g; setup(g, 1, {"enemy_crier_wick", "enemy_brute"}, 5);
        T::foe(g, 0).ai.cooldown = 0;
        T::plan(g, 0);
        for (int k = 0; k < 6 && moveOf(g, 0) != ai::Move::RALLY; ++k) { T::foe(g, 0).ai.cooldown = 0; T::plan(g, 0); }
        CHECK(moveOf(g, 0) == ai::Move::RALLY);
        T::run(g, 0);
        CHECK(T::foe(g, 1).status.inspired == 2);
        Game b; setup(b, 1, {"enemy_crier_bone", "enemy_ashbound"}, 6);
        Character& ash = T::foe(b, 1);
        ash.currentHp = ash.maxHp / 4;
        T::foe(b, 0).ai.cooldown = 0;
        T::plan(b, 0);
        CHECK(moveOf(b, 0) == ai::Move::KNIT);
        const int before = ash.currentHp;
        T::run(b, 0);
        CHECK(ash.currentHp > before);
        std::cout << "SUPPORTS: Rally cry (+2 to hit) and Knit bones (heals undead 20%)\n";
    }

    // ------------------------------------------------------------ flee rules + half XP
    {
        int xpFled = -1, xpKilled = -1;
        for (int pass = 0; pass < 2; ++pass) {
            Game g; setup(g, 1, {"enemy_wickthief"}, 11);
            Character& w = T::foe(g, 0);
            Character& hero = T::hero(g, 0);
            hero.gold = 200;
            w.ai.stolenGold = 10;
            hero.gold -= 10;
            if (pass == 0) {
                w.currentHp = std::max(1, w.maxHp / 5);
                T::plan(g, 0);
                CHECK(moveOf(g, 0) == ai::Move::SLIP_AWAY);
                CHECK(contains(g.getChatHistory(), "it will slip away next turn"));
                T::run(g, 0);
                CHECK(g.getEnemies().empty());
                CHECK(hero.gold == 190); // the stolen gold left with it
                xpFled = partyGains(g);
            } else {
                w.currentHp = 0;
                g.playerAttack(0); // resolves the 0-HP foe through the normal kill path
                if (!g.getEnemies().empty()) { const_cast<Character&>(*g.getEnemies().front()).currentHp = 0; g.processTurn(); }
                CHECK(hero.gold >= 200); // stolen gold returned (+ kill gold)
                xpKilled = partyGains(g);
            }
        }
        CHECK(xpFled > 0 && xpKilled > 0 && xpFled * 2 <= xpKilled + 1);
        // Easy never flees; undead and bosses never flee.
        Game e; setup(e, 0, {"enemy_wickthief"}, 12);
        T::foe(e, 0).currentHp = 1; T::plan(e, 0);
        CHECK(moveOf(e, 0) != ai::Move::SLIP_AWAY);
        Game u; setup(u, 3, {"enemy_ashbound", "boss_wick_king"}, 13);
        T::foe(u, 0).currentHp = 1; T::plan(u, 0);
        T::foe(u, 1).currentHp = 1; T::plan(u, 1);
        CHECK(moveOf(u, 0) != ai::Move::SLIP_AWAY && moveOf(u, 1) != ai::Move::SLIP_AWAY);
        std::cout << "FLEE: a Wickthief at <25% telegraphs, slips away with its gold, half XP ("
                  << xpFled << " vs " << xpKilled << "); never on Easy, never undead or bosses\n";
    }

    // ------------------------------------------------------------ downed heroes: Nightmare only, warned
    {
        bool planned = false;
        for (unsigned s = 1; s <= 20 && !planned; ++s) {
            Game g; setup(g, 3, {"enemy_brute"}, s);
            Character& comp = T::hero(g, 1);
            comp.takeDamage(comp.maxHp); // downed
            CHECK(comp.isDowned && !comp.isDead);
            T::foe(g, 0).ai.cooldown = 2; // no Slam this time
            T::plan(g, 0);
            if (moveOf(g, 0) != ai::Move::FINISH_DOWNED) continue;
            planned = true;
            CHECK(T::foe(g, 0).ai.target == 1);
            CHECK(contains(g.getChatHistory(), "WARNING! Millstone Brute eyes the fallen"));
            CHECK(contains(g.getEnemyIntents(), "Finish the fallen"));
            const int fails = comp.deathSaveFailures;
            T::foe(g, 0).curveToHit = 200; // make sure it lands
            T::run(g, 0);
            CHECK(comp.deathSaveFailures > fails || comp.isDead);
        }
        CHECK(planned);
        // Hard and below: never while someone stands.
        for (unsigned s = 1; s <= 30; ++s) {
            Game g; setup(g, 2, {"enemy_brute"}, s);
            T::hero(g, 1).takeDamage(T::hero(g, 1).maxHp);
            T::plan(g, 0);
            CHECK(moveOf(g, 0) != ai::Move::FINISH_DOWNED);
            CHECK(T::foe(g, 0).ai.target != 1);
        }
        std::cout << "DOWNED: only a Nightmare brute strikes the fallen, warned a turn ahead\n";
    }

    // ------------------------------------------------------------ 10 boss phases at 66% / 33%
    {
        struct Exp { const char* id; ai::Move p2; ai::Move p3; };
        const Exp bosses[] = {
            {"boss_sexton", ai::Move::SEXTON_CALL, ai::Move::SNUFF},
            {"boss_collector", ai::Move::TALLY, ai::Move::VENN_CALL},
            {"boss_warden", ai::Move::EMBER_WOUND, ai::Move::WARDEN_GUARD},
            {"boss_herald", ai::Move::TOLL, ai::Move::BELL_CRACK},
            {"boss_wick_king", ai::Move::KING_CALL, ai::Move::CROWN_FLARE},
            {"boss_prior", ai::Move::RELIQUARY, ai::Move::RAISE_DEAD},
            {"boss_drake", ai::Move::CINDER_BREATH, ai::Move::TAKE_FLIGHT},
            {"boss_hollow_crown", ai::Move::STRIKE, ai::Move::TYRANTS_DUE},
            {"boss_ember_hydra", ai::Move::HEADS, ai::Move::HEADS},
            {"boss_nightfang", ai::Move::WEBS, ai::Move::BROOD},
        };
        for (const auto& b : bosses) {
            Game g; setup(g, 1, {b.id}, 21);
            Character& boss = T::foe(g, 0);
            CHECK(boss.ai.phase == 1);
            const int seq = T::bannerSeq(g);
            T::damage(g, 0, boss.currentHp - (boss.maxHp * 60) / 100, 0);
            CHECK(boss.ai.phase == 2 && T::bannerSeq(g) == seq + 1);
            CHECK(contains(g.getEnemyIntents(), std::string("|PHASE 2 — ") + boss.name + "|"));
            if (moveOf(g, 0) != b.p2) std::cerr << b.id << " p2 move " << boss.ai.move << "\n";
            CHECK(moveOf(g, 0) == b.p2);
            boss.ai.flags = ai::F_CALL2; boss.ai.cooldown = 0; // phase-2 once-moves already used
            T::damage(g, 0, boss.currentHp - (boss.maxHp * 30) / 100, 0);
            CHECK(boss.ai.phase == 3 && T::bannerSeq(g) == seq + 2);
            boss.ai.cooldown = 0; T::plan(g, 0);
            if (moveOf(g, 0) != b.p3) std::cerr << b.id << " p3 move " << boss.ai.move << "\n";
            CHECK(moveOf(g, 0) == b.p3);
            // Phase moves run without crashing and keep the fight valid.
            for (int k = 0; k < 4 && !g.getEnemies().empty() && !g.isGameOver(); ++k) {
                for (auto& p : g.getPlayers()) { auto& h = const_cast<Character&>(*p); h.isDowned = false; h.isDead = false; h.currentHp = h.maxHp; }
                T::run(g, 0);
            }
        }
        std::cout << "BOSS PHASES: all 10 bosses change at 66% and 33% with a banner and their phase moves\n";
    }

    // ------------------------------------------------------------ telegraphed heavies skip the 45% guard
    {
        int over = 0, normalOver = 0;
        for (unsigned s = 1; s <= 30; ++s) {
            Game g; setup(g, 1, {"boss_sexton"}, s);
            Character& boss = T::foe(g, 0);
            boss.curveToHit = 200;
            boss.curveAvgDmgX10 = 20 * T::hero(g, 0).maxHp; // ~2x max HP per plain hit (before the boss scale)
            boss.ai.phase = 3; boss.ai.move = static_cast<int>(ai::Move::SNUFF); boss.ai.target = 0;
            Character& hero = T::hero(g, 0);
            T::run(g, 0);
            if (hero.isDowned) ++over; // a guarded hit could never drop a full-HP hero
            Game n; setup(n, 1, {"boss_sexton"}, s);
            Character& b2 = T::foe(n, 0);
            b2.curveToHit = 200; b2.curveAvgDmgX10 = 10 * T::hero(n, 0).maxHp;
            b2.ai.move = static_cast<int>(ai::Move::STRIKE); b2.ai.target = 0;
            T::run(n, 0);
            if (T::hero(n, 0).maxHp - T::hero(n, 0).currentHp > (T::hero(n, 0).maxHp * 45) / 100) ++normalOver;
        }
        CHECK(over > 20);
        CHECK(normalOver == 0);
        std::cout << "HEAVIES: telegraphed boss heavies (Snuff, Dive, Tyrant's due, Cinder breath) skip the 45% guard\n";
    }

    // ------------------------------------------------------------ Millstone Slam: telegraphed, stagger cancels
    {
        Game g; setup(g, 1, {"enemy_brute"}, 31);
        Character& brute = T::foe(g, 0);
        brute.ai.cooldown = 0;
        for (int k = 0; k < 6 && moveOf(g, 0) != ai::Move::SLAM; ++k) { brute.ai.cooldown = 0; T::plan(g, 0); }
        CHECK(moveOf(g, 0) == ai::Move::SLAM);
        CHECK(contains(g.getEnemyIntents(), "Millstone Slam"));
        T::damage(g, 0, (brute.maxHp + 4) / 5, 0);
        CHECK(contains(g.getChatHistory(), "the Millstone Slam is broken"));
        const int hp = T::hero(g, 0).currentHp + T::hero(g, 1).currentHp;
        T::run(g, 0);
        CHECK(T::hero(g, 0).currentHp + T::hero(g, 1).currentHp == hp);
        std::cout << "SLAM: telegraphed a turn ahead; 20% damage in between cancels it\n";
    }

    // ------------------------------------------------------------ statuses tick, save round trip mid-fight
    {
        Game g; setup(g, 2, {"boss_warden", "enemy_ashbound"}, 41);
        Character& hero = T::hero(g, 0);
        hero.status.burn = 2; hero.status.burnDmg = 3; hero.status.marked = 2; hero.status.shackled = 1;
        T::foe(g, 1).status.guarded = 2; T::foe(g, 1).armorClass += ai::kGuardAc;
        T::foe(g, 0).ai.phase = 2; T::foe(g, 0).ai.stolenGold = 7;
        const std::string s = g.serialize();
        Game r; CHECK(r.deserialize(s));
        CHECK(r.serialize() == s);
        Character& h2 = T::hero(r, 0);
        CHECK(h2.status.burn == 2 && h2.status.burnDmg == 3 && h2.status.marked == 2 && h2.status.shackled == 1);
        CHECK(T::foe(r, 1).status.guarded == 2 && T::foe(r, 1).armorClass == T::foe(g, 1).armorClass);
        CHECK(T::foe(r, 0).ai.phase == 2 && T::foe(r, 0).ai.stolenGold == 7 && T::foe(r, 0).ai.move == T::foe(g, 0).ai.move);
        CHECK(contains(r.getStatusEffects(), "0,B2.M2.K1"));
        // Shackled: no special this turn; burn ticks at the end of the bearer's turn.
        const int hp = h2.currentHp;
        T::endTurn(r, &h2);
        CHECK(h2.currentHp == hp - 3 && h2.status.burn == 1 && h2.status.shackled == 0 && h2.status.marked == 1);
        const int ac = T::foe(r, 1).armorClass;
        T::endTurn(r, &T::foe(r, 1)); T::endTurn(r, &T::foe(r, 1));
        CHECK(T::foe(r, 1).status.guarded == 0 && T::foe(r, 1).armorClass == ac - ai::kGuardAc);
        std::cout << "STATUSES: Burn / Marked / Shackled / Guarded tick down, survive save + load\n";
    }

    // ------------------------------------------------------------ v2.29 mid-fight save loads; AI picks up
    {
        Game g; setup(g, 1, {"boss_prior", "enemy_ashbound"}, 51);
        std::string s = g.serialize();
        // Make it v2.29-shaped: header back to 44 fields, players to 29, foes to 27 (no AI fields).
        auto cut = [](const std::string& rec, size_t keep) {
            std::string out; size_t n = 0;
            for (char c : rec) { if (c == ',') { if (++n == keep) break; } out.push_back(c); }
            return out;
        };
        std::vector<std::string> sec; { std::string cur; for (char c : s) { if (c == '|') { sec.push_back(cur); cur.clear(); } else cur.push_back(c); } sec.push_back(cur); }
        sec[0] = cut(sec[0], 44);
        auto cutAll = [&](const std::string& section, size_t keep) {
            std::string out, cur;
            for (char c : section) { if (c == ';') { out += cut(cur, keep) + ";"; cur.clear(); } else cur.push_back(c); }
            return out;
        };
        sec[2] = cutAll(sec[2], 29);
        sec[3] = cutAll(sec[3], 27);
        // A v2.29 boss below 66% HP.
        std::string old; for (size_t i = 0; i < sec.size(); ++i) { if (i) old += "|"; old += sec[i]; }
        Game r; CHECK(r.deserialize(old));
        CHECK(r.getEnemies().size() == 2);
        CHECK(T::foe(r, 0).ai.move == 0); // no plan saved: it plans on its turn
        T::foe(r, 0).currentHp = T::foe(r, 0).maxHp / 2;
        T::foe(r, 0).ai.phase = 0;
        r.setAsHost(true);
        T::run(r, 0);
        CHECK(T::foe(r, 0).ai.phase == 2 && T::foe(r, 0).ai.move != 0);
        CHECK(r.deserialize(r.serialize()));
        std::cout << "V2.29 MID-FIGHT SAVE: loads, boss phase set from HP, AI plans on its turn\n";
    }

    // ------------------------------------------------------------ story +1 resource kept through level-ups
    {
        Game g; // story start: +2 HP and +1 resource padding
        Character& hero = T::hero(g, 0);
        CHECK(hero.bonusMaxResources == 1);
        CHECK(hero.maxResources == hero.formulaMaxResources() + 1);
        hero.addXpLevels(100 + 200 + 300);
        CHECK(hero.level == 4 && hero.maxResources == hero.formulaMaxResources() + 1);
        // A v2.29 save (no field 29): the bonus is derived from its saved max resources.
        std::string s = g.serialize();
        Game r; CHECK(r.deserialize(s));
        CHECK(T::hero(r, 0).bonusMaxResources == 1);
        std::cout << "STORY +1 RESOURCE: kept through level-ups (and derived for older saves)\n";
    }

    // ------------------------------------------------------------ difficulty tiers change behaviour
    {
        // Sloppy (Easy) sometimes picks a random target; Coordinated (Hard) never does.
        int easyOdd = 0, hardOdd = 0;
        for (unsigned s = 1; s <= 60; ++s) {
            Game e; setup(e, 0, {"enemy_wickthief"}, s);
            T::hero(e, 0).currentHp = T::hero(e, 0).maxHp / 4; // hero 0 is the clear pick
            T::plan(e, 0);
            if (T::foe(e, 0).ai.target != 0) ++easyOdd;
            Game h; setup(h, 2, {"enemy_wickthief"}, s);
            T::hero(h, 0).currentHp = T::hero(h, 0).maxHp / 4;
            T::plan(h, 0);
            if (T::foe(h, 0).ai.target != 0) ++hardOdd;
        }
        CHECK(easyOdd > 0 && hardOdd == 0);
        // Coordinated casters interrupt healers.
        Game c; setup(c, 2, {"enemy_tithe_watcher"}, 61);
        T::foe(c, 0).ai.cooldown = 0; T::plan(c, 0);
        CHECK(moveOf(c, 0) == ai::Move::RATTLE_HEALER && T::foe(c, 0).ai.target == 1);
        std::cout << "TIERS: Easy sloppy (" << easyOdd << "/60 off-target), Hard focused, Hard casters rattle healers\n";
    }

    // ------------------------------------------------------------ full fights terminate (no stalls)
    {
        int fights = 0;
        const std::vector<std::vector<std::string>> rooms = {
            {"enemy_wickthief", "enemy_gloamhound", "enemy_gloamhound"}, {"enemy_crier_bone", "enemy_ashbound", "enemy_ashbound_bonebound"},
            {"enemy_tallow_rat", "enemy_tallow_rat", "enemy_tallow_rat"}, {"enemy_brute", "enemy_tithe_enforcer", "enemy_tithe_watcher"},
            {"boss_sexton"}, {"boss_collector"}, {"boss_warden"}, {"boss_herald"}, {"boss_wick_king"}, {"boss_prior"},
            {"boss_drake"}, {"boss_hollow_crown"}, {"boss_ember_hydra"}, {"boss_nightfang"}};
        for (int d = 0; d < 4; ++d) {
            for (const auto& room : rooms) {
                Game g; setup(g, d, room, 100 + fights);
                g.simAllPartyAuto();
                int steps = 0;
                while (!g.getEnemies().empty() && !g.isGameOver() && steps < 4000) { g.processTurn(); ++steps; }
                if (steps >= 4000) {
                    std::cerr << "stall d=" << d << " room0=" << room[0] << " foes=" << g.getEnemies().size();
                    for (const auto& e : g.getEnemies()) std::cerr << " [" << e->enemyId << " " << e->currentHp << "/" << e->maxHp << " move " << e->ai.move << "]";
                    for (const auto& p : g.getPlayers()) std::cerr << " {" << p->name << " " << p->currentHp << "/" << p->maxHp << (p->isDowned ? " down" : "") << (p->isStable ? " stable" : "") << "}";
                    std::cerr << "\n" << g.getChatHistory().substr(g.getChatHistory().size() > 1500 ? g.getChatHistory().size() - 1500 : 0) << "\n";
                }
                CHECK(steps < 4000);
                ++fights;
            }
        }
        std::cout << "FIGHTS: " << fights << " AI fights (every boss, all tiers) run to an end\n";
    }

    // ------------------------------------------------------------ online stays as-is
    {
        Game host;
        host.startDmSession("Keeper");
        host.addAlly("Guest", CharacterClass::FIGHTER);
        host.dmBeginDungeon();
        CHECK(!host.enemyAiActive());
        const std::string s = host.getEnemyIntents();
        CHECK(s.back() == '|'); // no entries
        for (int k = 0; k < 20 && !host.getEnemies().empty() && !host.isGameOver(); ++k) host.processTurn();
        for (const auto& e : host.getEnemies()) CHECK(e->ai.move == 0 && e->curveEL == 0);
        CHECK(host.getStatusEffects().empty());
        Game client; client.prepareClientJoin();
        CHECK(!client.enemyAiActive());
        std::cout << "ONLINE: Keeper table and joiners keep the old AI (no intents, no statuses)\n";
    }

    if (failures) { std::cerr << failures << " failure(s)\n"; return 1; }
    std::cout << "ai_test OK\n";
    return 0;
}
