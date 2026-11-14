// v2.31 universal stats (PR 3 of the combat/progression revamp): formulas and caps, the level-up
// cap, save migration (clamp + refund, a v2.30 mid-fight slot), Signature uses, Mending, sell /
// buy arbitrage, the capped Reference Lamplighter, the stat guide, and online unchanged.
// Run tools/migration_test/run.sh.
#include "Game.h"
#include "CombatCurve.h"
#include "StatRules.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
std::ostream aout(std::cout.rdbuf());
using namespace dnd;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n"; ++failures; } } while (0)
static bool contains(const std::string& s, const std::string& sub) { return s.find(sub) != std::string::npos; }

namespace dnd {
struct GameTestAccess {
    static Character& hero(Game& g, size_t i) { return *g.players_[i]; }
    static bool avail(const Game& g, const Character& c) { return g.specialAvailable(c); }
    static void spend(Game& g, Character& c) { g.spendSpecialUse(c); }
    static int luck(const Game& g, int gold) { return g.luckGold(gold); }
};
} // namespace dnd
using T = GameTestAccess;

static std::vector<std::string> split(const std::string& s, char d) {
    std::vector<std::string> out; std::string cur;
    for (char c : s) { if (c == d) { out.push_back(cur); cur.clear(); } else cur.push_back(c); }
    out.push_back(cur);
    return out;
}
static std::string join(const std::vector<std::string>& v, char d) {
    std::string s;
    for (size_t i = 0; i < v.size(); ++i) { if (i) s += d; s += v[i]; }
    return s;
}
/** Turn a v2.31 save into a v2.30 one: drop player field 31 (Signature uses). */
static std::string asV230(const std::string& save) {
    auto sec = split(save, '|');
    auto recs = split(sec[2], ';');
    for (auto& r : recs) {
        if (r.empty()) continue;
        auto f = split(r, ',');
        if (f.size() == 32) { f.pop_back(); r = join(f, ','); }
    }
    sec[2] = join(recs, ';');
    return join(sec, '|');
}
static size_t playerFieldCount(const std::string& save) {
    auto sec = split(save, '|');
    auto recs = split(sec[2], ';');
    return split(recs[0], ',').size();
}

static void solo(Game& g, CharacterClass hero, CharacterClass comp, unsigned seed = 7) {
    g.seedForTesting(seed);
    g.startNewGame(hero, "Mira", 1, 1, comp, true);
    g.setAsHost(true);
}

