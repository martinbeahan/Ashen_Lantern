#ifndef ASHEN_COMBAT_CURVE_H
#define ASHEN_COMBAT_CURVE_H

/**
 * v2.29 combat curve (combat/progression revamp, PR 1). See docs/COMBAT_CURVE.md.
 *
 * Every foe is built from a "Reference Lamplighter": a model hero at the room's Encounter
 * Level (EL), computed with the SAME Character formulas real heroes use (GameLogic.h), so a
 * change to a hero formula re-tunes the foes automatically. From that model:
 *   enemy AC     = refAtk + 8 + role dAC          (you hit about 65%)
 *   enemy to-hit = refAC - 11 + role dHit + diff  (foes hit about 50%)
 *   enemy HP     = refDmg x hitsToKill(role)
 *   enemy damage = refHP / hitsToDown(role)       (minimum 3)
 * Rooms are fitted to a threat budget by role. Every attack hits 15-90% of the time, and a
 * single foe hit can take at most 45% of a hero's max HP.
 *
 * Pure functions only (no Game state) so tools/combat_sim and the tests use the exact numbers
 * the game ships.
 */
#include "GameLogic.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace dnd {
namespace curve {

/** Append-only (saved as enemy field 20). */
enum class Role : int {
    NONE = 0,
    MINION = 1,
    SKIRMISHER = 2,
    CASTER = 3,
    SUPPORT = 4,
    BRUTE = 5,
    ELITE = 6,
    BOSS = 7,
};

struct RoleInfo {
    const char* name;
    double hitsToKill;  // party hits to fell it (boss: x party/2)
    double hitsToDown;  // its hits to drop a Reference Lamplighter
    int dHit;
    int dAC;
    double cost;        // threat units (boss: the whole room)
};

// Internal-only names for the reference hero's gear (never shown, never saved).
static constexpr const char* kRefWeaponName = "Ref Weapon";
static constexpr const char* kRefArmorName = "Ref Armor";

inline const RoleInfo& roleInfo(Role r) {
    static const RoleInfo kRoles[] = {
        {"None",       2.4, 8.0,  1,  1, 1.0},  // NONE behaves like a skirmisher
        {"Minion",     1.2, 14.0, -1, -1, 0.5},
        {"Skirmisher", 2.4, 8.0,  1,  1, 1.0},
        {"Caster",     1.8, 6.5,  0, -2, 1.0},
        {"Support",    2.2, 11.0, 0,  0, 1.0},
        {"Brute",      3.6, 5.5,  -1, -1, 1.5},
        {"Elite",      5.0, 5.0,  1,  1, 2.5},
        {"Boss",       9.0, 4.5,  2,  2, 0.0},
    };
    int i = static_cast<int>(r);
    if (i < 0 || i > 7) i = 0;
    return kRoles[i];
}

inline Role roleFromInt(int v) { return (v >= 0 && v <= 7) ? static_cast<Role>(v) : Role::NONE; }

/** Difficulty: enemy HP x, damage x, to-hit +, room budget per hero. */
struct DifficultyInfo { double hp, dmg; int hit; double budgetPerHero; int maxFoes; };
inline const DifficultyInfo& difficultyInfo(int d) {
    static const DifficultyInfo kDiff[] = {
        {0.85, 0.85, -1, 0.9, 4},  // Easy
        {1.00, 1.00, 0, 1.2, 4},   // Medium
        {1.12, 1.08, 1, 1.4, 4},   // Hard
        {1.25, 1.15, 1, 1.6, 5},   // Nightmare
    };
    return kDiff[std::clamp(d, 0, 3)];
}

/** Room-type budget multipliers. */
constexpr double kRoomSoftOpen = 0.6;
constexpr double kRoomNormal = 1.0;
constexpr double kRoomSetPiece = 1.3;
constexpr double kRoomDeepMilestone = 1.6;
/** Surprise boss (Ossuary Prior at Bone Gallery, Wick King at the Weir): capped at 1.5x. */
constexpr double kSurpriseBudgetCap = 1.5;
/** Boss adds use up to half the room budget. */
constexpr double kBossAddsShare = 0.5;
/** Room cost may land within +-15% of its budget (never two over-budget rooms in a row). */
constexpr double kBudgetVariance = 0.15;
/** Comeback (Rekindle) budget x0.8; story bosses never below x0.9. */
constexpr double kRekindleBudget = 0.8;
constexpr double kRekindleStoryBossFloor = 0.9;
/** Pressure: +5% budget (and +5% XP/gold) per flawless room, up to 4 steps (+20%). */
constexpr int kPressureMaxSteps = 4;
constexpr double kPressureStep = 0.05;
/** Hit clamp: you always need at least 3 and at most 18 on the d20 (15%..90%). */
constexpr int kMinNeed = 3;
constexpr int kMaxNeed = 18;
/** Overkill guard: one foe hit deals at most 45% of the target's max HP. */
constexpr double kOverkillShare = 0.45;
/** Crit softening: before EL 5 an enemy crit adds +50% instead of doubling. */
constexpr int kCritSoftenBelowEL = 5;
/** EL tables span levels 1..50 (cap lands in PR 4) plus mode bonuses / gear headroom. */
constexpr int kTableMaxLevel = 50;
constexpr int kMaxEL = 64;
/** EL weights. */
constexpr double kGearFollow = 0.5;
constexpr double kPrestigeFollow = 0.25;
constexpr int kGearSurplusMin = -4;
constexpr int kGearSurplusMax = 8;
constexpr int kElBelowLevel = 2;
constexpr int kElAboveLevel = 6;

/** Class starter armor base (Chain 3, clothes 0, Leather 1, Scale 4, Leather 1). */
inline int starterArmorBase(CharacterClass c) {
    switch (c) {
        case CharacterClass::FIGHTER: return 3;
        case CharacterClass::WIZARD: return 0;
        case CharacterClass::ROGUE: return 1;
        case CharacterClass::CLERIC: return 4;
        case CharacterClass::BARD: return 1;
    }
    return 0;
}

inline int primaryIndex(CharacterClass c) {
    switch (c) {
        case CharacterClass::WIZARD: return 3;
        case CharacterClass::ROGUE: return 1;
        case CharacterClass::CLERIC: return 4;
        case CharacterClass::BARD: return 5;
        default: return 0;
    }
}

/**
 * A model hero at level L: default point split (50% main stat, 30% CON, 20% DEX of the
 * 2 points per level), expected gear of L/2 on each slot. Built with the real Character.
 */
inline Character makeReferenceHero(CharacterClass cls, int level) {
    Character c("Reference", cls, "ref");
    c.level = std::clamp(level, 1, Character::kMaxLevelSafety);
    const int pts = 2 * (c.level - 1);
    const int main = primaryIndex(cls);
    auto add = [&](int idx, int n) {
        switch (idx) {
            case 0: c.attributes.strength += n; break;
            case 1: c.attributes.dexterity += n; break;
            case 2: c.attributes.constitution += n; break;
            case 3: c.attributes.intelligence += n; break;
            case 4: c.attributes.wisdom += n; break;
            case 5: c.attributes.charisma += n; break;
        }
    };
    add(main, static_cast<int>(pts * 0.5));
    add(2, static_cast<int>(pts * 0.3));
    add(1, static_cast<int>(pts * 0.2));
    const int cls_i = static_cast<int>(cls);
    c.equippedWeapon = Item::make(kRefWeaponName, ItemType::WEAPON, c.level / 2, ItemRarity::COMMON, cls_i);
    c.equippedArmor = Item::make(kRefArmorName, ItemType::ARMOR, starterArmorBase(cls) + c.level / 2,
                                 ItemRarity::COMMON, cls_i);
    c.applyStatsForLevel();
    c.calculateAC();
    c.currentHp = c.maxHp;
    return c;
}

struct RefStats { double atk, ac, hp, dmg; };

/** One class at level L, read straight off the model Character (gear is L/2 per slot). */
inline RefStats referenceFor(CharacterClass cls, int level) {
    const Character c = makeReferenceHero(cls, level);
    const int mod = CombatSystem::getPrimaryModifier(c);
    const int pb = CombatSystem::proficiencyBonusForLevel(c.level);
    const int wb = c.equippedWeapon ? c.equippedWeapon->bonus : 0;
    const double atk = mod + pb + wb;
    const double ac = c.armorClass;
    const double dmg = (c.weaponDamageDie() + 1) / 2.0 + mod + wb;
    return {atk, ac, static_cast<double>(c.maxHp), dmg};
}

/** Reference Lamplighter = average of a Fighter and a Wizard. Cached table EL 1..kMaxEL. */
inline const RefStats& reference(int el) {
    static const std::array<RefStats, kMaxEL + 1> kTable = [] {
        std::array<RefStats, kMaxEL + 1> t{};
        for (int L = 1; L <= kMaxEL; ++L) {
            const RefStats f = referenceFor(CharacterClass::FIGHTER, L);
            const RefStats w = referenceFor(CharacterClass::WIZARD, L);
            t[static_cast<size_t>(L)] = {(f.atk + w.atk) / 2, (f.ac + w.ac) / 2, (f.hp + w.hp) / 2, (f.dmg + w.dmg) / 2};
        }
        t[0] = t[1];
        return t;
    }();
    return kTable[static_cast<size_t>(std::clamp(el, 1, kMaxEL))];
}

/** Early-level grace on enemy HP and damage. */
inline double earlyGrace(int el) {
    if (el <= 1) return 0.85;
    if (el == 2) return 0.90;
    if (el == 3) return 0.95;
    return 1.0;
}

struct FoeStats {
    int ac = 10;
    int toHit = 0;
    int maxHp = 1;
    int avgDmgX10 = 30;
};

/**
 * Foe numbers for a role at an EL. hpScale is the room-budget fit (and comeback / pressure);
 * partySize scales a boss's HP (x party/2). accuracyEl (default = el) sets AC and to-hit: mode
 * bonuses raise HP and damage to the bonus EL but keep the hit odds of the party's own EL.
 */
inline FoeStats foeStats(Role role, int el, int difficulty, int partySize, double hpScale = 1.0,
                         int accuracyEl = 0) {
    const RefStats& ref = reference(el);
    const RefStats& acc = reference(accuracyEl > 0 ? accuracyEl : el);
    const RoleInfo& ri = roleInfo(role);
    const DifficultyInfo& di = difficultyInfo(difficulty);
    const double grace = earlyGrace(el);
    double hp = ref.dmg * ri.hitsToKill * di.hp * grace * hpScale;
    if (role == Role::BOSS) hp *= std::max(1, partySize) / 2.0;
    const double dmg = std::max(3.0, ref.hp / ri.hitsToDown * di.dmg * grace);
    FoeStats s;
    s.ac = static_cast<int>(std::lround(acc.atk + 8 + ri.dAC));
    s.toHit = static_cast<int>(std::lround(acc.ac - 11 + ri.dHit + di.hit));
    s.maxHp = std::max(4, static_cast<int>(hp));
    s.avgDmgX10 = static_cast<int>(std::lround(dmg * 10));
    return s;
}

/** Room budget in threat units. */
inline double roomBudget(int partySize, int difficulty, double roomType) {
    return std::max(1, partySize) * difficultyInfo(difficulty).budgetPerHero * roomType;
}

/** Expected total gear bonus (weapon + armor above its base) at level L: L/2 per slot. */
inline double expectedGear(int level) { return 2.0 * (level / 2); }

struct PartyMember { int level; int weaponBonus; int armorBonus; int armorBase; int prestige; };

/**
 * Encounter Level:
 *   EL = round(avgLevel + 0.5 x clamp(gearSurplus, -4, +8) + 0.25 x prestige)
 *   clamped to [avgLevel - 2, avgLevel + 6] (min 1), then + modeBonus.
 */
inline int encounterLevel(const std::vector<PartyMember>& party, int modeBonus) {
    if (party.empty()) return std::clamp(1 + modeBonus, 1, kMaxEL);
    double lv = 0, surplus = 0, prestige = 0;
    for (const auto& m : party) {
        lv += m.level;
        surplus += (m.weaponBonus + m.armorBonus - m.armorBase) - expectedGear(m.level);
        prestige += m.prestige;
    }
    const double n = static_cast<double>(party.size());
    lv /= n; surplus /= n; prestige /= n;
    surplus = std::clamp(surplus, static_cast<double>(kGearSurplusMin), static_cast<double>(kGearSurplusMax));
    double el = std::round(lv + kGearFollow * surplus + kPrestigeFollow * prestige);
    el = std::clamp(el, lv - kElBelowLevel, lv + kElAboveLevel);
    const int base = std::max(1, static_cast<int>(std::lround(el)));
    return std::clamp(base + modeBonus, 1, kMaxEL);
}

inline PartyMember memberOf(const Character& c) {
    PartyMember m;
    m.level = c.level;
    m.weaponBonus = (c.equippedWeapon && c.equippedWeapon->type == ItemType::WEAPON) ? c.equippedWeapon->bonus : 0;
    m.armorBonus = (c.equippedArmor && c.equippedArmor->type == ItemType::ARMOR) ? c.equippedArmor->bonus : 0;
    m.armorBase = starterArmorBase(c.characterClass);
    m.prestige = c.prestigeRank;
    return m;
}

/**
 * Hit clamp. die = natural d20, bonus = everything added to it, ac = target AC.
 * Natural 1 always misses, natural 20 always hits; otherwise you need between 3 and 18.
 */
struct HitCheck { bool hit; int need; bool clamped; };
inline HitCheck attackHits(int die, int bonus, int ac) {
    const int raw = ac - bonus;
    const int need = std::clamp(raw, kMinNeed, kMaxNeed);
    HitCheck h{false, need, need != raw};
    if (die <= 1) h.hit = false;
    else if (die >= 20) h.hit = true;
    else h.hit = die >= need;
    return h;
}

/** Hit chance (0..1) for the clamp, for tests and the sim tables. */
inline double hitChance(int bonus, int ac) {
    int hits = 0;
    for (int d = 1; d <= 20; ++d) if (attackHits(d, bonus, ac).hit) ++hits;
    return hits / 20.0;
}

/** Curve foe damage: average x U(0.75, 1.25); crit x2 (x1.5 before EL 5); sneak x1.3. */
inline int rollFoeDamage(int avgDmgX10, int el, bool crit, bool sneak) {
    const double avg = avgDmgX10 / 10.0;
    const double roll = avg * (0.75 + rng::below(501) / 1000.0);
    double d = roll;
    if (crit) d *= (el < kCritSoftenBelowEL ? 1.5 : 2.0);
    if (sneak) d *= 1.3;
    return std::max(1, static_cast<int>(std::lround(d)));
}

/** Overkill guard: cap one hit at 45% of the target's max HP (exempt = telegraphed heavy). */
inline int overkillCap(int dmg, int targetMaxHp, bool exempt = false) {
    if (exempt) return dmg;
    const int cap = std::max(1, static_cast<int>(std::floor(kOverkillShare * targetMaxHp)));
    return std::min(dmg, cap);
}

/** Budget multiplier from comeback / pressure. */
inline double pressureMultiplier(int pressure) {
    return 1.0 + kPressureStep * std::clamp(pressure, 0, kPressureMaxSteps);
}

/** HP fit for a non-boss room: target / cost, kept within sane bounds. */
inline double fitScale(double target, double cost) {
    if (cost <= 0) return 1.0;
    return std::clamp(target / cost, 0.5, 1.75);
}

} // namespace curve
} // namespace dnd

#endif // ASHEN_COMBAT_CURVE_H
