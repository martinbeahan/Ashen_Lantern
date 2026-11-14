// v2.29 combat curve (PR 1 of the combat/progression revamp): Encounter Level, Reference
// Lamplighter, role budgets, hit clamp, overkill guard, Rekindle/Pressure, surprise-boss cap +
// warning, and the four audit bug fixes (a) enemy AC on reload, (b) CON-point / story padding HP
// at level-up, (c) several levels from one XP award, (d) Legendary stats for HP / initiative /
// Search / Healing Word. Also: v2.28 saves (incl. mid-fight) load cleanly, online is untouched.
// Run tools/migration_test/run.sh.
#include "Game.h"
#include "CombatCurve.h"
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
std::ostream aout(std::cout.rdbuf());
using namespace dnd;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n"; ++failures; } } while (0)

static bool contains(const std::string& s, const std::string& sub) { return s.find(sub) != std::string::npos; }

/** v2.28-shaped header (37 fields: E2 marker + story choices). */
static std::string header(int beat, int difficulty, bool gameOver = false, int mode = 0) {
    std::vector<std::string> h(37, "0");
    h[0] = "solo-sid"; h[1] = "5"; h[2] = gameOver ? "1" : "0"; h[6] = "";
    h[8] = std::to_string(beat);
    h[9] = "1"; h[10] = beat > 6 ? "1" : "0"; h[11] = "1";
    h[14] = std::to_string(mode);
    h[15] = std::to_string(difficulty);
    h[33] = "5";
    h[34] = "E2"; h[35] = "0"; h[36] = "0";
    std::string out;
    for (size_t i = 0; i < h.size(); ++i) { if (i) out += ","; out += h[i]; }
    return out;
}
// v2.28 hero: Fighter L3, CON 15 (an odd point), max HP 33 = formula 29 + story padding 2 + odd CON 2.
static const char* kParty28 =
    "Mira,0,3,250,33,33,2,2,0,16,12,15,8,10,10,50,3,local-player,Longsword:0:W:0:0:0:-1:-1:-1:0,Chain Shirt:3:A:0:0:0:-1:-1:-1:0,0,0,0,0,0,0,;"
    "Corvin (NPC),1,3,250,24,24,6,6,0,8,14,12,16,10,10,0,2,ally-1,None:0:W:0:-1:0,None:0:A:0:-1:0,0,0,0,0,0,1,;";
// v2.28 mid-fight foe: 19 fields (no AC saved). Rogue: 10 + DEX 16 (+3) + Leather 1 = 14.
static const char* kFoe28 = "Wickthief,2,1,0,9,9,0,0,0,10,16,12,12,10,14,0,5,enemy_wickthief-1,enemy_wickthief;";

static std::string save28(int beat, int difficulty, bool foes, bool gameOver = false) {
    return header(beat, difficulty, gameOver) + "|Room~Event|" + kParty28 + "|" + (foes ? kFoe28 : "") + "|chat~|journal~";
}

static std::shared_ptr<Item> legendary(ItemType t, int s0, int s1, int s2) {
    auto it = Item::make("Test Relic", t, 2, ItemRarity::COMMON, -1);
    it->rarity = ItemRarity::LEGENDARY;
    it->upgradeLevel = 20; // all three bonus stats active
    it->legStat0 = s0; it->legStat1 = s1; it->legStat2 = s2;
    return it;
}