int main() {
    // ------------------------------------------------------------ pure formulas
    {
        CHECK(stats::attrCap(1, false) == 18 && stats::attrCap(1, true) == 20);
        CHECK(stats::attrCap(20, false) == 28 && stats::attrCap(20, true) == 30);
        CHECK(stats::attrCap(24, false) == 30 && stats::attrCap(50, true) == 32);
        CHECK(stats::attrCap(50, true, 2) == 34); // prestige ranks (PR 5)
        CHECK(stats::signatureTier(19) == 0 && stats::signatureTier(20) == 1 && stats::signatureTier(24) == 2
              && stats::signatureTier(28) == 3 && stats::signatureTier(32) == 4);
        CHECK(stats::heft(5) == 2 && stats::heft(-1) == 0);
        CHECK(!stats::breaksGuard(2) && stats::breaksGuard(3));
        CHECK(stats::acFromDex(9) == 6 && stats::acFromDex(-1) == -1);
        CHECK(stats::evade(3) > 0.089 && stats::evade(3) < 0.091 && stats::evade(10) == 0.24);
        CHECK(stats::deathSaveBonus(5) == 2 && stats::deathSaveBonus(-2) == 0);
        CHECK(stats::critMin(3, false) == 20 && stats::critMin(4, false) == 19 && stats::critMin(8, false) == 18
              && stats::critMin(0, true) == 19 && stats::critMin(10, true) == 17);
        CHECK(stats::mending(5) > 0.199 && stats::mending(5) < 0.201 && stats::mending(20) == 0.40);
        CHECK(!stats::restResourceBonus(3) && stats::restResourceBonus(4));
        CHECK(stats::buyDiscount(15) == 0.20 && stats::sellBonusPct(4) == 4 && stats::sellBonusPct(20) == 10);
        CHECK(stats::goldBonus(20) == 0.30 && stats::inspire(7) == 1 && stats::inspire(8) == 2);
        CHECK(stats::dartBonus(2) == 1 && stats::dartBonus(6) == 3);
        CHECK(stats::buyPrice(100, 5) == 90 && stats::buyPrice(1, 10) == 1);
        std::cout << "FORMULAS: caps 18+L/2 (30, main +2), Signature 20/24/28/32, every stat's numbers\n";
    }

    // ------------------------------------------------------------ level-up respects the cap (solo only)
    {
        Game g; solo(g, CharacterClass::FIGHTER, CharacterClass::CLERIC);
        g.syncStatRules();
        Character& h = T::hero(g, 0);
        CHECK(h.statRules);
        h.level = 4;
        h.attributes.dexterity = 20; // cap 20 at level 4 (not main)
        h.pendingStatPoints = 2;
        g.playerIncreaseStat("Mira", 1);
        CHECK(h.attributes.dexterity == 20 && h.pendingStatPoints == 2);
        CHECK(contains(g.getLastEvent(), "at its cap"));
        h.attributes.strength = 21; // main cap 22
        g.playerIncreaseStat("Mira", 0);
        CHECK(h.attributes.strength == 22 && h.pendingStatPoints == 1);
        g.playerIncreaseStat("Mira", 0);
        CHECK(h.attributes.strength == 22 && h.pendingStatPoints == 1);
        // Online: no caps.
        Game o; o.seedForTesting(3); o.startNewGame(CharacterClass::FIGHTER, "Mira", 1, 1, CharacterClass::CLERIC, true);
        o.setAsHost(false); o.syncStatRules();
        Character& oh = T::hero(o, 0);
        oh.attributes.dexterity = 25; oh.pendingStatPoints = 1;
        o.playerIncreaseStat("Mira", 1);
        CHECK(!oh.statRules && oh.attributes.dexterity == 26 && oh.pendingStatPoints == 0);
        std::cout << "LEVEL-UP: + refused at the cap with a reason; online tables uncapped\n";
    }

    // ------------------------------------------------------------ migration: clamp + refund
    {
        Game g; solo(g, CharacterClass::FIGHTER, CharacterClass::WIZARD);
        Character& h = T::hero(g, 0);
        h.level = 4;
        h.attributes.strength = 25;     // main cap 22 -> 3 back
        h.attributes.constitution = 23; // cap 20 -> 3 back
        h.pendingStatPoints = 1;
        std::string s = g.serialize();
        Game r; r.setAsHost(true);
        CHECK(r.deserialize(asV230(s)));
        Character& rh = T::hero(r, 0);
        CHECK(rh.statRules);
        CHECK(rh.attributes.strength == 22 && rh.attributes.constitution == 20 && rh.pendingStatPoints == 7);
        CHECK(rh.currentHp <= rh.maxHp);
        CHECK(contains(r.getStatMigrationNote(), "STR 25 → 22") && contains(r.getStatMigrationNote(), "6 points refunded"));
        CHECK(contains(r.getChatHistory(), "Stat caps (v2.31)"));
        // Idempotent: a second load does not refund again.
        Game r2; r2.setAsHost(true);
        CHECK(r2.deserialize(r.serialize()));
        CHECK(T::hero(r2, 0).pendingStatPoints == 7 && r2.getStatMigrationNote().empty());
        CHECK(playerFieldCount(r.serialize()) == 32);
        // Online join of the same save: nothing touched.
        Game j; j.setAsHost(false);
        CHECK(j.deserialize(asV230(s)));
        CHECK(T::hero(j, 0).attributes.strength == 25 && T::hero(j, 0).pendingStatPoints == 1 && !T::hero(j, 0).statRules);
        std::cout << "MIGRATION: over-cap stats clamp, points refunded once (STR 25 → 22, CON 23 → 20); online untouched\n";
    }

    // ------------------------------------------------------------ v2.30 mid-fight slot
    {
        Game g; solo(g, CharacterClass::WIZARD, CharacterClass::FIGHTER, 11);
        Character& h = T::hero(g, 0);
        h.addXpLevels(100 + 200 + 300 + 400 + 500 + 600 + 700 + 800 + 900); // level 10
        g.simBeginFight({"enemy_wickthief", "enemy_gloamhound"});
        // A v2.30 hero had no caps: write the over-cap scores straight into the saved slot.
        h.attributes.intelligence = 27; // main cap 25 -> 2 back
        h.attributes.dexterity = 24;    // cap 23 -> 1 back
        const size_t foes = g.getEnemies().size();
        const int pendingBefore = h.pendingStatPoints;
        std::string v230 = asV230(g.serialize());
        CHECK(playerFieldCount(v230) == 31);
        Game r; r.setAsHost(true);
        CHECK(r.deserialize(v230));
        Character& rh = T::hero(r, 0);
        CHECK(rh.level == h.level && rh.attributes.intelligence == 25 && rh.attributes.dexterity == 23);
        CHECK(rh.pendingStatPoints == pendingBefore + 3);
        CHECK(r.getEnemies().size() == foes);
        CHECK(rh.signatureUses == 1); // INT 25: Signature tier 2, the free use is granted on load
        r.simAllPartyAuto();
        int steps = 0;
        while (!r.getEnemies().empty() && !r.isGameOver() && steps < 4000) { r.processTurn(); ++steps; }
        CHECK(steps < 4000);
        std::cout << "V2.30 MID-FIGHT SAVE: loads with caps applied (+3 points), Signature use granted, fight finishes\n";
    }

    // ------------------------------------------------------------ Signature uses
    {
        Game g; solo(g, CharacterClass::FIGHTER, CharacterClass::CLERIC, 21);
        Character& f = T::hero(g, 0);
        Character& c = T::hero(g, 1);
        f.level = 10; f.attributes.strength = 20; f.resources = 1;
        c.level = 10; c.attributes.wisdom = 24; c.resources = 0;
        g.simBeginFight({"enemy_wickthief"});
        CHECK(f.signatureUses == 1 && c.signatureUses == 1);
        CHECK(f.signatureTier() == 1 && c.signatureTier() == 2);
        // Fighter: the first surge spends a supply, the Signature use is a second surge.
        CHECK(T::avail(g, f));
        T::spend(g, f); f.specialUsedThisCombat = true;
        CHECK(f.resources == 0 && f.signatureUses == 1 && T::avail(g, f));
        T::spend(g, f);
        CHECK(f.signatureUses == 0 && !T::avail(g, f));
        // Cleric with no supplies still has the free Healing Word.
        CHECK(T::avail(g, c));
        T::spend(g, c);
        CHECK(!T::avail(g, c));
        // Below 20: nothing extra.
        Game n; solo(n, CharacterClass::FIGHTER, CharacterClass::CLERIC, 22);
        n.simBeginFight({"enemy_wickthief"});
        CHECK(T::hero(n, 0).signatureUses == 0);
        std::cout << "SIGNATURE: main 20+ gives one free special per battle (Fighter: a second Action Surge)\n";
    }

    // ------------------------------------------------------------ Mending, Weak point, Luck
    {
        Game g; solo(g, CharacterClass::FIGHTER, CharacterClass::CLERIC, 31);
        g.syncStatRules();
        Character& h = T::hero(g, 0);
        h.attributes.wisdom = 20; // +5 -> +20%
        h.currentHp = 1;
        CHECK(h.heal(10) == 12);
        h.attributes.intelligence = 18; // +4 -> crit 19+
        CHECK(CombatSystem::critMin(h, false) == 19);
        h.attributes.charisma = 20; // +5 -> +15% gold
        T::hero(g, 1).attributes.charisma = 8;
        CHECK(T::luck(g, 100) == 115);
        Game o; o.seedForTesting(1); o.startNewGame(CharacterClass::FIGHTER, "Mira", 1, 1, CharacterClass::CLERIC, true);
        o.setAsHost(false); o.syncStatRules();
        Character& oh = T::hero(o, 0);
        oh.attributes.wisdom = 20; oh.attributes.intelligence = 18; oh.currentHp = 1;
        CHECK(oh.heal(10) == 10 && CombatSystem::critMin(oh, false) == 20);
        std::cout << "EFFECTS: Mending +20% (WIS 20), crit on 19 (INT 18), Luck +15% gold (CHA 20); online off\n";
    }

    // ------------------------------------------------------------ Barter can never be farmed
    {
        int checked = 0;
        for (int rar = 0; rar <= 4; ++rar) {
            for (int bonus = 0; bonus <= 30; ++bonus) {
                for (int up = 0; up <= 10; ++up) {
                    auto it = Item::make("Test Blade", ItemType::WEAPON, bonus, static_cast<ItemRarity>(rar), -1);
                    it->upgradeLevel = up;
                    for (int cha = -1; cha <= 20; ++cha) {
                        const int buy = stats::buyPrice(it->shopCost(), cha);
                        const int sell = it->sellPrice(cha);
                        CHECK(sell < buy);
                        CHECK(sell * 100 <= it->shopCost() * stats::kSellCapMax);
                        // Upgrading never pays for itself on resale.
                        auto nx = Item::make("Test Blade", ItemType::WEAPON, bonus + 1, static_cast<ItemRarity>(rar), -1);
                        nx->upgradeLevel = up + 1;
                        CHECK(nx->sellPrice(cha) - sell < it->upgradeCost());
                        ++checked;
                    }
                }
            }
        }
        auto pot = Item::make("Healing Potion", ItemType::POTION, 2, ItemRarity::COMMON, -1);
        CHECK(pot->sellPrice(20) < stats::buyPrice(pot->shopCost(), 20));
        auto plain = Item::make("Old Blade", ItemType::WEAPON, 3, ItemRarity::RARE, -1);
        CHECK(plain->sellPrice(0) == plain->sellPrice()); // online / CHA 10: unchanged
        std::cout << "BARTER: " << checked << " item/CHA combos — sell < discounted buy, cap 75%, upgrades never profit\n";
    }

    // ------------------------------------------------------------ the Reference Lamplighter respects the caps
    {
        for (int L : {1, 10, 20, 30, 50}) {
            for (auto cls : {CharacterClass::FIGHTER, CharacterClass::WIZARD, CharacterClass::ROGUE,
                             CharacterClass::CLERIC, CharacterClass::BARD}) {
                Character c = curve::makeReferenceHero(cls, L);
                c.statRules = true;
                for (int i = 0; i < 6; ++i) CHECK(c.baseAttr(i) <= c.attrCapFor(i));
                int total = 0;
                for (int i = 0; i < 6; ++i) total += c.baseAttr(i);
                Character base("x", cls, "x");
                int baseTotal = 0;
                for (int i = 0; i < 6; ++i) baseTotal += base.baseAttr(i);
                CHECK(total - baseTotal == static_cast<int>(2 * (L - 1) * 0.5) + static_cast<int>(2 * (L - 1) * 0.3)
                                           + static_cast<int>(2 * (L - 1) * 0.2)); // nothing lost, only moved
            }
        }
        const Character f50 = curve::makeReferenceHero(CharacterClass::FIGHTER, 50);
        CHECK(f50.attributes.strength == 32);
        std::cout << "REFERENCE: model hero capped (Fighter L50 STR 32), overflow to CON / DEX\n";
    }

    // ------------------------------------------------------------ stat guide for the UI
    {
        Game g; solo(g, CharacterClass::ROGUE, CharacterClass::CLERIC, 41);
        g.syncStatRules();
        T::hero(g, 0).attributes.dexterity = 20;
        auto parts = split(g.getStatGuide("Mira"), '|');
        CHECK(parts.size() == 12);
        CHECK(parts[0] == "1" && parts[1] == "1");
        CHECK(split(parts[2], ',').size() == 6 && split(parts[2], ',')[1] == "20");
        CHECK(contains(parts[6], "Main for Rogue") && contains(parts[6], "Evade"));
        CHECK(contains(parts[5], "Heft") && contains(parts[10], "Barter"));
        CHECK(contains(parts[11], "Signature"));
        Game o; o.seedForTesting(1); o.startNewGame(CharacterClass::ROGUE, "Mira", 1, 1, CharacterClass::CLERIC, true);
        o.setAsHost(false); o.syncStatRules();
        auto op = split(o.getStatGuide("Mira"), '|');
        CHECK(op.size() == 12 && op[0] == "0" && op[11].empty());
        CHECK(g.getStatGuide("Nobody").empty());
        std::cout << "GUIDE: per-class lines, caps and Signature for the level-up screen; online shows old rules\n";
    }

    // ------------------------------------------------------------ fights still finish with the new rules
    {
        int fights = 0;
        const std::vector<std::vector<std::string>> rooms = {
            {"enemy_wickthief", "enemy_gloamhound"}, {"enemy_brute", "enemy_tithe_enforcer", "enemy_tithe_watcher"},
            {"boss_sexton"}, {"boss_warden"}, {"boss_nightfang"}};
        for (auto cls : {CharacterClass::FIGHTER, CharacterClass::WIZARD, CharacterClass::ROGUE, CharacterClass::CLERIC, CharacterClass::BARD}) {
            for (const auto& room : rooms) {
                Game g; solo(g, cls, CharacterClass::CLERIC, 500 + fights);
                g.simBeginFight(room);
                g.simAllPartyAuto();
                int steps = 0;
                while (!g.getEnemies().empty() && !g.isGameOver() && steps < 4000) { g.processTurn(); ++steps; }
                CHECK(steps < 4000);
                ++fights;
            }
        }
        std::cout << "FIGHTS: " << fights << " solo fights with the stat rules run to an end\n";
    }

    if (failures) { std::cerr << failures << " failure(s)\n"; return 1; }
    std::cout << "stats_test OK\n";
    return 0;
}
