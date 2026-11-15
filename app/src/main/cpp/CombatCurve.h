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
#include <memory>
#include <cmath>
#include <utility>
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
        {1.15, 1.22, 1, 1.6, 5},   // Nightmare (v2.33: HP 1.25 -> 1.15 for shorter boss fights, damage 1.15 -> 1.22 keeps the risk)
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
/** v2.33: each spent Ember adds this much EL (30 from levels = +4.5 EL), well under what it buys. */
constexpr double kEmberFollow = 0.10;
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
/** Sim only: build the v2.30 (uncapped) model hero for before/after tables. Never set in the app. */
inline bool& legacyReferenceForSim() { static bool v = false; return v; }

inline Character makeReferenceHero(CharacterClass cls, int level) {
    Character c("Reference", cls, "ref");
    c.level = std::clamp(level, 1, Character::kMaxLevelSafety);
    // v2.32: the model earns stat points like a real hero (+2 a level to 20, then every 5th level).
    const int pts = (legacyReferenceForSim() || perks::legacyLevelRulesForSim()) ? 2 * (c.level - 1)
                                                                                : perks::statPointsThrough(c.level);
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
    // v2.31: the model respects the universal stat caps (StatRules.h). Points that would push a
    // stat past its cap overflow to CON, then DEX, then the rest — like a sensible player.
    auto get = [&](int idx) {
        switch (idx) {
            case 0: return c.attributes.strength;
            case 1: return c.attributes.dexterity;
            case 2: return c.attributes.constitution;
            case 3: return c.attributes.intelligence;
            case 4: return c.attributes.wisdom;
            default: return c.attributes.charisma;
        }
    };
    int overflow = 0;
    auto addCapped = [&](int idx, int n) {
        const int cap = legacyReferenceForSim() ? 1000 : stats::attrCap(c.level, idx == main, 0);
        const int room = std::max(0, cap - get(idx));
        const int put = std::min(room, n);
        add(idx, put);
        overflow += n - put;
    };
    addCapped(main, static_cast<int>(pts * 0.5));
    addCapped(2, static_cast<int>(pts * 0.3));
    addCapped(1, static_cast<int>(pts * 0.2));
    static const int kSpill[] = {2, 1, 0, 3, 4, 5};
    for (int idx : kSpill) {
        if (overflow <= 0) break;
        const int n = overflow;
        overflow = 0;
        addCapped(idx, n);
    }
    const int cls_i = static_cast<int>(cls);
    c.equippedWeapon = Item::make(kRefWeaponName, ItemType::WEAPON, c.level / 2, ItemRarity::COMMON, cls_i);
    c.equippedArmor = Item::make(kRefArmorName, ItemType::ARMOR, starterArmorBase(cls) + c.level / 2,
                                 ItemRarity::COMMON, cls_i);
    c.applyStatsForLevel();
    c.statRules = !legacyReferenceForSim(); // v2.31: DEX gives at most +6 AC, as for real heroes
    c.calculateAC();
    c.statRules = false; // the model's numbers are read raw (no Heft / crit range in the curve)
    c.currentHp = c.maxHp;
    return c;
}

/**
 * v2.31 universal stats: every stat now helps (Heft, Weak point, Signature uses, sharper Magic
 * Missile...), none of which is in the model hero. Solo foes get extra HP so fights keep their
 * length: +15% for normal foes, +10% for a boss with an escort, +40% for a boss alone (single
 * targets eat the Signature burst). Ramps in from EL 1 to EL 10. Tuned with tools/combat_sim.
 */
