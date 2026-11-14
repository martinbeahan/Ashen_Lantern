// v2.33 Rekindling (prestige, PR 5 of the combat/progression revamp): eligibility (level 50 + story,
// solo, out of a fight, not mid endgame run), the reset/keep matrix, rank bonuses I..X (attributes,
// caps, HP, Embers, flame cap 6, 3rd Ward / Art slots, +1 special, XP / gold, catch-up), attunement,
// the companion's half share, save fields 37-40 (append-only) with older saves, HP / resources on
// load, the Nightmare wipe clearing the rank, online untouched, EL, and fights after a Rekindling.
// Also covers the debug-build-only shortcut (compiled here because tests build without NDEBUG).
// Run tools/migration_test/run.sh.
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
    static void quiet(Game& g) { g.enemies_.clear(); g.turnOrder_.clear(); g.currentTurnIndex_ = 0; }
    static void storyDone(Game& g, bool on) { g.questAct4Complete_ = on; }
    static void mode(Game& g, int m) { g.soloPlayMode_ = m; }
    static int luck(const Game& g, int gold) { return g.luckGold(gold); }
    static void clearRoom(Game& g) { g.enterClearedRoom(g.players_[0].get()); }
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
static std::string editPlayers(const std::string& save, size_t keep) {
    auto sec = split(save, '|');
    auto recs = split(sec[2], ';');
    for (auto& r : recs) {
        if (r.empty()) continue;
        auto f = split(r, ',');
        if (f.size() > keep) f.resize(keep);
        r = join(f, ',');
    }
    sec[2] = join(recs, ';');
    return join(sec, '|');
}
static size_t playerFieldCount(const std::string& save) {
    auto sec = split(save, '|');
    return split(split(sec[2], ';')[0], ',').size();
}
static void solo(Game& g, CharacterClass hero, CharacterClass comp, unsigned seed = 7, int mode = 1, int diff = 1) {
    g.seedForTesting(seed);
    g.startNewGame(hero, "Mira", mode, diff, comp, true);
    g.setAsHost(true);
    T::quiet(g);
}
/** Level 50 with the story done, through the debug shortcut (debug builds and tests only). */
// The bare jump (points / Embers left to spend, gear untouched) — the tests spend them by hand.
static void ready(Game& g) { CHECK(g.debugJumpToRekindleReady(false)); }

