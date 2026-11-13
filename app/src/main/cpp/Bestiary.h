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
 * The ID string doubles as the Android drawable entry for the sprite
 * (MainActivity.spriteForEnemyId). Every ID has its own entry there; foes without
 * new art map to a placeholder through their own entry, not a shared fallback.
 *
 * legacyName = the pre-v2.20 display name. Used ONLY by save migration
 * (Game::deserialize) for saves written before enemyId existed.
 */
namespace dnd {
namespace bestiary {

struct EnemyArchetype {
    const char* id;
    const char* displayName;
    const char* legacyName;
    int bossTier;     // 0 = not a boss (loot/raid-key/gating); 1..5 as before
    bool endgameBoss; // Three Drowned Lanterns
};

// NOTE: append only — never rename or reuse an id (saves + online sync store it).
inline const EnemyArchetype* table(size_t& count) {
    static const EnemyArchetype kTable[] = {
        // Wickthieves (were goblins) — Ash & Ember art: enemy_wickthief
        {"enemy_wickthief",          "Wickthief",            "Goblin",              0, false},
        {"enemy_wickthief_scout",    "Wickthief Scout",      "Goblin Scout",        0, false},
        {"enemy_wickthief_cutpurse", "Wickthief Cutpurse",   "Goblin Ambusher",     0, false},
        {"enemy_wickthief_cache",    "Cache Wickthief",      "Cache Goblin",        0, false},
        {"enemy_wickthief_river",    "River Wickthief",      "River Goblin",        0, false},
        {"enemy_wickthief_root",     "Root Wickthief",       "Root Goblin",         0, false},
        {"enemy_wickthief_fen",      "Fen Wickthief",        "Marsh Goblin",        0, false},
        // Ashbound (were skeletons) — Ash & Ember art: enemy_ashbound
        {"enemy_ashbound",           "Ashbound",             "Skeleton",            0, false},
        {"enemy_ashbound_bonebound", "Bonebound Ashbound",   "Bonebound Skeleton",  0, false},
        // Gloamhound (was wolf) — placeholder art
        {"enemy_gloamhound",         "Gloamhound",           "Wolf",                0, false},
        // Criers (heralds) — placeholder art
        {"enemy_crier_bone",         "Bone Herald",          "Bone Herald",         0, false},
        {"enemy_crier_wick",         "Wickthief Herald",     "Goblin Herald",       0, false},
        // Not yet re-art'd: keep names, own ID entries (placeholder art)
        {"enemy_tallow_rat",         "Giant Rat",            "Giant Rat",           0, false},
        {"enemy_brute",              "Ogre",                 "Ogre",                0, false},
        {"enemy_tithe_enforcer",     "Debt Enforcer",        "Debt Enforcer",       0, false},
        {"enemy_tithe_scavenger",    "Ash Scavenger",        "Ash Scavenger",       0, false},
        {"enemy_tithe_watcher",      "Ashwake Scout",        "Ashwake Scout",       0, false},
        // Story bosses (bestiary names). Tier kept exactly as before (only the Warden was tiered).
        {"boss_sexton",              "The Lantern Sexton",   "Skeleton Champion",   0, false},
        {"boss_collector",           "Collector Sallow Venn","The Collector",       0, false},
        {"boss_warden",              "The First Lamplighter","Breach Warden",       3, false},
        {"boss_herald",              "The Bell-Herald of the Tithe", "Ashwake Herald", 0, false},
        // Crawl bosses
        {"boss_wick_king",           "The Wick King",        "Goblin King",         1, false},
        {"boss_prior",               "The Ossuary Prior",    "Skeleton King",       2, false},
        {"boss_drake",               "Ashen Drake",          "Ashen Drake",         3, false},
        // Endgame — the Three Drowned Lanterns
        {"boss_hollow_crown",        "Hollow Crown",         "Hollow Crown",        4, true},
        {"boss_ember_hydra",         "Ember Hydra",          "Ember Hydra",         5, true},
        {"boss_nightfang",           "Nightfang Matriarch",  "Nightfang Matriarch", 4, true},
    };
    count = sizeof(kTable) / sizeof(kTable[0]);
    return kTable;
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
    if (has("Bone Herald")) return "enemy_crier_bone";
    if (has("Goblin Herald")) return "enemy_crier_wick";
    if (has("Bonebound")) return "enemy_ashbound_bonebound";
    if (has("Skeleton")) return "enemy_ashbound";
    if (has("Wolf")) return "enemy_gloamhound";
    if (has("Ogre")) return "enemy_brute";
    if (has("Rat")) return "enemy_tallow_rat";
    if (has("Goblin")) return "enemy_wickthief";
    return "";
}

} // namespace bestiary
} // namespace dnd

#endif // ASHEN_BESTIARY_H