constexpr double kStatsToughOthers = 0.15;
constexpr double kStatsToughBoss = 0.10;
constexpr double kStatsToughBossAlone = 0.40;
inline double statsToughness(int el, bool boss, bool bossHasEscort) {
    const double ramp = std::clamp((el - 1) / 9.0, 0.0, 1.0);
    const double add = !boss ? kStatsToughOthers : (bossHasEscort ? kStatsToughBoss : kStatsToughBossAlone);
    return 1.0 + ramp * add;
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
    // Two cached tables: v2.31 (capped model hero) and the sim-only v2.30 baseline.
    static const auto build = [](bool legacy) {
        const bool saved = legacyReferenceForSim();
        legacyReferenceForSim() = legacy;
        std::array<RefStats, kMaxEL + 1> t{};
        for (int L = 1; L <= kMaxEL; ++L) {
            const RefStats f = referenceFor(CharacterClass::FIGHTER, L);
            const RefStats w = referenceFor(CharacterClass::WIZARD, L);
            t[static_cast<size_t>(L)] = {(f.atk + w.atk) / 2, (f.ac + w.ac) / 2, (f.hp + w.hp) / 2, (f.dmg + w.dmg) / 2};
        }
        t[0] = t[1];
        legacyReferenceForSim() = saved;
        return t;
    };
    // v2.33: one table per sim-flag combination (the app only ever uses {false, false}). Before,
    // the first caller's perks::legacyLevelRulesForSim() leaked into the shipped table in the sims.
    // The app's table is a function-local static (thread-safe init); the sim-only ones are lazy.
    const size_t key = (legacyReferenceForSim() ? 1u : 0u) | (perks::legacyLevelRulesForSim() ? 2u : 0u);
    if (key == 0) {
        static const std::array<RefStats, kMaxEL + 1> kTable = build(false);
        return kTable[static_cast<size_t>(std::clamp(el, 1, kMaxEL))];
    }
    static std::array<std::unique_ptr<std::array<RefStats, kMaxEL + 1>>, 4> cache;
    if (!cache[key]) cache[key] = std::make_unique<std::array<RefStats, kMaxEL + 1>>(build(legacyReferenceForSim()));
    return (*cache[key])[static_cast<size_t>(std::clamp(el, 1, kMaxEL))];
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
 * v2.33: foe numbers on two axes. offEl is the level the party's OFFENSE fights at (sets foe AC
 * and HP, so fights last as long as the model hero's), defEl the level its DEFENSE holds at (sets
 * foe to-hit and damage, so the risk matches). For a hero built like the model both equal the
 * EL. modeBonus (Raid +2 ...) raises HP and damage, never the hit odds.
 */
inline FoeStats foeStatsSplit(Role role, int offEl, int defEl, int modeBonus, int difficulty, int partySize,
                              double hpScale = 1.0) {
    offEl = std::max(1, offEl); defEl = std::max(1, defEl);
    const RefStats& offRef = reference(offEl + modeBonus);
    const RefStats& defRef = reference(defEl + modeBonus);
    const RefStats& offAcc = reference(offEl);
    const RefStats& defAcc = reference(defEl);
    const RoleInfo& ri = roleInfo(role);
    const DifficultyInfo& di = difficultyInfo(difficulty);
    double hp = offRef.dmg * ri.hitsToKill * di.hp * earlyGrace(offEl + modeBonus) * hpScale;
    if (role == Role::BOSS) hp *= std::max(1, partySize) / 2.0;
    const double dmg = std::max(3.0, defRef.hp / ri.hitsToDown * di.dmg * earlyGrace(defEl + modeBonus));
    FoeStats s;
    s.ac = static_cast<int>(std::lround(offAcc.atk + 8 + ri.dAC));
    s.toHit = static_cast<int>(std::lround(defAcc.ac - 11 + ri.dHit + di.hit));
    s.maxHp = std::max(4, static_cast<int>(hp));
    s.avgDmgX10 = static_cast<int>(std::lround(dmg * 10));
    return s;
}

/**
 * Foe numbers for a role at an EL. hpScale is the room-budget fit (and comeback / pressure);
 * partySize scales a boss's HP (x party/2). accuracyEl (default = el) sets AC and to-hit: mode
 * bonuses raise HP and damage to the bonus EL but keep the hit odds of the party's own EL.
 * (Single-axis form of foeStatsSplit; same numbers as v2.29-v2.32.)
 */
inline FoeStats foeStats(Role role, int el, int difficulty, int partySize, double hpScale = 1.0,
                         int accuracyEl = 0) {
    const int acc = accuracyEl > 0 ? accuracyEl : el;
    return foeStatsSplit(role, acc, acc, el - acc, difficulty, partySize, hpScale);
}

/** Room budget in threat units. */
inline double roomBudget(int partySize, int difficulty, double roomType) {
    return std::max(1, partySize) * difficultyInfo(difficulty).budgetPerHero * roomType;
}

/** Expected total gear bonus (weapon + armor above its base) at level L: L/2 per slot. */
inline double expectedGear(int level) { return 2.0 * (level / 2); }

/** v2.33: measured = the character's own numbers (to-hit, AC, HP, damage per hit) are in
 *  `actual` (solo only); embers = Embers spent on the Kindled Path. */
struct PartyMember {
    int level; int weaponBonus; int armorBonus; int armorBase; int prestige;
    bool measured = false; RefStats actual{0, 0, 0, 0};
    int embers = 0;
    int mythic = 0; // v2.40: Mythical pieces equipped (0..2)
};
/** v2.40: each equipped Mythical piece adds 1 EL and lifts the EL ceiling (level + 6) by 1, so
 *  Mythical gear is matched instead of running past the cap. Only Mythical holders change. */