int main() {
    // ------------------------------------------------------------ Numbers
    {
        CHECK(prestige::share(10, false) == 10 && prestige::share(10, true) == 5 && prestige::share(3, true) == 1);
        CHECK(prestige::flameCap(1) == 5 && prestige::flameCap(2) == 6);
        CHECK(prestige::wardSlots(2) == 2 && prestige::wardSlots(3) == 3 && prestige::artSlots(4) == 2 && prestige::artSlots(5) == 3);
        CHECK(prestige::extraSpecialUses(5) == 0 && prestige::extraSpecialUses(6) == 1);
        CHECK(std::abs(prestige::xpMult(3, 10, 50) - 1.15) < 1e-9);          // no catch-up before IV
        CHECK(std::abs(prestige::xpMult(4, 10, 50) - 1.2 * 1.5) < 1e-9);     // IV below peak
        CHECK(std::abs(prestige::xpMult(4, 50, 50) - 1.2) < 1e-9);           // at peak: none
        CHECK(prestige::attunedBonus(25, 4, true) == 5 && prestige::attunedBonus(25, 4, false) == 25 && prestige::attunedBonus(4, 4, true) == 4);
        CHECK(prestige::attuneCap(1) == 3 && prestige::attuneCap(10) == 8 && prestige::attuneCap(21) == 13 && prestige::attuneCap(50) == 28);
        CHECK(prestige::title(3) == "Rekindled III" && prestige::title(10) == "Everflame" && prestige::title(0).empty());
        CHECK(perks::embersEarned(1, 1) == 1 && perks::embersEarned(1, 9) == 11 && perks::embersEarned(50, 10) == 42);
        std::cout << "NUMBERS: half share, flame cap 6 (II), slots 3 (III / V), +1 use (VI), XP +5%/rank, catch-up x1.5 (IV), attune floor(L/2)+3\n";
    }

    // ------------------------------------------------------------ Eligibility (optional, never forced)
    {
        Game g; solo(g, CharacterClass::FIGHTER, CharacterClass::CLERIC, 3, 0); // story
        std::string why = g.rekindleBlockReason();
        CHECK(!g.rekindleReady() && contains(why, "story"));
        T::storyDone(g, true);
        T::hero(g, 0).addXpLevels(static_cast<int>(perks::totalXpTo(49)));
        CHECK(T::hero(g, 0).level == 49 && contains(g.rekindleBlockReason(), "level 50"));
        T::storyDone(g, false);
        ready(g);
        CHECK(T::hero(g, 0).level == 50 && T::hero(g, 1).level == 50 && g.isStoryFullyComplete());
        CHECK(g.rekindleReady() && g.canRekindle());
        // Nothing happens by itself: still level 50, rank 0, after more rooms of XP.
        T::clearRoom(g);
        CHECK(T::hero(g, 0).level == 50 && T::hero(g, 0).prestigeRank == 0);
        const std::string st = g.getRekindleState();
        CHECK(contains(st, "R|1|1|0|0|1||50|")); // ready, can, not offered yet, rank 0 -> I
        CHECK(contains(st, "P|3|A 3rd Ward slot"));
        g.markRekindleOffered(); // "Not now"
        CHECK(contains(g.getRekindleState(), "R|1|1|1|0|1|"));
        CHECK(T::hero(g, 0).level == 50 && T::hero(g, 0).prestigeRank == 0); // Not now loses nothing
        // Blocked mid-fight, in an endgame run, on a fallen party, online.
        g.simBeginFight({"enemy_wickthief"});
        CHECK(!g.rekindle(&why) && contains(why, "fight"));
        T::quiet(g);
        T::mode(g, static_cast<int>(SoloPlayMode::RAID));
        CHECK(!g.rekindle(&why) && contains(why, "endgame run"));
        T::mode(g, static_cast<int>(SoloPlayMode::STORY));
        g.setStatRulesEnabledForTesting(false);
        CHECK(!g.rekindle(&why) && contains(why, "solo") && !g.rekindleReady());
        g.setStatRulesEnabledForTesting(true);
        Game j; j.setAsHost(false);
        CHECK(j.deserialize(g.serialize()) && !j.rekindleReady() && !j.canRekindle());
        std::cout << "ELIGIBLE: level 50 + story only; Not now keeps everything; blocked in a fight, endgame run, online\n";
    }

    // ------------------------------------------------------------ Rekindle: reset / keep matrix
    {
        Game g; solo(g, CharacterClass::CLERIC, CharacterClass::FIGHTER, 9, 0); // story +2 HP / +1 use padding
        ready(g);
        Character& h = T::hero(g, 0);
        Character& c = T::hero(g, 1);
        for (int i = 0; i < 16; ++i) h.increaseAttribute(4);
        for (int i = 0; i < 9; ++i) h.increaseAttribute(2);
        CHECK(g.learnPerk(0, 0, perks::STILLFLAME) && g.learnPerk(0, 1, perks::LAST_LIGHT) && g.learnPerk(0, 2, 0));
        h.gold = 12345;
        auto relic = Item::make("Lantern Relic Mace", ItemType::WEAPON, 25, ItemRarity::LEGENDARY, static_cast<int>(CharacterClass::CLERIC));
        h.equippedWeapon = relic;
        h.addToInventory(Item::make("Spare Shield", ItemType::ARMOR, 6, ItemRarity::RARE, -1));
        const size_t bag = h.inventory.size();
        const int respecs = h.freeRespecs;
        const Character fresh("", CharacterClass::CLERIC, "x");
        std::string why;
        CHECK(g.rekindle(&why));
        CHECK(why.empty());
        CHECK(h.level == 1 && h.xp == 0 && h.pendingStatPoints == 0 && h.peakLevel == 50);
        CHECK(h.attributes.wisdom == fresh.attributes.wisdom && h.attributes.constitution == fresh.attributes.constitution);
        CHECK(h.embersSpent() == 0 && h.wardEq[0] == -1 && h.artEq[0] == -1 && h.flames[perks::STILLFLAME] == 0);
        CHECK(h.embersEarned() == 1 && h.freeRespecs == respecs + 1);
        CHECK(h.gold == 12345 && h.inventory.size() == bag && h.equippedWeapon == relic && relic->bonus == 25);
        CHECK(g.isStoryFullyComplete());
        CHECK(h.bonusMaxResources == 1 && h.bonusMaxHp == 2); // story padding kept, odd-CON HP gone
        CHECK(h.prestigeRank == 1 && !h.prestigeHalf && c.prestigeRank == 1 && c.prestigeHalf && c.level == 1);
        CHECK(h.currentHp == h.maxHp && h.resources == h.maxResources && !h.rekindleOffered);
        CHECK(h.effectiveAttr(4) == fresh.attributes.wisdom + 1 + h.legendaryAttrBonus(4)); // +1 all stats
        CHECK(c.effectiveAttr(0) == c.attributes.strength + c.legendaryAttrBonus(0));        // I / 2 = 0
        CHECK(h.attrCapFor(0) == stats::attrCap(1, false) + 1);
        CHECK(h.weaponBonusEff() == 3); // attuned: floor(1 / 2) + 3 = +3 of +25 at level 1
        CHECK(contains(g.getRekindleState(), "|1|" + std::to_string(g.getDifficulty()) + "|3|")); // Sheet cap
        // The companion is attuned too, even though its half share of rank I rounds to 0.
        c.equippedWeapon = Item::make("Ashen Blade", ItemType::WEAPON, 25, ItemRarity::LEGENDARY, 0);
        CHECK(c.prestigeShare() == 0 && c.attuned() && c.weaponBonusEff() == 3);
        CHECK(!g.rekindleReady() && contains(g.rekindleBlockReason(), "level 50"));
        CHECK(contains(g.getRekindleState(), "|Rekindled I|Rekindled II|"));
        CHECK(contains(g.getChatHistory(), "Rekindled I"));
        // Perks relock until 21 (the rank's Ember waits), then the climb is the same 1..50.
        CHECK(!g.learnPerk(0, 2, 0, &why) && contains(why, "level 22"));
        std::cout << "REKINDLE: level 1, base stats, Embers back + free Re-temper; gear, gold, story, padding kept; companion too\n";
    }

    // ------------------------------------------------------------ Ranks I..X: HP, slots, uses, caps
    {
        Game g; solo(g, CharacterClass::WIZARD, CharacterClass::ROGUE, 21, 1);
        T::storyDone(g, true);
        for (int r = 0; r < prestige::kMaxRank; ++r) {
            ready(g);
            CHECK(g.rekindle());
        }
        Character& h = T::hero(g, 0);
        Character& c = T::hero(g, 1);
        CHECK(h.prestigeRank == 10 && c.prestigeRank == 10 && c.prestigeShare() == 5);
        CHECK(contains(g.rekindleBlockReason(), "highest") && !g.rekindleReady());
        CHECK(h.freeRespecs >= 9 && h.freeRespecs <= 9); // capped token count
        CHECK(h.flameCap() == 6 && h.wardSlots() == 3 && h.artSlots() == 3 && c.wardSlots() == 3 && c.artSlots() == 3);
        CHECK(h.embersEarned() == 12 && c.embersEarned() == 5);
        CHECK(h.maxResources == h.formulaMaxResources() + h.bonusMaxResources + 1); // VI
        CHECK(c.maxResources == c.formulaMaxResources() + c.bonusMaxResources);     // half: V, no extra use
        CHECK(h.maxHp == static_cast<int>(std::max(1, h.formulaMaxHp() + h.bonusMaxHp) * 1.2));
        // The 3rd slots work; Wards learned with rank Embers at level 1.
        std::string why;
        CHECK(g.learnPerk(0, 1, perks::LAST_LIGHT) && g.learnPerk(0, 1, perks::ASH_VEIL) && g.learnPerk(0, 1, perks::CLEANSE));
        CHECK(h.wardEq[2] == perks::CLEANSE && h.wardEquipped(perks::CLEANSE));
        CHECK(g.equipPerk(0, 1, 2, perks::LAST_LIGHT) && h.wardEq[2] == perks::LAST_LIGHT && h.wardEq[0] == perks::CLEANSE);
        CHECK(!g.equipPerk(0, 1, 3, perks::LAST_LIGHT, &why));
        CHECK(contains(g.getPerkState(0), "|3|3|10\n")); // header: Ward / Art slots, rank
        for (int i = 0; i < 6; ++i) CHECK(g.learnPerk(0, 0, perks::BRIGHT_EDGE) == (i < 6));
        CHECK(h.flames[perks::BRIGHT_EDGE] == 6 || h.embersFree() == 0);
        // A rank-0 hero has no 3rd slot.
        Game z; solo(z, CharacterClass::FIGHTER, CharacterClass::CLERIC, 2);
        T::hero(z, 0).addXpLevels(static_cast<int>(perks::totalXpTo(30)));
        CHECK(z.learnPerk(0, 1, perks::LAST_LIGHT) && !z.equipPerk(0, 1, 2, perks::LAST_LIGHT, &why) && contains(why, "Rekindling"));
        CHECK(T::hero(z, 0).wardSlots() == 2 && T::hero(z, 0).flameCap() == 5);
        std::cout << "RANKS: X = 12 Embers at level 1, +20% HP, +1 use, flame 6, 3 Ward / Art slots; companion half (V)\n";
    }

    // ------------------------------------------------------------ XP, gold, catch-up, attunement, EL
    {
        Game g; solo(g, CharacterClass::FIGHTER, CharacterClass::CLERIC, 5, 1);
        ready(g);
        const int elBefore = g.currentEncounterLevel();
        T::hero(g, 0).prestigeRank = 4; T::hero(g, 1).prestigeRank = 4; T::hero(g, 1).prestigeHalf = true;
        CHECK(g.currentEncounterLevel() >= elBefore + 1); // 25% of the average rank share (4 / 2)
        CHECK(g.rekindle());
        Character& h = T::hero(g, 0);
        CHECK(std::abs(h.prestigeXpMult() - 1.25 * 1.5) < 1e-9); // rank V, below peak 50
        const int xp0 = h.xp, lv0 = h.level;
        T::clearRoom(g);
        CHECK(h.level > lv0 || h.xp > xp0);
        CHECK(T::luck(g, 1000) >= 1250); // +25% gold (and any CHA luck)
        h.equippedArmor = Item::make("Ashen Plate", ItemType::ARMOR, 20, ItemRarity::LEGENDARY, 0);
        h.calculateAC();
        CHECK(h.armorBonusEff() == h.level / 2 + 3);
        h.level = 20; CHECK(h.armorBonusEff() == 13);
        h.level = 50; CHECK(h.armorBonusEff() == 20); // +20 fits under 28: a level-50 hero is uncapped again
        h.level = lv0;
        // Rank 0: never capped, so ordinary heroes see no change.
        Game z; solo(z, CharacterClass::FIGHTER, CharacterClass::CLERIC, 5, 1);
        T::hero(z, 0).equippedArmor = Item::make("Ashen Plate", ItemType::ARMOR, 20, ItemRarity::LEGENDARY, 0);
        CHECK(T::hero(z, 0).armorBonusEff() == 20 && std::abs(T::hero(z, 0).prestigeXpMult() - 1.0) < 1e-9);
        CHECK(T::luck(z, 1000) == T::luck(z, 1000) && T::luck(z, 1000) < 1250);
        std::cout << "REWARDS: +5% XP / gold per rank, catch-up x1.5 below peak, gear attuned to floor(level / 2) + 3 (rank >= 1 only), EL +25% of rank\n";
    }

    // ------------------------------------------------------------ Saves: fields 37-40, older saves, HP on load
    {
        Game g; solo(g, CharacterClass::BARD, CharacterClass::WIZARD, 31, 0);
        T::storyDone(g, true);
        for (int r = 0; r < 6; ++r) { ready(g); CHECK(g.rekindle()); }
        Character& h = T::hero(g, 0);
        h.addXpLevels(static_cast<int>(perks::totalXpTo(35)));
        for (int i = 0; i < 10; ++i) h.increaseAttribute(2);
        CHECK(g.learnPerk(0, 1, perks::LAST_LIGHT) && g.learnPerk(0, 1, perks::ASH_VEIL) && g.learnPerk(0, 1, perks::CLEANSE));
        CHECK(g.learnPerk(0, 2, 0) && g.learnPerk(0, 2, 1) && g.learnPerk(0, 2, 2));
        for (int i = 0; i < 6; ++i) g.learnPerk(0, 0, perks::THICK_WAX);
        CHECK(h.flames[perks::THICK_WAX] == 6 && h.artEq[2] == 2 && h.wardEq[2] == perks::CLEANSE);
        g.markRekindleOffered();
        const std::string s = g.serialize();
        CHECK(playerFieldCount(s) == 41);
        Game r; r.setAsHost(true);
        CHECK(r.deserialize(s));
        r.syncStatRules();
        Character& rh = T::hero(r, 0);
        CHECK(rh.prestigeRank == 6 && !rh.prestigeHalf && T::hero(r, 1).prestigeHalf && rh.rekindleOffered);
        CHECK(rh.wardEq[2] == perks::CLEANSE && rh.artEq[2] == 2 && rh.flames[perks::THICK_WAX] == 6);
        if (rh.maxHp != h.maxHp) std::cerr << "maxHp " << rh.maxHp << " vs " << h.maxHp << "\n";
        CHECK(rh.maxHp == h.maxHp && rh.bonusMaxHp == h.bonusMaxHp);
        if (rh.maxResources != h.maxResources) std::cerr << "maxRes " << rh.maxResources << " vs " << h.maxResources << "\n";
        CHECK(rh.maxResources == h.maxResources && rh.bonusMaxResources == h.bonusMaxResources);
        // Twice more: nothing creeps.
        Game r2; r2.setAsHost(true);
        CHECK(r2.deserialize(r.serialize()));
        r2.syncStatRules();
        CHECK(T::hero(r2, 0).maxHp == h.maxHp && T::hero(r2, 0).bonusMaxResources == h.bonusMaxResources);
        // A v2.32 save (37 fields): rank 0, no 3rd slots, explainer not seen.
        Game o; o.setAsHost(true);
        Game base; solo(base, CharacterClass::ROGUE, CharacterClass::CLERIC, 8);
        base.debugJumpToRekindleReady(false);
        const std::string v232 = editPlayers(base.serialize(), 37);
        CHECK(playerFieldCount(v232) == 37);
        CHECK(o.deserialize(v232));
        CHECK(T::hero(o, 0).prestigeRank == 0 && T::hero(o, 0).wardEq[2] == -1 && !T::hero(o, 0).rekindleOffered && !T::hero(o, 1).prestigeHalf);
        // Slot card summary carries the rank (prestige is per save slot: it lives in the slot's save).
        Game::SaveSummary sum;
        CHECK(Game::describeSave(s, &sum) && sum.prestigeRank == 6);
        CHECK(Game::describeSave(v232, &sum) && sum.prestigeRank == 0);
        std::cout << "SAVES: fields 37-40 appended; v2.32 saves load at rank 0; HP / uses never creep on reload\n";
    }

    // ------------------------------------------------------------ Nightmare wipe clears the rank
    {
        Game g; solo(g, CharacterClass::FIGHTER, CharacterClass::CLERIC, 41, 1, 3); // Nightmare
        T::storyDone(g, true);
        for (int r = 0; r < 5; ++r) { ready(g); CHECK(g.rekindle()); }
        ready(g);
        Character& h = T::hero(g, 0);
        while (h.pendingStatPoints > 0 && h.increaseAttribute(0)) {}
        CHECK(h.attributes.strength == stats::attrCap(50, true) + 5);
        CHECK(g.learnPerk(0, 1, perks::LAST_LIGHT) && g.learnPerk(0, 1, perks::ASH_VEIL) && g.learnPerk(0, 1, perks::CLEANSE));
        for (int i = 0; i < 6; ++i) g.learnPerk(0, 0, perks::TEMPERED_ARM);
        h.gold = 777;
        const std::string s = g.serialize();
        const std::string cleared = Game::clearPrestigeInSave(s);
        CHECK(!cleared.empty());
        Game r; r.setAsHost(true);
        CHECK(r.deserialize(cleared));
        r.syncStatRules();
        Character& rh = T::hero(r, 0);
        CHECK(rh.prestigeRank == 0 && T::hero(r, 1).prestigeRank == 0 && !T::hero(r, 1).prestigeHalf);
        CHECK(rh.level == 50 && rh.gold == 777 && rh.flames[perks::TEMPERED_ARM] <= 5 && rh.wardEq[2] == -1);
        CHECK(rh.attributes.strength == stats::attrCap(50, true) && rh.embersSpent() <= rh.embersEarned());
        CHECK(rh.currentHp <= rh.maxHp && rh.maxHp > 0);
        // Same result as clearing the live game.
        g.clearPrestigeForNightmare();
        Game r3; r3.setAsHost(true);
        CHECK(r3.deserialize(g.serialize()));
        r3.syncStatRules();
        CHECK(T::hero(r3, 0).prestigeRank == 0 && T::hero(r3, 0).attributes.strength == rh.attributes.strength
              && T::hero(r3, 0).maxHp == rh.maxHp && T::hero(r3, 0).pendingStatPoints == rh.pendingStatPoints);
        CHECK(Game::clearPrestigeInSave("garbage").empty());
        std::cout << "NIGHTMARE: a wipe clears the rank (perks and caps trimmed, points refunded); level, gold, gear kept\n";
    }

    // ------------------------------------------------------------ Online: no prestige bonuses
    {
        Game g; solo(g, CharacterClass::FIGHTER, CharacterClass::CLERIC, 12);
        T::storyDone(g, true);
        ready(g); CHECK(g.rekindle());
        Game j; j.setAsHost(false);
        CHECK(j.deserialize(g.serialize()));
        Character& jh = T::hero(j, 0);
        CHECK(!jh.perksOn() && jh.prestigeActive() == 0);
        CHECK(jh.effectiveAttr(0) == jh.attributes.strength + jh.legendaryAttrBonus(0));
        CHECK(std::abs(jh.prestigeXpMult() - 1.0) < 1e-9 && jh.wardSlots() == 2);
        jh.equippedWeapon = Item::make("Ashen Blade", ItemType::WEAPON, 25, ItemRarity::LEGENDARY, 0);
        CHECK(!jh.attuned() && jh.weaponBonusEff() == 25); // no attunement online
        std::string why;
        CHECK(!j.rekindle(&why) && contains(why, "solo"));
        std::cout << "ONLINE: classic rules — no Rekindling bonuses or attunement, cannot Rekindle\n";
    }

    // ------------------------------------------------------------ Fights after a Rekindling finish
    {
        int fights = 0, finished = 0, won = 0;
        for (int rank : {1, 5, 10}) {
            for (unsigned seed = 0; seed < 4; ++seed) {
                Game g; solo(g, CharacterClass::FIGHTER, CharacterClass::CLERIC, 300 + seed, 1);
                T::storyDone(g, true);
                for (int r = 0; r < rank; ++r) { ready(g); g.rekindle(); }
                g.simAllPartyAuto();
                g.simBeginFight({"enemy_wickthief", "enemy_gloamhound"});
                ++fights;
                int steps = 0;
                while (!g.getEnemies().empty() && !g.isGameOver() && steps < 20000) { g.processTurn(); ++steps; }
                if (steps < 20000) ++finished;
                if (g.getEnemies().empty()) ++won;
            }
        }
        CHECK(finished == fights);
        std::cout << "FIGHTS: " << finished << "/" << fights << " level-1 fights at ranks I / V / X finished (" << won << " won)\n";
    }

    // ------------------------------------------------------------ v2.33 rebalance: EL follows real power
    {
        // The bare jump: level 50, every point and Ember unspent, starter gear (the old shortcut).
        Game bare; solo(bare, CharacterClass::FIGHTER, CharacterClass::WIZARD, 21);
        T::storyDone(bare, true);
        CHECK(bare.debugJumpToRekindleReady(false));
        const curve::EncounterLevels b = bare.currentEncounterLevels();
        CHECK(b.offEl < 20 && b.defEl < 35 && b.el < 30);  // no full-power level-50 foes
        CHECK(b.offEl <= b.defEl);                        // offense is what's missing most
        // Online Keeper rules: no measuring, the v2.32 level + gear EL (gear floor: level - 2).
        {
            std::vector<curve::PartyMember> pm;
            for (const auto& p : bare.getPlayers()) pm.push_back(curve::memberOf(*p, false));
            const curve::EncounterLevels o = curve::encounterLevels(pm, 0);
            CHECK(o.el == 48 && o.offEl == 48 && o.defEl == 48);
        }
        // The debug shortcut now builds the hero like a player: points, Embers and gear all used.
        Game built; solo(built, CharacterClass::FIGHTER, CharacterClass::WIZARD, 21);
        T::storyDone(built, true);
        CHECK(built.debugJumpToRekindleReady());
        for (size_t i = 0; i < 2; ++i) {
            const Character& p = T::hero(built, i);
            CHECK(p.level == 50 && p.embersFree() == 0 && p.wardsLearned != 0 && p.artsLearned != 0);
            CHECK(p.pendingStatPoints == 0 || !p.canRaise(p.mainStatIndex()));
            CHECK(p.equippedWeapon && p.equippedWeapon->bonus >= 25);
            CHECK(p.equippedArmor && p.equippedArmor->bonus >= curve::starterArmorBase(p.characterClass) + 25);
            CHECK(p.inventory.size() >= 2); // the starter pieces went to the bag
        }
        const curve::EncounterLevels k = built.currentEncounterLevels();
        CHECK(k.offEl == k.defEl && k.el >= 50 && k.el <= 56); // at (or above) the model: full EL
        // A model hero is measured at its own level on both axes (no change for built heroes).
        {
            std::vector<curve::PartyMember> pm;
            for (CharacterClass c : {CharacterClass::FIGHTER, CharacterClass::WIZARD}) {
                Character r = curve::makeReferenceHero(c, 30);
                r.statRules = true;
                pm.push_back(curve::memberOf(r, true));
            }
            const curve::EncounterLevels m = curve::encounterLevels(pm, 2);
            CHECK(m.offEl == 30 && m.defEl == 30 && m.el == 32 && m.mode == 2);
        }
        // Spent Embers count a little (+0.1 EL each), never as much as they buy.
        CHECK(std::abs(curve::kEmberFollow - 0.10) < 1e-9);
        // Split foe numbers: a low offense level means less HP and AC; defense sets the hits.
        const curve::FoeStats lo = curve::foeStatsSplit(curve::Role::SKIRMISHER, 10, 30, 0, 1, 2);
        const curve::FoeStats hi = curve::foeStatsSplit(curve::Role::SKIRMISHER, 30, 30, 0, 1, 2);
        CHECK(lo.maxHp < hi.maxHp && lo.ac < hi.ac && lo.toHit == hi.toHit && lo.avgDmgX10 == hi.avgDmgX10);
        CHECK(curve::foeStats(curve::Role::BOSS, 32, 1, 2, 1.0, 30).maxHp
              == curve::foeStatsSplit(curve::Role::BOSS, 30, 30, 2, 1, 2).maxHp); // single-axis form unchanged
        std::cout << "REBALANCE: EL follows the party's real offense / defense (bare L50: EL " << b.el
                  << ", off " << b.offEl << ", def " << b.defEl << "); debug shortcut builds like a player (EL "
                  << k.el << "); online unmeasured\n";
    }

    if (failures) { std::cerr << failures << " failure(s)\n"; return 1; }
    std::cout << "prestige_test OK\n";
    return 0;
}
