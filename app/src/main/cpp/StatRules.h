#pragma once
// v2.31 universal stats (combat revamp PR 3): pure numbers, no game state. Every stat helps every
// class; the main stat still drives hit, damage and the class special, has a higher cap and
// unlocks Signature tiers. Only solo play uses these (Character::statRules); online tables keep
// the old rules. See docs/STATS.md.
#include <algorithm>

namespace dnd {
namespace stats {

/** floor((score - 10) / 2), as Attributes::getModifier. */
inline int mod(int score) {
    const int d = score - 10;
    return (d >= 0) ? d / 2 : -((1 - d) / 2);
}

// --- Caps -----------------------------------------------------------------------------------
constexpr int kCapBase = 18;     // normal stats: 18 + floor(level / 2) ...
constexpr int kCapMax = 30;      // ... up to 30 (level 24)
constexpr int kMainExtra = 2;    // the main stat can go 2 higher (32)

/** Base-attribute cap (gear can go above it). Prestige adds +1 per rank (PR 5; 0 for now). */
inline int attrCap(int level, bool mainStat, int prestigeRank = 0) {
    const int lv = std::max(1, level);
    return std::min(kCapMax, kCapBase + lv / 2) + (mainStat ? kMainExtra : 0) + std::max(0, prestigeRank);
}

/** Main stat index (0 STR .. 5 CHA) for a CharacterClass ordinal. */
inline int mainIndex(int classOrdinal) {
    switch (classOrdinal) {
        case 1: return 3; // Wizard INT
        case 2: return 1; // Rogue DEX
        case 3: return 4; // Cleric WIS
        case 4: return 5; // Bard CHA
        default: return 0; // Fighter STR
    }
}

// --- Main-stat Signature tiers (effective main score) ------------------------------------------
/** 0 none; 1 at 20 (+1 special use per battle); 2 at 24 (+25% potency); 3 at 28 (special crit
 *  19-20); 4 at 32 (Lantern Arts cooldowns -1 — Arts arrive in PR 4). */
inline int signatureTier(int mainScore) {
    if (mainScore >= 32) return 4;
    if (mainScore >= 28) return 3;
    if (mainScore >= 24) return 2;
    if (mainScore >= 20) return 1;
    return 0;
}
constexpr double kSignaturePotency = 1.25;

// --- STR: Heft, Break guard, STR saves -------------------------------------------------------------
inline int heft(int strMod) { return strMod > 0 ? strMod / 2 : 0; }
inline bool breaksGuard(int strMod) { return strMod >= 3; }

// --- DEX: AC (capped), initiative, Evade ----------------------------------------------------------
constexpr int kDexAcCap = 6;
inline int acFromDex(int dexMod) { return std::min(dexMod, kDexAcCap); }
/** Damage cut against telegraphed heavies and area attacks: 3% per DEX mod, up to 24%. */
inline double evade(int dexMod) { return dexMod <= 0 ? 0.0 : std::min(0.24, 0.03 * dexMod); }

// --- CON: HP / rests as before, Grit saves, death saves ---------------------------------------------
inline int deathSaveBonus(int conMod) { return conMod > 0 ? conMod / 2 : 0; }

// --- INT: Search as before, Weak point, Read intent -----------------------------------------------
/** Lowest natural roll that crits: 19 at INT mod +4, 18 at +8; one lower again for a class
 *  special at Signature tier 3. Never below 17. */
inline int critMin(int intMod, bool specialSignature) {
    int m = 20;
    if (intMod >= 8) m = 18;
    else if (intMod >= 4) m = 19;
    if (specialSignature) m -= 1;
    return std::max(17, m);
}
constexpr int kReadIntentMod = 2; // exact chip numbers from INT mod +2 (INT 14); ranges below

// --- WIS: Mending, Resolve ------------------------------------------------------------------------
inline double mending(int wisMod) { return wisMod <= 0 ? 0.0 : std::min(0.40, 0.04 * wisMod); }
inline bool restResourceBonus(int wisMod) { return wisMod >= 4; }

// --- CHA: Barter, Luck of the Lantern, Inspire, Intimidate --------------------------------------
inline double buyDiscount(int chaMod) { return chaMod <= 0 ? 0.0 : std::min(0.20, 0.02 * chaMod); }
constexpr int kSellCapBase = 65;  // the old sell cap ...
constexpr int kSellCapMax = 75;   // ... rises to 75% with CHA
inline int sellBonusPct(int chaMod) { return std::clamp(chaMod, 0, kSellCapMax - kSellCapBase); }
inline double goldBonus(int chaMod) { return chaMod <= 0 ? 0.0 : std::min(0.30, 0.03 * chaMod); }
inline int inspire(int chaMod) { return chaMod > 0 ? chaMod / 4 : 0; }
/** Foes' flee threshold rises 2% per CHA mod (max +20%). */
inline double intimidate(int chaMod) { return chaMod <= 0 ? 0.0 : std::min(0.20, 0.02 * chaMod); }

// --- Class specials ---------------------------------------------------------------------------------
/** Magic Missile dart: 1d4 + 1 + floor(INTmod / 3). This is the flat part (min +1). */
inline int dartBonus(int intMod) { return 1 + (intMod > 0 ? intMod / 3 : 0); }

/** Buy price after Barter (never below 1). */
inline int buyPrice(int cost, int chaMod) {
    return std::max(1, static_cast<int>(cost * (1.0 - buyDiscount(chaMod)) + 0.5));
}

} // namespace stats
} // namespace dnd