constexpr double kMythicFollow = 1.5;

inline double hitChance(int bonus, int ac);
constexpr double kPowerMatch = 0.97;
/** Class features that grow with level alone (special uses, heals, potions scaling with max HP)
 *  aren't in the swing-by-swing defense measure, so a share of the level counts there. Offense
 *  gets none: foe HP and AC past what the party can actually hit turn fights hopeless. */
constexpr double kDefenseLevelShare = 0.15;
struct AxisPower { double off, def; };
/** The party's numbers against the standard foe built for the Reference Lamplighter at L,
 *  relative to the Reference Lamplighter itself (1.0 = exactly the model). */
inline AxisPower powerVs(const RefStats& me, const RefStats& ref) {
    const RoleInfo& ri = roleInfo(Role::SKIRMISHER);
    const int foeAc = static_cast<int>(std::lround(ref.atk + 8 + ri.dAC));
    const int foeHit = static_cast<int>(std::lround(ref.ac - 11 + ri.dHit));
    auto chance = [](double bonus, int ac) { return std::max(0.05, hitChance(static_cast<int>(std::lround(bonus)), ac)); };
    auto struck = [&](double ac) { return std::max(0.05, hitChance(foeHit, static_cast<int>(std::lround(ac)))); };
    const double off = chance(me.atk, foeAc) * std::max(1.0, me.dmg) / (chance(ref.atk, foeAc) * std::max(1.0, ref.dmg));
    const double def = (std::max(1.0, me.hp) / struck(me.ac)) / (std::max(1.0, ref.hp) / struck(ref.ac));
    return {off, def};
}
/** How many levels below `level` the party's offense / defense really sit (0 = at the model). */
inline std::pair<double, double> powerGaps(const RefStats& me, int level) {
    const RefStats& atLevel = reference(level);
    const AxisPower top = powerVs(me, atLevel);
    int off = top.off >= kPowerMatch ? level : 1, def = top.def >= kPowerMatch ? level : 1;
    for (int L = level - 1; L > 1 && (off == 1 || def == 1); --L) {
        const AxisPower p = powerVs(me, reference(L));
        if (off == 1 && p.off >= kPowerMatch) off = L;
        if (def == 1 && p.def >= kPowerMatch) def = L;
    }
    const double defBlend = def + kDefenseLevelShare * (level - def);
    return {static_cast<double>(level - off), level - defBlend};
}

/** v2.33: the EL on both axes. el (with the mode bonus) is shown, saved and pays XP; offEl /
 *  defEl (no mode bonus) set the foes' HP + AC and damage + to-hit (foeStatsSplit). */
struct EncounterLevels { int el = 1; int offEl = 1; int defEl = 1; int mode = 0; };

/**
 * Encounter Level:
 *   EL = round(avgLevel + 0.5 x clamp(gearSurplus, -4, +8) + 0.25 x prestige)
 *   clamped to [avgLevel - 2, avgLevel + 6] (min 1).
 *   v2.33: + 0.15 x Embers spent (solo; 30 Embers = +4.5, far less than they buy).
 * v2.33: the foes follow the party's ACTUAL power on two axes, never above that EL:
 *   offEl = min(EL, round(avgLevel - offense gap + 0.25 x prestige + 0.15 x Embers))  (foe HP, AC)
 *   defEl = min(EL, round(avgLevel - defense gap + ...))                           (foe damage, to-hit)
 *   where a gap is how many levels below its own level the hero's numbers really sit.
 * A hero built like the model hero of its level has offEl = defEl = EL (no change); one with
 * points or Embers unspent or old gear faces foes matched to what it actually hits and survives.
 * The shown EL is round((offEl + defEl) / 2) + modeBonus.
 */
