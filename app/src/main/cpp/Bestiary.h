#ifndef ASHEN_BESTIARY_H
#define ASHEN_BESTIARY_H

#include <cstring>
#include <string>

/**
 * Stable enemy IDs (Phase 1 of the Ash & Ember redesign).
 *
 * Every spawned foe carries Character::enemyId. Sprite lookup (Kotlin), boss tiers,
 * boss-seen flags, endgame loot gates and saves key off this ID — never off the
 * display name. Display names may change freely; IDs must not.
 *
 * The ID string doubles as the Android drawable name for the sprite
 * (MainActivity.spriteForEnemyId): res/drawable/<id>.xml, one distinct Ash & Ember
 * silhouette per ID (Phase 2, v2.21). Nothing borrows another foe's art.
 *
 * legacyName = the pre-v2.20 display name. Used ONLY by save migration
 * (Game::deserialize) for saves written before enemyId existed. Any other former
 * display name (e.g. a v2.20 Phase-1 name that Phase 2 renamed) goes in aliases().
 *
 * Saves that DO carry an enemyId are re-labelled with the current displayName on
 * load, so renames reach existing v2.20+ saves without touching the save format.
 */
namespace dnd {
namespace bestiary {

/** v2.30 enemy AI: signature behaviour per ID (EnemyAi.h acts on it). Append-only values. */
enum Behaviour : int {
    BEH_STRIKE = 0,     // plain attacks
    BEH_SNATCH = 1,     // wickthieves: steal 5% gold on hit; slip away when low
    BEH_BACKSTAB = 2,   // cutpurse / cache: +1 die on the lowest-AC hero
    BEH_SHIELDWALL = 3, // ashbound: Guard the most hurt ally (+3 AC)
    BEH_PACK = 4,       // gloamhound: +1 to hit per other hound on the same hero
    BEH_SWARM = 5,      // tallow rat: +1 damage per other living rat
    BEH_RALLY = 6,      // wick crier: Rally cry (+2 to hit for allies)
    BEH_KNIT = 7,       // bone crier: Rally cry + Knit bones (heals undead 20%)
    BEH_SLAM = 8,       // millstone brute: telegraphed 2x Millstone Slam (stagger cancels)
    BEH_SHACKLE = 9,    // tithe enforcer: Shackle (no special next turn, CON save)
    BEH_FINISHER = 10,  // ash scavenger: always the lowest HP% hero
    BEH_MARKER = 11,    // tithe watcher: Mark + Cinder bolt (auto-hit 0.7x)
    BEH_BOSS = 12,      // per-boss phase script (EnemyAi.h / Game::planBossMove)
};

struct EnemyArchetype {
    const char* id;
    const char* displayName;
    const char* legacyName;
    int bossTier;     // 0 = not a boss (loot/raid-key/gating); 1..5 as before
    bool endgameBoss; // Three Drowned Lanterns
    /** v2.29 encounter-curve role (curve::Role in CombatCurve.h): 1 minion, 2 skirmisher,
     *  3 caster, 4 support, 5 brute, 6 elite, 7 boss. Appended field; IDs unchanged. */
    int role;
    /** v2.30: signature behaviour (Behaviour), undead (never flees; Knit bones heals it), and
     *  whether it may slip away at low HP (wickthieves and hounds only). Appended fields. */
    int behaviour;
    bool undead;
    bool canFlee;
};

// NOTE: append only — never rename or reuse an id (saves + online sync store it).
inline const EnemyArchetype* table(size_t& count) {
    static const EnemyArchetype kTable[] = {
        // Wickthieves (were goblins)
        {"enemy_wickthief",          "Wickthief",            "Goblin",              0, false, 2, BEH_SNATCH, false, true},
        {"enemy_wickthief_scout",    "Wickthief Scout",      "Goblin Scout",        0, false, 2, BEH_SNATCH, false, true},
        {"enemy_wickthief_cutpurse", "Wickthief Cutpurse",   "Goblin Ambusher",     0, false, 2, BEH_BACKSTAB, false, true},
        {"enemy_wickthief_cache",    "Cache Wickthief",      "Cache Goblin",        0, false, 2, BEH_BACKSTAB, false, true},
        {"enemy_wickthief_river",    "River Wickthief",      "River Goblin",        0, false, 2, BEH_SNATCH, false, true},
        {"enemy_wickthief_root",     "Root Wickthief",       "Root Goblin",         0, false, 2, BEH_SNATCH, false, true},
        {"enemy_wickthief_fen",      "Fen Wickthief",        "Marsh Goblin",        0, false, 2, BEH_SNATCH, false, true},
        // Ashbound (were skeletons)
        {"enemy_ashbound",           "Ashbound",             "Skeleton",            0, false, 5, BEH_SHIELDWALL, true, false},
        {"enemy_ashbound_bonebound", "Bonebound Ashbound",   "Bonebound Skeleton",  0, false, 5, BEH_SHIELDWALL, true, false},
        // Gloamhound (was wolf)
        {"enemy_gloamhound",         "Gloamhound",           "Wolf",                0, false, 2, BEH_PACK, false, true},
        // Criers (heralds)
        {"enemy_crier_bone",         "Bone Crier",           "Bone Herald",         0, false, 4, BEH_KNIT, true, false},
        {"enemy_crier_wick",         "Wickthief Herald",     "Goblin Herald",       0, false, 4, BEH_RALLY, false, false},
        // Renamed in Phase 2 (v2.21) — display only; IDs unchanged. Old names live in kAliases.
        {"enemy_tallow_rat",         "Tallow Rat",           "Giant Rat",           0, false, 1, BEH_SWARM, false, false},
        {"enemy_brute",              "Millstone Brute",      "Ogre",                0, false, 5, BEH_SLAM, false, false},
        {"enemy_tithe_enforcer",     "Tithe Enforcer",       "Debt Enforcer",       0, false, 5, BEH_SHACKLE, false, false},
        {"enemy_tithe_scavenger",    "Ash Scavenger",        "Ash Scavenger",       0, false, 2, BEH_FINISHER, false, false},
        {"enemy_tithe_watcher",      "Tithe Watcher",        "Ashwake Scout",       0, false, 3, BEH_MARKER, false, false},
        // Story bosses (bestiary names). Tier kept exactly as before (only the Warden was tiered).
        {"boss_sexton",              "The Lantern Sexton",   "Skeleton Champion",   0, false, 7, BEH_BOSS, true, false},
        {"boss_collector",           "Collector Sallow Venn","The Collector",       0, false, 7, BEH_BOSS, false, false},
        {"boss_warden",              "The First Lamplighter","Breach Warden",       3, false, 7, BEH_BOSS, true, false},
        {"boss_herald",              "The Bell-Herald of the Tithe", "Ashwake Herald", 0, false, 7, BEH_BOSS, false, false},
        // Crawl bosses
        {"boss_wick_king",           "The Wick King",        "Goblin King",         1, false, 7, BEH_BOSS, false, false},
        {"boss_prior",               "The Ossuary Prior",    "Skeleton King",       2, false, 7, BEH_BOSS, true, false},
        {"boss_drake",               "Ashen Drake",          "Ashen Drake",         3, false, 7, BEH_BOSS, false, false},
        // Endgame — the Three Drowned Lanterns
        {"boss_hollow_crown",        "Hollow Crown",         "Hollow Crown",        4, true, 7, BEH_BOSS, true, false},
        {"boss_ember_hydra",         "Ember Hydra",          "Ember Hydra",         5, true, 7, BEH_BOSS, false, false},
        {"boss_nightfang",           "Nightfang Matriarch",  "Nightfang Matriarch", 4, true, 7, BEH_BOSS, false, false},
    };
    count = sizeof(kTable) / sizeof(kTable[0]);
    return kTable;
}

struct NameAlias {
    const char* name; // a former display name (pre-v2.20 or v2.20 Phase 1)
    const char* id;
};

/** Former display names that are neither the legacyName nor the current displayName. */
inline const NameAlias* aliases(size_t& count) {
    static const NameAlias kAliases[] = {
        // v2.20 (Phase 1) display names renamed in v2.21 (Phase 2). The pre-v2.20 names
        // for these are the table legacyName (Giant Rat, Ogre, Debt Enforcer, Ashwake
        // Scout, Bone Herald) — identical to the Phase-1 names except where listed here.
        {"Giant Rat",      "enemy_tallow_rat"},
        {"Ogre",           "enemy_brute"},
        {"Debt Enforcer",  "enemy_tithe_enforcer"},
        {"Ashwake Scout",  "enemy_tithe_watcher"},
        {"Bone Herald",    "enemy_crier_bone"},
    };
    count = sizeof(kAliases) / sizeof(kAliases[0]);
    return kAliases;
}

inline const EnemyArchetype* find(const std::string& id) {
    if (id.empty()) return nullptr;
    size_t n = 0;
    const EnemyArchetype* t = table(n);
    for (size_t i = 0; i < n; ++i) if (id == t[i].id) return &t[i];
    return nullptr;
}

/** Display name for an ID (falls back to the id itself — should never happen). */
inline std::string displayName(const std::string& id) {
    const EnemyArchetype* a = find(id);
    return a ? std::string(a->displayName) : id;
}

inline int bossTier(const std::string& id) {
    const EnemyArchetype* a = find(id);
    return a ? a->bossTier : 0;
}

inline bool isBoss(const std::string& id) { return bossTier(id) > 0; }

/** Encounter-curve role for an ID (curve::Role as int); unknown IDs act as skirmishers (2). */
inline int role(const std::string& id) {
    const EnemyArchetype* a = find(id);
    return a ? a->role : 2;
}

inline int behaviour(const std::string& id) {
    const EnemyArchetype* a = find(id);
    return a ? a->behaviour : BEH_STRIKE;
}
inline bool isUndead(const std::string& id) {
    const EnemyArchetype* a = find(id);
    return a && a->undead;
}
/** Wickthieves and hounds only — never undead, never bosses (approved rule 7). */
inline bool canFlee(const std::string& id) {
    const EnemyArchetype* a = find(id);
    return a && a->canFlee && !a->undead && a->role != 7 && a->bossTier == 0;
}

inline bool isEndgameBoss(const std::string& id) {
    const EnemyArchetype* a = find(id);
    return a && a->endgameBoss;
}

/**
 * SAVE MIGRATION ONLY: map a pre-ID save's enemy name to an ID.
 * Exact legacy or current display name first, then legacy substring rules that
 * mirror the old isBossName/spriteFor matching (e.g. "Dragon" -> drake).
 * Returns "" if nothing matches (caller keeps the name; sprite uses enemy_unknown).
 */
inline std::string migrateLegacyName(const std::string& name) {
    size_t n = 0;
    const EnemyArchetype* t = table(n);
    for (size_t i = 0; i < n; ++i) {
        if (name == t[i].legacyName || name == t[i].displayName) return t[i].id;
    }
    size_t na = 0;
    const NameAlias* al = aliases(na);
    for (size_t i = 0; i < na; ++i) if (name == al[i].name) return al[i].id;
    auto has = [&](const char* s) { return name.find(s) != std::string::npos; };
    // Bosses first (most specific), same precedence as the old substring logic.
    if (has("Hollow Crown")) return "boss_hollow_crown";
    if (has("Ember Hydra")) return "boss_ember_hydra";
    if (has("Nightfang")) return "boss_nightfang";
    if (has("Ashen Drake") || has("Dragon")) return "boss_drake";
    if (has("Breach Warden")) return "boss_warden";
    if (has("Ashwake Herald")) return "boss_herald";
    if (has("Collector")) return "boss_collector";
    if (has("Skeleton Champion")) return "boss_sexton";
    if (has("Skeleton King")) return "boss_prior";
    if (has("Goblin King")) return "boss_wick_king";
    if (has("Bone Herald") || has("Bone Crier")) return "enemy_crier_bone";
    if (has("Goblin Herald")) return "enemy_crier_wick";
    if (has("Bonebound")) return "enemy_ashbound_bonebound";
    if (has("Skeleton")) return "enemy_ashbound";
    if (has("Wolf")) return "enemy_gloamhound";
    if (has("Ogre") || has("Millstone Brute")) return "enemy_brute";
    if (has("Debt Enforcer") || has("Tithe Enforcer")) return "enemy_tithe_enforcer";
    if (has("Ashwake Scout") || has("Tithe Watcher")) return "enemy_tithe_watcher";
    if (has("Ash Scavenger")) return "enemy_tithe_scavenger";
    if (has("Rat")) return "enemy_tallow_rat";
    if (has("Goblin")) return "enemy_wickthief";
    return "";
}

} // namespace bestiary
} // namespace dnd

#endif // ASHEN_BESTIARY_H
