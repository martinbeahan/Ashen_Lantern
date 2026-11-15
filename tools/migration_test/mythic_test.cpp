// v2.40 Mythical gear + The Drowned Vigil (PR 6 of the combat/progression revamp, docs/MYTHICAL.md):
// item rules (bonus stats, sub-effect from +0, Mythic effect at +25, caps +30 / +32 at rank VII,
// cost, sell), drop rates per source (large-N sims within tolerance), the dry-streak pity (x2 after
// 50, guaranteed on the 150th), one per run, rank 0 / online / Keeper tables get none, the Vigil
// flow (rank gate, three chambers, EL bonus, finale-only Mythical, wipe, save mid-run), the Mythic
// effects, and the save migration (real v2.39 saves load identically, round trip, clamps).
// Run tools/migration_test/run.sh.
#include "Game.h"
#include "CombatCurve.h"
#include "Perks.h"
#include "v239_fixtures.h"
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
std::ostream aout(std::cout.rdbuf());
using namespace dnd;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n"; ++failures; } } while (0)

namespace dnd {
struct GameTestAccess {
    static Character& hero(Game& g, size_t i) { return *g.players_[i]; }
    static size_t heroes(const Game& g) { return g.players_.size(); }
    static void quiet(Game& g) { g.clearEnemies(); g.turnOrder_.clear(); g.currentTurnIndex_ = 0; }
    static void storyDone(Game& g) {
        g.questAct4Complete_ = g.questComplete_ = g.questAct2Complete_ = g.questAct3Complete_ = true;
        g.questBeat_ = 0;
    }
    static void mode(Game& g, int m) { g.soloPlayMode_ = m; }
    static void streak(Game& g, int s) { g.mythicDryStreak_ = s; }
    static void runFlag(Game& g, bool v) { g.mythicThisRun_ = v; }
    static void keeper(Game& g, bool v) { g.dmOnlyTable_ = v; }
    static std::shared_ptr<Item> roll(Game& g) { return g.rollMythicalDrop(-1, -1); }
    static int modeBonus(const Game& g) { return g.encounterModeBonus(); }
    static void allowLegendaryNext(Game& g) { g.pendingBossAllowLegendary_ = true; }
    /** Kill every foe and clear the room through the real path (loot, chamber clear). */
    static void winRoom(Game& g) {
        for (auto& e : g.enemies_) if (e) e->currentHp = 0;
        g.resolveEnemyDefeated(g.players_[0].get(), 0);
    }
    static std::string wards(Game& g, Character& h, int& dmg) { return g.wardsBeforeHit(h, dmg); }
    static void onHit(Game& g, Character* a, Character* t, int dmg) { g.applyMythicOnHit(a, t, dmg); }
    static void onKill(Game& g, Character* a) { g.applyMythicOnKill(a); }
    static void aura(Game& g) { g.refreshHearthAura(); }
    static int vigilChamberRaw(const Game& g) { return g.vigilChamber_; }
    static void defeat(Game& g) { g.checkPartyDefeat(); }
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
static std::string header(const std::string& save) { return save.substr(0, save.find('|')); }
static std::string setHeader(const std::string& save, size_t idx, const std::string& v) {
    auto sec = split(save, '|');
    auto h = split(sec[0], ',');
    if (h.size() <= idx) h.resize(idx + 1, "0");
    h[idx] = v;
    sec[0] = join(h, ',');
    return join(sec, '|');
}

/** A solo Rekindled hero (rank r) in mode m, story done, solo rules on, out of a fight. */
static void solo(Game& g, int rank, int m = static_cast<int>(SoloPlayMode::CRAWL), unsigned seed = 40,
                 CharacterClass hero = CharacterClass::FIGHTER, CharacterClass comp = CharacterClass::CLERIC) {
    g.seedForTesting(seed);
    g.startNewGame(hero, "Mira", static_cast<int>(SoloPlayMode::CRAWL), 1, comp, true);
    g.setAsHost(true);
    g.setStatRulesEnabledForTesting(true);
    T::quiet(g);
    T::storyDone(g);
    for (size_t i = 0; i < T::heroes(g); ++i) {
        T::hero(g, i).prestigeRank = rank;
        T::hero(g, i).prestigeHalf = i != 0;
    }
    g.syncStatRules();
    T::mode(g, m);
}

/** Base-rate sim: n independent eligible kills (streak and run flag reset each time). */
static double baseRate(Game& g, int n) {
    int hits = 0;
    for (int i = 0; i < n; ++i) {
        T::streak(g, 0); T::runFlag(g, false);
        if (T::roll(g)) ++hits;
    }
    return 100.0 * hits / n;
}

int main() {
    const int VIGIL = static_cast<int>(SoloPlayMode::VIGIL);
    const int RAID = static_cast<int>(SoloPlayMode::RAID);
    // ------------------------------------------------------------ Item rules
    {
        auto w = Item::make("Kindlewrath", ItemType::WEAPON, 14, ItemRarity::MYTHICAL, 0);
        CHECK(w->isMythical() && w->legendaryTier() && w->rarityLabel() == "Mythical");
        CHECK(w->legStat0 >= 0 && w->legStat1 >= 0 && w->legStat2 >= 0);
        CHECK(w->legStat0 != w->legStat1 && w->legStat1 != w->legStat2 && w->legStat0 != w->legStat2);
        CHECK(w->mythicEffect == 2 || w->mythicEffect == 3 || w->mythicEffect == 5); // weapon pool
        CHECK(w->subEffectUnlocked());                     // the Legendary sub-effect from +0
        CHECK(w->activeLegStatCount() == 1 && w->legendaryAttrBonus(w->legStat0) == 2 && w->legendaryAttrBonus(w->legStat1) == 0);
        w->upgradeLevel = 10; CHECK(w->activeLegStatCount() == 2 && w->legendaryAttrBonus(w->legStat1) == 2);
        w->upgradeLevel = 20; CHECK(w->activeLegStatCount() == 3 && w->legendaryAttrBonus(w->legStat2) == 2);
        w->upgradeLevel = 24; CHECK(!w->mythicEffectUnlocked());
        w->upgradeLevel = 25; CHECK(w->mythicEffectUnlocked());
        CHECK(w->maxUpgradeLevel() == 30);
        CHECK(w->getDescription().find("✦") != std::string::npos);
        CHECK(w->legendaryBonusText().find("Mythic: ") != std::string::npos);
        for (int i = 0; i < 200; ++i) {
            auto a = Item::make("Hearthwall Plate", ItemType::ARMOR, 12, ItemRarity::MYTHICAL, 0);
            CHECK(a->mythicEffect == 1 || a->mythicEffect == 4); // armor pool: Rekindle / Hearth Aura
            CHECK(a->subEffect == static_cast<int>(LegendarySubEffect::FIGHT_SHIELD));
        }
        // Cost base 90 (15 x (1 + 5)); sell at the 65% cap; shop value x6.
        auto c = Item::make("Firstflame Blade", ItemType::WEAPON, 10, ItemRarity::MYTHICAL, -1);
        CHECK(c->upgradeCost() == 90 * 1 * (1 + 10 / 2));
        CHECK(c->sellPrice(0) == c->shopCost() * 65 / 100);
        CHECK(c->shopCost() == (10 + 1) * 20 * 6);
        // Caps: +30, +32 at Rekindled VII.
        Character h("Mira", CharacterClass::FIGHTER, "t");
        h.prestigeRank = 6; CHECK(h.upgradeCapFor(*c) == 30);
        h.prestigeRank = 7; CHECK(h.upgradeCapFor(*c) == 32);
        auto l = Item::make("Crownbreaker", ItemType::WEAPON, 10, ItemRarity::LEGENDARY, 0);
        CHECK(h.upgradeCapFor(*l) == 25 && l->mythicEffect == 0);
        h.gold = 100000000; c->upgradeLevel = 31; c->bonus = 40;
        std::string err;
        CHECK(h.upgradeOwnedItem(c, err) && c->upgradeLevel == 32);
        CHECK(!h.upgradeOwnedItem(c, err) && err.find("+32") != std::string::npos);
        // Generated bonus: depth / 5 + 4 + 5 (Legendary + 4 + 2).
        for (int i = 0; i < 50; ++i) {
            auto m = LootSystem::generateMythical(30, 0, 3);
            CHECK(m && m->isMythical() && m->mythicEffect > 0);
            const int extra = m->type == ItemType::ARMOR ? (m->name == "Hearthwall Plate" || m->name == "Lantern-Saint Mail" ? 3
                              : (m->name == "Smokeveil Hide" || m->name == "Firstflame Mail" ? 2 : 1)) : 0;
            CHECK(m->bonus == 30 / 5 + 4 + 5 + extra);
        }
    }
    // ------------------------------------------------------------ Tokens (save format)
    {
        auto w = Item::make("Ashfang", ItemType::WEAPON, 16, ItemRarity::MYTHICAL, 2);
        w->upgradeLevel = 27;
        const std::string tok = w->toToken();
        CHECK(split(tok, ':').size() == 11);
        auto r = Item::fromToken(tok);
        CHECK(r && r->isMythical() && r->mythicEffect == w->mythicEffect && r->upgradeLevel == 27 && r->toToken() == tok);
        // Every older rarity keeps its exact 10-field v2.39 token.
        auto l = Item::make("Crownbreaker", ItemType::WEAPON, 12, ItemRarity::LEGENDARY, 0);
        CHECK(split(l->toToken(), ':').size() == 10);
        // Clamps (#91 style): absurd upgrade, effect id, a Mythic effect on a non-Mythical item, rarity > 5.
        auto x = Item::fromToken("Ashfang:16:W:5:2:999:0:1:2:1:77");
        CHECK(x && x->isMythical() && x->upgradeLevel == savecodec::limits::kMaxMythicUpgradeLevel && x->mythicEffect == 0);
        auto y = Item::fromToken("Crownbreaker:12:W:4:0:99:0:1:2:1:3");
        CHECK(y && y->rarity == ItemRarity::LEGENDARY && y->upgradeLevel == 25 && y->mythicEffect == 0);
        auto z = Item::fromToken("Odd:1:W:9:0:0:-1:-1:-1:0");
        CHECK(z && z->rarity == ItemRarity::LEGENDARY); // as in v2.39 (rarity 9 -> 4)
        auto old = Item::fromToken("Ashfang:16:W:5:2:3:0:1:2:1"); // a Mythical without field 11
        CHECK(old && old->isMythical() && old->mythicEffect == 0);
    }
    // ------------------------------------------------------------ Drop rates per source (large N)
    {
        const int N = 400000;
        struct Src { const char* name; int mode; int challenge; double want; };
        const Src srcs[] = {
            {"Drowned Vigil", VIGIL, 18, 7.0}, {"Boss Raid", RAID, 18, 1.5},
            {"Ember Ring Arena", static_cast<int>(SoloPlayMode::ARENA), 18, 1.5},
            {"Endless Deep", static_cast<int>(SoloPlayMode::ENDLESS), 18, 1.5},
            {"Challenge (Leg 18%)", static_cast<int>(SoloPlayMode::CHALLENGE), 18, 3.0},
            {"Challenge (Leg 12%)", static_cast<int>(SoloPlayMode::CHALLENGE), 12, 2.0},
            {"Challenge (Leg 6%)", static_cast<int>(SoloPlayMode::CHALLENGE), 6, 1.0},
            {"Crawl", static_cast<int>(SoloPlayMode::CRAWL), 18, 1.0},
            {"Post-story (Story mode)", static_cast<int>(SoloPlayMode::STORY), 18, 1.0},
        };
        std::printf("| Source | want | measured (N = %d) | tolerance |\n|---|---|---|---|\n", N);
        for (const auto& s : srcs) {
            Game g; solo(g, 1, s.mode, 4000u + static_cast<unsigned>(s.mode));
            g.setChallengeLegendaryChance(s.challenge);
            const double got = baseRate(g, N);
            const double p = s.want / 100.0;
            const double tol = 100.0 * 4.5 * std::sqrt(p * (1 - p) / N); // 4.5 sigma
            std::printf("| %s | %.1f%% | %.3f%% | ±%.3fpp |\n", s.name, s.want, got, tol);
            CHECK(std::fabs(got - s.want) <= tol);
        }
        // The story itself (Act 4 not done) never drops one.
        {
            Game s2; s2.seedForTesting(9); s2.startNewGame(CharacterClass::FIGHTER, "Mira", 0, 1, CharacterClass::CLERIC, true);
            s2.setAsHost(true); s2.setStatRulesEnabledForTesting(true); T::quiet(s2);
            T::hero(s2, 0).prestigeRank = 1; s2.syncStatRules();
            CHECK(s2.mythicBasePermille() == 0 && baseRate(s2, 20000) == 0.0);
        }
    }
    // ------------------------------------------------------------ Pity: x2 after 50 dry, guaranteed on the 150th
    {
        Game g; solo(g, 1, RAID);
        T::streak(g, 0);  CHECK(g.mythicPermilleWithPity() == 15);
        T::streak(g, 49); CHECK(g.mythicPermilleWithPity() == 15);
        T::streak(g, 50); CHECK(g.mythicPermilleWithPity() == 30);
        T::streak(g, 148); CHECK(g.mythicPermilleWithPity() == 30);
        T::streak(g, 149); CHECK(g.mythicPermilleWithPity() == 1000);
        T::streak(g, 149); T::runFlag(g, false);
        auto m = T::roll(g);
        CHECK(m && m->isMythical() && g.getMythicDryStreak() == 0 && g.mythicFoundThisRun());
        Game v; solo(v, 1, VIGIL);
        T::streak(v, 50); CHECK(v.mythicPermilleWithPity() == 140);
        // A miss adds one; the streak is clamped in the save (1000).
        T::streak(g, 10); T::runFlag(g, false);
        int before = g.getMythicDryStreak(); auto r = T::roll(g);
        CHECK(r ? g.getMythicDryStreak() == 0 : g.getMythicDryStreak() == before + 1);
        // Long sim: Raid kills, one Mythical per run (a run = until the hit), never more than 149 dry.
        int maxDry = 0, hits = 0, kills = 0, dry = 0, hitsAfter50 = 0, killsAfter50 = 0;
        T::streak(g, 0);
        for (int i = 0; i < 2000000; ++i) {
            T::runFlag(g, false);
            const int s = g.getMythicDryStreak();
            ++kills;
            if (s >= 50 && s < 149) ++killsAfter50;
            if (T::roll(g)) { ++hits; if (s >= 50 && s < 149) ++hitsAfter50; maxDry = std::max(maxDry, dry); dry = 0; }
            else ++dry;
        }
        CHECK(maxDry <= 149);
        const double after50 = 100.0 * hitsAfter50 / std::max(1, killsAfter50);
        CHECK(std::fabs(after50 - 3.0) < 0.25);
        std::printf("\nPity (Boss Raid, 2,000,000 eligible kills): %d Mythicals, one per %.1f kills (base 1.5%% = 66.7); "
                    "after 50 dry %.2f%% (want 3.0%%); longest dry run %d (cap 149)\n",
                    hits, static_cast<double>(kills) / hits, after50, maxDry);
        // Vigil finale: kills per Mythical.
        Game vv; solo(vv, 1, VIGIL); int vh = 0;
        for (int i = 0; i < 400000; ++i) { T::runFlag(vv, false); if (T::roll(vv)) ++vh; }
        std::printf("Pity (Drowned Vigil finale, 400,000 clears): one Mythical per %.1f clears (base 7%% = 14.3)\n\n",
                    400000.0 / vh);
    }
    // ------------------------------------------------------------ One per run, rank 0, online, Keeper table
    {
        Game g; solo(g, 1, RAID);
        T::streak(g, 149); T::runFlag(g, false);
        CHECK(T::roll(g));
        CHECK(!g.mythicEligibleNow());
        T::streak(g, 149);
        CHECK(!T::roll(g) && g.getMythicDryStreak() == 149); // capped: no roll, the streak waits
        // A new run (begin / finish) clears the cap.
        g.finishBossRaidKeepParty(); CHECK(!g.mythicFoundThisRun());
        CHECK(g.beginBossRaidFromCurrent() && !g.mythicFoundThisRun());

        Game z; solo(z, 0, RAID);
        CHECK(!z.mythicEligibleNow());
        CHECK(baseRate(z, 100000) == 0.0);
        T::streak(z, 149); CHECK(!T::roll(z) && z.getMythicDryStreak() == 149);

        Game k; solo(k, 1, RAID); T::keeper(k, true);
        CHECK(!k.mythicEligibleNow() && baseRate(k, 20000) == 0.0);
        Game j; solo(j, 1, RAID); j.setAsHost(false); // a joiner never rolls (the host decides)
        CHECK(!j.mythicEligibleNow());
        Game off; solo(off, 1, RAID); off.setStatRulesEnabledForTesting(false); off.syncStatRules(); // solo rules off
        CHECK(!off.mythicEligibleNow());
    }
    // ------------------------------------------------------------ Through the real kill-loot path
    {
        // Boss Raid at streak 149: the boss defeat drops a Mythical (not a Legendary), once.
        Game g; solo(g, 1, static_cast<int>(SoloPlayMode::CRAWL));
        CHECK(g.beginBossRaidFromCurrent());
        T::streak(g, 149);
        const size_t bag0 = T::hero(g, 0).inventory.size();
        T::winRoom(g);
        int myth = 0;
        for (const auto& it : T::hero(g, 0).inventory) if (it && it->isMythical()) ++myth;
        CHECK(myth == 1 && T::hero(g, 0).inventory.size() >= bag0 + 1 && g.mythicFoundThisRun());
        CHECK(g.getLastEvent().find("[Mythical]") != std::string::npos);
        // Trash in the crawl never rolls (no Legendary gate): the streak does not move.
        Game c; solo(c, 1, static_cast<int>(SoloPlayMode::CRAWL));
        T::streak(c, 149);
        c.simBeginFight({"enemy_wickthief"});
        T::winRoom(c);
        CHECK(c.getMythicDryStreak() == 149);
        for (const auto& it : T::hero(c, 0).inventory) CHECK(!it || !it->isMythical());
        // The Challenge Dungeon: a Mythical also triggers the Legendary-chance decay.
        Game d; solo(d, 1, static_cast<int>(SoloPlayMode::CRAWL));
        CHECK(d.beginChallengeDungeonFromCurrent(18));
        T::streak(d, 149);
        T::winRoom(d);
        CHECK(d.consumePendingChallengeLegendaryFound());
    }
    // ------------------------------------------------------------ The Drowned Vigil
    {
        Game z; solo(z, 0, static_cast<int>(SoloPlayMode::CRAWL));
        CHECK(!z.beginDrownedVigilFromCurrent() && z.getSoloPlayMode() == 1 && z.getEnemies().empty());

        Game k; solo(k, 1, static_cast<int>(SoloPlayMode::CRAWL)); T::keeper(k, true);
        CHECK(!k.beginDrownedVigilFromCurrent());

        for (int rank : {1, 5, 10}) {
            Game g; solo(g, rank, static_cast<int>(SoloPlayMode::CRAWL), 77u + static_cast<unsigned>(rank));
            CHECK(g.beginDrownedVigilFromCurrent());
            CHECK(g.getSoloPlayMode() == VIGIL && g.getVigilChamber() == 1 && g.isFocusedEndgameRun());
            CHECK(T::modeBonus(g) == 4 + 2 * rank);
            CHECK(g.getEnemies().size() == 4);
            for (const auto& e : g.getEnemies()) CHECK(LootSystem::bossTier(e->enemyId) == 0);
            // A mid-Vigil save keeps the chamber (header 40).
            {
                Game r; CHECK(r.deserialize(g.serialize()));
                CHECK(r.getSoloPlayMode() == VIGIL && r.getVigilChamber() == 1);
            }
            // The elite chambers never roll a Mythical (no boss gate): the streak stays.
            T::streak(g, 149);
            T::winRoom(g);
            CHECK(g.getMythicDryStreak() == 149 && g.getEnemies().empty());
            g.playerAdvanceFromCleared();
            CHECK(g.getVigilChamber() == 2 && g.getEnemies().size() == 2);
            T::winRoom(g);
            g.playerAdvanceFromCleared();
            CHECK(g.getVigilChamber() == 3 && g.getEnemies().size() == 3);
            CHECK(LootSystem::isEndgameBossId(g.getEnemies().front()->enemyId)); // the Drowned boss + 2 acolytes
            {
                Game r; CHECK(r.deserialize(g.serialize()));
                CHECK(r.getVigilChamber() == 3);
            }
            T::winRoom(g); // finale at streak 149: guaranteed Mythical
            int myth = 0;
            for (const auto& it : T::hero(g, 0).inventory) if (it && it->isMythical()) ++myth;
            CHECK(myth == 1 && g.getMythicDryStreak() == 0);
            g.playerAdvanceFromCleared(); // the Vigil is kept: back to the crawl
            CHECK(g.getSoloPlayMode() == 1 && g.getVigilChamber() == 0 && T::vigilChamberRaw(g) == 0 && !g.mythicFoundThisRun());
        }
        // A wipe mid-Vigil: Continue restores the hero and the previous mode.
        Game w; solo(w, 2, static_cast<int>(SoloPlayMode::CRAWL));
        CHECK(w.beginDrownedVigilFromCurrent());
        for (size_t i = 0; i < T::heroes(w); ++i) { auto& p = T::hero(w, i); p.currentHp = 0; p.isDowned = p.isDead = true; }
        T::defeat(w);
        CHECK(w.isGameOver());
        CHECK(w.recoverFromPartyWipe() && w.getSoloPlayMode() == 1 && !w.isGameOver());
    }
    // ------------------------------------------------------------ Mythic effects
    {
        Game g; solo(g, 1, static_cast<int>(SoloPlayMode::CRAWL));
        Character& h = T::hero(g, 0);
        auto arm = Item::make("Hearthwall Plate", ItemType::ARMOR, 12, ItemRarity::MYTHICAL, 0);
        arm->mythicEffect = static_cast<int>(MythicEffect::REKINDLE); arm->upgradeLevel = 25;
        h.equippedArmor = arm; h.refreshDerivedStats();
        h.wardsLearned = 0; h.wardEq = {{-1, -1, -1}};
        h.currentHp = 20;
        int dmg = 500; T::wards(g, h, dmg);
        CHECK(h.rekindleUsed && h.currentHp - dmg == std::max(1, h.maxHp / 4));
        h.currentHp = 20; dmg = 500; T::wards(g, h, dmg);
        CHECK(dmg == 500); // once per battle
        // Hearth Aura: party +1 AC.
        arm->mythicEffect = static_cast<int>(MythicEffect::HEARTH_AURA);
        const int ac0 = T::hero(g, 1).armorClass;
        T::aura(g);
        CHECK(T::hero(g, 1).armorClass == ac0 + 1 && T::hero(g, 1).auraAc == 1);
        arm->upgradeLevel = 24; T::aura(g);
        CHECK(T::hero(g, 1).armorClass == ac0);
        // Ember Echo: the 4th hit repeats for 50%.
        auto wpn = Item::make("Kindlewrath", ItemType::WEAPON, 14, ItemRarity::MYTHICAL, 0);
        wpn->mythicEffect = static_cast<int>(MythicEffect::EMBER_ECHO); wpn->upgradeLevel = 25;
        h.equippedWeapon = wpn;
        Character foe("Dummy", CharacterClass::FIGHTER, "foe-1");
        foe.maxHp = foe.currentHp = 1000;
        for (int i = 0; i < 3; ++i) T::onHit(g, &h, &foe, 20);
        CHECK(foe.currentHp == 1000);
        T::onHit(g, &h, &foe, 20);
        CHECK(foe.currentHp == 990);
        // Cinder Brand: hits Burn.
        wpn->mythicEffect = static_cast<int>(MythicEffect::CINDER_BRAND);
        T::onHit(g, &h, &foe, 24);
        CHECK(foe.status.burn == 2 && foe.status.burnDmg == 4);
        // Lantern's Due: a kill heals the party 5%.
        wpn->mythicEffect = static_cast<int>(MythicEffect::LANTERNS_DUE);
        Character& c = T::hero(g, 1);
        c.currentHp = 1; const int c0 = c.currentHp;
        T::onKill(g, &h);
        CHECK(c.currentHp == c0 + std::max(1, c.maxHp * 5 / 100));
        // Below +25 nothing fires.
        wpn->upgradeLevel = 24; c.currentHp = 1;
        T::onKill(g, &h);
        CHECK(c.currentHp == 1);
    }
    // ------------------------------------------------------------ Save migration
    {
        int n = 0;
        for (const char* old : {v239::kA, v239::kB, v239::kC}) {
            ++n;
            Game g; std::string err;
            CHECK(g.deserialize(old, &err));
            const std::string now = g.serialize();
            // Identical except the two appended header fields (47 streak, 48 run flag) = 0.
            const std::string o(old);
            CHECK(header(now) == header(o) + ",0,0");
            CHECK(now.substr(now.find('|')) == o.substr(o.find('|')));
            CHECK(g.getMythicDryStreak() == 0 && !g.mythicFoundThisRun() && g.getVigilChamber() == 0);
            if (failures) std::cerr << "(after fixture " << n << ")\n";
        }
        // Round trip with real values.
        Game g; solo(g, 1, static_cast<int>(SoloPlayMode::CRAWL));
        CHECK(g.beginBossRaidFromCurrent());
        T::streak(g, 77); T::runFlag(g, true);
        auto w = Item::make("Ashfang", ItemType::WEAPON, 16, ItemRarity::MYTHICAL, 2); w->upgradeLevel = 26;
        T::hero(g, 0).inventory.push_back(w);
        const std::string s = g.serialize();
        Game r; CHECK(r.deserialize(s));
        CHECK(r.getMythicDryStreak() == 77 && r.mythicFoundThisRun() && r.serialize() == s);
        bool found = false;
        for (const auto& it : T::hero(r, 0).inventory) if (it && it->isMythical() && it->upgradeLevel == 26 && it->mythicEffect == w->mythicEffect) found = true;
        CHECK(found);
        // Clamps.
        Game c1; CHECK(c1.deserialize(setHeader(setHeader(s, 47, "-5"), 48, "7")));
        CHECK(c1.getMythicDryStreak() == 0 && !c1.mythicFoundThisRun());
        Game c2; CHECK(c2.deserialize(setHeader(s, 47, "99999999")));
        CHECK(c2.getMythicDryStreak() == Game::kMythicStreakMax);
        Game c3; CHECK(c3.deserialize(setHeader(setHeader(s, 14, "6"), 40, "9")));
        CHECK(c3.getSoloPlayMode() == VIGIL && c3.getVigilChamber() == 3);
        Game c4; CHECK(c4.deserialize(setHeader(setHeader(s, 14, "6"), 40, "0")));
        CHECK(c4.getVigilChamber() == 1);
        Game c5; CHECK(c5.deserialize(setHeader(setHeader(s, 14, "1"), 40, "2")));
        CHECK(c5.getVigilChamber() == 0 && T::vigilChamberRaw(c5) == 0);
        Game c6; CHECK(c6.deserialize(setHeader(s, 14, "7")));
        CHECK(c6.getSoloPlayMode() == 0);
        Game c7; CHECK(!c7.deserialize(setHeader(s, 47, "abc"))); // strict parse: the load is refused, as for any header number
        CHECK(c7.getMythicDryStreak() == 0);
        // A crawl session is its own run: the flag clears on load there, a focused run keeps it.
        Game c8; CHECK(c8.deserialize(setHeader(setHeader(s, 14, "1"), 48, "1")));
        CHECK(!c8.mythicFoundThisRun());
        // Streak travels with the slot through the crawl too.
        CHECK(c8.getMythicDryStreak() == 77);
    }

    if (failures) { std::cerr << failures << " failure(s)\n"; return 1; }
    std::cout << "mythic_test OK\n";
    return 0;
}