inline EncounterLevels encounterLevels(const std::vector<PartyMember>& party, int modeBonus) {
    EncounterLevels out;
    out.mode = modeBonus;
    if (party.empty()) {
        out.el = std::clamp(1 + modeBonus, 1, kMaxEL);
        return out;
    }
    double lv = 0, surplus = 0, prestige = 0, embers = 0, mythic = 0;
    RefStats me{0, 0, 0, 0};
    bool measured = true;
    for (const auto& m : party) {
        lv += m.level;
        surplus += (m.weaponBonus + m.armorBonus - m.armorBase) - expectedGear(m.level);
        prestige += m.prestige;
        embers += std::max(0, m.embers);
        mythic += std::clamp(m.mythic, 0, 2);
        measured = measured && m.measured;
        me.atk += m.actual.atk; me.ac += m.actual.ac; me.hp += m.actual.hp; me.dmg += m.actual.dmg;
    }
    const double n = static_cast<double>(party.size());
    lv /= n; surplus /= n; prestige /= n; embers /= n; mythic /= n;
    me.atk /= n; me.ac /= n; me.hp /= n; me.dmg /= n;
    // The party's average numbers against the Reference Lamplighter (the Fighter / Wizard average
    // every foe is built from) — so a weak-hitting party (Cleric + Wizard) gets shorter fights too.
    double offGap = 0, defGap = 0;
    if (measured) {
        const auto g = powerGaps(me, std::clamp(static_cast<int>(std::lround(lv)), 1, kTableMaxLevel));
        offGap = g.first; defGap = g.second;
    }
    surplus = std::clamp(surplus, static_cast<double>(kGearSurplusMin), static_cast<double>(kGearSurplusMax));
    double el = std::round(lv + kGearFollow * surplus + kPrestigeFollow * prestige + kEmberFollow * embers
                           + kMythicFollow * mythic);
    el = std::clamp(el, lv - kElBelowLevel, lv + kElAboveLevel + kMythicFollow * mythic);
    const int base = std::max(1, static_cast<int>(std::lround(el)));
    auto axis = [&](double gap) {
        if (gap < 0.5) return base; // built like the model (or better): the EL as is
        const double a = lv - gap + kPrestigeFollow * prestige + kEmberFollow * embers + kMythicFollow * mythic;
        return std::clamp(static_cast<int>(std::lround(a)), 1, base);
    };
    out.offEl = axis(offGap);
    out.defEl = axis(defGap);
    const int shown = static_cast<int>(std::lround((out.offEl + out.defEl) / 2.0));
    out.el = std::clamp(shown + modeBonus, 1, kMaxEL);
    return out;
}
inline int encounterLevel(const std::vector<PartyMember>& party, int modeBonus) {
    return encounterLevels(party, modeBonus).el;
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

/** v2.33: a character's own numbers, read like the model hero's (CombatCurve.h referenceFor). */
/** How much damage per turn each class really deals next to a swing-by-swing hit, measured with
 *  the shipped party AI in tools/combat_sim (boss + escort rounds by party). The Reference
 *  Lamplighter is the Fighter / Wizard average (1.4 + 0.6 = 2 x 1.0), so that party is unchanged.
 *  Only used to measure power, never on real damage. */
inline double classOffenseFactor(CharacterClass c) {
    switch (c) {
        case CharacterClass::FIGHTER: return 1.4;  // Action Surge, Second Wind, heavy hits
        case CharacterClass::WIZARD: return 0.6;   // (Fighter + Wizard average to 1.0: the model)
        case CharacterClass::CLERIC: return 0.45;  // most turns go to heals and support
        case CharacterClass::ROGUE: return 2.0;    // Sneak Attack
        case CharacterClass::BARD: return 0.8;     // inspiration and heals
    }
    return 1.0;
}
inline RefStats actualStats(const Character& c) {
    const int mod = CombatSystem::getPrimaryModifier(c);
    const int pb = CombatSystem::proficiencyBonusForLevel(c.level);
    const int wb = c.weaponBonusEff();
    return {static_cast<double>(mod + pb + wb), static_cast<double>(c.armorClass), static_cast<double>(c.maxHp),
            ((c.weaponDamageDie() + 1) / 2.0 + mod + wb) * classOffenseFactor(c.characterClass)};
}
/** measurePower = false (online Keeper tables, the classic rules): EL from level and gear only, as v2.32. */
inline PartyMember memberOf(const Character& c, bool measurePower = true) {
    PartyMember m;
    m.level = c.level;
    // v2.33: a Rekindled hero's gear counts as attuned (level / 2 + 3); the companion's rank is half.
    m.weaponBonus = c.weaponBonusEff();
    m.armorBonus = c.armorBonusEff();
    m.armorBase = starterArmorBase(c.characterClass);
    m.prestige = c.prestigeShare();
    m.mythic = (c.equippedWeapon && c.equippedWeapon->isMythical() ? 1 : 0)
             + (c.equippedArmor && c.equippedArmor->isMythical() ? 1 : 0);
    if (measurePower) {
        m.measured = true;
        m.actual = actualStats(c);
        m.embers = std::min(c.embersSpent(), c.embersEarned());
    }
    return m;
}

} // namespace curve
} // namespace dnd

#endif // ASHEN_COMBAT_CURVE_H
