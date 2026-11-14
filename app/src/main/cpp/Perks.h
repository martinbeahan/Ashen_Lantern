#pragma once
// v2.32 Kindled Path (combat revamp PR 4): level cap 50, the XP curve, Embers, Steady Flames,
// Wards and Lantern Arts. Pure data and numbers, no game state. Solo only (Character::statRules);
// online tables keep the old rules. See docs/PERKS.md.
#include <algorithm>
#include <string>

namespace dnd {
namespace perks {

// --- Levels, XP and stat points ------------------------------------------------------------------
constexpr int kLevelCap = 50;
/** Sim only (tools/combat_sim): the v2.31 level rules (+2 points every level, proficiency never
 *  freezes) for before/after tables. Never set in the app. */
inline bool& legacyLevelRulesForSim() { static bool v = false; return v; }
constexpr int kProficiencyCapLevel = 20; // proficiency stops growing at level 20 (+6)

/** XP for the next level: 100 x L below 20 (unchanged), then 2000 + 100 x (L - 20); none at 50. */
inline int xpToNext(int level) {
    if (level >= kLevelCap) return 0;
    if (level < 20) return 100 * std::max(1, level);
    return 2000 + 100 * (level - 20);
}
/** Total XP to reach `level` from level 1. */
inline long long totalXpTo(int level) {
    long long t = 0;
    for (int l = 1; l < std::min(level, kLevelCap); ++l) t += xpToNext(l);
    return t;
}
/** Stat points gained when reaching `level`: +2 at 2..20, then +2 at 25, 30, ... 50 only. */
inline int statPointsAt(int level) {
    if (level <= 1) return 0;
    if (legacyLevelRulesForSim()) return 2;
    if (level <= 20) return 2;
    return (level % 5 == 0) ? 2 : 0;
}
/** Stat points earned by `level` in total. */
inline int statPointsThrough(int level) {
    int s = 0;
    for (int l = 2; l <= level; ++l) s += statPointsAt(l);
    return s;
}
/** Proficiency: 2 + (L - 1) / 4, frozen at level 20 (+6). */
inline int proficiency(int level) {
    if (legacyLevelRulesForSim()) return 2 + (std::max(1, level) - 1) / 4;
    const int l = std::clamp(level, 1, kProficiencyCapLevel);
    return 2 + (l - 1) / 4;
}

/** v2.32 solo XP per cleared room past level 20: at least 40 + 15 x EL (proposal 4b). */
inline int roomXpFloor(int encounterLevel) { return 40 + 15 * std::max(0, encounterLevel); }

// --- Embers ----------------------------------------------------------------------------------------
constexpr int kEmberFirstLevel = 21;         // one Ember per level 21..50 = 30
constexpr int kEmbersFromLevels = kLevelCap - kEmberFirstLevel + 1; // 30
constexpr int kEmbersPerPrestigeRank = 1;    // Rekindling (PR 5): +1 per rank I..X = 10
constexpr int kPrestigeBonusEmbers = 2;      // + a bonus 2 at rank IX
constexpr int kPrestigeMaxRank = 10;
constexpr int kEmberCap = kEmbersFromLevels + kEmbersPerPrestigeRank * kPrestigeMaxRank + kPrestigeBonusEmbers; // 42

inline int embersEarned(int level, int prestigeRank) {
    const int fromLevels = std::clamp(std::min(level, kLevelCap) - kEmberFirstLevel + 1, 0, kEmbersFromLevels);
    const int r = std::clamp(prestigeRank, 0, kPrestigeMaxRank);
    const int fromPrestige = r * kEmbersPerPrestigeRank + (r >= 9 ? kPrestigeBonusEmbers : 0);
    return std::min(kEmberCap, fromLevels + fromPrestige);
}

// --- Steady Flames (permanent, 1 Ember per rank, 5 ranks) -----------------------------------------
constexpr int kFlameCount = 8;
constexpr int kFlameMaxRank = 5;
constexpr int kFlameMaxRankRekindled = 6; // Rekindling rank II
enum Flame { TEMPERED_ARM = 0, QUICKWICK, DEEP_TALLOW, LAMP_LORE, STILLFLAME, SILVER_TONGUE, THICK_WAX, BRIGHT_EDGE };
inline const char* flameName(int i) {
    static const char* const n[kFlameCount] = {"Tempered Arm", "Quickwick", "Deep Tallow", "Lamp-Lore",
                                               "Stillflame", "Silver Tongue", "Thick Wax", "Bright Edge"};
    return (i >= 0 && i < kFlameCount) ? n[i] : "";
}
inline const char* flameDesc(int i) {
    static const char* const d[kFlameCount] = {"+1 STR per rank", "+1 DEX per rank", "+1 CON per rank",
                                               "+1 INT per rank", "+1 WIS per rank", "+1 CHA per rank",
                                               "+3% max HP per rank", "+2% damage per rank"};
    return (i >= 0 && i < kFlameCount) ? d[i] : "";
}
constexpr double kThickWaxPerRank = 0.03;
constexpr double kBrightEdgePerRank = 0.02;

// --- Wards (once per battle, 2 Embers to learn, 2 equipped) ------------------------------------------
constexpr int kWardCount = 8;
constexpr int kWardCost = 2;
constexpr int kWardSlots = 2;      // equipped Wards (3 from Rekindling rank III)
constexpr int kWardSlotsMax = 3;
enum Ward { LAST_LIGHT = 0, CLEANSE, SECOND_WICK, ASH_VEIL, KINDLE_RUSH, EMBER_WARD, SPITE_SPARK, STEADY_HAND };
inline const char* wardName(int i) {
    static const char* const n[kWardCount] = {"Last Light", "Cleanse", "Second Wick", "Ash Veil",
                                              "Kindle Rush", "Ember Ward", "Spite Spark", "Steady Hand"};
    return (i >= 0 && i < kWardCount) ? n[i] : "";
}
inline const char* wardDesc(int i) {
    static const char* const d[kWardCount] = {
        "Survive a lethal hit at 1 HP",
        "The first harmful status on you is removed at once",
        "Below 30% HP, heal 25% of max HP",
        "The first hit you take is halved",
        "+50% damage on your first action",
        "Start each battle with a shield of 10% max HP",
        "When an ally drops: +3 to hit and damage for 2 rounds",
        "Your first miss is re-rolled"};
    return (i >= 0 && i < kWardCount) ? d[i] : "";
}
constexpr int kBraceAc = 4;      // Iron Brace
constexpr int kRallyBonus = 2;   // Warcry / Rallying Verse
constexpr int kSunderAc = 3;     // Sunder
constexpr double kCleaveMult = 0.75;
constexpr double kFanMult = 0.60;
constexpr double kRelightHp = 0.30;
constexpr double kSecondWickBelow = 0.30;
constexpr double kSecondWickHeal = 0.25;
constexpr double kKindleRush = 1.5;
constexpr double kEmberWardShield = 0.10;
constexpr int kSpiteBonus = 3;
constexpr int kSpiteRounds = 2;

// --- Lantern Arts (actives; 3 Embers to learn, 2 equipped; ready at battle start) -------------------
constexpr int kArtsPerClass = 4;
constexpr int kArtCost = 3;
constexpr int kArtSlots = 2;       // equipped Arts (3 from Rekindling rank V)
constexpr int kArtSlotsMax = 3;
constexpr int kArtUnlock[kArtsPerClass] = {22, 26, 30, 34};
constexpr int kOncePerBattle = 99; // Relight

enum class ArtTarget { FOE, ALL_FOES, SELF, ALLY, DOWNED_ALLY, PARTY };

struct ArtInfo {
    const char* name;
    const char* desc;
    int cooldown;     // rounds; kOncePerBattle = once per battle
    ArtTarget target;
};

/** Class ordinal (0 Fighter, 1 Wizard, 2 Rogue, 3 Cleric, 4 Bard) x art 0..3. */
inline const ArtInfo& art(int classOrdinal, int i) {
    static const ArtInfo kArts[5][kArtsPerClass] = {
        { // Fighter
            {"Cleave", "Strike 2 foes at 75% damage", 3, ArtTarget::FOE},
            {"Iron Brace", "Foes must target you; +4 AC for 2 rounds", 4, ArtTarget::SELF},
            {"Sunder", "Hit and crack armor: -3 AC for 3 rounds", 3, ArtTarget::FOE},
            {"Warcry", "Party +2 to hit and +2 damage for 2 rounds", 5, ArtTarget::PARTY},
        },
        { // Wizard
            {"Cinder Burst", "All foes take 2d6 + INT (DEX save halves)", 4, ArtTarget::ALL_FOES},
            {"Ash Bind", "Stun a foe 1 round (CON save); a boss loses its planned move", 5, ArtTarget::FOE},
            {"Wick Ward", "Shield the most hurt ally for 10 + 2 x INT", 3, ArtTarget::ALLY},
            {"Overkindle", "Your next special deals double, then 1 round of exhaustion", 6, ArtTarget::SELF},
        },
        { // Rogue
            {"Smoke Step", "Untargetable for 1 round; your next hit is a crit", 4, ArtTarget::SELF},
            {"Bleeding Cut", "Hit and make a foe bleed for 3 rounds", 3, ArtTarget::FOE},
            {"Lift Purse", "Steal gold and cancel the foe's planned move", 4, ArtTarget::FOE},
            {"Fan of Knives", "Hit all foes at 60% damage", 4, ArtTarget::ALL_FOES},
        },
        { // Cleric
            {"Mending Circle", "Heal the party 1d6 + WIS", 3, ArtTarget::PARTY},
            {"Relight", "Revive a downed ally at 30% HP (once per battle)", kOncePerBattle, ArtTarget::DOWNED_ALLY},
            {"Sanctify", "Cleanse the party and resist new statuses for 2 rounds", 4, ArtTarget::PARTY},
            {"Dawnstrike", "Hit a foe; double damage vs Ashbound and Criers", 3, ArtTarget::FOE},
        },
        { // Bard
            {"Rallying Verse", "Party inspired: +2 to hit and damage for 3 rounds", 4, ArtTarget::PARTY},
            {"Discord", "A foe attacks another foe on its turn (WIS save)", 5, ArtTarget::FOE},
            {"Encore", "An ally takes an extra turn right now", 6, ArtTarget::ALLY},
            {"Ashen Lullaby", "A minion or skirmisher sleeps until hit", 4, ArtTarget::FOE},
        },
    };
    const int c = std::clamp(classOrdinal, 0, 4);
    return kArts[c][std::clamp(i, 0, kArtsPerClass - 1)];
}

/** Signature tier 4 (main stat 32): every Art recharges 1 round faster (never below 1). */
inline int cooldownFor(int base, int signatureTier) {
    if (base >= kOncePerBattle) return kOncePerBattle;
    return std::max(1, base - (signatureTier >= 4 ? 1 : 0));
}

// --- Re-temper (respec) -----------------------------------------------------------------------------
constexpr int kRetemperGoldPerLevel = 100;
inline int retemperCost(int level) { return kRetemperGoldPerLevel * std::max(1, level); }

/** Total Embers to learn everything (flames 40 + wards 16 + one class's arts 12 = 68). */
constexpr int kPathTotalCost = kFlameCount * kFlameMaxRank + kWardCount * kWardCost + kArtsPerClass * kArtCost;

} // namespace perks

// --- v2.33 Rekindling (prestige, combat revamp PR 5; docs/PRESTIGE.md) ------------------------------
// Optional at level 50 with the story complete. Level -> 1, gear/gold/story kept, +1 rank (I..X).
// Solo only: every bonus below counts under the solo rules (Character::perksOn), never online.
namespace prestige {
constexpr int kMaxRank = perks::kPrestigeMaxRank;   // 10
constexpr int kEligibleLevel = perks::kLevelCap;    // 50
constexpr double kHpPerRank = 0.02;                 // +2% max HP per rank
constexpr double kXpPerRank = 0.05;                 // +5% XP per rank
constexpr double kGoldPerRank = 0.05;               // +5% gold (loot, searches) per rank
constexpr double kCatchUpXp = 1.5;                  // rank IV+: XP x1.5 while below peak level
constexpr int kAttuneAbove = 3;                     // gear counts up to level + 3 (Rekindled heroes)
// Milestone ranks
constexpr int kRankFlameCap = 2;      // II: Steady Flame cap 6
constexpr int kRankThirdWard = 3;     // III: a 3rd Ward slot
constexpr int kRankCatchUp = 4;       // IV: catch-up XP
constexpr int kRankThirdArt = 5;      // V: a 3rd Art slot
constexpr int kRankExtraSpecial = 6;  // VI: +1 class special use
constexpr int kRankMythicCap = 7;     // VII: Mythical upgrade cap +2 (PR 6)
constexpr int kRankWhiteBadge = 8;    // VIII: ember-white badge
// IX: +2 Embers (perks::embersEarned); X: Everflame.

inline int clampRank(int r) { return std::clamp(r, 0, kMaxRank); }
/** Companion share: half the hero's rank, rounded down (attributes, caps, HP, Embers, milestones). */
inline int share(int rank, bool half) { const int r = clampRank(rank); return half ? r / 2 : r; }
inline int flameCap(int shareRank) { return shareRank >= kRankFlameCap ? perks::kFlameMaxRankRekindled : perks::kFlameMaxRank; }
inline int wardSlots(int shareRank) { return shareRank >= kRankThirdWard ? 3 : perks::kWardSlots; }
inline int artSlots(int shareRank) { return shareRank >= kRankThirdArt ? 3 : perks::kArtSlots; }
inline int extraSpecialUses(int shareRank) { return shareRank >= kRankExtraSpecial ? 1 : 0; }
inline double hpMult(int shareRank) { return 1.0 + kHpPerRank * clampRank(shareRank); }
/** XP multiplier (full rank, so the companion keeps pace with the hero). */
inline double xpMult(int rank, int level, int peakLevel) {
    const int r = clampRank(rank);
    double m = 1.0 + kXpPerRank * r;
    if (r >= kRankCatchUp && level < peakLevel) m *= kCatchUpXp;
    return m;
}
inline double goldMult(int rank) { return 1.0 + kGoldPerRank * clampRank(rank); }
/** Sim only (tools/combat_sim): the stricter alternative, gear up to level / 2 + 3 (the reference
 *  gear for the level + 3). Never set in the app. */
inline bool& attuneToReferenceForSim() { static bool v = false; return v; }
/** Attunement: a Rekindled hero's gear counts up to level + 3 (rank 0: never capped). */
inline int attunedBonus(int bonus, int level, int shareRank) {
    if (shareRank <= 0) return bonus;
    const int lv = std::max(0, level);
    return std::min(bonus, (attuneToReferenceForSim() ? lv / 2 : lv) + kAttuneAbove);
}
inline const char* roman(int r) {
    static const char* const n[kMaxRank + 1] = {"", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X"};
    return n[clampRank(r)];
}
/** Title shown on the sheet and the slot card: "Rekindled III", "Everflame" at X. */
inline std::string title(int r) {
    r = clampRank(r);
    if (r <= 0) return "";
    if (r >= kMaxRank) return "Everflame";
    return std::string("Rekindled ") + roman(r);
}
/** One line per rank for the explainer. */
inline const char* rankPerk(int r) {
    static const char* const d[kMaxRank + 1] = {
        "",
        "Title Rekindled, amber lantern badge; unlocks Mythical gear and The Drowned Vigil (coming next update)",
        "Steady Flames can reach rank 6",
        "A 3rd Ward slot",
        "Catch-up: XP x1.5 while below your peak level",
        "A 3rd Lantern Art slot",
        "+1 class special use",
        "Mythical upgrade cap +2 (coming next update)",
        "Ember-white badge",
        "+2 bonus Embers",
        "Everflame: the highest rank, animated badge"};
    return d[clampRank(r)];
}
} // namespace prestige
} // namespace dnd
