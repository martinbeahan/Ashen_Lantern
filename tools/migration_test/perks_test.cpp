// v2.32 Kindled Path (PR 4 of the combat/progression revamp): level cap 50 and the XP curve, stat
// points and proficiency past 20, Embers, save fields 32-36 (append-only) with the free Re-temper
// for older heroes, the level > 50 clamp, Wards once per battle (also across a reload), Lantern Arts
// cooldowns and the Signature-32 recharge, Re-temper cost, story padding kept, online untouched,
// and fights with every class's Arts finishing. Run tools/migration_test/run.sh.
#include "Game.h"
#include "CombatCurve.h"
#include "Perks.h"
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
    static Character& foe(Game& g, size_t i) { return *g.enemies_[i]; }
    static std::string beforeHit(Game& g, Character& h, int& dmg) { return g.wardsBeforeHit(h, dmg); }
    static void afterHit(Game& g, Character& h, bool wasDowned) { g.wardsAfterHit(h, wasDowned); }
    /** Start in a quiet chamber (the first crawl room may already hold a foe). */
    static void quiet(Game& g) { g.enemies_.clear(); g.turnOrder_.clear(); g.currentTurnIndex_ = 0; }
    static bool makeCurrent(Game& g, const Character* c) {
        for (size_t i = 0; i < g.turnOrder_.size(); ++i)
            if (g.turnOrder_[i] == c) { g.currentTurnIndex_ = static_cast<int>(i); return true; }
        return false;
    }
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
/** Rewrite every player record: keep the first `keep` fields, optionally set field `idx` = `val`. */
static std::string editPlayers(const std::string& save, size_t keep, int idx = -1, const std::string& val = "") {
    auto sec = split(save, '|');
    auto recs = split(sec[2], ';');
    for (auto& r : recs) {
        if (r.empty()) continue;
        auto f = split(r, ',');
        if (f.size() > keep) f.resize(keep);
        if (idx >= 0 && static_cast<size_t>(idx) < f.size()) f[static_cast<size_t>(idx)] = val;
        r = join(f, ',');
    }
    sec[2] = join(recs, ';');
    return join(sec, '|');
}
static size_t playerFieldCount(const std::string& save) {
    auto sec = split(save, '|');
    return split(split(sec[2], ';')[0], ',').size();
}
static void solo(Game& g, CharacterClass hero, CharacterClass comp, unsigned seed = 7, int mode = 1) {
    g.seedForTesting(seed);
    g.startNewGame(hero, "Mira", mode, 1, comp, true);
    g.setAsHost(true);
    GameTestAccess::quiet(g);
}
/** Level both heroes to `level` through the real XP path. */
static void levelTo(Game& g, int level) {
    for (auto& p : g.getPlayers()) {
        Character& h = const_cast<Character&>(*p);
        h.addXpLevels(static_cast<int>(perks::totalXpTo(level)));
    }
}

