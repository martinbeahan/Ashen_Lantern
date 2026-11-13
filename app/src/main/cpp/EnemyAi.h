#ifndef ASHEN_ENEMY_AI_H
#define ASHEN_ENEMY_AI_H

/**
 * v2.30 enemy AI (PR 2 of the combat & progression revamp; docs/ENEMY_AI.md).
 *
 * Pure rules: AI tiers by difficulty, the move list with its intent icon and numbers, target
 * scoring, saving throws, and the save codecs for statuses / foe AI state. Game.cpp plans and
 * runs the moves (planEnemyMove / runEnemyMove). Solo only — online Keeper tables and joiners
 * keep the old AI (Game::enemyAiActive()).
 */

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "GameLogic.h"

namespace dnd {
namespace ai {

/** AI tiers by difficulty (approved table 2b). */
enum class Tier : int { SLOPPY = 0, TACTICAL = 1, COORDINATED = 2, RUTHLESS = 3 };
inline Tier tierFor(int difficulty) {
    return static_cast<Tier>(std::clamp(difficulty, 0, 3));
}
inline const char* tierName(Tier t) {
    switch (t) {
        case Tier::SLOPPY: return "Sloppy";
        case Tier::TACTICAL: return "Tactical";
        case Tier::COORDINATED: return "Coordinated";
        case Tier::RUTHLESS: return "Ruthless";
    }
    return "Tactical";
}

/** What the intent chip shows (icon). Values are part of the getEnemyIntents() format. */
enum class Intent : int {
    NONE = 0, ATTACK = 1, HEAVY = 2, GUARD = 3, SPELL = 4, HEAL = 5, MARK = 6, FLEE = 7,
    BUFF = 8, SUMMON = 9, HEX = 10,
};

/** Every planned move. Append-only (saved in the foe AI field). */
enum class Move : int {
    NONE = 0, STRIKE, SNATCH, BACKSTAB, SHIELD_WALL, PACK_BITE, SWARM_BITE, RALLY, KNIT, SLAM,
    SHACKLE, FINISH, MARK, CINDER_BOLT, RATTLE_HEALER, SLIP_AWAY, FINISH_DOWNED,
    // Bosses (2d)
    SEXTON_CALL, SNUFF, TALLY, VENN_CALL, EMBER_WOUND, WARDEN_GUARD, TWIN_STRIKE, TOLL,
    BELL_CRACK, KING_CALL, CROWN_FLARE, RELIQUARY, RAISE_DEAD, CINDER_BREATH, TAKE_FLIGHT, DIVE,
    TYRANTS_DUE, HYDRA_FIRE, HEADS, WEBS, BROOD,
    COUNT_
};
inline Move moveFromInt(int v) {
    return (v > 0 && v < static_cast<int>(Move::COUNT_)) ? static_cast<Move>(v) : Move::NONE;
}

/** Once-per-fight boss moves already used (FoeAi::flags). */
enum Flag : int {
    F_CALL2 = 1 << 0,   // phase-2 summon / Tally / Reliquary
    F_CALL3 = 1 << 1,   // phase-3 summon / Raise dead / Bell cracks
    F_WARNED_DOWNED = 1 << 2,
};

struct MoveInfo {
    Intent intent;
    const char* label;      // chip label (target name is added by the game)
    double dmgMult;         // x the foe's average damage (0 = no damage)
    bool autoHit;           // spells: no attack roll
    bool heavyExempt;       // telegraphed boss heavy: exempt from the 45% overkill guard
    bool aoe;               // hits every standing hero
};

inline const MoveInfo& moveInfo(Move m) {
    static const MoveInfo kInfo[] = {
        {Intent::NONE,   "",               0.0, false, false, false}, // NONE
        {Intent::ATTACK, "Attack",         1.0, false, false, false}, // STRIKE
        {Intent::ATTACK, "Snatch",         1.0, false, false, false}, // SNATCH
        {Intent::ATTACK, "Backstab",       1.35, false, false, false},// BACKSTAB
        {Intent::GUARD,  "Shield-wall",    0.0, false, false, false}, // SHIELD_WALL
        {Intent::ATTACK, "Pack hunt",      1.0, false, false, false}, // PACK_BITE
        {Intent::ATTACK, "Swarm",          1.0, false, false, false}, // SWARM_BITE
        {Intent::BUFF,   "Rally cry",      0.0, false, false, false}, // RALLY
        {Intent::HEAL,   "Knit bones",     0.0, false, false, false}, // KNIT
        {Intent::HEAVY,  "Millstone Slam", 2.0, false, false, false}, // SLAM
        {Intent::ATTACK, "Shackle",        1.0, false, false, false}, // SHACKLE
        {Intent::ATTACK, "Finish",         1.0, false, false, false}, // FINISH
        {Intent::MARK,   "Mark",           0.0, false, false, false}, // MARK
        {Intent::SPELL,  "Cinder bolt",    0.7, true,  false, false}, // CINDER_BOLT
        {Intent::HEX,    "Rattle",         0.0, false, false, false}, // RATTLE_HEALER
        {Intent::FLEE,   "Slips away",     0.0, false, false, false}, // SLIP_AWAY
        {Intent::HEAVY,  "Finish the fallen", 1.0, false, false, false}, // FINISH_DOWNED
        {Intent::SUMMON, "Calls an Ashbound", 0.0, false, false, false}, // SEXTON_CALL
        {Intent::HEAVY,  "Snuff",          1.4, false, true,  false}, // SNUFF
        {Intent::MARK,   "Tally",          0.0, false, false, false}, // TALLY
        {Intent::SUMMON, "Calls an Enforcer", 0.0, false, false, false}, // VENN_CALL
        {Intent::SPELL,  "Ember wound",    0.0, true,  false, true }, // EMBER_WOUND
        {Intent::GUARD,  "Guards",         0.0, false, false, false}, // WARDEN_GUARD
        {Intent::ATTACK, "Twin strike",    1.0, false, false, false}, // TWIN_STRIKE
        {Intent::HEX,    "The toll",       0.0, false, false, true }, // TOLL
        {Intent::BUFF,   "Bell cracks",    0.0, false, false, false}, // BELL_CRACK
        {Intent::SUMMON, "Calls Wickthieves", 0.0, false, false, false}, // KING_CALL
        {Intent::HEX,    "Crown flare",    0.0, false, false, false}, // CROWN_FLARE
        {Intent::HEAL,   "Reliquary",      0.0, false, false, false}, // RELIQUARY
        {Intent::SUMMON, "Raises the dead", 0.0, false, false, false}, // RAISE_DEAD
        {Intent::HEAVY,  "Cinder breath",  0.7, true,  true,  true }, // CINDER_BREATH
        {Intent::GUARD,  "Takes flight",   0.0, false, false, false}, // TAKE_FLIGHT
        {Intent::HEAVY,  "Dive",           1.6, false, true,  false}, // DIVE
        {Intent::HEAVY,  "Tyrant's due",   1.6, false, true,  false}, // TYRANTS_DUE
        {Intent::SPELL,  "Ember spit",     0.7, true,  false, false}, // HYDRA_FIRE
        {Intent::ATTACK, "Three heads",    0.35, false, false, false}, // HEADS
        {Intent::HEX,    "Webs",           0.0, false, false, false}, // WEBS
        {Intent::SUMMON, "Calls the brood", 0.0, false, false, false}, // BROOD
    };
    static_assert(sizeof(kInfo) / sizeof(kInfo[0]) == static_cast<size_t>(Move::COUNT_), "move table");
    const int i = static_cast<int>(m);
    return kInfo[(i >= 0 && i < static_cast<int>(Move::COUNT_)) ? i : 0];
}
inline Intent intentOf(Move m) { return moveInfo(m).intent; }

// --- Tuning ------------------------------------------------------------------------------
constexpr int kGuardAc = 3;              // Guarded: +3 AC
constexpr int kMarkToHit = 2;            // allies +2 to hit a Marked hero
constexpr int kInspiredToHit = 2;        // Rally cry: +2 to hit
constexpr int kBlindToHit = 4;           // Crown flare: -4 to hit
constexpr int kRattle = 2;               // the toll: -2 next attack (existing nextAttackPenalty)
constexpr double kFleeBelow = 0.25;      // slips away under 25% HP
constexpr double kSnatchShare = 0.05;    // steals 5% of the target's gold on a hit
constexpr double kStaggerShare = 0.20;   // 20% of max HP before the Slam lands cancels it
constexpr double kKnitHeal = 0.20;       // Knit bones heals an undead ally 20%
constexpr double kReliquaryHeal = 0.15;  // the Prior heals 15% once
constexpr double kHydraRegrow = 0.05;    // the Hydra regrows 5% unless a crit / spell landed that round
constexpr double kSummonScale = 0.2;     // summoned foes are light (the boss keeps the room budget)
/** A boss that picks its targets (focus fire, scripted heavies) hits for 70% of the curve's
 *  boss damage so the v2.29 boss budget still holds (tuned with tools/combat_sim/ai_sim). */
constexpr double kBossDamageScale = 0.7;
constexpr double kSummonDmgScale = 0.25; // ... and hit for 25% of a normal foe of their role
constexpr int kIntentNumbersInt = 14;    // exact numbers once a hero has INT 14+ (mod +2)

/** Heal threshold (fraction of max HP) by tier: Sloppy only when it's obvious. */
inline double healBelow(Tier t) {
    switch (t) {
        case Tier::SLOPPY: return 0.25;
        case Tier::TACTICAL: return 0.40;
        case Tier::COORDINATED: return 0.40;
        case Tier::RUTHLESS: return 0.50;
    }
    return 0.40;
}
/** Target noise (±fraction of the score): Easy 30%, Medium 15%, above 0. */
inline double targetNoise(Tier t) {
    return t == Tier::SLOPPY ? 0.30 : (t == Tier::TACTICAL ? 0.15 : 0.0);
}
/** Signature-move cooldown, by tier (Sloppy slower, Ruthless faster). */
inline int cooldownFor(Tier t, int base) {
    if (t == Tier::SLOPPY) return base + 1;
    if (t == Tier::RUTHLESS) return std::max(1, base - 1);
    return base;
}

/** Saving throw: d20 + stat modifier vs DC 10 + EL/2 (approved 2e). */
inline int saveDc(int el) { return 10 + std::max(0, el) / 2; }
inline bool savePasses(int d20, int statScore, int el) {
    return d20 + Attributes::getModifier(statScore) >= saveDc(el);
}

// --- Target scoring (2a) ---------------------------------------------------------------
struct HeroView {
    bool alive = true;      // not dead
    bool downed = false;
    int hp = 1, maxHp = 1, ac = 10;
    bool healer = false;    // Cleric or Bard
    bool healedRecently = false;
    bool marked = false;
    bool hitMe = false;     // hit this foe since its last turn
    int focus = 0;          // other foes already aiming at this hero (Coordinated focus fire)
};

/** 40·missing% + 30·healer + 20·[<35%] + 2·(15−AC) + 15·Marked + 25·hit me (+ focus share). */
inline double scoreTarget(const HeroView& h, Tier tier, int behaviour) {
    if (!h.alive) return -1e9;
    const double missing = h.maxHp > 0 ? 1.0 - static_cast<double>(h.hp) / h.maxHp : 0.0;
    double s = 40.0 * missing;
    if ((h.healer || h.healedRecently) && tier != Tier::SLOPPY) s += 30.0; // Sloppy never focuses healers
    if (h.maxHp > 0 && h.hp * 100 < h.maxHp * 35) s += 20.0;
    s += 2.0 * (15 - h.ac);
    if (h.marked) s += 15.0;
    if (h.hitMe) s += 25.0;
    if (tier >= Tier::COORDINATED) s += 25.0 * std::min(h.focus, 2); // share one focus target
    switch (behaviour) {
        case bestiary::BEH_FINISHER: s += 80.0 * missing; break;          // lowest HP%
        case bestiary::BEH_BACKSTAB: s += 6.0 * (15 - h.ac); break;      // lowest AC
        case bestiary::BEH_PACK: s += 20.0 * std::min(h.focus, 3) + 20.0 * missing; break;
        default: break;
    }
    return s;
}

/**
 * Pick a hero. Standing heroes always come first; downed ones only when nobody stands
 * (Nightmare's "finish the fallen" is planned separately and telegraphed). roll01 / noise01
 * are uniform [0,1) draws from the game RNG so this stays deterministic under a seed.
 */
inline int pickTarget(const std::vector<HeroView>& heroes, Tier tier, int behaviour,
                      const std::vector<double>& noise01, double roll01) {
    std::vector<int> up;
    for (size_t i = 0; i < heroes.size(); ++i)
        if (heroes[i].alive && !heroes[i].downed) up.push_back(static_cast<int>(i));
    if (up.empty()) {
        // Nobody standing: the downed hero with the fewest saves to go (as before).
        int best = -1;
        for (size_t i = 0; i < heroes.size(); ++i)
            if (heroes[i].alive) { best = static_cast<int>(i); break; }
        return best;
    }
    if (tier == Tier::SLOPPY && roll01 < 0.30) {
        const int pick = std::min(static_cast<int>(up.size()) - 1, static_cast<int>(noise01.empty() ? 0 : noise01[0] * up.size()));
        return up[static_cast<size_t>(std::max(0, pick))];
    }
    const double n = targetNoise(tier);
    int best = up.front();
    double bestScore = -1e18;
    for (int i : up) {
        double s = scoreTarget(heroes[static_cast<size_t>(i)], tier, behaviour);
        if (n > 0 && static_cast<size_t>(i) < noise01.size()) s *= 1.0 + n * (2.0 * noise01[static_cast<size_t>(i)] - 1.0);
        if (s > bestScore) { bestScore = s; best = i; }
    }
    return best;
}

// --- Save codecs ------------------------------------------------------------------------
inline std::vector<int> parseDots(const std::string& s) {
    std::vector<int> out;
    std::string cur;
    auto flush = [&]() {
        int v = 0;
        out.push_back(savecodec::tryParseInt(cur, v) ? v : 0);
        cur.clear();
    };
    for (char c : s) { if (c == '.') flush(); else cur.push_back(c); }
    flush();
    return out;
}
inline std::string encodeStatuses(const Character::Statuses& st) {
    std::ostringstream ss;
    ss << st.burn << "." << st.burnDmg << "." << st.marked << "." << st.guarded << "." << st.blind
       << "." << st.slowed << "." << st.slowedFromInit << "." << st.shackled << "." << st.inspired
       << "." << st.healedRecently;
    return ss.str();
}
inline Character::Statuses decodeStatuses(const std::string& s) {
    Character::Statuses st;
    const std::vector<int> v = parseDots(s);
    auto at = [&](size_t i, int lo, int hi) { return i < v.size() ? std::clamp(v[i], lo, hi) : 0; };
    st.burn = at(0, 0, 9); st.burnDmg = at(1, 0, 100000); st.marked = at(2, 0, 9);
    st.guarded = at(3, 0, 9); st.blind = at(4, 0, 9); st.slowed = at(5, 0, 9);
    st.slowedFromInit = 6 < v.size() ? std::clamp(v[6], -1000, 1000) : 0; st.shackled = at(7, 0, 9); st.inspired = at(8, 0, 9);
    st.healedRecently = at(9, 0, 9);
    return st;
}
inline std::string encodeFoeAi(const Character::FoeAi& a) {
    std::ostringstream ss;
    ss << a.move << "." << a.target << "." << a.cooldown << "." << a.flags << "." << a.stolenGold
       << "." << a.stagger << "." << a.lastHitBy << "." << a.alt << "." << a.lastTarget
       << "." << (a.critThisRound ? 1 : 0);
    return ss.str();
}
inline Character::FoeAi decodeFoeAi(const std::string& s, int phase) {
    Character::FoeAi a;
    const std::vector<int> v = parseDots(s);
    auto at = [&](size_t i, int lo, int hi, int def) { return i < v.size() ? std::clamp(v[i], lo, hi) : def; };
    a.move = at(0, 0, static_cast<int>(Move::COUNT_) - 1, 0);
    a.target = at(1, -1, 199, -1);
    a.cooldown = at(2, 0, 9, 0);
    a.flags = at(3, 0, 0xFFFF, 0);
    a.stolenGold = at(4, 0, 10000000, 0);
    a.stagger = at(5, 0, 10000000, 0);
    a.lastHitBy = at(6, -1, 99, -1);
    a.alt = at(7, 0, 99, 0);
    a.lastTarget = at(8, -1, 99, -1);
    a.critThisRound = at(9, 0, 1, 0) == 1;
    a.phase = std::clamp(phase, 0, 3);
    return a;
}

/** Status codes for the UI ("B2" = Burn, 2 rounds). Order is stable. */
inline std::string statusCodes(const Character& c) {
    std::string out;
    auto add = [&](char k, int n) { if (n > 0) { if (!out.empty()) out += "."; out += k; out += std::to_string(n); } };
    add('B', c.status.burn);
    add('M', c.status.marked);
    add('G', c.status.guarded);
    add('L', c.status.blind);
    add('S', c.status.slowed);
    add('K', c.status.shackled);
    add('I', c.status.inspired);
    if (c.nextAttackPenalty > 0) add('R', 1);
    return out;
}

} // namespace ai
} // namespace dnd

#endif // ASHEN_ENEMY_AI_H