int main() {
    // ---------------------------------------------------------------- curve primitives
    {
        // Hit clamp: 15%..90% whatever the numbers.
        CHECK(std::fabs(curve::hitChance(0, 100) - 0.15) < 1e-9);
        CHECK(std::fabs(curve::hitChance(100, 5) - 0.90) < 1e-9);
        CHECK(std::fabs(curve::hitChance(5, 13) - 0.65) < 1e-9); // the curve's target: need 8+
        CHECK(!curve::attackHits(1, 100, 5).hit);                 // natural 1 still misses
        CHECK(curve::attackHits(20, 0, 100).hit);                 // natural 20 still hits
        CHECK(curve::attackHits(2, 100, 5).clamped && !curve::attackHits(2, 100, 5).hit);
        // Overkill guard: 45% of max HP, at least 1.
        CHECK(curve::overkillCap(50, 40) == 18);
        CHECK(curve::overkillCap(10, 40) == 10);
        CHECK(curve::overkillCap(50, 1) == 1);
        CHECK(curve::overkillCap(50, 40, /*exempt=*/true) == 50);
        // EL tables cover 1..50 (and mode headroom): every stat grows with level.
        for (int L = 2; L <= curve::kTableMaxLevel; ++L) {
            const auto& a = curve::reference(L - 1);
            const auto& b = curve::reference(L);
            CHECK(b.atk >= a.atk && b.ac >= a.ac && b.hp > a.hp && b.dmg >= a.dmg);
            const auto foe = curve::foeStats(curve::Role::SKIRMISHER, L, 1, 2);
            CHECK(foe.maxHp > 0 && foe.avgDmgX10 >= 30);
            // Targets: a reference hero hits a standard foe ~65% (need 8+) and is hit ~50%.
            CHECK(std::fabs((foe.ac - b.atk) - 9.0) <= 1.0);
            CHECK(std::fabs((b.ac - foe.toHit) - 10.0) <= 1.0);
        }
        // Encounter Level: gear counts 50% (surplus clamped -4..+8), prestige 25%, window L-2..L+6.
        auto member = [](int L, int w, int a, int base, int pr) { return curve::PartyMember{L, w, a, base, pr}; };
        CHECK(curve::encounterLevel({member(10, 5, 8, 3, 0), member(10, 5, 5, 0, 0)}, 0) == 10);
        CHECK(curve::encounterLevel({member(10, 15, 18, 3, 0), member(10, 15, 15, 0, 0)}, 0) == 14); // +20 surplus -> +8 -> +4
        CHECK(curve::encounterLevel({member(10, 0, 3, 3, 0), member(10, 0, 0, 0, 0)}, 0) == 8);      // -10 -> -4 -> -2
        CHECK(curve::encounterLevel({member(10, 5, 8, 3, 4), member(10, 5, 5, 0, 4)}, 0) == 11);     // prestige 4 -> +1
        CHECK(curve::encounterLevel({member(10, 5, 8, 3, 0), member(10, 5, 5, 0, 0)}, 2) == 12);     // Raid +2
        CHECK(curve::encounterLevel({member(50, 25, 28, 3, 0), member(50, 25, 25, 0, 0)}, 3) == 53); // level 50 works
        // Budgets: Medium 2-hero room = 2.4 units; surprise boss <= 1.5x.
        CHECK(std::fabs(curve::roomBudget(2, 1, 1.0) - 2.4) < 1e-9);
        CHECK(std::fabs(curve::roomBudget(2, 3, curve::kRoomSoftOpen) - 1.92) < 1e-9);
        std::cout << "CURVE: clamp 15-90%, guard 45%, EL 1..50 tables, budgets OK\n";
    }

    // ---------------------------------------------------------------- (c) multi-level XP
    {
        Character c("Tess", CharacterClass::FIGHTER);
        CHECK(c.level == 1);
        const int gained = c.addXpLevels(100 + 200 + 300 + 50); // L1->L4 with 50 left over
        CHECK(gained == 3);
        CHECK(c.level == 4 && c.xp == 50);
        CHECK(c.pendingStatPoints == 6);
        CHECK(c.currentHp == c.maxHp);
        CHECK(!c.addXp(0) && c.level == 4);
        std::cout << "BUG C: one award L1 -> L4, 6 stat points\n";
    }

    // ---------------------------------------------------------------- (b) CON point + story padding
    {
        Character c("Tess", CharacterClass::FIGHTER); // CON 14
        c.addXpLevels(100 + 200); // L3
        c.pendingStatPoints = 1;
        const int before = c.maxHp;
        c.increaseAttribute(2); // CON 15: odd point, formula unchanged, +3 HP right away
        CHECK(c.maxHp == before + 3);
        const int formulaBefore = c.formulaMaxHp();
        c.addXpLevels(300); // L4: formula adds hd/2+1+CON = 8; the odd-point HP must survive
        CHECK(c.formulaMaxHp() == formulaBefore + 8);
        CHECK(c.maxHp == c.formulaMaxHp() + 3);

        Game g; // story start (Easy): +2 padding on both party members
        const Character& hero = *g.getPlayers().front();
        CHECK(hero.bonusMaxHp == 2 && hero.maxHp == hero.formulaMaxHp() + 2);
        Character copy("x", CharacterClass::FIGHTER);
        copy = hero;
        copy.addXpLevels(100);
        CHECK(copy.level == 2 && copy.maxHp == copy.formulaMaxHp() + 2);

        // A v2.28 save: the bonus is derived from its saved max HP, so the next level-up keeps it.
        Game old;
        CHECK(old.deserialize(save28(3, 1, false)));
        const Character& m = *old.getPlayers().front();
        CHECK(m.maxHp == 33);           // unchanged on load
        CHECK(m.bonusMaxHp == 33 - m.formulaMaxHp());
        Character m2("y", CharacterClass::FIGHTER);
        m2 = m;
        m2.addXpLevels(m2.level * 100 - m2.xp); // L4
        CHECK(m2.maxHp == m2.formulaMaxHp() + (33 - m.formulaMaxHp()));
        std::cout << "BUG B: odd CON point and story +2 HP kept through level-ups (v2.28 save too)\n";
    }

    // ---------------------------------------------------------------- (d) Legendary stats count
    {
        Character c("Tess", CharacterClass::CLERIC);
        c.addXpLevels(100 + 200); // L3 (at L1 the 12-HP floor would hide a +1 CON mod)
        const int hp0 = c.formulaMaxHp(), init0 = c.initiativeModifier();
        const int search0 = c.searchScore(), heal0 = c.healingWordModifier();
        // Armor: +CON +DEX +INT; weapon: +WIS +CON +STR. Two CON points = +1 CON mod.
        c.equippedArmor = legendary(ItemType::ARMOR, 2, 1, 3);
        c.equippedWeapon = legendary(ItemType::WEAPON, 4, 2, 0);
        CHECK(c.effectiveAttr(2) == c.attributes.constitution + 2);
        // CON 14 -> 16 (+1 mod): +1 HP per level after the first (level 1 sits on the 12 floor).
        CHECK(c.formulaMaxHp() == hp0 + 2);
        CHECK(c.searchScore() == search0 + 1);
        // DEX 10 -> 11 and WIS 16 -> 17: same modifiers, so bump the base to cross a boundary.
        c.attributes.dexterity = 11; c.attributes.wisdom = 17;
        CHECK(c.initiativeModifier() == Attributes::getModifier(12));
        CHECK(c.healingWordModifier() == Attributes::getModifier(18));
        CHECK(c.initiativeModifier() > init0 || init0 == Attributes::getModifier(12));
        CHECK(c.healingWordModifier() > heal0);

        // Through the game: equipping a Legendary CON armour raises max HP (and current HP).
        Game g;
        Character& hero = const_cast<Character&>(*g.getPlayers().front());
        const int maxBefore = hero.maxHp;
        hero.addToInventory(legendary(ItemType::ARMOR, 2, 2, 2)); // +3 CON: 14 -> 17 (+1 mod)
        CHECK(g.equipInventoryItem(hero.name, static_cast<int>(hero.inventory.size()) - 1));
        CHECK(hero.maxHp == maxBefore + 1);
        CHECK(hero.currentHp == hero.maxHp);
        CHECK(g.unequipSlot(hero.name, 1));
        CHECK(hero.maxHp == maxBefore);
        std::cout << "BUG D: Legendary CON/DEX/INT/WIS reach HP, initiative, Search, Healing Word\n";
    }

    // ---------------------------------------------------------------- (a) enemy AC survives reload
    {
        Game g; // story start: Millhollow scout, fitted to the curve
        const auto& foes = g.getEnemies();
        CHECK(foes.size() == 1);
        const Character& scout = *foes.front();
        CHECK(scout.curveEL >= 1 && scout.curveRole == static_cast<int>(curve::Role::SKIRMISHER));
        Character probe("p", scout.characterClass);
        probe.attributes = scout.attributes;
        probe.calculateAC();
        CHECK(scout.armorClass != probe.armorClass); // curve AC differs from the class formula
        const std::string s = g.serialize();
        Game r;
        CHECK(r.deserialize(s));
        const Character& back = *r.getEnemies().front();
        CHECK(back.armorClass == scout.armorClass);
        CHECK(back.maxHp == scout.maxHp && back.currentHp == scout.currentHp);
        CHECK(back.curveEL == scout.curveEL && back.curveToHit == scout.curveToHit);
        CHECK(back.curveAvgDmgX10 == scout.curveAvgDmgX10 && back.curveRole == scout.curveRole);
        CHECK(r.serialize() == s); // stable round trip
        CHECK(contains(g.getPartyStatus(), "Curve: EL "));
        std::cout << "BUG A: foe AC " << scout.armorClass << " (formula " << probe.armorClass
                  << ") kept through save/load\n";
    }

    // ---------------------------------------------------------------- v2.28 saves load cleanly
    {
        Game g;
        CHECK(g.deserialize(save28(8, 1, /*foes=*/true)));
        CHECK(g.getEnemies().size() == 1);
        const Character& w = *g.getEnemies().front();
        CHECK(w.armorClass == 14 && w.curveEL == 0); // legacy foe keeps the old rules
        CHECK(w.maxHp == 9 && w.currentHp == 9);
        CHECK(g.getPlayers().front()->maxHp == 33);
        CHECK(g.getPressure() == 0 && g.getRekindleRooms() == 0);
        const std::string s = g.serialize();
        CHECK(contains(s.substr(0, s.find('|')), ",S3,"));
        Game again;
        CHECK(again.deserialize(s));
        CHECK(again.getEnemies().front()->armorClass == 14);
        CHECK(again.serialize() == s);
        // A v2.29 save is still readable by the v2.28 slot-card reader path (describeSave).
        Game::SaveSummary sum;
        CHECK(Game::describeSave(s, &sum) && sum.heroName == "Mira" && sum.level == 3);
        std::cout << "V2.28 MID-FIGHT SAVE: loads, foe AC 14, re-saves with v2.29 fields\n";
    }

    // ---------------------------------------------------------------- Pressure / Rekindle saved
    {
        std::string s = save28(8, 1, false);
        // Put v2.29 curve fields on the header: pressure 3, rekindle 1.
        const size_t bar = s.find('|');
        s = s.substr(0, bar) + ",S3,3,1,0,1,0,0" + s.substr(bar);
        Game g;
        CHECK(g.deserialize(s));
        CHECK(g.getPressure() == 3 && g.getRekindleRooms() == 1);
        CHECK(contains(g.getPartyStatus(), "Pressure 3/4"));
        // Hard wipe -> Continue: 3 Rekindle rooms; the respawned room uses one ("The dark draws back").
        std::string hard = save28(8, 2, false, /*gameOver=*/true);
        Game w;
        CHECK(w.deserialize(hard));
        CHECK(w.recoverFromPartyWipe());
        CHECK(w.getPressure() == 0);
        CHECK(w.getRekindleRooms() == 3 || w.getRekindleRooms() == 2);
        if (!w.getEnemies().empty()) CHECK(contains(w.getChatHistory(), "The dark draws back."));
        std::cout << "PRESSURE/REKINDLE: saved, wipe grants Rekindle (" << w.getRekindleRooms() << " left)\n";
    }

    // ---------------------------------------------------------------- Surprise boss: Elite, 1.5x cap, warning
    {
        bool seen = false, warned = false;
        for (int tries = 0; tries < 400 && !seen; ++tries) {
            Game g;
            CHECK(g.deserialize(save28(3, 1, false))); // Crypt Doors cleared, Medium
            g.playerAdvanceFromCleared();              // -> Bone Gallery (18% Ossuary Prior)
            if (g.getQuestBeat() != 4 || g.getEnemies().empty()) continue;
            const Character& b = *g.getEnemies().front();
            if (b.enemyId != "boss_prior") continue;
            seen = true;
            CHECK(g.getEnemies().size() == 1);
            CHECK(b.curveSurprise && b.curveWarnTurns == 1);
            CHECK(b.curveRole == static_cast<int>(curve::Role::ELITE));
            const double cap = curve::kSurpriseBudgetCap * curve::roomBudget(2, 1, 1.0);
            // v2.31: solo foes carry the universal-stats toughness (an Elite here: "others").
            const auto st = curve::foeStats(curve::Role::ELITE, b.curveEL, 1, 2,
                                            cap / 2.5 * curve::statsToughness(b.curveEL, false, false));
            CHECK(b.maxHp == st.maxHp);
            // It is weaker than the full Boss role would have been.
            CHECK(b.maxHp < curve::foeStats(curve::Role::BOSS, b.curveEL, 1, 2).maxHp);
            // Warning survives a reload, then the boss spends its first turn on it.
            Game r; CHECK(r.deserialize(g.serialize()));
            CHECK(r.getEnemies().front()->curveWarnTurns == 1);
            for (int step = 0; step < 40 && !warned; ++step) {
                Character* cur = r.getCurrentActor();
                if (!cur) break;
                const bool hero = cur == r.getPlayers().front().get();
                if (hero) r.playerAttack(0); else r.processTurn();
                if (r.getEnemies().empty()) break;
                if (contains(r.getLastEvent(), "WARNING! Bones knit into a mitre")) warned = true;
            }
            if (!r.getEnemies().empty() && warned) CHECK(r.getEnemies().front()->curveWarnTurns == 0);
        }
        CHECK(seen);
        CHECK(warned);
        std::cout << "SURPRISE BOSS: Ossuary Prior as Elite at 1.5x budget, 1-turn warning\n";
    }

    // ---------------------------------------------------------------- Online stays as-is
    {
        Game host;
        host.startDmSession("Keeper");
        host.addAlly("Guest", CharacterClass::FIGHTER);
        host.dmBeginDungeon();
        CHECK(!host.encounterCurveActive());
        for (const auto& e : host.getEnemies()) CHECK(e->curveEL == 0);
        CHECK(!contains(host.getPartyStatus(), "Curve:"));
        Game client;
        client.prepareClientJoin();
        CHECK(!client.encounterCurveActive());
        std::cout << "ONLINE: Keeper table and joiner keep the old rules\n";
    }

    // ---------------------------------------------------------------- Room budget fit
    {
        // Crawl, Medium, deep packs: never more than 4 foes; room cost lands near budget.
        int rooms = 0;
        for (int i = 0; i < 60; ++i) {
            Game g;
            g.startNewGame(CharacterClass::FIGHTER, "Hero", 1, 1);
            for (int k = 0; k < 6; ++k) {
                if (!g.getEnemies().empty()) {
                    CHECK(g.getEnemies().size() <= 4);
                    for (const auto& e : g.getEnemies()) CHECK(e->curveEL >= 1);
                    ++rooms;
                }
                // Clear the room by force and move on.
                for (const auto& e : g.getEnemies()) const_cast<Character&>(*e).currentHp = 0;
                if (!g.getEnemies().empty()) g.playerAttack(0);
                g.playerAdvanceFromCleared();
            }
        }
        CHECK(rooms > 0);
        std::cout << "BUDGET: " << rooms << " crawl rooms fitted, <= 4 foes each\n";
    }

    if (failures) { std::cerr << failures << " failure(s)\n"; return 1; }
    std::cout << "combat_test OK\n";
    return 0;
}