int main() {
    // ------------------------------------------------------------ XP curve, cap, points, proficiency
    {
        CHECK(perks::xpToNext(1) == 100 && perks::xpToNext(19) == 1900);      // unchanged below 20
        CHECK(perks::xpToNext(20) == 2000 && perks::xpToNext(49) == 4900 && perks::xpToNext(50) == 0);
        CHECK(perks::totalXpTo(20) == 19000 && perks::totalXpTo(50) == 122500);
        CHECK(perks::statPointsThrough(20) == 38 && perks::statPointsThrough(50) == 50);
        CHECK(perks::proficiency(16) == 5 && perks::proficiency(17) == 6 && perks::proficiency(50) == 6);
        CHECK(stats::attrCap(50, false) == 30 && stats::attrCap(50, true) == 32);
        Game g; solo(g, CharacterClass::FIGHTER, CharacterClass::CLERIC);
        Character& h = T::hero(g, 0);
        const int lv = h.addXpLevels(10000000);
        CHECK(h.level == 50 && lv == 49);
        CHECK(h.pendingStatPoints == 50);
        CHECK(h.xpToNextLevel() == 4900 && h.xp <= 4900 && h.xp >= 0);
        h.addXpLevels(5000);
        CHECK(h.level == 50 && h.xp == 4900); // the bar stays full at the cap
        CHECK(contains(h.getDetailedSheet(), "level cap 50"));
        CHECK(h.peakLevel == 50);
        CHECK(CombatSystem::proficiencyBonusForLevel(50) == 6);
        // All points spent at 50: main tops out at 32, the rest at 30.
        h.attributes = {32, 30, 30, 30, 30, 30};
        CHECK(!h.canRaise(0) && !h.canRaise(1));
        std::cout << "LEVELS: cap 50, 122,500 XP from 1, 50 stat points, proficiency +6 from 17, caps 30 / 32\n";
    }

    // ------------------------------------------------------------ Embers
    {
        CHECK(perks::embersEarned(20, 0) == 0 && perks::embersEarned(21, 0) == 1 && perks::embersEarned(50, 0) == 30);
        CHECK(perks::embersEarned(50, 8) == 38 && perks::embersEarned(50, 9) == 41 && perks::embersEarned(50, 10) == 42);
        CHECK(perks::kEmberCap == 42 && perks::kPathTotalCost == 68);
        Game g; solo(g, CharacterClass::FIGHTER, CharacterClass::CLERIC);
        levelTo(g, 30);
        Character& h = T::hero(g, 0);
        CHECK(h.level == 30 && h.embersEarned() == 10 && h.embersFree() == 10);
        CHECK(g.learnPerk(0, 0, 0));                 // Tempered Arm 1
        CHECK(g.learnPerk(0, 1, perks::LAST_LIGHT)); // 2
        CHECK(g.learnPerk(0, 2, 0));                 // Cleave 3 (level 22)
        CHECK(h.embersSpent() == 6 && h.embersFree() == 4);
        CHECK(h.wardEq[0] == perks::LAST_LIGHT && h.artEq[0] == 0);
        std::string why;
        CHECK(!g.learnPerk(0, 2, 3, &why) && contains(why, "level 34")); // Warcry unlocks at 34
        CHECK(g.learnPerk(0, 2, 1));                                      // Iron Brace (26)
        CHECK(!g.learnPerk(0, 1, perks::ASH_VEIL, &why) && contains(why, "Embers")); // 1 left
        const int strBefore = h.effectiveAttr(0);
        CHECK(g.learnPerk(0, 0, 0) && h.effectiveAttr(0) == strBefore + 1);
        CHECK(h.embersFree() == 0);
        std::cout << "EMBERS: 1 a level 21-50 (30), +1 per Rekindling rank and +2 at IX = 42 cap; costs 1 / 2 / 3\n";
    }

    // ------------------------------------------------------------ Save fields + free Re-temper migration
    {
        Game g; solo(g, CharacterClass::ROGUE, CharacterClass::WIZARD, 11);
        levelTo(g, 34);
        Character& h = T::hero(g, 0);
        CHECK(g.learnPerk(0, 0, perks::THICK_WAX) && g.learnPerk(0, 1, perks::STEADY_HAND)
              && g.learnPerk(0, 2, 2));
        const std::string s = g.serialize();
        CHECK(playerFieldCount(s) == 37);
        Game r; r.setAsHost(true);
        CHECK(r.deserialize(s));
        Character& rh = T::hero(r, 0);
        CHECK(rh.flames[perks::THICK_WAX] == 1 && (rh.wardsLearned & (1 << perks::STEADY_HAND)) && rh.artEq[0] == 2);
        CHECK(rh.freeRespecs == 0 && rh.peakLevel == 34);
        if (rh.maxHp != h.maxHp) std::cerr << "maxHp " << rh.maxHp << " vs " << h.maxHp << "\n";
        CHECK(rh.maxHp == h.maxHp);
        // A v2.31 save (31 + 1 fields): every hero gets one free Re-temper, nothing else changes.
        const std::string v231 = editPlayers(s, 32);
        CHECK(playerFieldCount(v231) == 32);
        Game m; m.setAsHost(true);
        CHECK(m.deserialize(v231));
        CHECK(T::hero(m, 0).freeRespecs == 1 && T::hero(m, 1).freeRespecs == 1);
        CHECK(T::hero(m, 0).flames[perks::THICK_WAX] == 0 && T::hero(m, 0).wardsLearned == 0 && T::hero(m, 0).peakLevel == 34);
        CHECK(T::hero(m, 0).embersFree() == 14); // level 34: Embers waiting
        // Using the token is saved: no second free one on the next load.
        CHECK(m.retemperCostFor(T::hero(m, 0)) == 0 && m.retemper(0));
        Game m2; m2.setAsHost(true);
        CHECK(m2.deserialize(m.serialize()));
        CHECK(T::hero(m2, 0).freeRespecs == 0 && T::hero(m2, 1).freeRespecs == 1);
        CHECK(m2.retemperCostFor(T::hero(m2, 0)) == 3400);
        // An older (v2.30, 31 fields) save also gets the token.
        Game o; o.setAsHost(true);
        CHECK(o.deserialize(editPlayers(s, 31)) && T::hero(o, 0).freeRespecs == 1);
        // Host/Join never touch solo saves: an online load keeps the fields but no perks apply.
        Game j; j.setAsHost(false);
        CHECK(j.deserialize(s));
        CHECK(T::hero(j, 0).flames[perks::THICK_WAX] == 1 && !T::hero(j, 0).perksOn());
        std::cout << "SAVES: fields 32-36 appended; v2.31 / v2.30 heroes get 1 free Re-temper (saved once used)\n";
    }

    // ------------------------------------------------------------ Level > 50 on load
    {
        Game g; solo(g, CharacterClass::BARD, CharacterClass::FIGHTER, 5);
        levelTo(g, 50);
        const std::string s = editPlayers(g.serialize(), 37, 2, "63");
        Game r; r.setAsHost(true);
        CHECK(r.deserialize(s));
        CHECK(T::hero(r, 0).level == 50 && T::hero(r, 0).xp == 4900 && T::hero(r, 0).currentHp <= T::hero(r, 0).maxHp);
        CHECK(contains(r.getChatHistory(), "Level cap 50"));
        std::cout << "CLAMP: a level 63 hero loads at 50 with a full XP bar\n";
    }

    // ------------------------------------------------------------ Re-temper
    {
        Game g; solo(g, CharacterClass::CLERIC, CharacterClass::WIZARD, 9, 0); // story: +2 HP / +1 resource padding
        Character& h = T::hero(g, 0);
        CHECK(h.bonusMaxResources == 1 && h.bonusMaxHp >= 2);
        levelTo(g, 30);
        const int pts = h.pendingStatPoints;
        for (int i = 0; i < 12; ++i) h.increaseAttribute(4); // WIS
        for (int i = 0; i < 5; ++i) h.increaseAttribute(2);  // CON (odd points store HP)
        CHECK(h.pendingStatPoints == pts - 17);
        CHECK(g.learnPerk(0, 0, perks::STILLFLAME) && g.learnPerk(0, 1, perks::SECOND_WICK));
        h.freeRespecs = 0;
        h.gold = 100;
        std::string why;
        CHECK(!g.retemper(0, &why) && contains(why, "3000 gold"));
        h.gold = 5000;
        CHECK(g.retemper(0));
        CHECK(h.gold == 2000 && h.pendingStatPoints == pts && h.attributes.wisdom == 16 && h.attributes.constitution == 14);
        CHECK(h.embersSpent() == 0 && h.embersFree() == 10 && h.wardEq[0] == -1);
        CHECK(h.bonusMaxResources == 1 && h.bonusMaxHp >= 2); // story padding kept
        CHECK(h.currentHp <= h.maxHp);
        // Not during a fight.
        g.simBeginFight({"enemy_wickthief"});
        CHECK(!g.retemper(0, &why) && contains(why, "fight"));
        CHECK(!g.learnPerk(0, 0, 0, &why));
        std::cout << "RE-TEMPER: 100 gold x level, refunds points and Embers, story padding kept, not in a fight\n";
    }

    // ------------------------------------------------------------ Wards: once per battle, survive reload
    {
        Game g; solo(g, CharacterClass::FIGHTER, CharacterClass::CLERIC, 3);
        levelTo(g, 24);
        Character& h = T::hero(g, 0);
        CHECK(g.learnPerk(0, 1, perks::LAST_LIGHT) && g.learnPerk(0, 1, perks::EMBER_WARD));
        g.simBeginFight({"enemy_ashbound", "enemy_ashbound"});
        CHECK(h.status.shield == static_cast<int>(h.maxHp * perks::kEmberWardShield)); // Ember Ward
        int dmg = h.currentHp + h.status.shield + 40;
        T::beforeHit(g, h, dmg);
        CHECK(dmg == h.currentHp - 1 && h.status.shield == 0); // shield soaked, Last Light held
        CHECK(!h.wardReady(perks::LAST_LIGHT));
        int again = h.currentHp + 10;
        T::beforeHit(g, h, again);
        CHECK(again == h.currentHp + 10); // once per battle
        const std::string mid = g.serialize();
        Game r; r.setAsHost(true);
        CHECK(r.deserialize(mid));
        CHECK(!T::hero(r, 0).wardReady(perks::LAST_LIGHT) && T::hero(r, 0).wardEquipped(perks::LAST_LIGHT));
        // Second Wick heals once below 30%.
        Character& c = T::hero(g, 1);
        c.wardsLearned |= 1 << perks::SECOND_WICK; c.wardEq[0] = perks::SECOND_WICK;
        c.currentHp = std::max(1, c.maxHp / 5);
        const int low = c.currentHp;
        T::afterHit(g, c, false);
        CHECK(c.currentHp > low && !c.wardReady(perks::SECOND_WICK));
        std::cout << "WARDS: Ember Ward shield, Last Light once per battle (kept across a mid-fight reload), Second Wick\n";
    }

    // ------------------------------------------------------------ Arts: cooldowns and Signature 32
    {
        for (int str : {30, 32}) {
            Game g; solo(g, CharacterClass::FIGHTER, CharacterClass::CLERIC, 21);
            levelTo(g, 30);
            Character& h = T::hero(g, 0);
            h.attributes.strength = str;
            CHECK(g.learnPerk(0, 2, 1)); // Iron Brace (CD 4, self)
            g.simBeginFight({"enemy_ashbound", "enemy_wickthief"});
            CHECK(T::makeCurrent(g, &h));
            CHECK(!g.getArtsBar().empty());
            CHECK(g.playerUseArt(0, 0));
            const int expect = perks::cooldownFor(4, h.signatureTier()); // left after its own turn ended
            CHECK(h.artCd[1] == expect);
            CHECK(expect == (str == 32 ? 3 : 4));
            CHECK(h.status.braced > 0);
            T::makeCurrent(g, &h);
            CHECK(!g.playerUseArt(0, 0)); // recharging: refused, the turn is not spent
        }
        CHECK(perks::cooldownFor(3, 4) == 2 && perks::cooldownFor(1, 4) == 1 && perks::cooldownFor(perks::kOncePerBattle, 4) == perks::kOncePerBattle);
        std::cout << "ARTS: cooldown after use; main stat 32 recharges 1 round faster; refused while recharging\n";
    }

    // ------------------------------------------------------------ Mid-fight statuses round-trip
    {
        Game g; solo(g, CharacterClass::WIZARD, CharacterClass::ROGUE, 13);
        levelTo(g, 30);
        g.simBeginFight({"enemy_ashbound", "enemy_gloamhound"});
        Character& f = T::foe(g, 0);
        f.status.sundered = 2; f.status.stunned = 1; f.status.bleed = 3; f.status.bleedDmg = 12; f.status.discord = 1;
        T::hero(g, 0).status.shield = 25; T::hero(g, 0).status.rallied = 2; T::hero(g, 0).status.smoke = 1;
        Game r; r.setAsHost(true);
        CHECK(r.deserialize(g.serialize()));
        const Character& rf = T::foe(r, 0);
        CHECK(rf.status.sundered == 2 && rf.status.stunned == 1 && rf.status.bleed == 3 && rf.status.bleedDmg == 12 && rf.status.discord == 1);
        CHECK(T::hero(r, 0).status.shield == 25 && T::hero(r, 0).status.rallied == 2 && T::hero(r, 0).status.smoke == 1);
        std::cout << "STATUSES: the new Art statuses survive a mid-fight save\n";
    }

    // ------------------------------------------------------------ Online / stat rules off: no perks
    {
        Game g; solo(g, CharacterClass::FIGHTER, CharacterClass::CLERIC, 2);
        levelTo(g, 30);
        Character& h = T::hero(g, 0);
        h.flames[perks::TEMPERED_ARM] = 5;
        g.setStatRulesEnabledForTesting(false);
        std::string why;
        CHECK(!g.learnPerk(0, 0, 0, &why) && contains(why, "solo"));
        CHECK(!h.perksOn() && h.effectiveAttr(0) == h.attributes.strength + h.legendaryAttrBonus(0));
        CHECK(g.getPerkState(0).find("|0|The Kindled Path is solo only.") != std::string::npos);
        g.simBeginFight({"enemy_wickthief"});
        CHECK(g.getArtsBar().empty());
        std::cout << "ONLINE: classic rules — no flames, Wards or Arts\n";
    }

    // ------------------------------------------------------------ Reclass keeps flames / Wards, drops Arts
    {
        Game g; solo(g, CharacterClass::FIGHTER, CharacterClass::CLERIC, 4);
        levelTo(g, 30);
        CHECK(g.learnPerk(1, 0, perks::STILLFLAME) && g.learnPerk(1, 1, perks::CLEANSE) && g.learnPerk(1, 2, 0));
        T::hero(g, 1).freeRespecs = 1;
        CHECK(g.setCompanionClass(CharacterClass::BARD));
        const Character& c = T::hero(g, 1);
        CHECK(c.flames[perks::STILLFLAME] == 1 && (c.wardsLearned & (1 << perks::CLEANSE)) && c.artsLearned == 0 && c.freeRespecs == 1);
        CHECK(c.embersFree() == 10 - 1 - 2);
        std::cout << "RECLASS: flames, Wards and the token stay; class Arts refund their Embers\n";
    }

    // ------------------------------------------------------------ Every class's Arts in real fights
    {
        int used = 0, fights = 0, finished = 0;
        const CharacterClass cls[] = {CharacterClass::FIGHTER, CharacterClass::WIZARD, CharacterClass::ROGUE,
                                      CharacterClass::CLERIC, CharacterClass::BARD};
        for (int a = 0; a < 5; ++a) {
            for (int pair = 0; pair < 2; ++pair) {
                Game g; solo(g, cls[a], cls[(a + 1) % 5], 100u + static_cast<unsigned>(a * 7 + pair));
                levelTo(g, 40);
                for (int hi = 0; hi < 2; ++hi) {
                    Character& h = T::hero(g, static_cast<size_t>(hi));
                    h.artsLearned = 0xF; h.wardsLearned = 0xFF;
                    h.artEq = {{pair * 2, pair * 2 + 1}};
                    h.wardEq = {{(a + pair) % 8, (a + pair + 3) % 8}};
                    h.flames.fill(1);
                }
                g.simAllPartyAuto();
                g.simBeginFight({"boss_sexton", "enemy_ashbound", "enemy_gloamhound"});
                ++fights;
                int steps = 0;
                int seen[2] = {0, 0};
                while (!g.getEnemies().empty() && !g.isGameOver() && steps < 20000) {
                    g.processTurn(); ++steps;
                    for (int hi = 0; hi < 2; ++hi)
                        for (int k = 0; k < 4; ++k)
                            if (T::hero(g, static_cast<size_t>(hi)).artCd[static_cast<size_t>(k)] > 0) seen[hi] |= 1 << k;
                }
                if (steps < 20000) ++finished;
                for (int m : seen) for (int k = 0; k < 4; ++k) if (m & (1 << k)) ++used;
            }
        }
        CHECK(finished == fights);
        CHECK(used >= 10);
        std::cout << "FIGHTS: " << finished << "/" << fights << " L40 fights with every class's Arts finished; " << used << " Art uses seen\n";
    }

    if (failures) { std::cerr << failures << " failure(s)\n"; return 1; }
    std::cout << "perks_test OK\n";
    return 0;
}
